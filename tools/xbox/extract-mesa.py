#!/usr/bin/env python3
"""Extract verified Mesa release sources on Windows with periodic CI progress.

The previous Windows tar.exe command went silent for 16+ minutes in GitHub
Actions. This uses Python's standard tarfile/LZMA reader instead and skips
upstream symbolic links that cannot reliably be created on Windows runners.
Only regular files and directories under the requested Mesa prefix are written.
"""
from __future__ import annotations

import os
import shutil
import sys
import tarfile
import time
from pathlib import Path, PurePosixPath


def extract(archive: Path, output: Path, version: str) -> None:
    prefix = f"mesa-{version}"
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    count = 0
    regular = 0
    skipped_links = 0
    start = time.monotonic()
    print(f"Extracting {archive} (size={archive.stat().st_size:,} bytes)", flush=True)

    # r|xz streams one member at a time without a giant .tar intermediate file.
    with tarfile.open(archive, mode="r|xz") as tar:
        for member in tar:
            name = PurePosixPath(member.name)
            parts = name.parts
            if not parts or parts[0] != prefix or name.is_absolute() or ".." in parts:
                raise ValueError(f"Unexpected Mesa archive path: {member.name!r}")

            # Symlinks in the Mesa release point mostly to CI files; do not let
            # them traverse or write outside the requested release directory.
            if member.issym() or member.islnk():
                skipped_links += 1
                continue
            if not (member.isdir() or member.isfile()):
                continue

            target = output.joinpath(*parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                fileobj = tar.extractfile(member)
                if fileobj is None:
                    raise OSError(f"Cannot read member: {member.name}")
                with fileobj, target.open("wb") as dest:
                    shutil.copyfileobj(fileobj, dest, length=1024 * 1024)
                regular += 1

            count += 1
            if count % 100 == 0:
                print(
                    f"Mesa extraction: {count} entries, {regular} files, "
                    f"{time.monotonic() - start:.1f}s elapsed",
                    flush=True,
                )

    required = output / prefix / "meson.build"
    if not required.is_file():
        raise FileNotFoundError(f"Missing Mesa release meson.build: {required}")
    print(
        f"Mesa extraction finished: {count} entries, {regular} files, "
        f"{skipped_links} links skipped, {time.monotonic() - start:.1f}s",
        flush=True,
    )


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("Usage: extract-mesa.py <mesa.tar.xz> <outdir> <version>")
    try:
        extract(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3])
    except Exception as exc:
        print(f"Mesa extraction failed: {type(exc).__name__}: {exc}", file=sys.stderr, flush=True)
        raise
