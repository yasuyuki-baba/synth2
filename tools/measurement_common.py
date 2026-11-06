"""Shared I/O for reference measurements, independent of DLLs and Synth2 builds.

Preserve numeric conventions here; capture histories, ABI checks and pass bounds
belong to the calling workflow. CLI modules re-export their historical helpers.
"""
import hashlib
import math
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def built_executable(directory, name, *, config=None):
    """Locate a normal or Windows executable, including a multi-config build."""
    directories = [Path(directory)]
    if config:
        directories.append(Path(directory)/config)
    for parent in directories:
        for filename in (name, name + ".exe"):
            path = parent/filename
            if path.is_file():
                return path.resolve()
    raise ValueError(f"Missing {name} executable in {directory}")


def clean_environment(env=None, *, wine=None):
    """Copy the environment, clearing diagnostic overrides but retaining trust/VPN settings."""
    cleaned = {key: value for key, value in (os.environ if env is None else env).items()
               if not key.startswith(("SYNTH2_", "SYNTH1_", "SYNTH_OSC"))}
    if wine:
        cleaned["WINELOADER"] = str(Path(wine).resolve())
    return cleaned


def run_logged(command, log, *, env=None, cwd=ROOT, timeout=None, text=False,
               check=True, failure_label="Command failed"):
    """Run without a shell, retaining merged output before reporting failure."""
    try:
        result = subprocess.run([str(value) for value in command], cwd=cwd, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=timeout, text=text)
    except subprocess.TimeoutExpired as error:
        # TimeoutExpired.stdout is bytes even when the command uses text mode.
        output = error.stdout or b""
        if isinstance(output, str):
            output = output.encode("utf-8")
        Path(log).write_bytes(output)
        raise
    if text:
        Path(log).write_text(result.stdout, encoding="utf-8")
    else:
        Path(log).write_bytes(result.stdout)
    if check and result.returncode:
        raise RuntimeError(f"{failure_label} ({result.returncode}); see {log}")
    return result


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_wav(path):
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"Not a RIFF WAV: {path}")
    offset, fmt, payload = 12, None, None
    while offset + 8 <= len(data):
        name, size = struct.unpack_from("<4sI", data, offset)
        chunk = data[offset + 8:offset + 8 + size]
        if len(chunk) != size:
            raise ValueError("Truncated WAV chunk")
        if name == b"fmt ": fmt = struct.unpack_from("<HHIIHH", chunk)
        if name == b"data": payload = chunk
        offset += 8 + size + size % 2
    if fmt is None or payload is None:
        raise ValueError("Missing WAV format or data")
    encoding, channels, rate, _, alignment, bits = fmt
    if channels < 1 or rate < 1 or alignment != channels * (bits // 8) or len(payload) % alignment:
        raise ValueError("Invalid WAV frame format")
    width = bits // 8
    if encoding == 3 and bits == 32:
        values = list(struct.unpack("<" + "f" * (len(payload) // 4), payload))
    elif encoding == 1 and bits in (16, 24, 32):
        values = [int.from_bytes(payload[i:i + width], "little", signed=True) / (2 ** (bits - 1))
                  for i in range(0, len(payload), width)]
    else:
        raise ValueError(f"Unsupported WAV encoding: {encoding}/{bits}")
    if not values or not all(math.isfinite(value) for value in values):
        raise ValueError("Empty or non-finite audio")
    mono = [sum(values[i:i + channels]) / channels for i in range(0, len(values), channels)]
    return rate, channels, values, mono


def statistics(path, note_seconds, pre_roll_seconds=0.0):
    rate, channels, values, mono = read_wav(path)
    offset = round(pre_roll_seconds * rate)
    begin, end = offset + int(note_seconds * rate * 0.4), offset + int(note_seconds * rate * 0.8)
    steady = mono[begin:end]
    if not steady: raise ValueError("No steady-note analysis window")
    crossings = [begin + i - 1 + (-steady[i-1]) / (steady[i] - steady[i-1])
                 for i in range(1, len(steady)) if steady[i-1] < 0 <= steady[i]]
    rms = lambda samples: math.sqrt(sum(value * value for value in samples) / len(samples))
    return {
        "sample_rate": rate, "channels": channels, "frames": len(mono),
        "channel_peak": [max(abs(value) for value in values[channel::channels]) for channel in range(channels)],
        "peak": max(abs(value) for value in values), "mono_rms": rms(mono),
        "steady_mono_rms": rms(steady),
        "positive_crossing_hz": ((len(crossings) - 1) * rate / (crossings[-1] - crossings[0])
                                 if len(crossings) > 2 else None),
        "first_nonzero_frame": next((i for i, value in enumerate(mono) if abs(value) > 1e-7), None),
        "mono_dc": sum(mono) / len(mono),
    }


def full_file_levels(reference, candidate):
    # Alignment changes the overlap used by CompareWaves RMS. Also retain
    # fixed full-file levels so before/after level comparisons share one window.
    levels = {}
    for name, path in (("reference", reference), ("candidate", candidate)):
        rate, channels, _, mono = read_wav(path)
        levels[name] = {"sample_rate": rate, "channels": channels, "frames": len(mono),
                        "mono_rms": math.sqrt(sum(value*value for value in mono) / len(mono))}
    levels["gain_fitted"] = levels["time_aligned"] = False
    ref_rms, test_rms = levels["reference"]["mono_rms"], levels["candidate"]["mono_rms"]
    levels["level_error_db"] = 20 * math.log10(test_rms / ref_rms) if ref_rms > 0 and test_rms > 0 else None
    return levels


def raw_stereo_nrmse(reference, candidate):
    """Full interleaved-channel error without alignment, fitting or downmixing.

    A zero-energy reference has no defined normalized error and returns None,
    matching the comparison report's existing JSON null convention.
    """
    rate, channels, x, _ = read_wav(reference)
    test_rate, test_channels, y, _ = read_wav(candidate)
    if (rate, channels, len(x)) != (test_rate, test_channels, len(y)):
        raise ValueError("Different raw stereo dimensions")
    energy = sum(v*v for v in x)
    return math.sqrt(sum((a-b)**2 for a, b in zip(x, y))/energy) if energy > 0 else None
