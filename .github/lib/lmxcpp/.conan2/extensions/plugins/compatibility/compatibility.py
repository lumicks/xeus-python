"""See the docs: https://docs.conan.io/2/reference/extensions/binary_compatibility.html

By default, Conan 2 assumes that binaries with different `cppstd` and `cstd` values are compatible.
We've found that this fails with packages like `abseil` that provide backported C++ standard type.
For example, an `abseil` compiled with C++14 will use `abseil::string_view` which will produce a
linker error for projects using C++17 or newer which have `std::string_view`.

The looser compatibility rules are meant to improve efficiency by reusing binaries where possible.
This isn't a big concern for us since we tend to keep all project on the same C++ standard. We would
rather enforce strict binary compatibility to avoid headaches like the example above.
"""


def compatibility(conanfile):
    """Returning nothing ensures strict binary compatibility (no extra compatibility rules)"""
    return []
