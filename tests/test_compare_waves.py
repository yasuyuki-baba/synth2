import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import wave


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from measurement_common import full_file_levels

class WaveComparison(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.binary = os.environ["SYNTH2_COMPARE_BIN"]

    def tearDown(self):
        self.directory.cleanup()

    def wav(self, name, values):
        path = self.root / name
        with wave.open(str(path), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(44100)
            output.writeframes(struct.pack("<" + "h" * len(values), *values))
        return path

    def compare(self, a, b, window=220):
        return subprocess.run([self.binary, str(a), str(b), "--window", str(window)],
                              capture_output=True, text=True, timeout=15)

    def test_short_percussion_uses_onset_when_middle_is_silent(self):
        values = [int(16000 * math.sin(i * 0.17) * math.exp(-i / 800)) if i < 6000 else 0
                  for i in range(88200)]
        for kind, signal in [("decay", values), ("impulse", [16000] + [0] * 88199)]:
            with self.subTest(kind=kind):
                a = self.wav("reference.wav", signal)
                b = self.wav("delayed.wav", [0] * 37 + signal[:-37])
                result = self.compare(a, b)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("onset_fallback", result.stdout)
                self.assertIn("alignment_valid = 1", result.stdout)
                self.assertIn("best offset found: -37 samples", result.stdout)
                self.assertRegex(result.stdout, r"RMS diff\s*= 0(?:\s|$)")

    def test_silence_does_not_report_negative_one_correlation(self):
        a = self.wav("silence.wav", [0] * 2000)
        result = self.compare(a, a)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("alignment_valid = 0", result.stdout)
        self.assertRegex(result.stdout, r"corr\s*= 0(?:\s|$)")
        self.assertNotIn("abs_corr=-1", result.stdout)

    def test_one_silent_file_has_no_valid_alignment(self):
        a = self.wav("tone.wav", [int(10000 * math.sin(i * 0.17)) for i in range(2000)])
        b = self.wav("silence.wav", [0] * 2000)
        result = self.compare(a, b)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("alignment_valid = 0", result.stdout)
        value = re.search(r"RMS diff\s*= ([0-9.eE+-]+)", result.stdout)
        self.assertIsNotNone(value)
        self.assertGreater(float(value[1]), 0.1)

    def test_full_file_levels_keep_one_window_despite_delay_and_gain(self):
        # Equal-duration impulses at different times must keep their complete
        # window, not the candidate-dependent overlap selected by alignment.
        a = self.wav("reference.wav", [0]*20 + [16384] + [0]*79)
        b = self.wav("candidate.wav", [0]*50 + [8192] + [0]*49)
        levels = full_file_levels(a, b)
        self.assertEqual(levels["reference"]["frames"], 100)
        self.assertEqual(levels["candidate"]["frames"], 100)
        self.assertAlmostEqual(levels["reference"]["mono_rms"], .05)
        self.assertAlmostEqual(levels["candidate"]["mono_rms"], .025)
        self.assertAlmostEqual(levels["level_error_db"], -6.0205999133)
        self.assertFalse(levels["gain_fitted"])
        self.assertFalse(levels["time_aligned"])


    def test_negative_search_window_is_rejected(self):
        a = self.wav("silence.wav", [0] * 2000)
        result = self.compare(a, a, -1)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("nonnegative", result.stdout + result.stderr)




if __name__ == "__main__":
    unittest.main()
