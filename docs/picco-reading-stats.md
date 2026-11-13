# Local reading statistics: milestone 1

This milestone implements and tests the collection and storage foundation.
It is not connected to the reader, Home, Library or settings yet. Installing
this revision does not start collecting reading statistics or add any controls.
Existing saved positions and plugin reading-session events are unchanged.

The intended first feature collects reading time and forward page turns locally,
with saved-position percentages in Library and progress/time on Home. Reader
integration and screens are subsequent milestones.

## Counting contract

`readingstats::Collector` owns fixed-size, single-owner state. The owner supplies
monotonic milliseconds; there is no wall clock, RTC, network, timer task or file
I/O in the collector. A fresh instance can start at timestamp zero.

- Call `pageShown()` after a readable page is visible. Repeated calls while the
  same dwell is active do not restart timing.
- Call `depart()` before leaving that page, distinguishing an ordinary successful
  forward turn, other navigation, or a pause/exit/sleep.
- A dwell from 2,000 through 300,000 milliseconds inclusive contributes time.
  Shorter intervals and longer idle intervals contribute nothing.
- Only an ordinary forward departure after a qualifying dwell adds a forward
  turn. Backward turns and jumps can contribute reading time without a turn.
- After departure, timing is off until another `pageShown()`. Menus and loading
  screens must not call it. A failed turn does not call `depart()`.
- `preview()` includes a qualifying active dwell without committing it. Repeated
  previews cannot split a long idle interval into counted pieces.
- Repeated pause/exit/sleep calls do not count the same dwell twice. Time on a
  final page can be settled without another forward turn.

Totals retain milliseconds to avoid losing fractions on every page. Both time
and turn counters are unsigned 64-bit values and saturate rather than wrap.
Timestamp subtraction supports a 32-bit millisecond wrap; the owner must not
leave a dwell active for a full counter period (about 49.7 days). Sleep must
settle and pause it. Reading time remains an estimate: a page read for over five
minutes is excluded, while an unattended page left for less can count.

Existing plugin session counting has different thresholds and remains separate.
There is no attempt to reconstruct historical reading time from saved progress.

## Persistence contract

`readingstats::Store` owns the last successfully saved totals for one book. The
reader will own the sole writer; other views must not save stale copies.
Pass the collector's settled totals to `save()`. On failure, the collector keeps
its pending totals and can retry; the store's saved totals stay unchanged.
Checkpoint scheduling, thread synchronization and UI messages belong to the
later reader integration, not this storage class.

Records live outside rendering caches in:

```text
/.crosspoint/reading-stats/<path-hash>-a.bin
/.crosspoint/reading-stats/<path-hash>-b.bin
/.crosspoint/reading-stats/<path-hash>-t.bin  (uncommitted temporary)
```

The filename uses the 16-digit lowercase FNV-1a 64-bit hash of the normalized
absolute SD path. The full path is stored and checked inside the record. A hash
collision returns `PathConflict` and disables saving/reset rather than replacing
another book's data. There is no collision database or alternate-name probing.

Paths are case-sensitive and limited to 511 UTF-8 bytes, including the leading
slash, excluding the terminator. Repeated slashes and `.`/`..` components are
normalized; relative paths, embedded NUL, escape above root, root alone and
trailing slashes are rejected. A path exceeding the input limit is rejected
even if normalization could shorten it. The caller must supply the same canonical
case as the reader's book path. No UTF-8 conversion or content matching occurs.
External moves/renames and copies at another path have separate identities.

### Binary format, version 1

All integers are explicitly little-endian; no native struct layout is written.

| Offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 4 | ASCII `CPRS` |
| 4 | 2 | Version, 1 |
| 6 | 2 | Path length, excluding terminator |
| 8 | 8 | Generation, nonzero |
| 16 | 8 | Accumulated reading milliseconds |
| 24 | 8 | Forward page turns |
| 32 | path length | Normalized path bytes, no terminator |
| 32 + path length | 4 | CRC-32/ISO-HDLC over all preceding bytes |

Record size is `36 + path length`, at most 547 bytes. Snapshot generations do
not wrap: exhaustion is reported. Temporary files are never treated as committed
records or included in totals by a future aggregator.

### Save and recovery

1. Check storage readiness and ensure the statistics directory exists.
2. Write the complete next generation to the per-book temporary file and check
   write length and close result.
3. Read it back and validate checksum, version, identity, generation and totals.
4. Remove only the inactive snapshot and rename the temporary file into its place.
5. Update in-memory saved state only after successful promotion.

The active snapshot survives a short write, close/readback failure, or failed
promotion. On load, the newest valid generation wins. One damaged copy with a
valid survivor returns `Recovered` and permits saving. Equal generations with
different totals are ambiguous and require explicit reset. Damage is left on
disk until reset or a subsequent successful alternating save replaces it.

If neither copy is valid, ordinary saves are disabled; an explicit user reset
can publish zero totals for known corrupt data. Failed reset retains the previous
active copy when one exists. Reset does not erase rendering progress or bookmarks.
A snapshot rollback after later corruption can also roll back a reset, as with
any alternating-history recovery; this is not a secure-erasure operation.

Unreadable snapshots, unsupported versions and path conflicts disable both
saving and reset. Reopen after fixing an I/O problem. Unknown-version snapshots
are preserved even when a readable older-version copy exists. The caller must
check the load status before displaying totals; zero is not proof that corrupt
or unavailable history was empty.

The HAL exposes readiness and boolean existence, not a detailed error for a
failed directory lookup. A card error that appears solely as `exists() == false`
while readiness remains true cannot be distinguished from a missing file here.
Read/open failures for known snapshots disable writes. Physical SD error behavior
and power-interruption durability still require Picco testing; readback is not
proof against card-controller or FAT metadata loss.

## Resource ownership

The collector has no allocation. Store retains one 512-byte path buffer, an
80-byte filename stem and scalar state. A checked workspace allocation of about
2.1 KiB owns the 547-byte encoding buffer, two decoded records and normalized
path scratch. It is allocated once on the first load, reused for later loads,
saves and resets, and freed with the store. Allocation failure returns
`OutOfMemory` without touching disk. No function places these large buffers on
the C3's stack, and no new static mutable buffer or resident service is added.
HAL file handles retain their existing allocation, mutex and destructor behavior.
Final physical stack margin and runtime heap measurements remain pending
integration/hardware checks.

## Host verification

```sh
cmake -S test -B build/test -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build build/test --target ReadingStatsTest -j 6
./build/test/reading_stats/ReadingStatsTest
```

Tests include exact dwell boundaries, preview/idle behavior, pauses and final
pages, rollover and saturation, golden serialized bytes independently generated
with Python struct/zlib, UTF-8 paths, collisions, every short write, corruption,
unsupported formats, reset and retry. Host fault injection is not physical-card
verification. Reader integration, aggregation, cache/delete workflows, UI and
save-error reporting are not claimed by milestone 1.
