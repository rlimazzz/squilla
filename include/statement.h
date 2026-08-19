#ifndef SQUILLA_STATEMENT_H
#define SQUILLA_STATEMENT_H

#include "csv_table.h"
#include "input_buffer.h"

#define STATEMENT_MAX_FILENAME_LEN 256

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

typedef enum {
  PREPARE_SUCCESS,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_SYNTAX_ERROR
} PrepareResult;

typedef enum {
  EXECUTE_SUCCESS,
  EXECUTE_FILE_NOT_FOUND,
  EXECUTE_EMPTY_FILE,
  EXECUTE_COLUMN_COUNT_MISMATCH
} ExecuteResult;

// The "compiled" form of an input line: which CSV file it targets and, for
// an INSERT, the values to append to it.
typedef struct {
  StatementType type;
  char filename[STATEMENT_MAX_FILENAME_LEN];
  char values[CSV_MAX_COLUMNS][CSV_MAX_VALUE_LEN];
  int value_count;
} Statement;

// Front-end: tokenizes/parses a line of input into a Statement.
PrepareResult prepare_statement(InputBuffer* input_buffer,
                                 Statement* statement);

// Back-end: runs an already-prepared statement against its target CSV file.
ExecuteResult execute_statement(Statement* statement);

#endif
