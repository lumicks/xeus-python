import os

from conan.tools import files


class LmxBoilerplate:
    """A Conan generator that handles the boilerplate of having to `find_package()` everything

    For example, if you have this in your `conanfile.py`:
    ```
    requires = (
        "foo/1.0.0",
        "bar/1.4.2",
        "baz/2.1.0",
    )
    generators = "CMakeDeps"
    ```
    You usually need to match it in `CMakeLists.txt` with:
    ```
    find_package(foo REQUIRED)
    find_package(bar REQUIRED)
    find_package(baz REQUIRED)
    ```
    This is needless boilerplate which we can get rid of it simply by adding `LmxBoilerplate` to
    the list of generators in `conanfile.py`:
    ```
    generators = "CMakeDeps", "LmxBoilerplate"
    ```
    `LmxBoilerplate` will then generate the boilerplate for us from `conanfile.py`.
    """

    def __init__(self, conanfile):
        self._conanfile = conanfile

    def generate(self):
        if os.environ.get("LMX_ROOT_PROJECT") == self._conanfile.name:
            deps = self._conanfile.dependencies  # all dependencies, including private build/test
        else:
            deps = self._conanfile.dependencies.host  # only transitive dependencies

        result = f"set(CONAN_PACKAGE_VERSION {self._conanfile.version})\n"
        for require, dep in deps.items():
            # The name that needs to be given to `find_package()` may not be the same as the Conan
            # package name, e.g. `abseil` vs `absl` (it's weird, but that's the way it is).
            cmake_name = dep.cpp_info.get_property("cmake_file_name") or dep.ref.name
            if not require.direct or not cmake_name:
                continue  # only find direct deps because transitive ones are handled naturally
            result += f"find_package({cmake_name} REQUIRED QUIET) # {dep.ref}\n"

        files.save(self._conanfile, "boilerplate-generated.cmake", result)
