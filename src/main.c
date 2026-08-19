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
        printf("File '%s' not found.\n", statement.filename);
        break;
      case EXECUTE_EMPTY_FILE:
        printf("File '%s' has no rows.\n", statement.filename);
        break;
      case EXECUTE_COLUMN_COUNT_MISMATCH:
        printf("Value count doesn't match the number of columns in '%s'.\n",
               statement.filename);
        break;
    }
  }

  return 0;
}
