import os
import pathlib


def _ensure_llvm_is_portable(root: pathlib.Path):
    """Ensure that we can build binaries that are compatible with multiple macOS versions

    Every C++ standard library has two parts:
     1. The header-only part that is always portable since it gets fully compiled into the app.
     2. The shared library part that relies on OS specifics and the app just links to it.

    LLVM libc++ has two modes it can use for the shared library:
     1. Link to the OS libc++ and use availability annotations to disable certain features.
        This is portable to several OS versions where the minimum is specified at compile time.
        All header-only features will always be available, but some shared library features will
        be disabled based on the specified OS minimum (disabled at compile time, not runtime).
     2. The other mode links to the very latest libc++ compiled with LLVM itself. This provides
        all shared library features, but the downside is that we can only run on the same OS on
        which we compiled.

    LLVM and homebrew used to default to mode 1, but at some point change to mode 2. This doesn't
    even seem to have been intentional. I believe it was a consequence of a refactor in LLVM and
    homebrew never picked up on the change. In any case, we definitely want to use mode 1 since
    we need to deploy to older versions of macOS.

    The oldest macOS that we can deploy to is controlled by the `MACOSX_DEPLOYMENT_TARGET` env var.
    If we try to use a libc++ shared library feature that's newer than that, we'll get a compiler
    error. Thankfully, there are very few shared library features that are removed by this. And we
    still get all the header-only features from the latest LLVM libc++ versions.

    We can enable mode 1 by removing a specific flag in the `__config_site` file that is meant to
    be configured per distribution (we remove the flag to enable availability annotations):
    https://github.com/llvm/llvm-project/blob/llvmorg-18.1.5/libcxx/include/__config_site.in
    Once enabled, the annotations will indicate at compile time which features are available based
    on the value in `MACOSX_DEPLOYMENT_TARGET`. For example, if we have macOS 12.7 set as the min,
    we get the `filesystem` feature which is marked `availability(macos, strict, introduced=10.15)`.
    On the other hand, we don't get `std::to_chars()` with floating-point numbers which is marked
    `availability(macos, strict, introduced=13.3)`. The full list is available here:
    https://github.com/llvm/llvm-project/blob/llvmorg-18.1.5/libcxx/include/__availability

    The shared library feature segmentation so far:
     * macOS 10.15: `std::filesystem`
     * macOS 11.0: <barrier>, <latch>, <semaphore>, and notification functions on `std::atomic`
     * macOS 13.3: `std::to_chars()` with floating-point numbers
     * macOS 14.0: `std::pmr`
     * macOS next: C++20 time zone database, C++23 <print>
    """
    config_site = root / "include/c++/v1/__config_site"
    # The file doesn't exist in old versions, but it's fine since they have the opposite default.
    if not config_site.exists():
        return

    target = "#define _LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS"
    replacement = "// lmxcpp configure removed _LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS"
    contents = config_site.read_text()
    if target in contents:
        config_site.write_text(contents.replace(target, replacement))


def _homebrew_llvm_paths() -> list[pathlib.Path]:
    """List all homebrew LLVMs found on this system"""
    prefix = os.environ.get("HOMEBREW_PREFIX", "/opt/homebrew")
    return sorted(pathlib.Path(prefix, "opt").glob("llvm@*"))


def ensure_homebrew_llvm_is_portable():
    """Ensure that all homebrew LLVMs are portable"""
    for path in _homebrew_llvm_paths():
        _ensure_llvm_is_portable(path)


def ensure_latest_homebrew_llvm_is_env_default():
    """We want to create Conan's default profile with LLVM clang instead of Apple clang

    Escape hatch: This function doesn't do anything if the user has already set
    CC and CXX environment variables manually.
    """
    paths = _homebrew_llvm_paths()
    if not paths:
        return

    latest = paths[-1]
    os.environ.setdefault("CC", str(latest / "bin/clang"))
    os.environ.setdefault("CXX", str(latest / "bin/clang++"))
