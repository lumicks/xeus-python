import os
import sys

from conan.api.conan_api import ConanAPI
from conan.cli.cli import Cli

from . import checks, doctor, macos

_running_on_ci = "CI" in os.environ or "RUNNER_NAME" in os.environ


def _pre_install_ensure():
    # On macOS, it's possible to update LLVM and forget to re-run `lmxcpp`. This can result in
    # compiler errors related to `libc++` portability features. It's quick to ensure it here.
    if sys.platform == "darwin":
        macos.ensure_homebrew_llvm_is_portable()


def _pre_install_checks():
    doctor.run_checks(checks.artifactory_login)


def main():
    # CI containers already have the correct settings baked in. We don't want to slow down
    # their CMake configuration step by making them re-check anything.
    if not _running_on_ci:
        _pre_install_ensure()
        _pre_install_checks()

    api = ConanAPI()
    Cli(api).run(["install", *sys.argv[1:]])


if __name__ == "__main__":
    main()
