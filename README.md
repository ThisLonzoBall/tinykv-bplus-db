# TinyKV

Phases 0–2 of the B+Tree distributed database project (see
`../bplus-tree-distributed-database-plan.md`): a TCP client/server with an
in-memory key-value store, plus a disk pager (`src/storage/pager.hpp`) that
maps a database file onto checksummed 4 KiB pages. The server does not use
the pager yet; that happens once the B+ tree layer exists.

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
