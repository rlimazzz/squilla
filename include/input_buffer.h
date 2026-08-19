#ifndef SQUILLA_INPUT_BUFFER_H
#define SQUILLA_INPUT_BUFFER_H

#include <sys/types.h>

// Owns the raw line read from stdin, growing as needed via getline().
typedef struct {
  char* buffer;
  size_t buffer_length;
  ssize_t input_length;
} InputBuffer;

InputBuffer* new_input_buffer();
void print_prompt();
void read_input(InputBuffer* input_buffer);
void close_input_buffer(InputBuffer* input_buffer);

#endif
