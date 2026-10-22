"""Pure comparison-contract tests; no renderer/quality/appearance certification."""
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / 'tools/CompareWaterDiagnosticFrames.py'
spec = importlib.util.spec_from_file_location('water_pair', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class WaterDiagnosticRasterComparatorTest(unittest.TestCase):
    def setUp(self):
        matrix = ','.join('1065353216' if i % 5 == 0 else '0' for i in range(16))+','
        self.base = dict(uploaded_vp_bits=matrix, uploaded_unjittered_vp_bits=matrix,
                         jitter='0.0546875,-0.166666716', window='0.5,0.545,0.052,0.182,1,0.985,0.65,0,')

    def test_exact_finite_actual_raster_accepts(self):
        self.assertTrue(module.raster_equal(self.base, dict(self.base)))

    def test_unjittered_match_does_not_cover_uploaded_change(self):
        other = dict(self.base, uploaded_vp_bits=self.base['uploaded_vp_bits'].replace('1065353216', '1065353217', 1))
        self.assertFalse(module.raster_equal(self.base, other))

    def test_jitter_change_rejects(self):
        self.assertFalse(module.raster_equal(self.base, dict(self.base, jitter='0.3046875,-0.388888955')))

    def test_view_window_change_rejects(self):
        self.assertFalse(module.raster_equal(self.base, dict(self.base, window='0.5,0.546,0.052,0.182,1,0.985,0.65,0,')))

    def test_missing_and_nonfinite_metadata_rejects(self):
        missing = dict(self.base); del missing['jitter']
        self.assertFalse(module.raster_equal(missing, missing))
        for key, bad in [('jitter', 'nan,0'), ('window', '0,0,0,0,0,0,0,inf'),
                         ('uploaded_vp_bits', self.base['uploaded_vp_bits'].replace('1065353216', '2139095040', 1)),
                         ('uploaded_unjittered_vp_bits', '0,'*15),
                         ('uploaded_vp_bits', self.base['uploaded_vp_bits'].replace('1065353216', '4294967296', 1))]:
            with self.subTest(key=key, bad=bad):
                other = dict(self.base); other[key] = bad
                self.assertFalse(module.raster_equal(other, other))

if __name__ == '__main__':
    unittest.main()
