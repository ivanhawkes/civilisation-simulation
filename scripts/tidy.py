#!/usr/bin/env python3
"""Run clang-tidy on project sources, excluding the fetched engine and dependencies."""
import concurrent.futures
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve()
commands = json.loads((build / 'compile_commands.json').read_text())
sources = sorted({entry['file'] for entry in commands
                  if Path(entry['file']).is_relative_to(root / 'source')})
if not sources:
    sys.exit('No Manifest translation units in the compile database')


def check(source):
    result = subprocess.run(['clang-tidy', '-p', str(build), '--warnings-as-errors=*', source],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode:
        return result.stdout
    return ''


with concurrent.futures.ThreadPoolExecutor(max_workers=int(os.environ.get('MANIFEST_TIDY_JOBS', 2))) as pool:
    failures = list(pool.map(check, sources))
for failure in failures:
    if failure:
        print(failure)
sys.exit(any(failures))
