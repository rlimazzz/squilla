#include "statement.h"

#include <string.h>

static void copy_filename(Statement* statement, const char* filename) {
  strncpy(statement->filename, filename, STATEMENT_MAX_FILENAME_LEN - 1);
  statement->filename[STATEMENT_MAX_FILENAME_LEN - 1] = '\0';
}

// Parses "insert <arquivo.csv> <v1>, <v2>, ...": a filename token followed
// by a comma-separated list of values, one per CSV column.
static PrepareResult prepare_insert(InputBuffer* input_buffer,
                                     Statement* statement) {
  statement->type = STATEMENT_INSERT;

  strtok(input_buffer->buffer, " ");  // discard the "insert" keyword itself
  char* filename = strtok(NULL, " ");
  char* rest = strtok(NULL, "");  // everything after the filename

  if (filename == NULL || rest == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }
  copy_filename(statement, filename);

  statement->value_count = csv_split_fields(rest, ',', statement->values);
  if (statement->value_count == 0) {
    return PREPARE_SYNTAX_ERROR;
  }

  return PREPARE_SUCCESS;
}

// Parses "select <arquivo.csv>".
static PrepareResult prepare_select(InputBuffer* input_buffer,
                                     Statement* statement) {
  statement->type = STATEMENT_SELECT;

  strtok(input_buffer->buffer, " ");  // discard the "select" keyword itself
  char* filename = strtok(NULL, " ");
  if (filename == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }
  copy_filename(statement, filename);

  return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer* input_buffer,
                                 Statement* statement) {
  if ((strncmp(input_buffer->buffer, "insert", 6) == 0) ||
      (strncmp(input_buffer->buffer, "INSERT", 6) == 0)) {
    return prepare_insert(input_buffer, statement);
  }

  if ((strncmp(input_buffer->buffer, "select", 6) == 0) ||
      (strncmp(input_buffer->buffer, "SELECT", 6) == 0)) {
    return prepare_select(input_buffer, statement);
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

static ExecuteResult to_execute_result(CsvResult result) {
  switch (result) {
    case CSV_OK:
      return EXECUTE_SUCCESS;
    case CSV_FILE_NOT_FOUND:
      return EXECUTE_FILE_NOT_FOUND;
    case CSV_EMPTY_FILE:
      return EXECUTE_EMPTY_FILE;
    case CSV_COLUMN_COUNT_MISMATCH:
      return EXECUTE_COLUMN_COUNT_MISMATCH;
  }
  return EXECUTE_SUCCESS;
}

static ExecuteResult execute_insert(Statement* statement) {
  return to_execute_result(csv_insert_row(
      statement->filename, statement->values, statement->value_count));
}

static ExecuteResult execute_select(Statement* statement) {
  return to_execute_result(csv_print_rows(statement->filename));
}

ExecuteResult execute_statement(Statement* statement) {
  switch (statement->type) {
    case STATEMENT_INSERT:
      return execute_insert(statement);
    case STATEMENT_SELECT:
      return execute_select(statement);
  }

  return EXECUTE_SUCCESS;
}
