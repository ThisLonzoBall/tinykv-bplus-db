# TinyKV

Phase 0 + Phase 1 of the B+Tree distributed database project (see
`../bplus-tree-distributed-database-plan.md`): a TCP client/server with an
in-memory key-value store. No disk persistence yet — that starts in Phase 2.

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
