import os
import pathlib
import subprocess
import sys

from . import checks, doctor, macos
from .conan_tools import conan_api, conan_home


def _create_conan_v2_default_profile():
    """Create default profile for Conan v2 with some updates"""
    profile = conan_api().profiles.detect()
    profile.settings["compiler.cppstd"] = "23"
    content = profile.dumps()
    path = conan_home() / "profiles/default"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    print(content)


def _get_compiler_executables():
    """Generate `tools.build:compiler_executables={"c":"path/c","cpp":"path/cxx"}`"""
    mapping = {"c": "CC", "cpp": "CXX"}
    compilers = [f'"{k}":"{os.environ[v]}"' for k, v in mapping.items() if v in os.environ]
    return "{" + ",".join(compilers) + "}"


def update_conan_conf(new_values: dict[str, str]):
    """Add or replace `key=value` pairs in a Conan .conf file"""
    path = conan_home() / "global.conf"
    old_conf_text = path.read_text() if path.exists() else ""
    conf = dict(
        line.strip().split("=", 1)
        for line in old_conf_text.splitlines()
        if "=" in line and not line.startswith("#")
    )
    conf.update(new_values)
    new_conf_text = "\n".join(f"{k}={v}" for k, v in conf.items())
    path.write_text(new_conf_text + "\n")


def configure(skip_checks):
    """Configure the development tools in your environment and run `doctor` after"""
    if not skip_checks:
        doctor.run_checks(checks.self, checks.git)

    if sys.platform == "darwin":
        macos.ensure_homebrew_llvm_is_portable()
        macos.ensure_latest_homebrew_llvm_is_env_default()

    _create_conan_v2_default_profile()

    new_conf_values = {
        "tools.cmake.cmaketoolchain:generator": "Ninja",
        "tools.build:compiler_executables": _get_compiler_executables(),
        "core.package_id:default_build_mode": "None",
        "core.package_id:default_embed_mode": "full_mode",
        "core.package_id:default_non_embed_mode": "full_mode",
        "core.package_id:default_unknown_mode": "full_mode",
        "core.package_id:default_python_mode": "minor_mode",
    }
    update_conan_conf(new_conf_values)

    src = pathlib.Path(__file__).parent / ".conan2"
    conan_api().config.install(str(src), verify_ssl=False)

    if not skip_checks:
        subprocess.run(["pre-commit", "install"], check=True)
        doctor.run_checks(checks.artifactory_login)
