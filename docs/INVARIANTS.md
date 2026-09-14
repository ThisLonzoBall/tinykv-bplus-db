# Invariants

Running log of invariants the system must uphold, added as each phase
introduces new state. Keep this updated — it's the first thing to check
against when something behaves unexpectedly.

## Phase 1 — In-Memory KV Store

- A `SET` for a key always makes the immediately following `GET` for that key
  (on any connection) return the value just set, once the client has received
  the `OK` response.
- A `DELETE` for a key always makes the immediately following `GET` for that
  key return `NOT_FOUND`.
- The store holds at most one value per key.

## Core B+ Tree Invariants (from the project plan; apply once Phase 3+ lands)

1. Keys inside each node are sorted.
2. Internal nodes direct searches to the correct child.
3. All actual records live in leaf nodes.
4. All leaves exist at the same tree depth.
5. Leaf sibling links remain valid.
6. Except for the root, nodes respect minimum occupancy.
7. Splits preserve all existing records.
8. Merges preserve all remaining records.
9. The root page ID always points to the current root.
10. After a crash, committed operations remain recoverable.
