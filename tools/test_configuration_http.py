#!/usr/bin/env python3
"""Read-only HTTP regression probe against a running clock (no retries)."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("origin", help="Clock URL, for example http://192.168.1.10")
    parser.add_argument("--count", type=int, default=40)
    parser.add_argument("--clients", type=int, default=1)
    args = parser.parse_args()
    if args.count < 1 or args.clients < 1:
        parser.error("count and clients must be positive")

    def probe(_):
        started = time.monotonic()
        # Each probe opens a fresh connection. Never print configuration data,
        # which may contain the device control secret.
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        try:
            with opener.open(args.origin.rstrip("/") + "/api/config", timeout=10) as response:
                payload = json.load(response)
                if response.status != 200 or payload.get("ok") is not True:
                    return False, "unexpected response", time.monotonic() - started
                if "saveConfirmationId" not in payload or "clockStyle" not in payload:
                    return False, "incomplete configuration", time.monotonic() - started
            return True, "", time.monotonic() - started
        except Exception as error:
            return False, type(error).__name__, time.monotonic() - started

    with ThreadPoolExecutor(max_workers=args.clients) as pool:
        results = list(pool.map(probe, range(args.count)))
    failures = [reason for ok, reason, _ in results if not ok]
    print(f"{args.count - len(failures)}/{args.count} successful; "
          f"clients={args.clients}; retries=0; "
          f"max latency={max(elapsed for _, _, elapsed in results):.3f}s")
    if failures:
        print("Failures: " + ", ".join(sorted(set(failures))))
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
