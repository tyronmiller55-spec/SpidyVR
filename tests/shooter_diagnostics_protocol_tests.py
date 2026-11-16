"""Run the actual decoder without importing Windows-only VR transport modules."""
import ast
from pathlib import Path
import struct
import unittest

SOURCE = Path(__file__).resolve().parents[1] / 'tools' / 'run_game_vr.py'
tree = ast.parse(SOURCE.read_text())
names = {'gadgets_of', 'shooter_dispatch_of', 'shooter_snapshot'}
selected = ast.Module(body=[n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in names], type_ignores=[])
namespace = dict(struct=struct, GADGETS=('none',) * 10)
exec(compile(selected, str(SOURCE), 'exec'), namespace)
snapshot = namespace['shooter_snapshot']


def fixture(version=5):
    raw = bytearray(432 if version == 5 else 296)
    struct.pack_into('<3I', raw, 0, 0x53484f44, version, len(raw))
    struct.pack_into('<q', raw, 16, 2)
    return raw


class Reader:
    def __init__(self, raw, torn=False):
        self.raw, self.torn = raw, torn

    def read(self, address, size):
        if address == 16 and self.torn:
            return struct.pack('<q', 4)
        return self.raw[address:address + size]


class ShooterDiagnosticsProtocolTests(unittest.TestCase):
    def test_v4_prefix_remains_supported(self):
        result = snapshot(Reader(fixture(4)), 0)
        self.assertEqual(result['fired'], 0)
        self.assertNotIn('dispatch', result)

    def test_v5_observations_and_geometry(self):
        raw = fixture()
        struct.pack_into('<9Q', raw, 296, 8, 1, 2, 5, 1, 3, 2, 6, 4)
        struct.pack_into('<Q9f4I', raw, 368, 0x1234, 1, 2, 3, 0, 1, 0, 4, 5, 6, 2, 3, 1, 0)
        d = snapshot(Reader(raw), 0)['dispatch']
        self.assertEqual((d['call_success'], d['object_known'], d['object_unknown']), (5, 3, 2))
        self.assertEqual((d['rejected'], d['expired'], d['queue_full']), (1, 2, 1))
        self.assertEqual(d['origin'], [1, 2, 3])
        self.assertEqual(d['direction'], [0, 1, 0])
        self.assertEqual(d['aim_point'], [4, 5, 6])
        self.assertEqual((d['target'], d['route'], d['outcome'], d['hand']), ('0x1234', 2, 3, 1))
        self.assertNotIn('visible', d)
        self.assertNotIn('damage', d)

    def test_truncated_extension_rejected(self):
        self.assertIsNone(snapshot(Reader(fixture()[:-1]), 0))

    def test_odd_sequence_retried_not_reported(self):
        raw = fixture()
        struct.pack_into('<q', raw, 16, 3)
        self.assertIsNone(snapshot(Reader(raw), 0))

    def test_torn_sequence_rejected(self):
        self.assertIsNone(snapshot(Reader(fixture(), torn=True), 0))

    def test_unsupported_header_rejected(self):
        for version, size in ((6, 432), (5, 296), (4, 432)):
            raw = fixture()
            struct.pack_into('<2I', raw, 4, version, size)
            with self.assertRaisesRegex(RuntimeError, 'protocol mismatch'):
                snapshot(Reader(raw), 0)

    def test_counter_line_fits_fixed_log_slot(self):
        fmt = 'wb dispatch: q%llu full%llu exp%llu call%llu rej%llu obj%llu unk%llu g%llu f%llu'
        self.assertIn(fmt, (SOURCE.parents[1] / 'src/game_xr.cpp').read_text())
        self.assertLess(len(fmt.replace('%llu', str(2**64 - 1))), 240)


if __name__ == '__main__':
    unittest.main()
