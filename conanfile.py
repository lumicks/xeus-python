import pathlib

from conan import ConanFile
from conan.tools import files
from conan.tools.cmake import CMake, CMakeToolchain


def embedded_python_env():
    import pathlib

    """Read the requirements for the embedded Python environment"""
    project_root = pathlib.Path(__file__).parent
    with open(project_root / "env.txt") as f:
        return f.read().replace("\n", "\t")


# noinspection PyUnresolvedReferences
class XeusPythonConan(ConanFile):
    name = "xeus-python"
    version = "0.17.4-lmx.1"
    license = "BSD-3-Clause"
    description = "Jupyter kernel for the Python programming language"
    topics = "python", "jupyter", "jupyter-kernels"
    homepage = "https://github.com/jupyter-xeus/xeus-python"
    url = "https://github.com/conan-io/conan-center-index"
    settings = "os", "arch", "compiler", "build_type"
    exports = ["env.txt"]
    exports_sources = [
        "CMakeLists.txt",
        "include/*",
        "src/*",
        "share/*",
        "test/*",
        "*.cmake.in",
    ]
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {
        "shared": False,
        "fPIC": True,
        "embedded_python/*:packages": embedded_python_env(),
    }
    generators = "CMakeDeps"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        # We need to keep the `user/channel` internally consistent for cookbook dependencies.
        self.requires(f"xeus-zmq/3.1.0-lmx.1@lumicks/cookbook-testing")
        self.requires(f"pybind11/2.11.1@lumicks/stable", force=True)
        self.requires("nlohmann_json/3.11.3", force=True, transitive_headers=True)
        self.requires("pybind11_json/0.2.13")
        self.requires("doctest/2.4.11")
        self.requires("embedded_python/1.10.0@lumicks/stable")

    def generate(self):
        tc = CMakeToolchain(self)
        if self.settings.os == "Windows":
            tc.preprocessor_definitions["NOMINMAX"] = 1
        tc.variables["XPYT_BUILD_SHARED"] = self.options.shared
        tc.variables["XPYT_BUILD_XPYTHON_EXECUTABLE"] = True
        tc.variables["XPYT_USE_SHARED_XEUS"] = self.dependencies["xeus"].options.shared
        tc.variables["XPYT_USE_SHARED_XEUS_PYTHON"] = self.options.shared
        tc.variables["PYBIND11_FINDPYTHON"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        pck_folder = pathlib.Path(self.package_folder)
        files.copy(self, "LICENSE*", self.build_folder, pck_folder / "licenses")
        files.copy(
            self,
            "*",
            pathlib.Path(self.build_folder) / "include",
            pck_folder / "include",
        )
        files.copy(self, "lib*", self.build_folder, pck_folder / "lib")
        files.copy(self, "*.dll", self.build_folder, pck_folder / "lib")
        files.rm(self, "*.cmake", pck_folder / "lib")

    def package_info(self):
        self.cpp_info.libs = files.collect_libs(self)
        if self.settings.os == "Windows" and not self.options.shared:
            self.cpp_info.defines.append("XEUS_PYTHON_STATIC_LIB")
