#include "csv_import.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

// Picks ',' or ';' as the delimiter, whichever shows up more often in the
// header line — CSVs exported in different locales use either.
static char detect_delimiter(const char* line) {
  int commas = 0, semicolons = 0;
  for (const char* p = line; *p != '\0'; p++) {
    if (*p == ',') commas++;
    if (*p == ';') semicolons++;
  }
  return (semicolons > commas) ? ';' : ',';
}

TableResult table_import_csv(const char* table_filename,
                              const char* csv_filename) {
  FILE* file = fopen(csv_filename, "r");
  if (file == NULL) {
    return TABLE_IMPORT_SOURCE_NOT_FOUND;
  }

  char line[MAX_FIELDS * MAX_FIELD_LEN];
  if (fgets(line, sizeof(line), file) == NULL) {
    fclose(file);
    return TABLE_IMPORT_SOURCE_EMPTY;
  }
  str_trim(line);
  char delimiter = detect_delimiter(line);

  char columns[MAX_FIELDS][MAX_FIELD_LEN];
  int column_count = split_fields(line, delimiter, columns);
  if (column_count == 0) {
    fclose(file);
    return TABLE_IMPORT_SOURCE_EMPTY;
  }

  // First pass: size every column from the widest value seen, so the table
  // is created with tight-fitting fixed widths instead of one arbitrary
  // default for everything.
  int widths[MAX_FIELDS] = {0};
  long data_start = ftell(file);
  bool has_rows = false;

  char fields[MAX_FIELDS][MAX_FIELD_LEN];
  while (fgets(line, sizeof(line), file) != NULL) {
    str_trim(line);
    if (line[0] == '\0') {
      continue;
    }

    int field_count = split_fields(line, delimiter, fields);
    has_rows = true;
    for (int i = 0; i < field_count && i < column_count; i++) {
      int len = (int)strlen(fields[i]);
      if (len > widths[i]) {
        widths[i] = (len < MAX_FIELD_LEN) ? len : MAX_FIELD_LEN - 1;
      }
    }
  }

  if (!has_rows) {
    for (int i = 0; i < column_count; i++) {
      widths[i] = TABLE_DEFAULT_COLUMN_WIDTH;
    }
  } else {
    for (int i = 0; i < column_count; i++) {
      if (widths[i] == 0) widths[i] = 1;  // column was always empty
    }
  }

  TableResult result = table_create(table_filename, columns, widths,
                                     column_count);
  if (result != TABLE_OK) {
    fclose(file);
    return result;
  }

  // Second pass: now that the table exists, actually insert the rows.
  int imported = 0;
  int skipped = 0;
  fseek(file, data_start, SEEK_SET);
  while (fgets(line, sizeof(line), file) != NULL) {
    str_trim(line);
    if (line[0] == '\0') {
      continue;
    }

    int field_count = split_fields(line, delimiter, fields);
    if (field_count != column_count ||
        table_insert_row(table_filename, fields, field_count) != TABLE_OK) {
      skipped++;
      continue;
    }
    imported++;
  }
  fclose(file);

  printf("Imported %d row(s) from '%s'", imported, csv_filename);
  if (skipped > 0) {
    printf(", skipped %d malformed row(s)", skipped);
  }
  printf(".\n");

  return TABLE_OK;
}
