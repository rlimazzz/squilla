#include "statement.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "csv_import.h"
#include "util.h"

static void resolve_filename(Statement* statement, const char* name) {
  table_path(name, statement->filename, STATEMENT_MAX_FILENAME_LEN);
}

// Parses a "<coluna>" or "<coluna>(<tamanho>)" column spec, writing the name
// into `name_out` and the declared max width (or TABLE_DEFAULT_COLUMN_WIDTH
// if omitted) into `*width_out`.
static bool parse_column_spec(char* token, char* name_out, int* width_out) {
  char* paren = strchr(token, '(');
  if (paren == NULL) {
    strncpy(name_out, str_trim(token), MAX_FIELD_LEN - 1);
    name_out[MAX_FIELD_LEN - 1] = '\0';
    *width_out = TABLE_DEFAULT_COLUMN_WIDTH;
    return name_out[0] != '\0';
  }

  char* close = strchr(paren, ')');
  if (close == NULL) {
    return false;
  }

  *paren = '\0';
  strncpy(name_out, str_trim(token), MAX_FIELD_LEN - 1);
  name_out[MAX_FIELD_LEN - 1] = '\0';
  if (name_out[0] == '\0') {
    return false;
  }

  *close = '\0';
  char* size_text = str_trim(paren + 1);
  char* end;
  long width = strtol(size_text, &end, 10);
  if (end == size_text || *end != '\0' || width <= 0 ||
      width >= MAX_FIELD_LEN) {
    return false;
  }

  *width_out = (int)width;
  return true;
}

// Parses "create table <nome> (<col1>[(<tamanho>)], <col2>[(<tamanho>)],
// ...)".
static PrepareResult prepare_create_table(InputBuffer* input_buffer,
                                           Statement* statement) {
  statement->type = STATEMENT_CREATE_TABLE;

  strtok(input_buffer->buffer, " ");  // discard the "create" keyword itself
  char* table_kw = strtok(NULL, " ");
  char* rest = strtok(NULL, "");  // "<nome> (<col1>, <col2>, ...)"
  if (table_kw == NULL || strcmp(table_kw, "table") != 0 || rest == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }
  rest = str_trim(rest);

  char* paren_start = strchr(rest, '(');
  char* paren_end = strrchr(rest, ')');
  if (paren_start == NULL || paren_end == NULL || paren_end < paren_start) {
    return PREPARE_SYNTAX_ERROR;
  }

  *paren_start = '\0';
  char* name = str_trim(rest);
  if (name[0] == '\0') {
    return PREPARE_SYNTAX_ERROR;
  }
  resolve_filename(statement, name);

  *paren_end = '\0';
  statement->value_count =
      split_fields(paren_start + 1, ',', statement->values);
  if (statement->value_count == 0) {
    return PREPARE_SYNTAX_ERROR;
  }

  for (int i = 0; i < statement->value_count; i++) {
    char column_name[MAX_FIELD_LEN];
    int width;
    if (!parse_column_spec(statement->values[i], column_name, &width)) {
      return PREPARE_SYNTAX_ERROR;
    }
    strncpy(statement->values[i], column_name, MAX_FIELD_LEN - 1);
    statement->values[i][MAX_FIELD_LEN - 1] = '\0';
    statement->column_widths[i] = width;
  }

  return PREPARE_SUCCESS;
}

// Parses "drop table <nome>".
static PrepareResult prepare_drop_table(InputBuffer* input_buffer,
                                         Statement* statement) {
  statement->type = STATEMENT_DROP_TABLE;

  strtok(input_buffer->buffer, " ");  // discard the "drop" keyword itself
  char* table_kw = strtok(NULL, " ");
  char* name = strtok(NULL, " ");
  if (table_kw == NULL || strcmp(table_kw, "table") != 0 || name == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }
  resolve_filename(statement, name);

  return PREPARE_SUCCESS;
}

// Parses "import <tabela> from <arquivo.csv>".
static PrepareResult prepare_import(InputBuffer* input_buffer,
                                     Statement* statement) {
  statement->type = STATEMENT_IMPORT;

  strtok(input_buffer->buffer, " ");  // discard the "import" keyword itself
  char* table_name = strtok(NULL, " ");
  char* from_kw = strtok(NULL, " ");
  char* csv_path = strtok(NULL, " ");
  if (table_name == NULL || from_kw == NULL || strcmp(from_kw, "from") != 0 ||
      csv_path == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  resolve_filename(statement, table_name);
  strncpy(statement->import_source, csv_path, STATEMENT_MAX_FILENAME_LEN - 1);
  statement->import_source[STATEMENT_MAX_FILENAME_LEN - 1] = '\0';

  return PREPARE_SUCCESS;
}

// Parses "insert <tabela> <v1>, <v2>, ...": a table name token followed
// by a comma-separated list of values, one per column.
static PrepareResult prepare_insert(InputBuffer* input_buffer,
                                     Statement* statement) {
  statement->type = STATEMENT_INSERT;

  strtok(input_buffer->buffer, " ");  // discard the "insert" keyword itself
  char* filename = strtok(NULL, " ");
  char* rest = strtok(NULL, "");  // everything after the filename

  if (filename == NULL || rest == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }
  resolve_filename(statement, filename);

  statement->value_count = split_fields(rest, ',', statement->values);
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
        if (len < MAX_FIELD_LEN - 1) out_value[len++] = '"';
        text += 2;
        continue;
      }
      if (*text == '"') {
        text++;
        break;
      }
      if (len < MAX_FIELD_LEN - 1) out_value[len++] = *text;
      text++;
    }
  } else {
    while (*text != '\0' && *text != ' ') {
      if (len < MAX_FIELD_LEN - 1) out_value[len++] = *text;
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
static PrepareResult parse_where_clause(char** rest, TableQuery* query) {
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
  strncpy(query->where_column, column, MAX_FIELD_LEN - 1);
  query->where_column[MAX_FIELD_LEN - 1] = '\0';
  query->has_where = true;

  *rest = str_trim(remainder);
  return PREPARE_SUCCESS;
}

// Parses the optional "order by <coluna> [asc|desc]" clause at the start of
// `rest`, if present.
static PrepareResult parse_order_by_clause(char* rest, TableQuery* query) {
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
  strncpy(query->order_by_column, column, MAX_FIELD_LEN - 1);
  query->order_by_column[MAX_FIELD_LEN - 1] = '\0';
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

// Parses "select <tabela> [where <coluna> = <valor>]
// [order by <coluna> [asc|desc]]".
static PrepareResult prepare_select(InputBuffer* input_buffer,
                                     Statement* statement) {
  statement->type = STATEMENT_SELECT;
  statement->query = (TableQuery){0};

  strtok(input_buffer->buffer, " ");  // discard the "select" keyword itself
  char* filename = strtok(NULL, " ");
  if (filename == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }
  resolve_filename(statement, filename);

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
  if ((strncmp(input_buffer->buffer, "create", 6) == 0) ||
      (strncmp(input_buffer->buffer, "CREATE", 6) == 0)) {
    return prepare_create_table(input_buffer, statement);
  }

  if ((strncmp(input_buffer->buffer, "drop", 4) == 0) ||
      (strncmp(input_buffer->buffer, "DROP", 4) == 0)) {
    return prepare_drop_table(input_buffer, statement);
  }

  if ((strncmp(input_buffer->buffer, "insert", 6) == 0) ||
      (strncmp(input_buffer->buffer, "INSERT", 6) == 0)) {
    return prepare_insert(input_buffer, statement);
  }

  if ((strncmp(input_buffer->buffer, "select", 6) == 0) ||
      (strncmp(input_buffer->buffer, "SELECT", 6) == 0)) {
    return prepare_select(input_buffer, statement);
  }

  if ((strncmp(input_buffer->buffer, "import", 6) == 0) ||
      (strncmp(input_buffer->buffer, "IMPORT", 6) == 0)) {
    return prepare_import(input_buffer, statement);
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

static ExecuteResult to_execute_result(TableResult result) {
  switch (result) {
    case TABLE_OK:
      return EXECUTE_SUCCESS;
    case TABLE_NOT_FOUND:
      return EXECUTE_FILE_NOT_FOUND;
    case TABLE_EMPTY:
      return EXECUTE_EMPTY_FILE;
    case TABLE_COLUMN_COUNT_MISMATCH:
      return EXECUTE_COLUMN_COUNT_MISMATCH;
    case TABLE_UNKNOWN_COLUMN:
      return EXECUTE_UNKNOWN_COLUMN;
    case TABLE_ALREADY_EXISTS:
      return EXECUTE_TABLE_ALREADY_EXISTS;
    case TABLE_VALUE_TOO_LONG:
      return EXECUTE_VALUE_TOO_LONG;
    case TABLE_IMPORT_SOURCE_NOT_FOUND:
      return EXECUTE_IMPORT_SOURCE_NOT_FOUND;
    case TABLE_IMPORT_SOURCE_EMPTY:
      return EXECUTE_IMPORT_SOURCE_EMPTY;
  }
  return EXECUTE_SUCCESS;
}

static ExecuteResult execute_create_table(Statement* statement) {
  return to_execute_result(
      table_create(statement->filename, statement->values,
                    statement->column_widths, statement->value_count));
}

static ExecuteResult execute_drop_table(Statement* statement) {
  return to_execute_result(table_drop(statement->filename));
}

static ExecuteResult execute_insert(Statement* statement) {
  return to_execute_result(table_insert_row(
      statement->filename, statement->values, statement->value_count));
}

static ExecuteResult execute_select(Statement* statement) {
  return to_execute_result(
      table_print_rows(statement->filename, &statement->query));
}

static ExecuteResult execute_import(Statement* statement) {
  return to_execute_result(
      table_import_csv(statement->filename, statement->import_source));
}

ExecuteResult execute_statement(Statement* statement) {
  switch (statement->type) {
    case STATEMENT_CREATE_TABLE:
      return execute_create_table(statement);
    case STATEMENT_DROP_TABLE:
      return execute_drop_table(statement);
    case STATEMENT_INSERT:
      return execute_insert(statement);
    case STATEMENT_SELECT:
      return execute_select(statement);
    case STATEMENT_IMPORT:
      return execute_import(statement);
  }

  return EXECUTE_SUCCESS;
}
