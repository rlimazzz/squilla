#include "csv_table.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

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

CsvResult csv_print_rows(const char* filename) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    return CSV_FILE_NOT_FOUND;
  }

  char line[CSV_MAX_LINE_LEN];
  char fields[CSV_MAX_COLUMNS][CSV_MAX_VALUE_LEN];
  int printed_any = 0;

  while (fgets(line, sizeof(line), file) != NULL) {
    str_trim(line);
    if (line[0] == '\0') {
      continue;
    }

    int field_count = csv_split_fields(line, CSV_DELIMITER, fields);
    printed_any = 1;

    printf("(");
    for (int i = 0; i < field_count; i++) {
      print_display_field(fields[i]);
      printf("%s", (i == field_count - 1) ? "" : ", ");
    }
    printf(")\n");
  }
  fclose(file);

  if (!printed_any) {
    return CSV_EMPTY_FILE;
  }

  return CSV_OK;
}
