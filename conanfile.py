import pathlib
import textwrap
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.files import rm, get, copy, save, collect_libs, replace_in_file

required_conan_version = ">=1.56.0"

def embedded_python_env():
    import pathlib
    """Read the requirements for the embedded Python environment"""
    project_root = pathlib.Path(__file__).parent
    with open(project_root / "env.txt") as f:
        return f.read().replace("\n", "\t")


# noinspection PyUnresolvedReferences
class XeusPythonConan(ConanFile):
    name = "xeus-python"
    version = "0.15.9-lmx.0"
    license = "BSD-3-Clause"
    description = "Jupyter kernel for the Python programming language"
    topics = "python", "jupyter", "jupyter-kernels"
    homepage = "https://github.com/jupyter-xeus/xeus-python"
    url = "https://github.com/conan-io/conan-center-index"
    settings = "os", "arch", "compiler", "build_type"
    exports = ["env.txt"]
    exports_sources = ["CMakeLists.txt", "include/*", "src/*", "share/*", "test/*", "*.cmake.in"]
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {
        "shared": False,
        "fPIC": True,
        "embedded_python:packages": embedded_python_env(),
    }
    generators = "CMakeDeps"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        # We need to keep the `user/channel` internally consistent for cookbook dependencies.
        self.requires(f"xeus-zmq/1.1.1-lmx.0@lumicks/cookbook-testing")
        self.requires(f"pybind11/2.10.4@lumicks/cookbook-testing")
        self.requires("cppzmq/4.7.1")
        self.requires("pybind11_json/0.2.11")
        self.requires("nlohmann_json/3.10.5")
        self.requires("xtl/0.7.4")
        self.requires("doctest/2.4.11")

    def generate(self):
        # Conan's `pybind11_json` target name is a bit different from the original
        replace_in_file(
            self,
            "CMakeLists.txt",
            "PRIVATE pybind11::pybind11 pybind11_json",
            "PRIVATE pybind11::pybind11 pybind11_json::pybind11_json",
        )
        # We don't build the `xpython` exe so we don't need this find_package() that finds the
        # Python interpreter exe and version. We remove it because it can fail since it always
        # looks for a system Python installation instead of our embedded one.
        replace_in_file(
            self,
            "CMakeLists.txt",
            "find_package(PythonInterp ${PythonLibsNew_FIND_VERSION} REQUIRED)",
            "",
        )
        # They forgot that `dllexport/import` should not be included when building a static library
        if self.settings.os == "Windows" and not self.options.shared:
            header = "include/xeus-python/xeus_python_config.hpp"
            replace_in_file(self, header, "__declspec(dllexport)", "")
            replace_in_file(self, header, "__declspec(dllimport)", "")
        tc = CMakeToolchain(self)
        if self.settings.os == "Windows":
            tc.preprocessor_definitions["NOMINMAX"] = 1
        tc.variables["XPYT_BUILD_SHARED"] = self.options.shared
        tc.variables["XPYT_BUILD_XPYTHON_EXECUTABLE"] = True
        tc.variables["XPYT_USE_SHARED_XEUS"] = self.options["xeus"].shared
        tc.variables["XPYT_USE_SHARED_XEUS_PYTHON"] = self.options.shared
        tc.variables["XPYT_DISABLE_ARCH_NATIVE"] = True
        tc.variables["XPYT_DISABLE_TUNE_GENERIC"] = True
        tc.variables["PYBIND11_FINDPYTHON"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        pck_folder = pathlib.Path(self.package_folder)
        copy(self, "LICENSE*", self.build_folder, pck_folder / "licenses")
        copy(self, "*", pathlib.Path(self.build_folder) / "include", pck_folder / "include")
        copy(self, "lib*", self.build_folder, pck_folder / "lib")
        copy(self, "*.dll", self.build_folder, pck_folder / "lib")
        rm(self, "*.cmake", pck_folder / "lib")
        self._create_cmake_module_alias_targets(
            self,
            pck_folder / self._module_file_rel_path,
            {
                "xeus-python": "xeus-python::xeus-python",
                "xeus-python-static": "xeus-python::xeus-python",
            },
        )

    @property
    def _module_subfolder(self):
        return pathlib.Path("lib", "cmake")

    @property
    def _module_file_rel_path(self):
        return pathlib.Path(self._module_subfolder, f"conan-official-{self.name}-targets.cmake")

    @staticmethod
    def _create_cmake_module_alias_targets(conanfile, module_file, targets):
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
        save(conanfile, module_file, content)

    def package_info(self):
        self.cpp_info.libs = collect_libs(self)
        self.cpp_info.builddirs.append(self._module_subfolder)
        if self.settings.os == "Windows" and self.options.shared:
            self.cpp_info.set_property("defines", "XEUS_PYTHON_EXPORTS")
