#!/usr/bin/env python3
"""
Plot MFCC features saved by mfcc_demo.c.

Expected file format:
  - raw float32
  - row-major
  - default shape [49, 13]
"""

import argparse
import os
import sys

import numpy as np


def load_mfcc(path, frames, coeffs):
    data = np.fromfile(path, dtype=np.float32)
    expected = frames * coeffs
    if data.size == 0:
        raise ValueError(f"empty file: {path}")
    if data.size % expected != 0:
        raise ValueError(
            f"file size mismatch: got {data.size} float32 values, "
            f"expected a multiple of {expected}"
        )
    sample_count = data.size // expected
    return data.reshape(sample_count, frames, coeffs)


def plot_sample(sample, title, cmap):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit("matplotlib is required: pip install matplotlib") from exc

    fig, ax = plt.subplots(figsize=(10, 4))
    image = ax.imshow(
        sample.T,
        origin="lower",
        aspect="auto",
        interpolation="nearest",
        cmap=cmap,
    )
    ax.set_xlabel("Frame")
    ax.set_ylabel("MFCC Coeff")
    ax.set_title(title)
    fig.colorbar(image, ax=ax, label="Value")
    fig.tight_layout()
    return fig


def main():
    parser = argparse.ArgumentParser(description="Plot MFCC binary output.")
    parser.add_argument("input", help="Raw float32 mfcc bin file")
    parser.add_argument("--frames", type=int, default=49)
    parser.add_argument("--coeffs", type=int, default=13)
    parser.add_argument("--sample-index", type=int, default=0)
    parser.add_argument("--cmap", default="viridis")
    parser.add_argument("--title", default="")
    parser.add_argument("--save", default="", help="Save figure to image file")
    parser.add_argument("--show", action="store_true", help="Open an interactive window")
    parser.add_argument("--dpi", type=int, default=160)
    args = parser.parse_args()

    mfcc = load_mfcc(args.input, args.frames, args.coeffs)
    if args.sample_index < 0 or args.sample_index >= mfcc.shape[0]:
        raise SystemExit(
            f"sample-index out of range: {args.sample_index}, "
            f"available 0..{mfcc.shape[0] - 1}"
        )

    if mfcc.shape[0] > 1:
        print(
            f"loaded {mfcc.shape[0]} samples, showing sample {args.sample_index}",
            file=sys.stderr,
        )

    sample = mfcc[args.sample_index]
    title = args.title or f"{os.path.basename(args.input)} [{args.sample_index}]"
    fig = plot_sample(sample, title, args.cmap)

    if args.show:
        import matplotlib.pyplot as plt

        plt.show()
        return

    output_path = args.save or os.path.splitext(args.input)[0] + ".png"
    fig.savefig(output_path, dpi=args.dpi, bbox_inches="tight")
    print(f"saved to {output_path}")


if __name__ == "__main__":
    main()
