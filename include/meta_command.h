#ifndef SQUILLA_META_COMMAND_H
#define SQUILLA_META_COMMAND_H

#include "input_buffer.h"

typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

// Handles non-SQL commands prefixed with '.', e.g. ".exit".
MetaCommandResult do_meta_command(InputBuffer* input_buffer);

#endif
