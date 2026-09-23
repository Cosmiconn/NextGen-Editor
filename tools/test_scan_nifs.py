import csv
import io
import subprocess
import sys
import tempfile
import time
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch
from scan_nifs import inventory, probe


class ScanHarnessTest(unittest.TestCase):
    def test_nested_archives_and_corruption(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            out = root / 'report'
            out.mkdir()
            inner = io.BytesIO()
            nif = b'Gamebryo File Format, Version 20.0.0.4\n\x04\x00\x00\x14'
            with zipfile.ZipFile(inner, 'w') as z:
                z.writestr('nested.nif', nif)
            with zipfile.ZipFile(root / 'outer.zip', 'w') as z:
                z.writestr('first.nif', nif)
                z.writestr('inner.zip', inner.getvalue())
            (root / 'broken.zip').write_bytes(b'not a ZIP central directory')
            rows = inventory([root / 'outer.zip', root / 'broken.zip'], out)
            self.assertEqual(len(rows), 1)
            with (out / 'inventory.tsv').open(encoding='utf-8', newline='') as f:
                self.assertEqual(len(list(csv.DictReader(f, delimiter='\t'))), 2)
            self.assertIn('ARCHIVE_NOT_TESTABLE', (out / 'archives.tsv').read_text())
            self.assertEqual(rows[0]['version'], '20.0.0.4')

    def test_real_hard_timeout_and_next_file(self):
        # Use real OS children while replacing only the worker command. The first
        # child would block for 60s; the next must still return immediately.
        real_run = subprocess.run
        def child(command, **kwargs):
            script = 'import time; time.sleep(60)' if 'slow.nif' in command[-1] else 'print("OK_GEOMETRY\\t1\\t3\\t1\\t0\\t")'
            return real_run([sys.executable, '-c', script], **kwargs)
        start = time.monotonic()
        with patch('scan_nifs.subprocess.run', side_effect=child):
            slow = probe(Path(sys.executable), Path('.'), {'sha256': 'slow'}, 'recovery', 0.2)
            good = probe(Path(sys.executable), Path('.'), {'sha256': 'good'}, 'standard', 3)
        self.assertEqual(slow['status'], 'TIMEOUT')
        self.assertEqual(good['status'], 'OK_GEOMETRY')
        self.assertLess(time.monotonic() - start, 5)


if __name__ == '__main__':
    unittest.main()
