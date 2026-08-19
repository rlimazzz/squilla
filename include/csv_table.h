#ifndef SQUILLA_CSV_TABLE_H
#define SQUILLA_CSV_TABLE_H

#include <stdbool.h>

// A CSV file is treated as a "table": its first line is the header (column
// names separated by CSV_DELIMITER), every line after that is a row.
#define CSV_DELIMITER ';'
#define CSV_MAX_COLUMNS 32
#define CSV_MAX_VALUE_LEN 256
#define CSV_MAX_LINE_LEN (CSV_MAX_COLUMNS * CSV_MAX_VALUE_LEN)

typedef enum {
  CSV_OK,
  CSV_FILE_NOT_FOUND,
  CSV_EMPTY_FILE,
  CSV_COLUMN_COUNT_MISMATCH,
  CSV_UNKNOWN_COLUMN
} CsvResult;

typedef struct {
  char names[CSV_MAX_COLUMNS][CSV_MAX_VALUE_LEN];
  int count;
} CsvHeader;

// Optional WHERE (equality on one column) and ORDER BY (one column, either
// direction) clauses for a SELECT.
typedef struct {
  bool has_where;
  char where_column[CSV_MAX_VALUE_LEN];
  char where_value[CSV_MAX_VALUE_LEN];

  bool has_order_by;
  char order_by_column[CSV_MAX_VALUE_LEN];
  bool order_by_descending;
} CsvQuery;

// Splits `line` on `delimiter` into up to CSV_MAX_COLUMNS fields, honoring
// double-quoted fields (RFC 4180 style): a quoted field may contain the
// delimiter or leading/trailing spaces literally, and "" inside quotes is an
// escaped quote. Unquoted fields are trimmed of surrounding whitespace.
// Returns how many fields were found.
int csv_split_fields(const char* line, char delimiter,
                      char fields[][CSV_MAX_VALUE_LEN]);

// Reads just the header line of `filename` into `header`.
CsvResult csv_read_header(const char* filename, CsvHeader* header);

// Appends one row to `filename`, failing if `value_count` doesn't match the
// file's existing column count.
CsvResult csv_insert_row(const char* filename,
                          char values[][CSV_MAX_VALUE_LEN], int value_count);

// Prints every row of `filename` (header included) as "(col1, col2, ...)",
// optionally filtered by `query->where_*` and sorted by `query->order_by_*`.
CsvResult csv_print_rows(const char* filename, const CsvQuery* query);

#endif
