#ifndef SQUILLA_TABLE_H
#define SQUILLA_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "util.h"

// A table is a binary file: a fixed-size header (magic number + column
// definitions) followed by fixed-size rows. Every value is stored in a
// column's declared width, null-padded/truncated to fit — there is no
// delimiter to parse, unlike a text format such as CSV.
#define TABLE_MAGIC 0x53514c31u  // "SQL1"
#define TABLE_MAX_FILENAME_LEN 256
#define TABLE_DEFAULT_COLUMN_WIDTH 32
#define COLUMN_NAME_MAX_LEN 32

typedef struct {
  char name[COLUMN_NAME_MAX_LEN];
  uint32_t width;  // max characters this column can hold
} ColumnDef;

// Fixed-size regardless of column_count, so row data always starts at the
// same offset (sizeof(TableHeader)) and never needs to be relocated.
typedef struct {
  uint32_t magic;
  uint32_t column_count;
  ColumnDef columns[MAX_FIELDS];
} TableHeader;

typedef enum {
  TABLE_OK,
  TABLE_NOT_FOUND,
  TABLE_EMPTY,
  TABLE_COLUMN_COUNT_MISMATCH,
  TABLE_UNKNOWN_COLUMN,
  TABLE_ALREADY_EXISTS,
  TABLE_VALUE_TOO_LONG,
  TABLE_IMPORT_SOURCE_NOT_FOUND,
  TABLE_IMPORT_SOURCE_EMPTY
} TableResult;

// Optional WHERE (equality on one column) and ORDER BY (one column, either
// direction) clauses for a SELECT.
typedef struct {
  bool has_where;
  char where_column[MAX_FIELD_LEN];
  char where_value[MAX_FIELD_LEN];

  bool has_order_by;
  char order_by_column[MAX_FIELD_LEN];
  bool order_by_descending;
} TableQuery;

// Resolves a table name to its backing file: "pessoas" -> "pessoas.tbl",
// while a name that already has an extension is used as-is.
void table_path(const char* name, char* out, int out_size);

// Trims a trailing ".tbl" off `filename` in place and returns it, so
// user-facing messages can refer to the table by name, not by file.
char* table_display_name(char* filename);

// Creates `filename` with the given columns (parallel `widths` array, one
// max-character-count per column), failing if the file already exists.
TableResult table_create(const char* filename, char columns[][MAX_FIELD_LEN],
                          const int* widths, int column_count);

// Deletes `filename`.
TableResult table_drop(const char* filename);

// Reads just the header of `filename` into `header`.
TableResult table_read_header(const char* filename, TableHeader* header);

// Appends one fixed-size row to `filename`, failing if `value_count`
// doesn't match the table's column count or a value overflows its column's
// declared width.
TableResult table_insert_row(const char* filename,
                              char values[][MAX_FIELD_LEN], int value_count);

// Prints every row of `filename` (header included) as "(col1, col2, ...)",
// optionally filtered by `query->where_*` and sorted by `query->order_by_*`.
TableResult table_print_rows(const char* filename, const TableQuery* query);

#endif
