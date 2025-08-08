import os
import pathlib
import shutil
import subprocess
import sys

from . import clang_tidy_cache, decorators
from .configure import update_conan_conf


def _print_disk_usage(paths):
    """Print disk usage in a cross-platform consistent format"""

    def to_gibi(size_bytes):
        """AWS uses 1024 units for storage so this is the best way to report on all platforms"""
        return f"{size_bytes // 1024**3:.0f}Gi"

    def to_gibi_and_percent(size, total):
        return to_gibi(size) + f" ({size / total:.0%})"

    fmt = "{path: <10} {size: >6} {used: >12} {free: >12}"
    print(fmt.format(path="Path", size="Size", used="Used", free="Free"))
    for path in paths:
        usage = shutil.disk_usage(path)
        line = fmt.format(
            path=str(path),
            size=to_gibi(usage.total),
            used=to_gibi_and_percent(usage.used, usage.total),
            free=to_gibi_and_percent(usage.free, usage.total),
        )
        print(line)


def _cache_dir() -> pathlib.Path:
    """The OS-specific cache directory"""
    match sys.platform:
        case "win32":
            return pathlib.Path("C:\\cache")
        case "linux":
            return pathlib.Path("/cache")
        case _:
            return pathlib.Path("~/.cache").expanduser()


def _configure_conan_cache_path():
    """Conan v2 doesn't support cache concurrency yet, so we give each runner a different dir"""
    if sys.platform == "darwin":
        return

    # e.g. "linux-cpp2-i-0b3bc6e358f7261e7" -> "linux-cpp2"
    runner_name = os.environ["RUNNER_NAME"].split("-i")[0]
    update_conan_conf({"core.cache:storage_path": f"{_cache_dir()}/conan2/{runner_name}"})


def pre():
    """Actions that run before every CI run"""
    _configure_conan_cache_path()
    if sys.platform == "linux":
        clang_tidy_cache.prune()  # the only platform where we run clang-tidy
        _print_disk_usage([".", _cache_dir()])
    elif sys.platform == "win32":
        _print_disk_usage([".", _cache_dir()])
    elif sys.platform == "darwin":
        _print_disk_usage(["."])


def post():
    """Actions that run after every CI run

    The critical step here is releasing any cache locks. If a workflow run is canceled by the user,
    processes are killed so they can't gracefully release locks. This post step is called even for
    canceled runs so we can clean up here.
    """
    if sys.platform == "linux":
        clang_tidy_cache.unlock()
        _print_disk_usage([".", _cache_dir()])
    elif sys.platform == "win32":
        _print_disk_usage([".", _cache_dir()])
    elif sys.platform == "darwin":
        # We currently don't use locks on this platform since it only has 1 runner per EC2 instance.
        _print_disk_usage(["."])


@decorators.timed
def _prune(dry_run=False, keep_weeks=2, custom_cache_dir: pathlib.Path | None = None):
    """Remove packages last used more than `keep_weeks` ago, and all source and build folders

    Source and build folders are always safe to remove. Sources are quick to re-download if a
    different variant of the same recipe is needed. Build folders are temporary by nature. Only
    the package folders need to be kept in the cache, but only the most recently used ones.
    """
    flags = []
    if dry_run:
        flags += ["--dry-run"]
    if custom_cache_dir:
        flags += [f"--core-conf=core.cache:storage_path={custom_cache_dir}"]
    subprocess.run(["conan", "cache", "clean", "*", *flags], check=True)
    # Remove package binaries that have not been used recently. Note that recipe and binary
    # usage is tracked separately. We can only remove a recipe if both the recipe and all of
    # its binaries are out of the LRU range. This is why we specifically target binaries here.
    subprocess.run(["conan", "remove", "*:*", "-c", f"--lru={keep_weeks}w", *flags], check=True)
    # Once all unused binaries have been removed, we can remove unused recipes.
    subprocess.run(["conan", "remove", "*", "-c", f"--lru={keep_weeks}w", *flags], check=True)


def prune_conan_cache():
    """On CI, each runner has its own Conan 2 cache"""
    cache_dirs = (p for p in (_cache_dir() / "conan2").iterdir() if p.is_dir())
    for d in cache_dirs:
        _prune(custom_cache_dir=d)
