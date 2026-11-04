"""Portable contracts for shared measurement I/O, without DLL/Wine dependencies."""
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from measurement_common import (
    built_executable, clean_environment, digest,
    full_file_levels, raw_stereo_nrmse, read_wav, run_logged,
)


class MeasurementIO(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def wav(self, name, payload, *, bits=16, encoding=1, channels=2,
            rate=44100, alignment=None, junk=False):
        fmt = struct.pack('<HHIIHH', encoding, channels, rate,
                          rate*channels*(bits//8),
                          channels*(bits//8) if alignment is None else alignment, bits)
        def chunk(tag, data):
            return tag + struct.pack('<I', len(data)) + data + b'\0'*(len(data) % 2)
        body = b'WAVE' + (chunk(b'JUNK', b'x') if junk else b'')
        body += chunk(b'fmt ', fmt) + chunk(b'data', payload)
        path = self.root/name
        path.write_bytes(b'RIFF' + struct.pack('<I', len(body)) + body)
        return path

    def test_pcm_widths_and_odd_metadata_keep_channel_order(self):
        for bits in (16, 24, 32):
            with self.subTest(bits=bits):
                scale = 2**(bits-1)
                values = [-scale, scale//2, 0, -scale//2]
                payload = b''.join(v.to_bytes(bits//8, 'little', signed=True) for v in values)
                path = self.wav(f'pcm{bits}.wav', payload, bits=bits, junk=True)
                rate, channels, decoded, mono = read_wav(path)
                self.assertEqual((rate, channels), (44100, 2))
                self.assertEqual(decoded, [-1., .5, 0., -.5])
                self.assertEqual(mono, [-.25, -.25])

    def test_float_audio_preserves_levels_above_unity(self):
        path = self.wav('float.wav', struct.pack('<4f', 1.5, -1.5, .25, -.25),
                        encoding=3, bits=32)
        self.assertEqual(read_wav(path)[2:], ([1.5, -1.5, .25, -.25], [0., 0.]))

    def test_invalid_frames_and_nonfinite_audio_are_rejected(self):
        valid = struct.pack('<4h', 1, 2, 3, 4)
        cases = [self.wav('empty.wav', b''),
                 self.wav('alignment.wav', valid, alignment=1),
                 self.wav('partial.wav', b'\0'*3),
                 self.wav('rate.wav', valid, rate=0),
                 self.wav('encoding.wav', valid, encoding=9),
                 self.wav('nan.wav', struct.pack('<2f', math.nan, 0), encoding=3, bits=32)]
        for path in cases:
            with self.subTest(path=path.name), self.assertRaises(ValueError):
                read_wav(path)

    def test_truncated_or_missing_wav_chunks_are_rejected(self):
        path = self.wav('truncated.wav', struct.pack('<4h', 1, 2, 3, 4))
        path.write_bytes(path.read_bytes()[:-1])
        with self.assertRaisesRegex(ValueError, 'Truncated'):
            read_wav(path)
        path.write_bytes(b'RIFF' + struct.pack('<I', 4) + b'WAVE')
        with self.assertRaisesRegex(ValueError, 'Missing'):
            read_wav(path)

    def test_stereo_error_does_not_downmix_or_fit_polarity(self):
        a = self.wav('a.wav', struct.pack('<4h', 16384, -16384, 8192, -8192))
        b = self.wav('b.wav', struct.pack('<4h', -16384, 16384, -8192, 8192))
        self.assertEqual(raw_stereo_nrmse(a, a), 0.)
        self.assertEqual(raw_stereo_nrmse(a, b), 2.)
        # Both downmixes are silent; a mono metric would miss the stereo error.
        self.assertEqual(full_file_levels(a, b)['reference']['mono_rms'], 0.)

    def test_raw_error_rejects_dimension_changes(self):
        a = self.wav('a.wav', struct.pack('<4h', 1, 2, 3, 4))
        others = [self.wav('rate.wav', struct.pack('<4h', 1, 2, 3, 4), rate=48000),
                  self.wav('mono.wav', struct.pack('<4h', 1, 2, 3, 4), channels=1),
                  self.wav('length.wav', struct.pack('<2h', 1, 2))]
        for b in others:
            with self.subTest(path=b.name), self.assertRaisesRegex(ValueError, 'dimensions'):
                raw_stereo_nrmse(a, b)

    def test_silent_reference_has_no_normalized_error(self):
        a = self.wav('silence.wav', b'\0'*8)
        b = self.wav('tone.wav', struct.pack('<4h', 1, 2, 3, 4))
        self.assertIsNone(raw_stereo_nrmse(a, b))

    def test_digest_and_decoded_sample_values(self):
        path = self.root/'abc'
        path.write_bytes(b'abc')
        expected = 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
        self.assertEqual(digest(path), expected)
        wav = self.wav('legacy.wav', struct.pack('<2h', 8192, -8192))
        self.assertEqual(read_wav(wav)[2:], ([.25, -.25], [0.]))

    def test_environment_cleanup_preserves_runtime_and_trust(self):
        env = {'SYNTH1_SEED': 'bad', 'SYNTH2_NOTE_OFF_SEC': 'bad', 'SYNTH_OSC_MODE': 'bad',
               'WINEPREFIX': '/wine prefix', 'WINELOADER': '/old/wine',
               'HTTPS_PROXY': 'http://proxy:8080', 'SSL_CERT_FILE': '/trust/ca.pem',
               'PATH': '/bin', 'SYNTH_OTHER': 'keep'}
        original = dict(env)
        wine = self.root/'wine with spaces'
        cleaned = clean_environment(env, wine=wine)
        self.assertEqual(env, original)
        self.assertEqual(cleaned, {**{k: v for k, v in original.items()
                                     if k not in ('SYNTH1_SEED', 'SYNTH2_NOTE_OFF_SEC', 'SYNTH_OSC_MODE')},
                                   'WINELOADER': str(wine.resolve())})


    def test_runner_passes_literal_arguments_and_keeps_binary_logs(self):
        log = self.root/'binary.log'
        literal = 'a b; $HOME $(not-a-command)'
        result = run_logged([sys.executable, '-c',
            'import sys;sys.stdout.buffer.write(sys.argv[1].encode()+b"\\xff");'
            'sys.stdout.flush();sys.stderr.buffer.write(b"err")', literal], log)
        expected = literal.encode() + b'\xfferr'
        self.assertEqual(result.stdout, expected)
        self.assertEqual(log.read_bytes(), expected)

    def test_runner_logs_failures_before_raising_or_returning(self):
        log = self.root/'failure.log'
        command = [sys.executable, '-c',
                   'import sys;sys.stdout.buffer.write(b"failed\\n");raise SystemExit(7)']
        with self.assertRaisesRegex(RuntimeError, 'Render failed \\(7\\)'):
            run_logged(command, log, text=True, failure_label='Render failed')
        self.assertEqual(log.read_text(), 'failed\n')
        result = run_logged(command, log, check=False)
        self.assertEqual(result.returncode, 7)
        self.assertEqual(log.read_bytes(), b'failed\n')

    def test_runner_applies_cwd_and_environment_in_text_mode(self):
        env = {**os.environ, 'MEASUREMENT_TEST_VALUE': 'literal value'}
        result = run_logged([sys.executable, '-c',
            'import os;print(os.getcwd());print(os.environ["MEASUREMENT_TEST_VALUE"])'],
            self.root/'text.log', cwd=self.root, env=env, text=True)
        lines = result.stdout.splitlines()
        self.assertTrue(self.root.samefile(lines[0]))
        self.assertEqual(lines[1:], ['literal value'])

    def test_timeout_retains_partial_output(self):
        error = subprocess.TimeoutExpired(['host'], 120, output=b'partial native log\n')
        with patch('measurement_common.subprocess.run', side_effect=error):
            with self.assertRaises(subprocess.TimeoutExpired):
                run_logged(['host'], self.root/'timeout.log', timeout=120, text=True)
        self.assertEqual((self.root/'timeout.log').read_bytes(), b'partial native log\n')


    def test_windows_executable_suffix_and_multi_config_directory(self):
        release = self.root/'Release'; release.mkdir()
        exe = release/'synth1_factory_host.exe'; exe.touch()
        self.assertEqual(built_executable(self.root, 'synth1_factory_host', config='Release'),
                         exe.resolve())
        with self.assertRaisesRegex(ValueError, 'Missing'):
            built_executable(self.root, 'absent', config='Release')


if __name__ == '__main__':
    unittest.main()
