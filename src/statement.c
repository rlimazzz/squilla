#include "statement.h"

#include <stdbool.h>
#include <string.h>

#include "util.h"

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

// Parses a single value starting at `text`: either a bare token ending at
// the next space, or a double-quoted string (which may contain spaces; ""
// is an escaped quote). Writes the value into `out_value` and points
// `*out_remainder` at whatever in `text` follows it.
static bool parse_value(char* text, char* out_value, char** out_remainder) {
  while (*text == ' ') {
    text++;
  }

  int len = 0;
  if (*text == '"') {
    text++;
    while (*text != '\0') {
      if (*text == '"' && *(text + 1) == '"') {
        if (len < CSV_MAX_VALUE_LEN - 1) out_value[len++] = '"';
        text += 2;
        continue;
      }
      if (*text == '"') {
        text++;
        break;
      }
      if (len < CSV_MAX_VALUE_LEN - 1) out_value[len++] = *text;
      text++;
    }
  } else {
    while (*text != '\0' && *text != ' ') {
      if (len < CSV_MAX_VALUE_LEN - 1) out_value[len++] = *text;
      text++;
    }
    if (len == 0) {
      return false;
    }
  }
  out_value[len] = '\0';

  *out_remainder = text;
  return true;
}

// Parses the optional "where <coluna> = <valor>" clause at the start of
// `rest`, if present, advancing `rest` past it.
static PrepareResult parse_where_clause(char** rest, CsvQuery* query) {
  if (strncmp(*rest, "where ", 6) != 0) {
    return PREPARE_SUCCESS;
  }
  char* text = *rest + 6;

  char* column = strtok(text, " ");
  char* eq = strtok(NULL, " ");
  char* value_text = strtok(NULL, "");
  if (column == NULL || eq == NULL || strcmp(eq, "=") != 0 ||
      value_text == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  char* remainder;
  if (!parse_value(value_text, query->where_value, &remainder)) {
    return PREPARE_SYNTAX_ERROR;
  }
  strncpy(query->where_column, column, CSV_MAX_VALUE_LEN - 1);
  query->where_column[CSV_MAX_VALUE_LEN - 1] = '\0';
  query->has_where = true;

  *rest = str_trim(remainder);
  return PREPARE_SUCCESS;
}

// Parses the optional "order by <coluna> [asc|desc]" clause at the start of
// `rest`, if present.
static PrepareResult parse_order_by_clause(char* rest, CsvQuery* query) {
  if (rest[0] == '\0') {
    return PREPARE_SUCCESS;
  }
  if (strncmp(rest, "order by ", 9) != 0) {
    return PREPARE_SYNTAX_ERROR;
  }
  rest = str_trim(rest + 9);

  char* column = strtok(rest, " ");
  if (column == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }
  strncpy(query->order_by_column, column, CSV_MAX_VALUE_LEN - 1);
  query->order_by_column[CSV_MAX_VALUE_LEN - 1] = '\0';
  query->has_order_by = true;

  char* direction = strtok(NULL, " ");
  if (direction == NULL || strcmp(direction, "asc") == 0) {
    query->order_by_descending = false;
  } else if (strcmp(direction, "desc") == 0) {
    query->order_by_descending = true;
  } else {
    return PREPARE_SYNTAX_ERROR;
  }

  return PREPARE_SUCCESS;
}

// Parses "select <arquivo.csv> [where <coluna> = <valor>]
// [order by <coluna> [asc|desc]]".
static PrepareResult prepare_select(InputBuffer* input_buffer,
                                     Statement* statement) {
  statement->type = STATEMENT_SELECT;
  statement->query = (CsvQuery){0};

  strtok(input_buffer->buffer, " ");  // discard the "select" keyword itself
  char* filename = strtok(NULL, " ");
  if (filename == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }
  copy_filename(statement, filename);

  char* rest = strtok(NULL, "");  // everything after the filename
  if (rest == NULL) {
    return PREPARE_SUCCESS;
  }
  rest = str_trim(rest);

  PrepareResult result = parse_where_clause(&rest, &statement->query);
  if (result != PREPARE_SUCCESS) {
    return result;
  }

  return parse_order_by_clause(rest, &statement->query);
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
    case CSV_UNKNOWN_COLUMN:
      return EXECUTE_UNKNOWN_COLUMN;
  }
  return EXECUTE_SUCCESS;
}

static ExecuteResult execute_insert(Statement* statement) {
  return to_execute_result(csv_insert_row(
      statement->filename, statement->values, statement->value_count));
}

static ExecuteResult execute_select(Statement* statement) {
  return to_execute_result(
      csv_print_rows(statement->filename, &statement->query));
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
