import pathlib
import shutil
import subprocess
import tomllib
from importlib import metadata

from packaging import version

from .conan_tools import conan_api


def _subprocess_capture(args) -> str:
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout


def _find_pyproject() -> dict:
    """Search up the directory tree until we find the `pyproject.toml` file with `lmxcpp`"""
    cwd = pathlib.Path.cwd()
    candidate_files = (directory / "pyproject.toml" for directory in (cwd, *cwd.parents))
    candidate_contents = (tomllib.loads(f.read_text()) for f in candidate_files if f.exists())

    def is_pyproject(contents: dict) -> bool:
        return contents.get("tool", {}).get("poetry", {}).get("name") == "lmxcpp"

    return next(filter(is_pyproject, candidate_contents), {})


def self() -> list[str]:
    """Verify that `lmxcpp` itself is fine"""
    try:
        installed_version = metadata.version("lmxcpp")
    except metadata.PackageNotFoundError:
        return []  # no issues since we may be running `boilerplate` tests without an installation

    issues = []
    pyproject = _find_pyproject()
    if not pyproject:
        issues.append(
            "lmxcpp must be called inside of a LUMICKS project. "
            "It's identified by a 'pyproject.toml' file containing 'lmxcpp'."
        )

    pyproject_version = pyproject.get("tool", {}).get("poetry", {}).get("version")
    if not pyproject_version:
        return []  # no issues since we may be running `boilerplate` tests without an installation

    pyproject_version = pyproject_version[1:]  # strip leading 'v': v1.2.3 -> 1.2.3
    if pyproject_version != installed_version:
        issues.append(
            f"The 'pyproject.toml' version of your project ({pyproject_version}) "
            f"does not match the version of this 'lmxcpp' script ({installed_version}). "
            f"Run `pip install . && lmxcpp configure` to fix this."
        )

    return issues


def git() -> list[str]:
    issues = []

    git_exe = shutil.which("git")
    if not git_exe:
        issues.append("Git executable has not been found, please install git and git-lfs.")
    else:
        git_ver = _subprocess_capture([git_exe, "--version"]).split(" ")[-1]
        git_ver = ".".join(git_ver.split(".")[0:3])  # in case of something like 2.36.0.windows.1
        if version.parse(git_ver) < version.parse("2.35.2"):
            issues.append("Minimum git version required is 2.35.2, please update.")

    git_lfs_exe = shutil.which("git-lfs")
    if not git_lfs_exe:
        issues.append("Please install git-lfs from https://git-lfs.github.com/.")
    else:
        git_config = _subprocess_capture([git_exe, "config", "--global", "--list"])
        if "filter.lfs" not in git_config:
            issues.append("git-lfs is installed but not configured. Run `git lfs install`.")

    return issues


def artifactory_login() -> list[str]:
    remotes = conan_api().remotes.list()
    artifactory = next(filter(lambda r: r.name == "artifactory", remotes), None)
    if not artifactory:
        return [
            "You don't have 'artifactory' in your list of Conan remotes. "
            "Run `lmxcpp configure` to add it."
        ]
    user_info = conan_api().remotes.user_info(artifactory)
    if not user_info["user_name"]:
        return ["You are not logged into 'artifactory'. Run `conan remote login artifactory`."]
    return []


everything = [self, git, artifactory_login]
