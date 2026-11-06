"""The normal product probe must not accept process-wide DSP bypasses."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PROBE = Path(sys.argv.pop(1)).resolve()


class RenderDiagnosticsTest(unittest.TestCase):
    def test_product_ignores_legacy_bypass_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            clean = {key: value for key, value in os.environ.items()
                     if not key.startswith(('SYNTH1_', 'SYNTH2_', 'SYNTH_OSC1_'))}
            def render(name, env):
                path = root / name
                subprocess.run([str(PROBE), str(path), '60', '0.2', '--float-wav'],
                               env=env, check=True, capture_output=True)
                return path.read_bytes()
            expected = render('normal.wav', clean)
            altered = dict(clean, SYNTH2_BYPASS_ENV='1', SYNTH2_BYPASS_FILTER='1',
                           SYNTH2_BYPASS_FX='1')
            self.assertEqual(expected, render('environment.wav', altered))

    def test_product_rejects_diagnostic_switches(self):
        with tempfile.TemporaryDirectory() as directory:
            for option in ('--bypass-env', '--bypass-filter', '--bypass-fx'):
                with self.subTest(option=option):
                    result = subprocess.run([str(PROBE), str(Path(directory) / 'out.wav'),
                                             '60', '0.2', option], capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('Synth2DiagnosticProbe', result.stderr)


if __name__ == '__main__':
    unittest.main()
