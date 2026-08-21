#ifndef SQUILLA_CSV_IMPORT_H
#define SQUILLA_CSV_IMPORT_H

#include "table.h"

// Reads a text CSV file (auto-detecting ',' or ';' as the delimiter, with
// the same double-quote escaping as split_fields()), sizes each column from
// the widest value seen in the file, then creates and populates
// `table_filename` from it. Fails with TABLE_ALREADY_EXISTS if the table
// already exists.
TableResult table_import_csv(const char* table_filename,
                              const char* csv_filename);

#endif
