#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 the OpenRAC contributors
"""The OpenRAC mod SDK that sdk.lock pins, into .sdk/.

    python3 tools/fetch_sdk.py [--from ZIP]

downloads sdk.lock's url (or takes a local archive with --from), checks it against sdk.lock's
sha256, and unpacks it as .sdk/openrac-mod-sdk-<version>/, which CMakeLists.txt finds. An SDK
already there of that version and hash is kept. Prints the SDK's folder. Stdlib only.

A lock with no sha256 yet (a repository made from the SDK's template, whose own archive cannot name
its hash) takes the hash the release publishes beside the archive (<url>.sha256) and writes it into
sdk.lock: commit it, and every later build is held to it. This file is the SDK's
(template/tools/fetch_sdk.py); copy it again when it changes.

This mod's copy also takes an SDK that has no release yet: a lock with "git" (OpenRAC's repository)
and "commit" (a full commit id) instead of "url" and "sha256" takes the SDK's source folder, sdk/,
from that commit (a sparse checkout, with git), checks that its sdk/VERSION is the lock's version,
and puts it where the archive would go. Back to a released SDK with a lock of url and sha256.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def from_git(lock: dict) -> int:
    """The SDK of lock's commit, into .sdk/openrac-mod-sdk-<version>/; prints its folder."""
    version, repo, commit = lock["version"], lock["git"], lock["commit"].lower()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        print("fetch_sdk: sdk.lock's commit is not a full commit id", file=sys.stderr)
        return 1
    top = f"openrac-mod-sdk-{version}"
    dest = ROOT / ".sdk"
    stamp = dest / f"{top}.commit"
    if (dest / top / "cmake").is_dir() and stamp.is_file() and stamp.read_text().strip() == commit:
        print(dest / top)
        return 0
    dest.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dest) as tmp:
        src = Path(tmp) / "openrac"
        git = ["git", "-c", "advice.detachedHead=false"]
        for step in (
            [*git, "clone", "--quiet", "--filter=blob:none", "--no-checkout", repo, str(src)],
            [*git, "-C", str(src), "sparse-checkout", "set", "sdk"],
            [*git, "-C", str(src), "checkout", "--quiet", commit],
        ):
            if subprocess.run(step, stdout=sys.stderr).returncode != 0:
                print(f"fetch_sdk: {' '.join(step[3:])} failed", file=sys.stderr)
                return 1
        got = (src / "sdk" / "VERSION").read_text().strip()
        if got != version:
            print(f"fetch_sdk: the SDK at {commit[:9]} is {got}, sdk.lock says {version}", file=sys.stderr)
            return 1
        shutil.rmtree(dest / top, ignore_errors=True)
        (src / "sdk").rename(dest / top)
    stamp.write_text(commit + "\n")
    print(dest / top)
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--from", dest="source", type=Path, help="a local openrac-mod-sdk-<version>.zip")
    args = ap.parse_args(argv)
    lock = json.loads((ROOT / "sdk.lock").read_text())
    if "git" in lock:
        return from_git(lock)
    version, want = lock["version"], (lock.get("sha256") or "").lower()
    if not want:
        if args.source:
            print("fetch_sdk: sdk.lock has no sha256 to check a local archive against", file=sys.stderr)
            return 1
        listed = urllib.request.urlopen(lock["url"] + ".sha256", timeout=60).read().decode().split()
        if not listed or len(listed[0]) != 64:
            print(f"fetch_sdk: {lock['url']}.sha256 gives no SHA-256", file=sys.stderr)
            return 1
        want = listed[0].lower()
        lock["sha256"] = want
        (ROOT / "sdk.lock").write_text(json.dumps(lock, indent=2) + "\n")
        print(f"fetch_sdk: sdk.lock now pins {want}; commit it", file=sys.stderr)
    top = f"openrac-mod-sdk-{version}"
    dest = ROOT / ".sdk"
    stamp = dest / f"{top}.sha256"
    if (dest / top / "cmake").is_dir() and stamp.is_file() and stamp.read_text().strip() == want:
        print(dest / top)
        return 0
    data = args.source.read_bytes() if args.source else urllib.request.urlopen(lock["url"], timeout=60).read()
    got = hashlib.sha256(data).hexdigest()
    if got != want:
        print(f"fetch_sdk: the SDK's SHA-256 is {got}, sdk.lock pins {want}", file=sys.stderr)
        return 1
    dest.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dest) as tmp:
        archive = Path(tmp) / f"{top}.zip"
        archive.write_bytes(data)
        with zipfile.ZipFile(archive) as z:
            for name in z.namelist():
                if name.startswith("/") or ".." in Path(name).parts or not name.startswith(top + "/"):
                    print(f"fetch_sdk: {name} is outside {top}/", file=sys.stderr)
                    return 1
            z.extractall(tmp)
        shutil.rmtree(dest / top, ignore_errors=True)
        (Path(tmp) / top).rename(dest / top)
    stamp.write_text(want + "\n")
    print(dest / top)
    return 0


if __name__ == "__main__":
    sys.exit(main())
