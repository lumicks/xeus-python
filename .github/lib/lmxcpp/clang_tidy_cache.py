import json
import os
import pathlib
import subprocess

from . import decorators

_cache_dir = pathlib.Path(os.environ.get("CLANG_TIDY_CACHE_DIR", pathlib.Path.home() / ".ctcache"))


@decorators.timed
def info():
    """Print the total number of cache entries"""
    with open(_cache_dir / "entries.json") as f:
        entries = json.load(f)
    print(f"Found {len(entries)} cache entries in {_cache_dir}")


@decorators.timed
@decorators.locked(_cache_dir / "lmx.lock")
def prune(keep_weeks=2):
    """Remove all cache entries that were last used more than `keep_weeks` ago"""
    subprocess.run(["clang-tidy-cache", "prune", str(keep_weeks)], check=True)


def unlock():
    """Remove the lock setup by `prune`"""
    (_cache_dir / "lmx.lock").unlink(missing_ok=True)
