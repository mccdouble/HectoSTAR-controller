"""Writes pattern parity test cases (CSV) covering every waveform type,
edge-case parameters and every device mapping.

Usage: python make_cases.py <case dir> [random case count]
"""

import os
import random
import sys

DEVICES = ["4 Shank Chronic", "2 Shank Chronic", "4 Shank Acute", "Demo Board"]
TYPES = ["none", "pulse", "fixedpulse", "pulse_staircase", "sawtooth",
         "fixedsawtooth", "sine", "fixedsine", "doublesine", "wavelet"]


def fmt(v):
    # repr round-trips doubles exactly; MATLAB str2double and C strtod read it back exactly
    return repr(float(v))


def write_case(path, duration, device, rows):
    with open(path, "w", newline="\n") as f:
        f.write(f"duration,{fmt(duration)}\n")
        f.write(f"device,{device}\n")
        for ch, wtype, freq, amp, off, duty, delay, step in rows:
            f.write(",".join([str(ch), wtype, fmt(freq), fmt(amp), fmt(off),
                              fmt(duty), fmt(delay), fmt(step)]) + "\n")


def handmade():
    """Targeted cases, one feature each."""
    c = []
    # every type on one channel, reasonable parameters
    c.append(("sine_basic", 0.25, 0, [(1, "sine", 10, 40, 50, 0, 0, 0)]))
    c.append(("sine_delay_unaligned", 0.2, 0, [(5, "sine", 37.3, 30, 30, 0, 0.012345, 0)]))
    c.append(("pulse_basic", 0.25, 0, [(2, "pulse", 20, 50, 50, 25, 0, 0)]))
    c.append(("pulse_duty_0_100", 0.1, 2, [(3, "pulse", 20, 50, 50, 0, 0, 0),
                                           (4, "pulse", 20, 50, 50, 100, 0, 0)]))
    c.append(("fixedpulse_basic", 0.5, 0, [(7, "fixedpulse", 10, 60, 0.02, 50, 0.01, 0)]))
    c.append(("fixedpulse_amp_over_100", 0.1, 3, [(8, "fixedpulse", 3, 250, 0.01, 30, 0, 0)]))
    c.append(("staircase_up", 0.5, 0, [(9, "pulse_staircase", 8, 10, 0.05, 40, 0.0, 12)]))
    c.append(("staircase_down_negative", 0.5, 2, [(10, "pulse_staircase", 8, 90, 0.05, 40, 0.003, -15)]))
    c.append(("sawtooth_basic", 0.25, 0, [(11, "sawtooth", 12, 40, 50, 0, 0, 0)]))
    c.append(("sawtooth_negative_freq", 0.1, 1, [(12, "sawtooth", -7, 40, 50, 0, 0, 0)]))
    c.append(("fixedsawtooth_duty", 0.3, 0, [(13, "fixedsawtooth", 5, 80, 0.04, 60, 0.005, 0)]))
    c.append(("fixedsine_duty", 0.3, 2, [(14, "fixedsine", 4, 70, 0.05, 50, 0.02, 0)]))
    c.append(("fixedsine_duty_over_100", 0.2, 3, [(15, "fixedsine", 4, 70, 0.03, 150, 0, 0)]))
    c.append(("doublesine_basic", 0.25, 0, [(16, "doublesine", 5, 60, 40, 30, 0, 0)]))
    c.append(("wavelet_odd", 0.2, 0, [(17, "wavelet", 5, 90, 0.1, 0, 0.05, 0)]))
    c.append(("wavelet_even_peaks", 0.2, 2, [(18, "wavelet", 4, 90, 0.1, 0, 0.02, 0)]))
    c.append(("zero_period_and_counts", 0.05, 0, [(19, "fixedpulse", 5, 50, 0, 50, 0, 0),
                                                  (20, "fixedsine", 0, 50, 0.01, 50, 0, 0),
                                                  (21, "wavelet", 0, 50, 0.01, 0, 0, 0)]))
    c.append(("delay_past_end", 0.05, 0, [(22, "sine", 10, 40, 50, 0, 1.0, 0)]))
    c.append(("nan_inf_params", 0.02, 0, [(23, "sine", float("nan"), 40, 50, 0, 0, 0),
                                          (24, "fixedsine", 3, 40, float("inf"), 50, 0, 0),
                                          (25, "pulse", 10, float("nan"), 50, 50, 0, 0)]))
    # padding: sample counts not a multiple of 4
    for n in (1, 2, 3, 5, 7):
        c.append((f"pad_{n}_samples", n / 20000.0, 0, [(1, "sine", 1000, 50, 50, 0, 0, 0)]))
    # mapping sweep: each channel gets a distinct constant so remapping errors show up
    for d, dev in enumerate(DEVICES):
        rows = [(ch, "pulse", 1, 0, ch, 50, 0, 0) for ch in range(1, 65)]  # offset = ch uA
        c.append((f"mapping_{dev.replace(' ', '_')}", 0.001, d, rows))
    # staggered sweep down all LEDs (typical use)
    rows = [(ch, "fixedpulse", 5, 40, 0.01, 30, (ch - 1) * 0.002, 0) for ch in range(1, 65)]
    c.append(("stagger_all_64", 0.3, 0, rows))
    return c


def random_row(rng, ch):
    wtype = rng.choice(TYPES)
    edge = rng.random() < 0.2
    freq = rng.choice([0, 1, 2, 3, 5, 8, 13, 0.5, 2.5, 7.3, 40, 123.456]) if edge else rng.uniform(0.1, 200)
    if wtype in ("fixedpulse", "pulse_staircase", "fixedsawtooth", "fixedsine", "wavelet"):
        freq = rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 10, 2.5, 3.49, 3.5]) if edge else rng.randint(1, 20)
    amp = rng.choice([0, 1, 50, 99.5, 100, 100.5, 150, -10]) if edge else rng.uniform(0, 100)
    if wtype in ("fixedpulse", "pulse_staircase", "fixedsawtooth", "fixedsine", "wavelet"):
        offset = rng.choice([0, 0.001, 0.0001, 0.05, 1 / 3, -0.01]) if edge else rng.uniform(0.0005, 0.08)
    elif wtype == "doublesine":
        offset = rng.uniform(0.1, 200)  # second frequency
    else:
        offset = rng.choice([0, 50, 100, -20]) if edge else rng.uniform(0, 100)
    duty = rng.choice([0, 100, 50, 33.3, 150, -5, 99.999]) if edge else rng.uniform(0, 100)
    delay = rng.choice([0, 1 / 20000, 0.5 / 20000, 0.0123456789, 0.1]) if edge else rng.uniform(0, 0.15)
    step = rng.choice([0, 5, -5, 25.5, -33]) if edge else rng.uniform(-20, 20)
    return (ch, wtype, freq, amp, offset, duty, delay, step)


def main():
    out = sys.argv[1]
    n_random = int(sys.argv[2]) if len(sys.argv) > 2 else 150
    os.makedirs(out, exist_ok=True)

    count = 0
    for name, duration, dev, rows in handmade():
        write_case(os.path.join(out, f"h_{name}.csv"), duration, DEVICES[dev], rows)
        count += 1

    rng = random.Random(20261103)
    for i in range(n_random):
        duration = rng.choice([0.0123, 0.05, 0.1, 0.2, 0.25, 0.3337])
        channels = rng.sample(range(1, 65), rng.randint(1, 64))
        rows = [random_row(rng, ch) for ch in sorted(channels)]
        write_case(os.path.join(out, f"r_{i:03d}.csv"), duration, rng.choice(DEVICES), rows)
        count += 1

    print(f"wrote {count} cases to {out}")


if __name__ == "__main__":
    main()
