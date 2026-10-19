import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
spec = importlib.util.spec_from_file_location('package', Path(__file__).parents[1]/'tools/package.py')
pkg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pkg)
class PackageTests(unittest.TestCase):
    def test_unfinished_cannot_be_packaged_as_mod(self):
        m = json.loads((pkg.ROOT/'release.json').read_text())
        m['release_ready'] = False
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError, 'unfinished'):
                pkg.validate_runtime(Path(d), m)
    def test_missing_and_fake_files_rejected(self):
        m = {'release_ready':True,'required_features':{'test':True},'required_files':['test.dll']}
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            with self.assertRaisesRegex(ValueError, 'Missing'):
                pkg.validate_runtime(p, m)
            (p/'test.dll').write_text('not a DLL')
            with self.assertRaisesRegex(ValueError, 'PE binary'):
                pkg.validate_runtime(p, m)
    def test_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError): pkg.archive(Path(d)/'bad.zip', {'../bad':b'x'})
    def test_archive_reproducible(self):
        with tempfile.TemporaryDirectory() as d:
            a, b = Path(d)/'a.zip', Path(d)/'b.zip'
            pkg.archive(a, {'b':b'2','a':b'1'}); pkg.archive(b, {'a':b'1','b':b'2'})
            self.assertEqual(a.read_bytes(), b.read_bytes())
if __name__ == '__main__': unittest.main()
