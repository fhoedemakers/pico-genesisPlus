#!/usr/bin/env python3
"""
hosttest/wavglitch.py — measure audio glitches in a WAV capture.

Built to diagnose "the audio crackles": it turns a vague complaint into
numbers that say *what kind* of glitch it is and *where it comes from*.

    python3 hosttest/wavglitch.py <file.wav> [more.wav ...]

Reports, per file:

  glitch rate      discontinuities per second, found as samples whose value
                   deviates far from a linear prediction relative to the
                   local residual level (so it scales with the music).

  frame lock       the best-fitting repeat period for those glitches. A
                   lock at 60 Hz (735 samples) or 50 Hz (882) means the
                   artifact is generated once per emulated frame — a
                   buffer/pacing problem, not chip emulation. A low vector
                   strength (< ~0.2) means no periodicity: musical
                   transients, i.e. probably clean.

  dip widths       length of each drop-to-silence. Widths that are exact
                   multiples of 4 are HDMI data-island silence packets
                   spliced in by hstx_di_queue_get_audio_packet() when the
                   ring runs dry — the fingerprint of a sink underrun.

  clipping         samples pinned at full scale (a mixer/volume problem,
                   a different bug entirely).

  repeats          longest run of identical samples (a stalled producer).

Exit status is 1 if any file shows a 50/60 Hz-locked glitch pattern, so it
can be used as a pass/fail gate on a recapture.
"""

import sys
import wave

import numpy as np

# A residual this many times the local RMS is a discontinuity, not music.
GLITCH_SIGMA = 8.0
# Detections closer together than this belong to the same event.
CLUSTER_GAP = 20
# How far the emulated frame rate may sit from nominal. A real per-frame
# artifact lands within a few parts in 10^5 (the capture clock); anything
# a percent away is a coincidental fit, not a frame lock.
PERIOD_TOLERANCE = 0.005
# Rayleigh test on the folded phases. exp(-n*r^2) is the probability that n
# random phases would cluster this tightly; the search tries ~700 periods
# per frame rate, so demand far more than a nominal 0.05.
MAX_LOCK_P = 1e-6
MIN_LOCK_GLITCHES = 20
# Half-width of the window a dip is measured in. A dip that fills the
# window is not a dip, it is a quiet passage.
DIP_WINDOW = 40


def read_mono(path):
    with wave.open(path) as w:
        if w.getsampwidth() != 2:
            raise SystemExit(f"{path}: need 16-bit PCM, got {w.getsampwidth() * 8}-bit")
        rate = w.getframerate()
        channels = w.getnchannels()
        data = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
    if channels > 1:
        data = data[::channels]
    return data.astype(np.float64), rate


def find_glitches(x):
    """Indices where the signal jumps far more than it locally does."""
    residual = np.abs(x[2:] - (2 * x[1:-1] - x[:-2]))
    window = 2048
    if len(residual) <= window:
        return np.empty(0, dtype=int)
    energy = np.cumsum(np.concatenate(([0.0], residual**2)))
    local = np.sqrt((energy[window:] - energy[:-window]) / window)
    # Centre the moving RMS over the samples it describes.
    pad_lo = window // 2
    pad_hi = len(residual) - len(local) - pad_lo
    local = np.concatenate((np.full(pad_lo, local[0]), local, np.full(pad_hi, local[-1])))

    hits = np.flatnonzero(residual > GLITCH_SIGMA * local) + 2
    if len(hits) == 0:
        return hits
    clustered = [hits[0]]
    for h in hits[1:]:
        if h - clustered[-1] > CLUSTER_GAP:
            clustered.append(h)
    return np.array(clustered)


def best_period(hits, rate):
    """Fold the glitch positions over candidate frame periods.

    Returns (strength, period, p_value) for the best fit near 50 or 60 Hz,
    or None when there are too few glitches to say anything.
    """
    n = len(hits)
    if n < MIN_LOCK_GLITCHES:
        return None
    best = (0.0, 0.0)
    for frame_hz in (60.0, 50.0):
        nominal = rate / frame_hz
        lo, hi = nominal * (1 - PERIOD_TOLERANCE), nominal * (1 + PERIOD_TOLERANCE)
        for period in np.arange(lo, hi, nominal * 0.00003):
            strength = abs(np.exp(2j * np.pi * (hits % period) / period).mean())
            if strength > best[0]:
                best = (strength, period)
    return best[0], best[1], float(np.exp(-n * best[0] ** 2))


def dip_widths(x, hits):
    """Width of the drop-to-silence around each glitch, in samples."""
    widths = []
    for h in hits:
        lo, hi = max(0, h - DIP_WINDOW), min(len(x), h + DIP_WINDOW)
        reference = np.percentile(np.abs(x[max(0, h - 300) : h + 300]), 75)
        if reference < 50:  # too quiet to tell a dip from the signal
            continue
        quiet = np.abs(x[lo:hi]) < 0.20 * reference
        centre = int(np.argmin(np.abs(x[max(0, h - 8) : h + 8]))) + max(0, h - 8) - lo
        if not (0 <= centre < len(quiet)) or not quiet[centre]:
            continue
        start = end = centre
        while start > 0 and quiet[start - 1]:
            start -= 1
        while end < len(quiet) - 1 and quiet[end + 1]:
            end += 1
        if start == 0 or end == len(quiet) - 1:
            continue  # runs off the window: a quiet passage, not a splice
        widths.append(end - start + 1)
    return np.array(widths, dtype=int)


def longest_repeat(x):
    same = (x[1:] == x[:-1]).view(np.int8)
    edges = np.flatnonzero(np.diff(np.concatenate(([0], same, [0]))))
    if len(edges) == 0:
        return 0
    return int((edges[1::2] - edges[0::2]).max())


def report(path):
    x, rate = read_mono(path)
    seconds = len(x) / rate
    print(f"{path}: {seconds:.2f} s @ {rate} Hz, peak {int(np.abs(x).max())}")

    clipped = int(np.sum(x >= 32767) + np.sum(x <= -32768))
    print(f"  clipping : {clipped} samples")
    print(f"  repeats  : longest identical run {longest_repeat(x)} samples")

    hits = find_glitches(x)
    print(f"  glitches : {len(hits)} ({len(hits) / seconds:.2f}/s)")
    if len(hits) == 0:
        return False

    locked = False
    fold = best_period(hits, rate)
    if fold is None:
        print(f"  frame    : only {len(hits)} glitches, too few to test for periodicity")
    else:
        strength, period, p_value = fold
        locked = p_value < MAX_LOCK_P
        verdict = "LOCKED" if locked else "no lock"
        print(f"  frame    : {verdict} — period {period:.1f} samples "
              f"({rate / period:.3f} Hz), strength {strength:.2f}, p={p_value:.1e}")

    widths = dip_widths(x, hits)
    if len(widths):
        counts = np.bincount(widths)
        common = " ".join(f"{w}:{counts[w]}" for w in np.argsort(counts)[::-1][:6] if counts[w])
        # +/-1 sample: the dip edges are smeared by the capture chain.
        packetish = int(np.sum(np.minimum(widths % 4, 4 - widths % 4) <= 1))
        print(f"  dips     : {len(widths)} measured, widths {common} "
              f"({packetish} within 1 of a multiple of 4)")
        if locked and packetish > len(widths) // 2:
            print("             -> matches HDMI silence-packet insertion (sink underrun)")
    return locked


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip())
        return 2
    bad = False
    for path in argv[1:]:
        bad |= report(path)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
