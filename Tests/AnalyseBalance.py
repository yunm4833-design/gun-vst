"""Offline 48 kHz stereo comparison for BalanceMeasure raw float renders."""
import argparse
import csv
import json
from pathlib import Path
import numpy as np
from scipy.signal import lfilter

MATERIALS = ["Drums", "Bass", "C-major pad", "Vocal-like", "Holdout drums", "Holdout transposed pad"]
MODES = ["PISTOL", "SHOTGUN", "SMG", "SNIPER", "REVOLVER"]


def estimate(samples):
    """Gated K-weighted estimate, not a certified LUFS meter."""
    y = lfilter([1.53512485958697, -2.69169618940638, 1.19839281085285],
                [1, -1.69065929318241, .73248077421585], samples, axis=0)
    y = lfilter([1, -2, 1], [1, -1.99004745483398, .99007225036621], y, axis=0)
    power = np.array([np.mean(np.sum(y[n:n + 19200] ** 2, axis=1))
                      for n in range(0, len(y) - 19200 + 1, 4800)])
    power = power[power > 10 ** ((-70 + .691) / 10)]
    if not len(power):
        return float("-inf")
    return float(-.691 + 10 * np.log10(np.mean(power[power > np.mean(power) * .1])))


def measure(folder):
    return np.array([[estimate(np.fromfile(folder / f"mode-{m}-input-{t}.f32", dtype="<f4").reshape(-1, 2))
                      for m in range(5)] for t in range(6)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--fit", type=Path, help="Optional untrimmed stage 6 renders, for reproducing trim selection")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {}
    with (args.output / "levels.csv").open("w", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(["stage", "input", *MODES, "spread_db"])
        for stage, folder in [("before", args.before), ("after", args.after)]:
            levels = measure(folder)
            spreads = np.ptp(levels, axis=1)
            report[stage] = dict(levels=levels.tolist(), spreads=spreads.tolist(), median_spread=float(np.median(spreads)))
            for i, name in enumerate(MATERIALS):
                writer.writerow([stage, name, *levels[i], spreads[i]])
            print(stage, "spreads", np.round(spreads, 2), "median", round(float(np.median(spreads)), 2))
    if args.fit:
        data = measure(args.fit)[:4]
        trims = -np.median(data - np.median(data, axis=1)[:, None], axis=0)
        trims -= np.median(trims)
        report["fitted_trim_db"] = np.round(np.clip(trims, -12, 12), 1).tolist()
        print("Fixed trim fit:", report["fitted_trim_db"])
    (args.output / "measurements.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
