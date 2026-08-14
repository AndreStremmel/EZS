#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
trace_excerpt.py
  Cuts a time window out of a TeSSLa trace file and optionally shifts
  timestamps to start at 0.

  Why: a full trace easily exceeds 20,000 events - too many to
  meaningfully paste into the TeSSLa Playground's (play.tessla.io) trace
  editor, and too cluttered for a useful visualization. For a
  presentation, a focused excerpt around a specific occurrence is more
  informative anyway.

  Examples:
    # 200ms starting at timestamp 235000000, shift timestamps to 0
    python3 trace_excerpt.py trace.input -v 235000000 -d 200 -o short.input

    # keep only certain streams (repeatable flag)
    python3 trace_excerpt.py trace.input -v 235000000 -d 200 \\
        -s mtx_uart_ok -s mtx_uart_unlock -s exec -o short.input

    # just look at what's in the trace first
    python3 trace_excerpt.py trace.input --stats
"""

import argparse
import re
import sys
from collections import Counter

LINE_RE = re.compile(r"^\s*(\d+)\s*:\s*([A-Za-z_]\w*)\s*=\s*(.+?)\s*$")


def read_trace(path):
    """Reads the trace file and returns (timestamp, stream, value) tuples."""
    entries = []
    with open(path, encoding="utf-8") as f:
        for nr, line in enumerate(f, 1):
            if not line.strip():
                continue
            m = LINE_RE.match(line)
            if not m:
                print(f"Warning: line {nr} does not match the trace format, "
                      f"skipping: {line.rstrip()!r}", file=sys.stderr)
                continue
            entries.append((int(m.group(1)), m.group(2), m.group(3)))
    return entries


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="trace file (converter output)")
    ap.add_argument("-o", "--output", default="excerpt.input")
    ap.add_argument("-v", "--from-ts", type=int, default=None, dest="from_ts",
                    help="start timestamp in ns (default: first entry)")
    ap.add_argument("-d", "--duration", type=float, default=200.0,
                    help="excerpt duration in milliseconds (default: 200)")
    ap.add_argument("-s", "--stream", action="append", default=[],
                    help="keep only this stream (repeatable)")
    ap.add_argument("--keep-time", action="store_true",
                    help="do NOT shift timestamps to 0")
    ap.add_argument("--stats", action="store_true",
                    help="only show which streams occur how often")
    args = ap.parse_args()

    entries = read_trace(args.input)
    if not entries:
        print("No usable lines found.", file=sys.stderr)
        return 1

    if args.stats:
        counts = Counter(s for _, s, _ in entries)
        t_min = min(t for t, _, _ in entries)
        t_max = max(t for t, _, _ in entries)
        print(f"{len(entries)} events, "
              f"range {t_min} .. {t_max} ns "
              f"({(t_max - t_min) / 1e6:.1f} ms)")
        for stream, count in sorted(counts.items()):
            print(f"  {stream:30s} {count:6d}")
        return 0

    start = args.from_ts if args.from_ts is not None else min(t for t, _, _ in entries)
    end = start + int(args.duration * 1e6)

    filtered = [(t, s, w) for t, s, w in entries if start <= t <= end]
    if args.stream:
        allowed = set(args.stream)
        filtered = [(t, s, w) for t, s, w in filtered if s in allowed]

    if not filtered:
        print("The excerpt contains no events - check the time window or "
              "stream selection (--stats helps).", file=sys.stderr)
        return 1

    offset = 0 if args.keep_time else filtered[0][0]

    with open(args.output, "w", encoding="utf-8") as out:
        for t, s, w in filtered:
            out.write(f"{t - offset}: {s} = {w}\n")

    counts = Counter(s for _, s, _ in filtered)
    duration_ms = (filtered[-1][0] - filtered[0][0]) / 1e6
    print(f"{len(filtered)} events -> {args.output} "
          f"({duration_ms:.1f} ms excerpt)")
    for stream, count in sorted(counts.items()):
        print(f"  {stream:30s} {count:6d}")

    if len(filtered) > 500:
        print("\nNote: a few hundred events is the practical upper bound "
              "for the Playground. Use a smaller -d or a more targeted "
              "-s to cut it down further.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
