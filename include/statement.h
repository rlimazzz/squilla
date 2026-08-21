#ifndef SQUILLA_STATEMENT_H
#define SQUILLA_STATEMENT_H

#include "input_buffer.h"
#include "table.h"

#define STATEMENT_MAX_FILENAME_LEN TABLE_MAX_FILENAME_LEN

typedef enum {
  STATEMENT_CREATE_TABLE,
  STATEMENT_DROP_TABLE,
  STATEMENT_INSERT,
  STATEMENT_SELECT,
  STATEMENT_IMPORT
} StatementType;

typedef enum {
  PREPARE_SUCCESS,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_SYNTAX_ERROR
} PrepareResult;

typedef enum {
  EXECUTE_SUCCESS,
  EXECUTE_FILE_NOT_FOUND,
  EXECUTE_EMPTY_FILE,
  EXECUTE_COLUMN_COUNT_MISMATCH,
  EXECUTE_UNKNOWN_COLUMN,
  EXECUTE_TABLE_ALREADY_EXISTS,
  EXECUTE_VALUE_TOO_LONG,
  EXECUTE_IMPORT_SOURCE_NOT_FOUND,
  EXECUTE_IMPORT_SOURCE_EMPTY
} ExecuteResult;

// The "compiled" form of an input line: which table file it targets, plus
// whatever the statement needs: column names (and, for CREATE TABLE, their
// widths) for CREATE TABLE, values to append for INSERT, WHERE/ORDER BY
// clauses for SELECT, or the source CSV path for IMPORT.
typedef struct {
  StatementType type;
  char filename[STATEMENT_MAX_FILENAME_LEN];
  char values[MAX_FIELDS][MAX_FIELD_LEN];
  int value_count;
  int column_widths[MAX_FIELDS];  // used only by CREATE TABLE
  TableQuery query;
  char import_source[STATEMENT_MAX_FILENAME_LEN];  // used only by IMPORT
} Statement;

// Front-end: tokenizes/parses a line of input into a Statement.
PrepareResult prepare_statement(InputBuffer* input_buffer,
                                 Statement* statement);

// Back-end: runs an already-prepared statement against its target table.
ExecuteResult execute_statement(Statement* statement);

#endif
