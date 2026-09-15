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

## Phase 2 — Pages and Pager

- The file size is a multiple of `kPageSize` (4096), and
  `page_count * kPageSize <= file size`.
- Page 0 is the metadata page (magic `TKV1`, format version, page count,
  free-list head). It is never handed out by `allocate_page()` and callers
  cannot read, write, or free it.
- Every page with id `< page_count` has a valid CRC-32 checksum, and its
  header id equals its position in the file. A mismatch is corruption.
- Every page on the free list has type `Free`. The list ends at
  `kInvalidPageId`.
- `allocate_page()` returns a page of type `Empty` with a zeroed payload, even
  when it reuses a freed page.
- Not yet guaranteed: atomicity across a crash. A crash mid-allocate or
  mid-free can leak a page (never corrupt the free list), and nothing is
  durable until `sync()`. The WAL (Phase 9) takes over from here.

## Phase 3 — Leaf Pages

- Slots are stored in ascending key order, so a lookup is a binary search.
  Every operation preserves that order.
- A key appears at most once in a leaf. `insert` on an existing key replaces
  its value rather than adding a second copy.
- Records never overlap the slot array: records grow down from the end of the
  page and slots grow up from the header.
- `insert` either fully succeeds or leaves the page byte-for-byte unchanged and
  returns false. A caller that sees false must split rather than assume the
  entry landed.
- Removals leave gaps, which `compact()` reclaims when the next insert needs
  contiguous room. `free_after_compaction()` is the real space available.
- Known limit: a record up to `kMaxLeafRecordSize` (just under a whole page) is
  accepted, so one huge record can fill a leaf. Real B+ trees cap records at a
  fraction of a page and spill the rest to overflow pages. Revisit when
  splitting lands in Phase 4.

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
