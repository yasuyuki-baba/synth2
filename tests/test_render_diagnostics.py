"""The normal product probe must not accept process-wide DSP bypasses."""
import os
import math
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

    def test_controller_trace_contains_only_active_state_columns(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);trace=root/'controllers.tsv'
            env={key:value for key,value in os.environ.items()
                 if not key.startswith(('SYNTH1_', 'SYNTH2_', 'SYNTH_OSC1_'))}
            env.update(SYNTH2_PROBE_CONTROLLER_TRACE=str(trace),SYNTH2_PROBE_BLOCK_SIZE='64')
            subprocess.run([str(PROBE),str(root/'audio.wav'),'60','0.02','--float-wav'],
                           env=env,check=True,capture_output=True)
            rows=[line.split('\t') for line in trace.read_text().splitlines()]
            self.assertGreater(len(rows),1)
            for row in rows:
                self.assertEqual(len(row),10)
                self.assertEqual(list(map(int,row[1:5])),[0,0,0,0])
                self.assertTrue(all(math.isfinite(float(value)) for value in row[5:]))
            self.assertEqual(int(rows[0][0]),64)
            self.assertEqual(int(rows[-1][0]),882)

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
