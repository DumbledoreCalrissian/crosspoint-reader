"""Build presolved Sudoku puzzle files (CPSD version 2) from the Sudoku Exchange bank.

Usage:
    python scripts/build_sudoku_bank.py --source DIR --out DIR [--jobs N]

DIR for --source holds easy.txt, medium.txt, hard.txt and diabolical.txt from
https://github.com/grantm/sudoku-exchange-puzzle-bank at SOURCE_COMMIT. Each
input must match its recorded SHA256. Every puzzle must have exactly one
solution and appear only once across all files; any bad record stops the build
and leaves the output folder unchanged. Output: <difficulty>.cpsd files and
PROVENANCE.json. The format is described in docs/sudoku-bank-format.md.
Standard library only; same input gives byte-identical output.
"""

import argparse
import hashlib
import json
import multiprocessing
import os
import shutil
import struct
import sys
import tempfile
import zlib
from pathlib import Path

SOURCE_URL = "https://github.com/grantm/sudoku-exchange-puzzle-bank"
SOURCE_COMMIT = "d8c8ebaee0c08c412cfba96af1923dfa61c83317"
LICENCE = "Public domain: dedicated by the author with Unlicense-style text (LICENSE.txt in the source repository)."

CONVERTER_VERSION = 2
FORMAT_VERSION = 2
MAGIC = b"CPSD"
HEADER_SIZE = 512
RECORD_SIZE = 96
SOURCE_RECORD_SIZE = 100
PROVENANCE_OFFSET = 48
PROVENANCE_SIZE = 460

# (name, difficulty id, SHA256 of the source file at SOURCE_COMMIT)
SOURCES = (
    ("easy", 0, "789aab6f52cc4588e0c3aa4269ad3282cfda7e1cc79ea693d16507decde3fc50"),
    ("medium", 1, "c2a5f8fac99dcf215b25d46ece47f346375e51730b182de70a6f8f2107b2b38d"),
    ("hard", 2, "abcff1512411e601abd5e861c60add5be5d877739b6faa249ee86960f5ad85e9"),
    ("diabolical", 3, "08553d0c1145ea4d7c13008040f47ea8205d21fe1eaf8f4ab17a1a6981928b35"),
)

ROW = [i // 9 for i in range(81)]
COL = [i % 9 for i in range(81)]
BOX = [(i // 27) * 3 + (i % 9) // 3 for i in range(81)]
ALL_DIGITS = 0x1FF
CHUNK_RECORDS = 5000


class BankError(Exception):
    """A source or output problem that must stop the build."""


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def parse_source_record(record, index):
    """Return (digits, sha1 prefix bytes, rating x10) or raise BankError."""
    where = f"record {index}"
    if len(record) != SOURCE_RECORD_SIZE:
        raise BankError(f"{where}: length {len(record)}, expected {SOURCE_RECORD_SIZE}")
    try:
        text = record.decode("ascii")
    except UnicodeDecodeError as error:
        raise BankError(f"{where}: not ASCII") from error
    if text[12] != " " or text[94] != " " or text[99] != "\n":
        raise BankError(f"{where}: bad field separators")
    hash_hex, digits, rating = text[:12], text[13:94], text[95:99]
    if not all(c in "0123456789abcdef" for c in hash_hex):
        raise BankError(f"{where}: bad hash field")
    if not digits.isdigit():
        raise BankError(f"{where}: puzzle must be 81 digits")
    if hashlib.sha1(digits.encode("ascii")).hexdigest()[:12] != hash_hex:
        raise BankError(f"{where}: SHA1 prefix does not match the puzzle")
    # Rating is "nn.n", right-aligned with a leading space for values below 10.
    if not (rating[0] == " " or rating[0].isdigit()) or not (
            rating[1].isdigit() and rating[2] == "." and rating[3].isdigit()):
        raise BankError(f"{where}: bad rating {rating!r}")
    rating10 = int(rating[:2].strip()) * 10 + int(rating[3])
    if not 0 < rating10 <= 0xFFFF:
        raise BankError(f"{where}: rating out of range")
    return digits, bytes.fromhex(hash_hex), rating10


def count_solutions(digits, limit=2):
    """Return (number of solutions up to limit, first solution or None).

    Returns (-1, None) when the givens already conflict.
    """
    grid = [ord(c) - 48 for c in digits]
    rows, cols, boxes = [0] * 9, [0] * 9, [0] * 9
    for i, value in enumerate(grid):
        if value:
            bit = 1 << (value - 1)
            if (rows[ROW[i]] | cols[COL[i]] | boxes[BOX[i]]) & bit:
                return -1, None
            rows[ROW[i]] |= bit
            cols[COL[i]] |= bit
            boxes[BOX[i]] |= bit
    found = []

    def search():
        best, best_mask, best_count = -1, 0, 10
        for i in range(81):
            if grid[i] == 0:
                mask = ALL_DIGITS & ~(rows[ROW[i]] | cols[COL[i]] | boxes[BOX[i]])
                count = bin(mask).count("1")
                if count < best_count:
                    best, best_mask, best_count = i, mask, count
                    if count <= 1:
                        break
        if best < 0:
            found.append("".join(map(str, grid)))
            return len(found) >= limit
        mask = best_mask
        while mask:
            bit = mask & -mask
            mask ^= bit
            grid[best] = bit.bit_length()
            rows[ROW[best]] |= bit
            cols[COL[best]] |= bit
            boxes[BOX[best]] |= bit
            stop = search()
            rows[ROW[best]] &= ~bit
            cols[COL[best]] &= ~bit
            boxes[BOX[best]] &= ~bit
            grid[best] = 0
            if stop:
                return True
        return False

    search()
    return len(found), (found[0] if found else None)


def is_valid_solution(solution, digits):
    if len(solution) != 81 or any(c not in "123456789" for c in solution):
        return False
    if any(g != "0" and g != s for g, s in zip(digits, solution)):
        return False
    full = set("123456789")
    for k in range(9):
        for unit in (ROW, COL, BOX):
            if {solution[i] for i in range(81) if unit[i] == k} != full:
                return False
    return True


def pack_cells(cells):
    """Pack 81 digit characters into 41 bytes, two cells per byte, high nibble first."""
    values = [ord(c) - 48 for c in cells] + [0]
    return bytes((values[i] << 4) | values[i + 1] for i in range(0, 82, 2))


def unpack_cells(data):
    if len(data) != 41:
        raise BankError("packed grid must be 41 bytes")
    values = []
    for byte in data:
        values.append(byte >> 4)
        values.append(byte & 0x0F)
    if values[81] != 0:
        raise BankError("unused bits after cell 80 must be zero")
    if any(v > 9 for v in values[:81]):
        raise BankError("cell value above 9")
    return "".join(str(v) for v in values[:81])


def encode_record(digits, solution, sha1_prefix, rating10):
    givens = 81 - digits.count("0")
    body = pack_cells(digits) + pack_cells(solution) + sha1_prefix + struct.pack("<HBB", rating10, givens, 0)
    assert len(body) == RECORD_SIZE - 4
    return body + struct.pack("<I", crc32(body))


def provenance_text(name):
    text = (f"Sudoku Exchange puzzle bank, {name}.txt at commit {SOURCE_COMMIT}, {SOURCE_URL}. "
            f"{LICENCE} Solutions added and every puzzle checked to have exactly one solution "
            f"by scripts/build_sudoku_bank.py (converter version {CONVERTER_VERSION}).")
    data = text.encode("utf-8")
    if len(data) > PROVENANCE_SIZE:
        raise BankError("provenance text too long")
    return data.ljust(PROVENANCE_SIZE, b"\0")


def records_body_crc(blob):
    """CRC32 of bytes 0-91 of every record, in order.

    Each record ends with its own CRC32, and a CRC over data followed by its CRC
    is the same constant for any data. Leaving those 4 bytes out makes the value
    depend on the puzzles, not only on their count (format version 1 included them).
    """
    crc = 0
    for start in range(0, len(blob), RECORD_SIZE):
        crc = zlib.crc32(blob[start:start + RECORD_SIZE - 4], crc)
    return crc & 0xFFFFFFFF


def encode_header(name, difficulty, records_blob, count, min_rating, max_rating):
    head = struct.pack("<4sHHHBBII", MAGIC, FORMAT_VERSION, HEADER_SIZE, RECORD_SIZE, difficulty, 0, count,
                       records_body_crc(records_blob))
    head += bytes.fromhex(SOURCE_COMMIT)
    head += struct.pack("<HHI", min_rating, max_rating, CONVERTER_VERSION)
    head += provenance_text(name)
    assert len(head) == HEADER_SIZE - 4
    return head + struct.pack("<I", crc32(head))


def check_chunk(task):
    """Worker: parse and solve the records in one slice of the hashed source bytes.

    Returns a list of (digits, solution, sha1 prefix, rating10) in order, or raises BankError.
    """
    start, data = task
    count = len(data) // SOURCE_RECORD_SIZE
    results = []
    for k in range(count):
        index = start + k
        digits, prefix, rating10 = parse_source_record(
            data[k * SOURCE_RECORD_SIZE:(k + 1) * SOURCE_RECORD_SIZE], index)
        givens = 81 - digits.count("0")
        if givens < 17:
            raise BankError(f"record {index}: only {givens} givens")
        solutions, solution = count_solutions(digits)
        if solutions == -1:
            raise BankError(f"record {index}: givens conflict")
        if solutions != 1:
            raise BankError(f"record {index}: {solutions} solutions, expected exactly 1")
        if not is_valid_solution(solution, digits):
            raise BankError(f"record {index}: solver returned an invalid grid")
        results.append((digits, solution, prefix, rating10))
    return results


def read_pack(path):
    """Independent reader: validate a CPSD file and return (header dict, records).

    Each record is (givens, solution, sha1 prefix bytes, rating10). Raises BankError.
    """
    data = Path(path).read_bytes()
    if len(data) < HEADER_SIZE:
        raise BankError(f"{path}: shorter than the header")
    header = data[:HEADER_SIZE]
    if struct.unpack_from("<I", header, HEADER_SIZE - 4)[0] != crc32(header[:HEADER_SIZE - 4]):
        raise BankError(f"{path}: header CRC mismatch")
    magic, version, header_size, record_size, difficulty, reserved, count, records_crc = struct.unpack_from(
        "<4sHHHBBII", header, 0)
    if magic != MAGIC or version != FORMAT_VERSION:
        raise BankError(f"{path}: not a CPSD version {FORMAT_VERSION} file")
    if header_size != HEADER_SIZE or record_size != RECORD_SIZE or difficulty > 3 or reserved != 0:
        raise BankError(f"{path}: bad header fields")
    expected_difficulty = {name: number for name, number, _ in SOURCES}.get(Path(path).stem)
    if expected_difficulty is not None and expected_difficulty != difficulty:
        raise BankError(f"{path}: difficulty {difficulty} does not match the file name")
    if len(data) != HEADER_SIZE + RECORD_SIZE * count:
        raise BankError(f"{path}: length does not match the record count")
    blob = data[HEADER_SIZE:]
    if records_body_crc(blob) != records_crc:
        raise BankError(f"{path}: records CRC mismatch")
    min_rating, max_rating, converter = struct.unpack_from("<HHI", header, 40)
    provenance = header[PROVENANCE_OFFSET:PROVENANCE_OFFSET + PROVENANCE_SIZE].rstrip(b"\0").decode("utf-8")
    records = []
    for i in range(count):
        record = blob[i * RECORD_SIZE:(i + 1) * RECORD_SIZE]
        if struct.unpack_from("<I", record, 92)[0] != crc32(record[:92]):
            raise BankError(f"{path}: record {i} CRC mismatch")
        givens = unpack_cells(record[0:41])
        solution = unpack_cells(record[41:82])
        rating10, given_count, flags = struct.unpack_from("<HBB", record, 88)
        if flags != 0 or given_count != 81 - givens.count("0"):
            raise BankError(f"{path}: record {i} bad count or flags")
        if not is_valid_solution(solution, givens):
            raise BankError(f"{path}: record {i} solution is not a valid grid for its givens")
        if not min_rating <= rating10 <= max_rating:
            raise BankError(f"{path}: record {i} rating outside the header range")
        records.append((givens, solution, record[82:88], rating10))
    info = {"difficulty": difficulty, "count": count, "records_crc": records_crc, "min_rating": min_rating,
            "max_rating": max_rating, "converter": converter, "commit": header[20:40].hex(),
            "provenance": provenance}
    return info, records


def write_atomically(path, data):
    temporary = Path(str(path) + ".tmp")
    temporary.write_bytes(data)
    os.replace(temporary, path)


def build(source_dir, out_dir, sources=SOURCES, jobs=None):
    """Convert every source file; return the provenance dictionary."""
    source_dir, out_dir = Path(source_dir), Path(out_dir).resolve()
    if jobs is not None and jobs < 1:
        raise BankError("--jobs must be at least 1")
    # Build into a new staging folder; files replace the output only after every check passes.
    out_dir.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{out_dir.name}-", dir=out_dir.parent))
    try:
        return _build_into(source_dir, out_dir, staging, sources, jobs)
    finally:
        shutil.rmtree(staging, ignore_errors=True)


def _build_into(source_dir, out_dir, staging, sources, jobs):
    provenance = {"format": "CPSD", "format_version": FORMAT_VERSION, "converter_version": CONVERTER_VERSION,
                  "source": SOURCE_URL, "source_commit": SOURCE_COMMIT, "licence": LICENCE, "files": []}
    seen = {}
    # jobs=1 runs in this process (tests load the module by path, which worker
    # processes cannot re-import on macOS).
    pool = multiprocessing.Pool(jobs) if jobs != 1 else None
    imap = pool.imap if pool else map
    try:
        for name, difficulty, expected_sha256 in sources:
            source_path = source_dir / f"{name}.txt"
            raw = source_path.read_bytes()
            source_sha256 = sha256_bytes(raw)
            if source_sha256 != expected_sha256:
                raise BankError(f"{source_path}: SHA256 {source_sha256} does not match the pinned value")
            if len(raw) == 0 or len(raw) % SOURCE_RECORD_SIZE:
                raise BankError(f"{source_path}: length is not a positive multiple of {SOURCE_RECORD_SIZE}")
            count = len(raw) // SOURCE_RECORD_SIZE
            # Slices of the hashed bytes, so the checked data is exactly the pinned data.
            tasks = [(start, raw[start * SOURCE_RECORD_SIZE:(start + CHUNK_RECORDS) * SOURCE_RECORD_SIZE])
                     for start in range(0, count, CHUNK_RECORDS)]
            encoded, expected_records = [], []
            min_rating, max_rating = 0xFFFF, 0
            index = 0
            for chunk in imap(check_chunk, tasks):
                for digits, solution, prefix, rating10 in chunk:
                    if digits in seen:
                        raise BankError(f"{name} record {index}: duplicate of {seen[digits]}")
                    seen[digits] = f"{name} record {index}"
                    encoded.append(encode_record(digits, solution, prefix, rating10))
                    expected_records.append((digits, solution, prefix, rating10))
                    min_rating = min(min_rating, rating10)
                    max_rating = max(max_rating, rating10)
                    index += 1
            blob = b"".join(encoded)
            pack = encode_header(name, difficulty, blob, count, min_rating, max_rating) + blob
            out_path = staging / f"{name}.cpsd"
            write_atomically(out_path, pack)

            info, records = read_pack(out_path)
            if info["count"] != count or info["difficulty"] != difficulty:
                raise BankError(f"{out_path}: read-back header differs")
            for i, (read, expected) in enumerate(zip(records, expected_records)):
                if read != expected:
                    raise BankError(f"{out_path}: read-back record {i} differs")
            provenance["files"].append({
                "difficulty": name, "difficulty_id": difficulty,
                "source_file": f"{name}.txt", "source_sha256": source_sha256,
                "output_file": out_path.name, "output_sha256": sha256_bytes(pack), "output_bytes": len(pack),
                "records": count, "rating_min": min_rating / 10, "rating_max": max_rating / 10,
                "records_body_crc32": f"{info['records_crc']:08x}",
            })
            print(f"{name}: {count} puzzles, {len(pack)} bytes, sha256 {sha256_bytes(pack)}", flush=True)
    finally:
        if pool:
            pool.terminate()
            pool.join()
    write_atomically(staging / "PROVENANCE.json", (json.dumps(provenance, indent=2, sort_keys=True) + "\n").encode())
    out_dir.mkdir(parents=True, exist_ok=True)
    for path in sorted(staging.iterdir()):
        os.replace(path, out_dir / path.name)
    return provenance


def main(argv=None):
    parser = argparse.ArgumentParser(description="Build presolved CPSD Sudoku files from the Sudoku Exchange bank.")
    parser.add_argument("--source", required=True, help="folder with easy/medium/hard/diabolical .txt")
    parser.add_argument("--out", required=True, help="output folder")
    parser.add_argument("--jobs", type=int, default=None, help="worker processes (default: all cores)")
    args = parser.parse_args(argv)
    try:
        build(args.source, args.out, jobs=args.jobs)
    except BankError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
