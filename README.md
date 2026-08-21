### Squilla

A simple SQL database engine built from scratch in C. Tables are stored in a
custom binary format with fixed-size records — not plain text/CSV — and are
queried through a small SQL-like REPL.

## Building

Requires a C compiler (`cc`) and `make`.

```sh
make        # builds ./squilla
make run    # builds and starts the REPL
make clean  # removes build/ and the squilla binary
```

## Running

```sh
./squilla
```

You'll get a prompt:

```
squilla >
```

Type SQL-like statements (see below) or a meta command. To leave:

```
squilla > .exit
```

## Commands

Statement keywords (`CREATE`, `DROP`, `INSERT`, `SELECT`, `IMPORT`) accept
either lowercase or uppercase. Sub-keywords (`table`, `from`, `where`,
`order by`, `asc`, `desc`) must be lowercase.

### CREATE TABLE

```sql
create table <nome> (<coluna1>[(<tamanho>)], <coluna2>[(<tamanho>)], ...)
```

Creates a new table file. Each column may declare a max character width in
parentheses (like `VARCHAR(n)`); if omitted, it defaults to 32 characters.
Fails if the table already exists.

```sql
create table produtos (nome, categoria, preco)
create table clientes (nome(16), cidade(16), saldo(8))
```

### DROP TABLE

```sql
drop table <nome>
```

Deletes the table file. Fails if it doesn't exist.

```sql
drop table produtos
```

### INSERT

```sql
insert <tabela> <valor1>, <valor2>, ...
```

Appends one row. Values are comma-separated; wrap a value in double quotes
if it contains a comma, or leading/trailing spaces (`""` inside a quoted
value is a literal `"`). Fails if the number of values doesn't match the
table's column count, or if a value is longer than its column's declared
width.

```sql
insert produtos Caneta, Escritorio, 2.5
insert clientes "Silva, Souza", "Sao Paulo", 500
```

### SELECT

```sql
select <tabela> [where <coluna> = <valor>] [order by <coluna> [asc|desc]]
```

Prints every matching row (header included) as `(col1, col2, ...)`.

- `where` filters on equality (`=`) for a single column. `AND`/`OR` and
  other operators (`>`, `<`, `!=`) aren't supported yet.
- `order by` sorts by one column, ascending by default. Numeric-looking
  values sort numerically; everything else sorts alphabetically.
- Values with a comma are shown quoted in the output so the row stays
  unambiguous to read.

```sql
select produtos
select produtos where categoria = Escritorio
select produtos order by preco desc
select produtos where categoria = Escritorio order by preco desc
select clientes where cidade = "Sao Paulo"
```

### IMPORT

```sql
import <tabela> from <arquivo.csv>
```

Reads a text CSV file, auto-detecting `,` or `;` as the delimiter (whichever
is more frequent in the header line), sizes each column from the widest
value found, then creates and populates the table from it. Fails if the
table already exists or the CSV file can't be found. Rows with the wrong
number of fields are skipped, and a summary of imported/skipped rows is
printed.

```sql
import clientes from clientes.csv
```

### Meta commands

```
.exit
```

Closes the REPL.

## Example session

```
squilla > create table produtos (nome(16), categoria(16), preco(8))
Executed.
squilla > insert produtos Caneta, Escritorio, 2.5
Executed.
squilla > insert produtos Caderno, Escritorio, 12
Executed.
squilla > select produtos where categoria = Escritorio order by preco desc
(nome, categoria, preco)
(Caderno, Escritorio, 12)
(Caneta, Escritorio, 2.5)
Executed.
squilla > drop table produtos
Executed.
squilla > .exit
```

## Storage format

Each table is a single binary file (default extension `.tbl`, resolved
automatically from the table name — `produtos` -> `produtos.tbl`):

- A fixed-size header holding a magic number and up to 32 column
  definitions (name + declared max width).
- Fixed-size rows immediately after the header — each value stored in its
  column's declared width, null-padded, with no delimiter to parse.

This means a `.csv` file is *not* a valid table on its own; bring its data
in first with `IMPORT`.

## Project layout

```
src/            implementation
include/        headers
src/main.c      REPL loop
src/statement.c SQL front-end: parses input into a Statement, dispatches execution
src/table.c     binary storage engine (create/insert/select/drop)
src/csv_import.c CSV -> table bridge for IMPORT
src/input_buffer.c  reads a line of input
src/meta_command.c  handles "." commands (e.g. .exit)
src/util.c      shared string helpers (trim, quoted-field splitting)
```

## Known limitations

No indexes (every `SELECT` scans the whole table), no buffer pool/cache, no
write-ahead log, no concurrency control, and no transactions. `WHERE`
supports only equality on a single column.
