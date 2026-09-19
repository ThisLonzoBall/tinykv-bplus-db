# TinyKV

Phases 0–3 of the B+Tree distributed database project (see
`../bplus-tree-distributed-database-plan.md`):

- a TCP client/server with an in-memory key-value store
- a disk pager (`src/storage/pager.hpp`) mapping a database file onto
  checksummed 4 KiB pages with a free list
- B+ tree leaf pages (`src/storage/leaf_page.hpp`) storing sorted key/value
  records inside a page, with a sibling pointer for future range scans
- leaf splitting: a full leaf divides in two and returns the separator key an
  internal node will store
- internal nodes (`src/storage/internal_page.hpp`) holding separator keys and
  child pointers
- a multi-level B+ tree (`src/storage/btree.hpp`) with `get`/`put`, split
  propagation and root splits, rooted at a page id kept in the metadata page

The server still uses the in-memory store. It gets wired to the disk layers
once the B+ tree above the leaves exists (Phases 4–5).

## Build

```sh
make
```

Produces `build/db_server` and `build/db_client`.

## Run

```sh
./build/db_server 9000
```

In another terminal:

```sh
./build/db_client 127.0.0.1 9000
```

Then type commands:

```
SET name Alice
GET name
DELETE name
GET name
```

## Protocol

Newline-terminated text commands:

```
SET <key> <value...>   -> OK
GET <key>              -> <value> | NOT_FOUND
DELETE <key>           -> OK | NOT_FOUND
```

Anything else returns `ERROR <reason>`.

## Test

```sh
make test
```
