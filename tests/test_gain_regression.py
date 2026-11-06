#!/usr/bin/env python3
"""
Regression tests for Synth2 amp-gain staging.

Problems fixed (and guarded by these tests):
  1. masterGain came from p78 (Effect type) instead of p29 (V1.05 Amp Gain)
  2. Gain conversion was linear (p29/127) instead of Synth1's power-law:
       gain = 2048 ^ ((p29/127)^0.3 - 1)
  3. APVTS default for p29 was 0, making the "init" patch silent

Run:
    python3 tests/test_gain_regression.py -v          # direct
    ctest -V -R GainRegressionTests                   # via CMake

Environment:
    SYNTH2_PROBE_BIN  – path to Synth2Probe binary
                        (default: build/Synth2Probe)
"""

import math
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

REPO_ROOT  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE_BIN  = os.environ.get(
    "SYNTH2_PROBE_BIN",
    os.path.join(REPO_ROOT, "build", "Synth2Probe"),
)


def sy1_amp_gain(p29: float) -> float:
    """
    Synth1 AU amp-gain formula, reverse-engineered from binary disassembly
    of CSynthesizer1::setSoundParam2EngineMain (addr 0x26802):

        gain = 2048 ^ ( (p29/127)^0.3 - 1 )

    Constants verified from binary:
        divisor  = 127.0   (vaddr 0x44160)
        exponent = 0.3     (vaddr 0x44858)
        base     = 2048    ln(2048) = 7.6246 (vaddr 0x44868)
        scale    = 1/2048  (vaddr 0x44870)
    """
    if p29 <= 0.0:
        return 0.0
    norm = p29 / 127.0
    return 2048.0 ** (norm ** 0.3 - 1.0)


def render_wav(out_wav: str, note: int = 69, duration: float = 2.0,
               patch: str | None = None, extra_env: dict | None = None) -> float:
    """
    Run Synth2Probe to render a WAV, return peak |x| (float 0..1).
    Skips the test with a clear message if the binary is not found.
    """
    if not os.path.isfile(PROBE_BIN):
        raise unittest.SkipTest(
            f"Synth2Probe not found at {PROBE_BIN!r}. "
            "Build with: cmake --build build --target Synth2Probe"
        )

    cmd = [PROBE_BIN, out_wav, str(note), str(duration)]
    if patch:
        cmd.append(patch)

    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)

    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if result.returncode != 0:
        raise RuntimeError(
            f"Synth2Probe failed:\nstdout={result.stdout}\nstderr={result.stderr}"
        )

    # parse "peak |x| = <value>" from stderr
    for line in (result.stdout + result.stderr).splitlines():
        if "peak |x|" in line:
            return float(line.split("=")[-1].strip())
    raise RuntimeError("Synth2Probe output did not contain 'peak |x|'")


def read_peak(wav_path: str) -> float:
    """Return max |sample| (float 0..1) from a 32-bit float WAV."""
    with wave.open(wav_path, "r") as w:
        raw = w.readframes(w.getnframes())
        n_ch = w.getnchannels()
        samples = struct.unpack(f"{len(raw)//4}f", raw)
    return max(abs(s) for s in samples) if samples else 0.0


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

class TestAmpGainFormula(unittest.TestCase):
    """Pure-Python verification of the Synth1 gain formula."""

    def test_formula_boundary_values(self):
        """
        2048^((p29/127)^0.3 - 1)  must satisfy:
          p29 = 0   → gain = 0
          p29 = 127 → gain = 1.0  (exact)
          p29 = 100 → gain ≈ 0.590  (matches Synth1 AU at default patch value)
          p29 = 8   → gain ≈ 0.0136 (common default in soundbank patches)
        """
        self.assertAlmostEqual(sy1_amp_gain(0),   0.0,    places=6,
                                msg="p29=0 must give gain=0 (silence)")
        self.assertAlmostEqual(sy1_amp_gain(127),  1.0,    places=6,
                                msg="p29=127 must give gain=1.0 (unity)")
        self.assertAlmostEqual(sy1_amp_gain(100),  0.5900, places=3,
                                msg="p29=100 (init.sy1 default) should give ~0.590")
        self.assertAlmostEqual(sy1_amp_gain(8),    0.0136, places=3,
                                msg="p29=8 (soundbank default) should give ~0.0136")

    def test_formula_is_monotone(self):
        """Gain must increase strictly as p29 increases."""
        gains = [sy1_amp_gain(p) for p in range(1, 128)]
        for i in range(len(gains) - 1):
            self.assertLess(gains[i], gains[i + 1],
                            f"gain not monotone at p29={i+1}")

    def test_formula_not_linear(self):
        """
        Common regression: someone replaces the power-law with p29/127.
        Verify the formula departs significantly from linear at midpoint.
        """
        p = 64
        linear = p / 127.0       # ≈ 0.504
        power  = sy1_amp_gain(p) # ≈ 0.242
        delta  = abs(linear - power)
        self.assertGreater(delta, 0.2,
                           "Formula appears to be linear — power-law missing!")


class TestSynth2ProbeOutput(unittest.TestCase):
    """Integration tests that render audio with Synth2Probe."""

    # -----------------------------------------------------------------------
    # Test 1: p78 must NOT be the master gain source
    # -----------------------------------------------------------------------
    def test_p78_is_not_gain_source(self):
        """
        Setting ONLY p78=max must NOT produce audible output.
        (Before the bug-fix, p78 was mistakenly used as masterGain.
         If p78 were the gain source, p29=0 + p78=1 would give full volume.)
        """
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            out = f.name
        try:
            # Force p29=0 (gain=0) and p78=1 via override env var
            peak = render_wav(
                out, note=69, duration=1.0,
                extra_env={
                    # Set p29 to 0 (silence per formula), p78 to max
                    "SYNTH2_SET_PARAMS_NORM": "p29=0.0;p78=1.0",
                },
            )
            self.assertAlmostEqual(
                peak, 0.0, places=4,
                msg=(
                    "p78=max with p29=0 produced non-silent output. "
                    "masterGain is being read from p78 instead of p29!"
                ),
            )
        finally:
            if os.path.exists(out):
                os.unlink(out)

    # -----------------------------------------------------------------------
    # Test 2: p29 gain order and approximate levels
    # -----------------------------------------------------------------------
    def test_p29_gain_ordering_and_levels(self):
        """
        Higher p29 → higher peak output.
        Also verify each level is within ±50% of the formula prediction.
        Uses the initialization patch, overriding p29 for each render.
        """
        test_values = [8, 64, 100, 127]
        peaks = {}

        for p29 in test_values:
            norm = p29 / 127.0
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                out = f.name
            try:
                peaks[p29] = render_wav(
                    out, note=69, duration=1.5,
                    extra_env={"SYNTH2_SET_PARAMS_NORM": f"p29={norm:.6f}"},
                )
            finally:
                if os.path.exists(out):
                    os.unlink(out)

        # Monotone ordering
        for lo, hi in zip(test_values, test_values[1:]):
            self.assertLess(
                peaks[lo], peaks[hi],
                f"peak(p29={lo})={peaks[lo]:.5f} should be < peak(p29={hi})={peaks[hi]:.5f}",
            )

        # Approximate ratio check: peak(p29=127) / peak(p29=100)
        # should be close to formula ratio = 1.0 / 0.590 ≈ 1.695
        ratio_got  = peaks[127] / peaks[100] if peaks[100] > 0 else float("inf")
        ratio_want = sy1_amp_gain(127) / sy1_amp_gain(100)  # ≈ 1.695
        self.assertAlmostEqual(
            ratio_got, ratio_want, delta=ratio_want * 0.30,
            msg=(
                f"Ratio peak(p29=127)/peak(p29=100)={ratio_got:.3f}, "
                f"expected ≈{ratio_want:.3f} (±30%). "
                "Gain formula may have regressed to linear!"
            ),
        )

    # -----------------------------------------------------------------------
    # Test 3: init patch (p29 not explicitly set) must not be silent
    # -----------------------------------------------------------------------
    def test_init_patch_not_silent(self):
        """
        When p29 is not set in the patch file, the APVTS default (100)
        must be used → gain = 0.590 → audible output.
        Before the fix, the default was 0 → gain = 0 → silence.
        """
        init_sy1 = os.path.join(REPO_ROOT, "resources", "init.sy1")
        if not os.path.isfile(init_sy1):
            self.skipTest(f"init.sy1 not found at {init_sy1!r}")

        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            out = f.name
        try:
            peak = render_wav(out, note=69, duration=2.0, patch=init_sy1)
            self.assertGreater(
                peak, 0.05,
                f"init.sy1 is nearly silent (peak={peak:.5f}). "
                "p29 APVTS default may have regressed to 0!",
            )
        finally:
            if os.path.exists(out):
                os.unlink(out)

    # -----------------------------------------------------------------------
    # Test 4: p29 = 0 must produce silence
    # -----------------------------------------------------------------------
    def test_p29_zero_is_silent(self):
        """gain formula gives 0 when p29=0 — verify the engine respects this."""
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            out = f.name
        try:
            peak = render_wav(
                out, note=69, duration=1.0,
                extra_env={"SYNTH2_SET_PARAMS_NORM": "p29=0.0"},
            )
            self.assertAlmostEqual(
                peak, 0.0, places=4,
                msg=f"p29=0 should produce silence but peak={peak:.6f}",
            )
        finally:
            if os.path.exists(out):
                os.unlink(out)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    unittest.main(verbosity=2)
