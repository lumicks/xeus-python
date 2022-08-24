import os
import textwrap
from conans import ConanFile, CMake, tools

required_conan_version = ">=1.33.0"


def embedded_python_env():
    import pathlib
    """Read the requirements for the embedded Python environment"""
    project_root = pathlib.Path(__file__).parent
    with open(project_root / "env.txt") as f:
        return f.read().replace("\n", "\t")


# noinspection PyUnresolvedReferences
class XeusPythonConan(ConanFile):
    name = "xeus-python"
    version = "0.15.0-lmx.0"
    license = "BSD-3-Clause"
    description = "Jupyter kernel for the Python programming language"
    topics = "python", "jupyter", "jupyter-kernels"
    homepage = "https://github.com/jupyter-xeus/xeus-python"
    url = "https://github.com/conan-io/conan-center-index"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {
        "shared": False,
        "fPIC": True,
        "embedded_python:packages": embedded_python_env(),
    }
    generators = "cmake_find_package"
    exports = ["env.txt"]
    exports_sources = ["CMakeLists.txt", "include/*", "src/*", "share/*", "*.cmake.in"]

    _cmake = None

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        self.requires(f"xeus/2.3.1@lumicks/stable")
        self.requires(f"pybind11/2.9.1@lumicks/stable")
        self.requires("pybind11_json/0.2.11")
        self.requires("nlohmann_json/3.10.5")

    def _configure_cmake(self):
        if self._cmake:
            return self._cmake
        # They forgot that `dllexport/import` should not be included when building a static library
        if self.settings.os == "Windows" and not self.options.shared:
            header = "include/xeus-python/xeus_python_config.hpp"
            tools.replace_in_file(header, "__declspec(dllexport)", "")
            tools.replace_in_file(header, "__declspec(dllimport)", "")

        self._cmake = CMake(self)
        d = self._cmake.definitions
        d["XPYT_BUILD_SHARED"] = self.options.shared
        d["XPYT_BUILD_XPYTHON_EXECUTABLE"] = False
        d["XPYT_USE_SHARED_XEUS"] = self.options["xeus"].shared
        d["XPYT_USE_SHARED_XEUS_PYTHON"] = self.options.shared
        d["XPYT_DISABLE_ARCH_NATIVE"] = True
        d["XPYT_DISABLE_TUNE_GENERIC"] = True
        d["PYBIND11_FINDPYTHON"] = True

        self._cmake.configure(build_folder="build")
        return self._cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        self.copy("LICENSE*", dst="licenses")
        self.copy("*", dst="include", src="include")
        self.copy("lib*", dst="lib", src="build")
        self.copy("*.dll", dst="lib", src="build")
        self._create_cmake_module_alias_targets(
            os.path.join(self.package_folder, self._module_file_rel_path),
            {
                "xeus-python": "xeus-python::xeus-python",
                "xeus-static-python": "xeus-python::xeus-python",
            },
        )

    @staticmethod
    def _create_cmake_module_alias_targets(module_file, targets):
        content = ""
        for alias, aliased in targets.items():
            content += textwrap.dedent(
                f"""\
                if(TARGET {aliased} AND NOT TARGET {alias})
                    add_library({alias} INTERFACE IMPORTED)
                    set_property(TARGET {alias} PROPERTY INTERFACE_LINK_LIBRARIES {aliased})
                endif()
            """
            )
        tools.save(module_file, content)

    @property
    def _module_subfolder(self):
        return os.path.join("lib", "cmake")

    @property
    def _module_file_rel_path(self):
        return os.path.join(self._module_subfolder, f"conan-official-{self.name}-targets.cmake")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
        self.cpp_info.builddirs.append(self._module_subfolder)
        self.cpp_info.build_modules["cmake_find_package"] = [self._module_file_rel_path]
        self.cpp_info.build_modules["cmake_find_package_multi"] = [self._module_file_rel_path]
        if self.settings.os == "Windows" and self.options.shared:
            self.cpp_info.defines.append("XEUS_PYTHON_EXPORTS")
