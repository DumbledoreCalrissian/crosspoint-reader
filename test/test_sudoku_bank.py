"""Presolved Sudoku bank builder: strict input checks, CPSD format and determinism."""

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib


spec = importlib.util.spec_from_file_location(
    "build_sudoku_bank", Path(__file__).resolve().parents[1] / "scripts/build_sudoku_bank.py"
)
bank = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bank)

# Two uniquely solvable puzzles from the Sudoku Exchange bank (public domain).
PUZZLE_A = "050703060007000800000816000000030000005000100730040086906000204840572093000409000"
PUZZLE_B = "302401809001000300000000000040708010780502036000090000200609003900000008800070005"


def source_line(digits, rating="1.2", hash_hex=None):
    hash_hex = hash_hex or hashlib.sha1(digits.encode()).hexdigest()[:12]
    line = f"{hash_hex} {digits} {rating:>4}\n".encode()
    assert len(line) == 100
    return line


def two_solution_puzzle():
    """A full grid with one 2x2 'deadly rectangle' emptied has exactly two solutions."""
    _, solution = bank.count_solutions(PUZZLE_A)
    grid = list(solution)
    for r1 in range(9):
        for r2 in range(r1 + 1, 9):
            if r1 // 3 != r2 // 3:
                continue
            for c1 in range(9):
                for c2 in range(c1 + 1, 9):
                    if c1 // 3 == c2 // 3:
                        continue
                    a, b = grid[r1 * 9 + c1], grid[r1 * 9 + c2]
                    if grid[r2 * 9 + c1] == b and grid[r2 * 9 + c2] == a:
                        for cell in (r1 * 9 + c1, r1 * 9 + c2, r2 * 9 + c1, r2 * 9 + c2):
                            grid[cell] = "0"
                        return "".join(grid)
    raise AssertionError("no deadly rectangle in the test grid")


class BankTestCase(unittest.TestCase):
    def build(self, files):
        """files: {name: bytes}. Builds with hashes pinned to the given contents."""
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        source, out = Path(temporary.name, "src"), Path(temporary.name, "out")
        source.mkdir()
        sources = []
        ids = {name: number for name, number, _ in bank.SOURCES}
        for name, data in files.items():
            (source / f"{name}.txt").write_bytes(data)
            sources.append((name, ids[name], hashlib.sha256(data).hexdigest()))
        provenance = bank.build(source, out, sources=tuple(sources), jobs=1)
        return out, provenance

    def assert_rejected(self, files, reason):
        with self.assertRaises(bank.BankError) as caught:
            self.build(files)
        self.assertIn(reason, str(caught.exception))


class SolverTest(unittest.TestCase):
    def test_bank_puzzles_are_unique(self):
        for digits in (PUZZLE_A, PUZZLE_B):
            count, solution = bank.count_solutions(digits)
            self.assertEqual(count, 1)
            self.assertTrue(bank.is_valid_solution(solution, digits))

    def test_deadly_rectangle_has_two_solutions(self):
        self.assertEqual(bank.count_solutions(two_solution_puzzle())[0], 2)

    def test_conflicting_givens(self):
        self.assertEqual(bank.count_solutions("11" + "0" * 79)[0], -1)


class FormatTest(unittest.TestCase):
    def test_cell_packing_round_trip_includes_cell_80(self):
        cells = "123456789" * 9
        packed = bank.pack_cells(cells)
        self.assertEqual(len(packed), 41)
        self.assertEqual(packed[0], 0x12)
        self.assertEqual(packed[40], 0x90)  # cell 80 high nibble, low nibble unused
        self.assertEqual(bank.unpack_cells(packed), cells)

    def test_unused_low_nibble_must_be_zero(self):
        packed = bytearray(bank.pack_cells("0" * 81))
        packed[40] |= 0x01
        with self.assertRaises(bank.BankError):
            bank.unpack_cells(bytes(packed))

    def test_record_layout(self):
        _, solution = bank.count_solutions(PUZZLE_A)
        prefix = bytes.fromhex(hashlib.sha1(PUZZLE_A.encode()).hexdigest()[:12])
        record = bank.encode_record(PUZZLE_A, solution, prefix, 23)
        self.assertEqual(len(record), 96)
        self.assertEqual(record[82:88], prefix)
        self.assertEqual(struct.unpack_from("<HBB", record, 88), (23, 81 - PUZZLE_A.count("0"), 0))
        self.assertEqual(struct.unpack_from("<I", record, 92)[0], bank.crc32(record[:92]))


class BuildTest(BankTestCase):
    def test_build_and_read_back(self):
        out, provenance = self.build({"easy": source_line(PUZZLE_A) + source_line(PUZZLE_B, " 2.3")})
        data = (out / "easy.cpsd").read_bytes()
        self.assertEqual(len(data), 512 + 2 * 96)
        self.assertEqual(data[:4], b"CPSD")
        info, records = bank.read_pack(out / "easy.cpsd")
        self.assertEqual((info["count"], info["min_rating"], info["max_rating"]), (2, 12, 23))
        self.assertEqual(info["commit"], bank.SOURCE_COMMIT)
        self.assertIn("Public domain", info["provenance"])
        self.assertEqual(records[0][0], PUZZLE_A)
        self.assertEqual(records[0][1], bank.count_solutions(PUZZLE_A)[1])
        entry = provenance["files"][0]
        self.assertEqual(entry["records"], 2)
        self.assertEqual(entry["output_sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(entry["records_body_crc32"], f"{info['records_crc']:08x}")
        self.assertEqual(json.loads((out / "PROVENANCE.json").read_text()), provenance)

    def test_output_is_deterministic(self):
        files = {"easy": source_line(PUZZLE_A), "medium": source_line(PUZZLE_B, " 1.7")}
        first, _ = self.build(files)
        second, _ = self.build(files)
        for name in ("easy.cpsd", "medium.cpsd", "PROVENANCE.json"):
            self.assertEqual((first / name).read_bytes(), (second / name).read_bytes(), name)

    def test_rejects_non_unique_puzzle(self):
        self.assert_rejected({"easy": source_line(two_solution_puzzle())}, "2 solutions")

    def test_rejects_conflicting_givens(self):
        digits = "55" + PUZZLE_A[2:]
        self.assert_rejected({"easy": source_line(digits)}, "givens conflict")

    def test_rejects_fewer_than_17_givens(self):
        _, solution = bank.count_solutions(PUZZLE_A)
        digits = solution[:16] + "0" * 65
        self.assert_rejected({"easy": source_line(digits)}, "only 16 givens")

    def test_rejects_bad_sha1_prefix(self):
        self.assert_rejected({"easy": source_line(PUZZLE_A, hash_hex="000000000000")}, "SHA1 prefix")

    def test_rejects_bad_separator(self):
        line = bytearray(source_line(PUZZLE_A))
        line[12] = ord("x")
        self.assert_rejected({"easy": bytes(line)}, "separators")

    def test_rejects_truncated_file(self):
        self.assert_rejected({"easy": source_line(PUZZLE_A)[:-1]}, "multiple of 100")

    def test_rejects_duplicate_across_files(self):
        self.assert_rejected({"easy": source_line(PUZZLE_A), "medium": source_line(PUZZLE_A, " 1.6")}, "duplicate")

    def test_rejects_unpinned_source(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        Path(temporary.name, "easy.txt").write_bytes(source_line(PUZZLE_A))
        with self.assertRaises(bank.BankError) as caught:
            bank.build(temporary.name, Path(temporary.name, "out"), sources=(("easy", 0, "0" * 64),), jobs=1)
        self.assertIn("pinned", str(caught.exception))


class StrictInputTest(BankTestCase):
    def test_golden_output_pins_the_format(self):
        # Any change to the CPSD layout or provenance text changes this hash and
        # must come with a format or converter version change.
        out, _ = self.build({"easy": source_line(PUZZLE_A) + source_line(PUZZLE_B, " 2.3")})
        self.assertEqual(hashlib.sha256((out / "easy.cpsd").read_bytes()).hexdigest(),
                         "529018190e572aedae04af98e31aad7b956fcddb3ca26f94198edcbfa1d07c76")

    def test_records_crc_depends_on_content_and_order(self):
        first, _ = self.build({"easy": source_line(PUZZLE_A) + source_line(PUZZLE_B)})
        swapped, _ = self.build({"easy": source_line(PUZZLE_B) + source_line(PUZZLE_A)})
        other, _ = self.build({"easy": source_line(PUZZLE_A) + source_line(PUZZLE_B, " 1.3")})
        crcs = {bank.read_pack(out / "easy.cpsd")[0]["records_crc"] for out in (first, swapped, other)}
        self.assertEqual(len(crcs), 3)
        blob = (first / "easy.cpsd").read_bytes()[512:]
        self.assertNotEqual(bank.records_body_crc(blob), bank.crc32(blob))
        # The documented definition, computed independently of the converter.
        bodies = b"".join(blob[i:i + 92] for i in range(0, len(blob), 96))
        self.assertEqual(bank.read_pack(first / "easy.cpsd")[0]["records_crc"], zlib.crc32(bodies))

    def test_reader_rejects_format_version_1(self):
        out, _ = self.build({"easy": source_line(PUZZLE_A)})
        path = out / "easy.cpsd"
        data = bytearray(path.read_bytes())
        struct.pack_into("<H", data, 4, 1)
        struct.pack_into("<I", data, 508, bank.crc32(bytes(data[:508])))
        path.write_bytes(bytes(data))
        with self.assertRaisesRegex(bank.BankError, "version 2"):
            bank.read_pack(path)

    def test_two_digit_rating(self):
        out, _ = self.build({"diabolical": source_line(PUZZLE_A, "10.4")})
        self.assertEqual(bank.read_pack(out / "diabolical.cpsd")[0]["max_rating"], 104)

    def test_rejects_bad_rating(self):
        for rating in ("1.25", " 1,2", "  12", "\t1.2"):
            with self.subTest(rating=rating):
                self.assert_rejected({"easy": source_line(PUZZLE_A, rating)}, "bad rating")

    def test_rejects_upper_case_hash(self):
        upper = hashlib.sha1(PUZZLE_A.encode()).hexdigest()[:12].upper()
        if upper.isdigit():
            self.skipTest("hash prefix has no letters")
        self.assert_rejected({"easy": source_line(PUZZLE_A, hash_hex=upper)}, "bad hash field")

    def test_rejects_bad_jobs(self):
        with self.assertRaisesRegex(bank.BankError, "--jobs"):
            bank.build("unused", "unused", jobs=0)

    def test_failed_build_leaves_output_unchanged(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        source, out = Path(temporary.name, "src"), Path(temporary.name, "out")
        source.mkdir()
        out.mkdir()
        (out / "easy.cpsd").write_bytes(b"previous")
        good, bad = source_line(PUZZLE_A), source_line(two_solution_puzzle())
        (source / "easy.txt").write_bytes(good)
        (source / "medium.txt").write_bytes(bad)
        sources = (("easy", 0, hashlib.sha256(good).hexdigest()), ("medium", 1, hashlib.sha256(bad).hexdigest()))
        with self.assertRaises(bank.BankError):
            bank.build(source, out, sources=sources, jobs=1)
        self.assertEqual(sorted(p.name for p in out.iterdir()), ["easy.cpsd"])
        self.assertEqual((out / "easy.cpsd").read_bytes(), b"previous")


def rewrite_record(path, index, mutate):
    """Change one record, then repair every CRC so only the targeted check can fail."""
    data = bytearray(path.read_bytes())
    start = 512 + 96 * index
    record = bytearray(data[start:start + 96])
    mutate(record)
    struct.pack_into("<I", record, 92, bank.crc32(bytes(record[:92])))
    data[start:start + 96] = record
    struct.pack_into("<I", data, 16, bank.records_body_crc(bytes(data[512:])))
    struct.pack_into("<I", data, 508, bank.crc32(bytes(data[:508])))
    path.write_bytes(bytes(data))


class ReaderTest(BankTestCase):
    def built_pack(self):
        out, _ = self.build({"easy": source_line(PUZZLE_A) + source_line(PUZZLE_B)})
        return out / "easy.cpsd"

    def damaged(self, path, offset):
        data = bytearray(path.read_bytes())
        data[offset] ^= 0x01
        path.write_bytes(bytes(data))

    def test_detects_header_damage(self):
        path = self.built_pack()
        self.damaged(path, 100)
        with self.assertRaisesRegex(bank.BankError, "header CRC"):
            bank.read_pack(path)

    def test_detects_record_damage(self):
        path = self.built_pack()
        self.damaged(path, 512 + 96 + 10)
        with self.assertRaisesRegex(bank.BankError, "records CRC"):
            bank.read_pack(path)

    def test_detects_record_crc_alone(self):
        path = self.built_pack()
        data = bytearray(path.read_bytes())
        data[512 + 5] ^= 0x10  # record 0 body changes, its stored CRC does not
        struct.pack_into("<I", data, 16, bank.records_body_crc(bytes(data[512:])))
        struct.pack_into("<I", data, 508, bank.crc32(bytes(data[:508])))
        path.write_bytes(bytes(data))
        with self.assertRaisesRegex(bank.BankError, "record 0 CRC"):
            bank.read_pack(path)

    def test_detects_each_record_field(self):
        cases = {
            "count or flags": lambda r: r.__setitem__(90, r[90] + 1),
            "flags": lambda r: r.__setitem__(91, 1),
            "rating outside": lambda r: struct.pack_into("<H", r, 88, 99),
            "not a valid grid": lambda r: r.__setitem__(41, (r[41] & 0x0F) | ((r[41] >> 4) % 9 + 1) << 4),
            "unused bits": lambda r: r.__setitem__(81, r[81] | 0x01),
        }
        for reason, mutate in cases.items():
            with self.subTest(reason=reason):
                path = self.built_pack()
                rewrite_record(path, 1, mutate)
                with self.assertRaisesRegex(bank.BankError, reason.split()[0]):
                    bank.read_pack(path)

    def test_detects_difficulty_name_mismatch(self):
        path = self.built_pack()
        renamed = path.with_name("hard.cpsd")
        path.rename(renamed)
        with self.assertRaisesRegex(bank.BankError, "file name"):
            bank.read_pack(renamed)

    def test_detects_wrong_length(self):
        path = self.built_pack()
        path.write_bytes(path.read_bytes()[:-1])
        with self.assertRaisesRegex(bank.BankError, "length"):
            bank.read_pack(path)


if __name__ == "__main__":
    unittest.main()
