#include "table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  char fields[MAX_FIELDS][MAX_FIELD_LEN];
  int count;
} TableRow;

static int find_column(const TableHeader* header, const char* name) {
  for (uint32_t i = 0; i < header->column_count; i++) {
    if (strcmp(header->columns[i].name, name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

// On disk, a column's declared `width` characters are followed by a null
// terminator, so the field always occupies width + 1 bytes.
static uint32_t row_size(const TableHeader* header) {
  uint32_t size = 0;
  for (uint32_t i = 0; i < header->column_count; i++) {
    size += header->columns[i].width + 1;
  }
  return size;
}

void table_path(const char* name, char* out, int out_size) {
  const char* dot = strrchr(name, '.');
  if (dot != NULL) {
    strncpy(out, name, out_size - 1);
    out[out_size - 1] = '\0';
  } else {
    snprintf(out, out_size, "%s.tbl", name);
  }
}

char* table_display_name(char* filename) {
  size_t len = strlen(filename);
  if (len >= 4 && strcmp(filename + len - 4, ".tbl") == 0) {
    filename[len - 4] = '\0';
  }
  return filename;
}

TableResult table_read_header(const char* filename, TableHeader* header) {
  FILE* file = fopen(filename, "rb");
  if (file == NULL) {
    return TABLE_NOT_FOUND;
  }

  size_t read = fread(header, sizeof(TableHeader), 1, file);
  fclose(file);

  if (read != 1 || header->magic != TABLE_MAGIC) {
    return TABLE_NOT_FOUND;
  }

  return TABLE_OK;
}

TableResult table_create(const char* filename, char columns[][MAX_FIELD_LEN],
                          const int* widths, int column_count) {
  if (access(filename, F_OK) == 0) {
    return TABLE_ALREADY_EXISTS;
  }

  TableHeader header = {0};
  header.magic = TABLE_MAGIC;
  header.column_count = (uint32_t)column_count;
  for (int i = 0; i < column_count; i++) {
    strncpy(header.columns[i].name, columns[i], COLUMN_NAME_MAX_LEN - 1);
    header.columns[i].width = (uint32_t)widths[i];
  }

  FILE* file = fopen(filename, "wb");
  if (file == NULL) {
    return TABLE_NOT_FOUND;
  }
  fwrite(&header, sizeof(header), 1, file);
  fclose(file);

  return TABLE_OK;
}

TableResult table_drop(const char* filename) {
  if (access(filename, F_OK) != 0) {
    return TABLE_NOT_FOUND;
  }

  remove(filename);
  return TABLE_OK;
}

// Copies `value` into `dest`, a `width + 1`-byte slot, null-padding the
// remainder so the field is always a valid, null-terminated C string on disk.
static void write_field(char* dest, uint32_t width, const char* value) {
  memset(dest, 0, width + 1);
  strncpy(dest, value, width);
}

TableResult table_insert_row(const char* filename,
                              char values[][MAX_FIELD_LEN], int value_count) {
  TableHeader header;
  TableResult result = table_read_header(filename, &header);
  if (result != TABLE_OK) {
    return result;
  }

  if ((uint32_t)value_count != header.column_count) {
    return TABLE_COLUMN_COUNT_MISMATCH;
  }

  for (int i = 0; i < value_count; i++) {
    if (strlen(values[i]) > header.columns[i].width) {
      return TABLE_VALUE_TOO_LONG;
    }
  }

  uint32_t size = row_size(&header);
  char* row = malloc(size);

  uint32_t offset = 0;
  for (int i = 0; i < value_count; i++) {
    write_field(row + offset, header.columns[i].width, values[i]);
    offset += header.columns[i].width + 1;
  }

  FILE* file = fopen(filename, "ab");
  if (file == NULL) {
    free(row);
    return TABLE_NOT_FOUND;
  }
  fwrite(row, size, 1, file);
  fclose(file);
  free(row);

  return TABLE_OK;
}

// Prints `value` quoted (RFC 4180 style) if it contains a comma or a quote,
// so the ", "-joined row stays unambiguous to read.
static void print_display_field(const char* value) {
  bool needs_quotes = strchr(value, ',') || strchr(value, '"');
  if (!needs_quotes) {
    fputs(value, stdout);
    return;
  }

  fputc('"', stdout);
  for (const char* p = value; *p != '\0'; p++) {
    if (*p == '"') {
      fputc('"', stdout);
    }
    fputc(*p, stdout);
  }
  fputc('"', stdout);
}

static void print_row(char fields[][MAX_FIELD_LEN], int count) {
  printf("(");
  for (int i = 0; i < count; i++) {
    print_display_field(fields[i]);
    printf("%s", (i == count - 1) ? "" : ", ");
  }
  printf(")\n");
}

// Parses `s` as a double, requiring the whole string to be consumed so
// values like "12abc" are treated as text, not numbers.
static bool parse_double(const char* s, double* out) {
  char* end;
  *out = strtod(s, &end);
  if (end == s) {
    return false;
  }
  while (*end == ' ') {
    end++;
  }
  return *end == '\0';
}

// Sort context for compare_rows(): qsort's comparator takes no extra
// argument, so the column to sort by is stashed here right before sorting.
static int g_order_by_index;
static bool g_order_by_descending;

static int compare_rows(const void* a, const void* b) {
  const char* value_a = ((const TableRow*)a)->fields[g_order_by_index];
  const char* value_b = ((const TableRow*)b)->fields[g_order_by_index];

  double num_a, num_b;
  int cmp;
  if (parse_double(value_a, &num_a) && parse_double(value_b, &num_b)) {
    cmp = (num_a > num_b) - (num_a < num_b);
  } else {
    cmp = strcmp(value_a, value_b);
  }

  return g_order_by_descending ? -cmp : cmp;
}

TableResult table_print_rows(const char* filename, const TableQuery* query) {
  TableHeader header;
  TableResult result = table_read_header(filename, &header);
  if (result != TABLE_OK) {
    return result;
  }

  int where_index = -1;
  if (query->has_where &&
      (where_index = find_column(&header, query->where_column)) < 0) {
    return TABLE_UNKNOWN_COLUMN;
  }

  int order_by_index = -1;
  if (query->has_order_by &&
      (order_by_index = find_column(&header, query->order_by_column)) < 0) {
    return TABLE_UNKNOWN_COLUMN;
  }

  FILE* file = fopen(filename, "rb");
  if (file == NULL) {
    return TABLE_NOT_FOUND;
  }
  fseek(file, sizeof(TableHeader), SEEK_SET);

  uint32_t size = row_size(&header);
  char* raw_row = malloc(size);

  TableRow* rows = NULL;
  int row_count = 0;
  int capacity = 0;

  while (fread(raw_row, size, 1, file) == 1) {
    TableRow row;
    row.count = (int)header.column_count;

    uint32_t offset = 0;
    for (uint32_t i = 0; i < header.column_count; i++) {
      strncpy(row.fields[i], raw_row + offset, MAX_FIELD_LEN - 1);
      row.fields[i][MAX_FIELD_LEN - 1] = '\0';
      offset += header.columns[i].width + 1;
    }

    if (query->has_where &&
        strcmp(row.fields[where_index], query->where_value) != 0) {
      continue;
    }

    if (row_count == capacity) {
      capacity = (capacity == 0) ? 16 : capacity * 2;
      rows = realloc(rows, capacity * sizeof(TableRow));
    }
    rows[row_count++] = row;
  }
  fclose(file);
  free(raw_row);

  if (query->has_order_by) {
    g_order_by_index = order_by_index;
    g_order_by_descending = query->order_by_descending;
    qsort(rows, row_count, sizeof(TableRow), compare_rows);
  }

  char header_names[MAX_FIELDS][MAX_FIELD_LEN];
  for (uint32_t i = 0; i < header.column_count; i++) {
    strncpy(header_names[i], header.columns[i].name, MAX_FIELD_LEN - 1);
    header_names[i][MAX_FIELD_LEN - 1] = '\0';
  }
  print_row(header_names, (int)header.column_count);

  for (int i = 0; i < row_count; i++) {
    print_row(rows[i].fields, rows[i].count);
  }
  free(rows);

  return (row_count == 0) ? TABLE_EMPTY : TABLE_OK;
}
