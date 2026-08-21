#include <stdio.h>

#include "input_buffer.h"
#include "meta_command.h"
#include "statement.h"

// REPL stands for Read Eval Print Loop
int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  InputBuffer* input_buffer = new_input_buffer();

  while (1) {
    print_prompt();
    read_input(input_buffer);

    if (input_buffer->buffer[0] == '.') {
      switch (do_meta_command(input_buffer)) {
        case META_COMMAND_SUCCESS:
          continue;
        case META_COMMAND_UNRECOGNIZED_COMMAND:
          printf("Unrecognized meta command '%s'.\n", input_buffer->buffer);
          continue;
      }
    }

    Statement statement;
    switch (prepare_statement(input_buffer, &statement)) {
      case PREPARE_SUCCESS:
        break;
      case PREPARE_SYNTAX_ERROR:
        printf("Syntax error. Could not parse statement.\n");
        continue;
      case PREPARE_UNRECOGNIZED_STATEMENT:
        printf("Unrecognized keyword at start of '%s'.\n",
               input_buffer->buffer);
        continue;
    }

    switch (execute_statement(&statement)) {
      case EXECUTE_SUCCESS:
        printf("Executed.\n");
        break;
      case EXECUTE_FILE_NOT_FOUND:
        printf("Table '%s' not found.\n",
               table_display_name(statement.filename));
        break;
      case EXECUTE_EMPTY_FILE:
        printf("Table '%s' has no rows.\n",
               table_display_name(statement.filename));
        break;
      case EXECUTE_COLUMN_COUNT_MISMATCH:
        printf("Value count doesn't match the number of columns in table "
               "'%s'.\n",
               table_display_name(statement.filename));
        break;
      case EXECUTE_UNKNOWN_COLUMN:
        printf("Unknown column referenced in WHERE/ORDER BY on table "
               "'%s'.\n",
               table_display_name(statement.filename));
        break;
      case EXECUTE_TABLE_ALREADY_EXISTS:
        printf("Table '%s' already exists.\n",
               table_display_name(statement.filename));
        break;
      case EXECUTE_VALUE_TOO_LONG:
        printf("A value is too long for its column in table '%s'.\n",
               table_display_name(statement.filename));
        break;
      case EXECUTE_IMPORT_SOURCE_NOT_FOUND:
        printf("CSV file '%s' not found.\n", statement.import_source);
        break;
      case EXECUTE_IMPORT_SOURCE_EMPTY:
        printf("CSV file '%s' has no header row.\n", statement.import_source);
        break;
    }
  }

  return 0;
}
