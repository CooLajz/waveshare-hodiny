#!/usr/bin/env python3
"""Compare the compiled clock converter against an independent IANA reader.

Usage: python3 tools/test_clock_timezone_database.py ZONEINFO TEST_BINARY
Compile TEST_BINARY from test_clock_timezone.cpp and ClockTimezone.cpp.
Use the same pinned IANA release and zic as generate_timezones.py.
"""
import datetime as dt
from pathlib import Path
import re
import subprocess
import sys
from zoneinfo import ZoneInfo

root = Path(sys.argv[1])
header = (Path(__file__).resolve().parents[1] / "WaveshareHodiny/ClockTimezoneData.h").read_text()
rules = {int(i): [int(x) for x in re.findall(r"\{(\d+)U,", body)]
         for i, body in re.findall(r"timezoneTransitions(\d+)\[\] = \{(.*?)\};", header, re.S)}
names = [(name, int(i)) for name, i in re.findall(r'\{"([^"]+)", (\d+)\}', header)]
seen, probes, expected = set(), [], []
for name, index in names:
    with (root / name).open("rb") as stream:
        zone = ZoneInfo.from_file(stream)
    timestamps = {int(dt.datetime(y, m, 15, 12, tzinfo=dt.timezone.utc).timestamp())
                  for y in [2020, 2026, 2038, 2050, 2100] for m in [1, 4, 7, 10]}
    if index not in seen:
        seen.add(index)
        timestamps.update(t + delta for t in rules[index][1:] for delta in [-1, 0, 1])
    for stamp in sorted(timestamps):
        local = dt.datetime.fromtimestamp(stamp, dt.timezone.utc).astimezone(zone)
        probes.append(f"{name} {stamp}")
        expected.append(f"{local.year} {local.month} {local.day} {local.hour} "
                        f"{local.minute} {local.second} {int(bool(local.dst()))}")
result = subprocess.run([sys.argv[2], "--probe"], input="\n".join(probes) + "\n",
                        capture_output=True, text=True, check=True)
actual = result.stdout.splitlines()
assert len(actual) == len(expected)
for probe, want, got in zip(probes, expected, actual):
    assert want == got, (probe, want, got)
print(f"IANA comparison passed: {len(probes)} cases, {len(names)} zones, {len(seen)} rule sets")
