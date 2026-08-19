#include "csv_table.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

typedef struct {
  char fields[CSV_MAX_COLUMNS][CSV_MAX_VALUE_LEN];
  int count;
} CsvRow;

static int find_column(const CsvHeader* header, const char* name) {
  for (int i = 0; i < header->count; i++) {
    if (strcmp(header->names[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int csv_split_fields(const char* line, char delimiter,
                      char fields[][CSV_MAX_VALUE_LEN]) {
  int count = 0;
  const char* p = line;

  while (count < CSV_MAX_COLUMNS) {
    while (*p == ' ' || *p == '\t') {
      p++;
    }

    bool quoted = (*p == '"');
    if (quoted) {
      p++;
    }

    char* out = fields[count];
    int out_len = 0;

    while (*p != '\0') {
      if (quoted) {
        if (*p == '"' && *(p + 1) == '"') {
          if (out_len < CSV_MAX_VALUE_LEN - 1) out[out_len++] = '"';
          p += 2;
          continue;
        }
        if (*p == '"') {
          p++;
          quoted = false;  // closing quote found; field ends at next delimiter
          continue;
        }
      } else if (*p == delimiter) {
        break;
      }

      if (out_len < CSV_MAX_VALUE_LEN - 1) out[out_len++] = *p;
      p++;
    }
    out[out_len] = '\0';
    count++;

    if (*p != delimiter) {
      break;
    }
    p++;  // skip the delimiter and parse the next field
  }

  return count;
}

// Wraps `value` in double quotes (escaping embedded quotes as "") when it
// contains a character that would otherwise be ambiguous in the CSV file.
static void write_csv_field(FILE* file, const char* value) {
  bool needs_quotes =
      strchr(value, CSV_DELIMITER) || strchr(value, '"') || value[0] == ' ' ||
      (*value != '\0' && value[strlen(value) - 1] == ' ');

  if (!needs_quotes) {
    fputs(value, file);
    return;
  }

  fputc('"', file);
  for (const char* p = value; *p != '\0'; p++) {
    if (*p == '"') {
      fputc('"', file);
    }
    fputc(*p, file);
  }
  fputc('"', file);
}

CsvResult csv_read_header(const char* filename, CsvHeader* header) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    return CSV_FILE_NOT_FOUND;
  }

  char line[CSV_MAX_LINE_LEN];
  if (fgets(line, sizeof(line), file) == NULL) {
    fclose(file);
    return CSV_EMPTY_FILE;
  }
  fclose(file);

  str_trim(line);
  header->count = csv_split_fields(line, CSV_DELIMITER, header->names);

  return CSV_OK;
}

CsvResult csv_insert_row(const char* filename,
                          char values[][CSV_MAX_VALUE_LEN], int value_count) {
  CsvHeader header;
  CsvResult result = csv_read_header(filename, &header);
  if (result != CSV_OK) {
    return result;
  }

  if (value_count != header.count) {
    return CSV_COLUMN_COUNT_MISMATCH;
  }

  FILE* file = fopen(filename, "a");
  if (file == NULL) {
    return CSV_FILE_NOT_FOUND;
  }

  for (int i = 0; i < value_count; i++) {
    write_csv_field(file, values[i]);
    fputc((i == value_count - 1) ? '\n' : CSV_DELIMITER, file);
  }
  fclose(file);

  return CSV_OK;
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

static void print_row(char fields[][CSV_MAX_VALUE_LEN], int count) {
  printf("(");
  for (int i = 0; i < count; i++) {
    print_display_field(fields[i]);
    printf("%s", (i == count - 1) ? "" : ", ");
  }
  printf(")\n");
}

// Parses `s` as a double, requiring the whole (trimmed) string to be
// consumed so values like "12abc" are treated as text, not numbers.
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
  const char* value_a = ((const CsvRow*)a)->fields[g_order_by_index];
  const char* value_b = ((const CsvRow*)b)->fields[g_order_by_index];

  double num_a, num_b;
  int cmp;
  if (parse_double(value_a, &num_a) && parse_double(value_b, &num_b)) {
    cmp = (num_a > num_b) - (num_a < num_b);
  } else {
    cmp = strcmp(value_a, value_b);
  }

  return g_order_by_descending ? -cmp : cmp;
}

CsvResult csv_print_rows(const char* filename, const CsvQuery* query) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    return CSV_FILE_NOT_FOUND;
  }

  char line[CSV_MAX_LINE_LEN];
  if (fgets(line, sizeof(line), file) == NULL) {
    fclose(file);
    return CSV_EMPTY_FILE;
  }
  str_trim(line);

  CsvHeader header;
  header.count = csv_split_fields(line, CSV_DELIMITER, header.names);

  int where_index = -1;
  if (query->has_where &&
      (where_index = find_column(&header, query->where_column)) < 0) {
    fclose(file);
    return CSV_UNKNOWN_COLUMN;
  }

  int order_by_index = -1;
  if (query->has_order_by &&
      (order_by_index = find_column(&header, query->order_by_column)) < 0) {
    fclose(file);
    return CSV_UNKNOWN_COLUMN;
  }

  CsvRow* rows = NULL;
  int row_count = 0;
  int capacity = 0;

  while (fgets(line, sizeof(line), file) != NULL) {
    str_trim(line);
    if (line[0] == '\0') {
      continue;
    }

    CsvRow row;
    row.count = csv_split_fields(line, CSV_DELIMITER, row.fields);

    if (query->has_where &&
        strcmp(row.fields[where_index], query->where_value) != 0) {
      continue;
    }

    if (row_count == capacity) {
      capacity = (capacity == 0) ? 16 : capacity * 2;
      rows = realloc(rows, capacity * sizeof(CsvRow));
    }
    rows[row_count++] = row;
  }
  fclose(file);

  if (query->has_order_by) {
    g_order_by_index = order_by_index;
    g_order_by_descending = query->order_by_descending;
    qsort(rows, row_count, sizeof(CsvRow), compare_rows);
  }

  print_row(header.names, header.count);
  for (int i = 0; i < row_count; i++) {
    print_row(rows[i].fields, rows[i].count);
  }
  free(rows);

  return (row_count == 0) ? CSV_EMPTY_FILE : CSV_OK;
}
