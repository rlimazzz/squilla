#ifndef SQUILLA_UTIL_H
#define SQUILLA_UTIL_H

// Shared limits for anything that looks like "a list of text fields":
// table columns, a row's values, a SELECT clause's column name, etc.
#define MAX_FIELD_LEN 256
#define MAX_FIELDS 32

// Trims leading/trailing whitespace (including \r\n) in place and returns a
// pointer to the first non-whitespace character.
char* str_trim(char* s);

// Splits `line` on `delimiter` into up to MAX_FIELDS fields, honoring
// double-quoted fields (RFC 4180 style): a quoted field may contain the
// delimiter or leading/trailing spaces literally, and "" inside quotes is an
// escaped quote. Unquoted fields are trimmed of surrounding whitespace.
// Returns how many fields were found.
int split_fields(const char* line, char delimiter,
                  char fields[][MAX_FIELD_LEN]);

#endif
