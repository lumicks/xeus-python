# Make sure that CMake doesn't complain about an unused variable
# since we never use the C compiler (only C++).
set(_ignore ${CMAKE_C_COMPILER})

# Produce an informative error early instead of an obscure one later
get_property(multiconfig_generator GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multiconfig_generator)
    message(FATAL_ERROR "We don't support multi-config CMake generators (Visual Studio, Xcode). "
                        "Please make sure that you configure CMake with a single-config generator "
                        "(Ninja, Make, NMake, etc.)")
endif()

# We don't work with old compilers
if(MSVC AND MSVC_VERSION VERSION_LESS 1940)
    message(FATAL_ERROR "Visual Studio >= v17.10 is required")
endif()
if(${CMAKE_CXX_COMPILER_ID} STREQUAL Clang AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18.0)
    message(FATAL_ERROR "Clang >= v18.0 is required")
endif()
if(${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
    message(WARNING "GCC is untested/unsupported")
endif()

# Prevent accidental compilation in 32-bit mode (default on MSVC)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only 64-bit builds are supported")
endif()

option(LMX_USE_CCACHE "Use `ccache` if it's available" ON)
option(LMX_CONAN_INSTALL "Run `conan install` as part of CMake configuration" ON)
set(LMX_CONAN_EXTRA_ARGS "" CACHE STRING "Add or override Conan command line arguments")

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_REQUIRED TRUE)

# Disable compiler-specific extensions: use `-std=c++23` instead of `std=gnu++23`.
# CMake recommends turning this off when using C++ modules.
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable C++20 modules scanning since the feature isn't usable (yet) and the scanning slows down
# builds. See: https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Even when building static libraries, `-fPIC` is a good default because the generated
# code may end up in a shared library. This variable only matters for Linux. Windows and
# macOS do this by default anyway.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Only affects targets that use Qt moc. New flag in CMake 3.27. Strangely, it seems to do the
# opposite of what it says. Unless we turn it OFF (ON by default), it will add Qt headers as
# user includes in moc targets. That results in Qt warnings (and -Werror) blocking our build.
set(CMAKE_AUTOGEN_USE_SYSTEM_INCLUDE OFF)

# By default, CMake sets rpaths on build (development) binaries but strips them out from
# install (distribution) binaries. This is a good default. However, we'll use `macdeployqt`
# on the install binaries and it needs the LC_RPATH information, so we want CMake to leave it.
# `macdeployqt` will gather `.framework` and `.dylib` files from the LC_RPATH locations and
# finally strip out those paths in favor of `@executable_path/../Frameworks`.
if(APPLE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()

# The build type should default to Debug for development, however debug mode is too slow to be
# usable on MSVC so we fall back to the next best thing: release mode with debug symbols.
if(NOT CMAKE_BUILD_TYPE)
    if(MSVC)
        set(CMAKE_BUILD_TYPE RelWithDebInfo)
    else()
        set(CMAKE_BUILD_TYPE Debug)
    endif()
endif()

# Make relwithdebinfo a little more like debug by removing NDEBUG and adding QT_DEBUG
string(REPLACE "/DNDEBUG" "/DQT_DEBUG" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
string(REPLACE "-DNDEBUG" "-DQT_DEBUG" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})

if(MSVC)
    # Append "_d" to DLL file names in debug mode
    set(CMAKE_DEBUG_POSTFIX _d)
    # The default debug format is `ProgramDatabase` (-Zi) that creates separate .pdb files. However,
    # this is not compatible with `ccache` since it can't track multiple outputs. `Embedded` (-Z7)
    # stores the debug info together with the generated object file (like gcc/clang do by default).
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()

# Use ccache if available
find_program(ccache_exe ccache)
if(ccache_exe AND LMX_USE_CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${ccache_exe})
endif()

# Conan dependencies
include("${CMAKE_CURRENT_LIST_DIR}/cmake/conan.cmake")

# In Conan v1, `CONAN_EXPORTED` was automatically injected by Conan to indicate that it invoked
# CMake (as opposed to CMake invoking Conan). With Conan v2, this is no longer the case, but we
# started adding this variable manually since we still need to distinguish the two cases. More
# recent Conan v2 release have added `CONAN_RUNTIME_LIB_DIRS` that we can use for the same purpose.
# This way, we don't need to manually add `CONAN_EXPORTED` anymore.
if(NOT DEFINED CONAN_EXPORTED AND DEFINED CONAN_RUNTIME_LIB_DIRS)
    set(CONAN_EXPORTED ON)
endif()

# Make sure CMake is reconfigured if `conanfile.py` is modified
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS conanfile.py)

# Run `conan install` with the correct settings for the current CMake config
function(lmx_run_conan_install_if_needed)
    # When CMake is being run as part of Conan package creation (`conan create` or `conan export`),
    # Conan has already installed everything for us so we should not do it all again ourselves.
    # We detect this case via the presence of the `CONAN_EXPORTED` variable.
    #
    # Users can also choose to disable this function with `LMX_CONAN_INSTALL=OFF` and run the Conan
    # install step manually. This is never recommended but it's provided as an escape hatch.
    #
    # Lastly, when using a workspace, it will take care of package installation, so neither the
    # workspace itself or its sub-projects need to run `conan install` individually.
    if(CONAN_EXPORTED OR NOT LMX_CONAN_INSTALL OR LMX_WORKSPACE)
        return()
    endif()

    # Updating `conanfile.py` requires us to (re)run `conan install`
    set(conanfile_py "${CMAKE_CURRENT_SOURCE_DIR}/conanfile.py")
    set(landmark "${PROJECT_BINARY_DIR}/generators/cmakedeps_macros.cmake")
    set(legacy_landmark "${PROJECT_BINARY_DIR}/cmakedeps_macros.cmake")
    if(EXISTS "${landmark}" AND "${conanfile_py}" IS_NEWER_THAN "${landmark}")
        set(LMX_CONAN_NEEDS_RERUN TRUE)
    elseif(EXISTS "${legacy_landmark}" AND "${conanfile_py}" IS_NEWER_THAN "${legacy_landmark}")
        set(LMX_CONAN_NEEDS_RERUN TRUE)
    endif()

    # Settings changes (e.g. C++ standard version) should also trigger a rerun
    lmx_detect_conan_args(conan_args)
    list(APPEND conan_args ${LMX_CONAN_EXTRA_ARGS})
    if(NOT "${conan_args}" STREQUAL "${LMX_CONAN_ARGS}")
        set(LMX_CONAN_NEEDS_RERUN TRUE)
    endif()

    # Users can also manually specify `LMX_CONAN_NEEDS_RERUN=TRUE` to always run `conan install`
    if(NOT LMX_CONAN_NEEDS_RERUN)
        return()
    endif()

    # Identify that we're invoking Conan from CMake with this recipe as the root project. As
    # opposed to a leaf in the dependency tree. This tells our recipes to copy DLLs into `bin`
    # as the root but not as a leaf. It's not a mistake to always copy DLLs, but it is wasteful
    # (both in time and storage) since leaf builds make use of Conan's `VirtualRunEnv` to modify
    # the `PATH` instead of copying DLLs.
    set(ENV{LMX_ROOT_PROJECT} ${CMAKE_PROJECT_NAME})

    execute_process(
        COMMAND python -m lmxcpp.conan_install ${conanfile_py} ${conan_args} --build missing --output-folder ${PROJECT_BINARY_DIR}
        COMMAND_ERROR_IS_FATAL ANY
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
    set(LMX_CONAN_ARGS ${conan_args} CACHE INTERNAL "")
endfunction()

lmx_run_conan_install_if_needed()

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/bin ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

# Allow CMake `find_package()` to find Conan targets generated by `CMakeDeps`. It will generate
# `*-config.cmake` files for all dependencies in `PROJECT_BINARY_DIR` so we need to add it to
# the path for discoverability and tell CMake to prefer `-config.cmake` files as opposed to
# `Find*.cmake` files. More details can be found in the Conan docs, specifically:
# https://docs.conan.io/en/latest/reference/conanfile/tools/cmake/cmaketoolchain.html
list(PREPEND CMAKE_PREFIX_PATH ${PROJECT_BINARY_DIR} ${PROJECT_BINARY_DIR}/generators)
list(PREPEND CMAKE_MODULE_PATH ${PROJECT_BINARY_DIR} ${PROJECT_BINARY_DIR}/generators)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
# Auto-find those targets if using `LmxBoilerplate` in addition to `CMakeDeps`
set(generator "${PROJECT_BINARY_DIR}/generators/boilerplate-generated.cmake")
set(legacy_generator "${PROJECT_BINARY_DIR}/boilerplate-generated.cmake")
if(EXISTS "${generator}")
    include("${generator}")
elseif(EXISTS "${legacy_generator}")
    include("${legacy_generator}")
endif()

# Don't run any `clang-tidy` checks on autogenerated `.cpp` files in the build directory.
# Running `clang-tidy` without any checks (`-*`) is an error, so instead, we enable only
# `cppcoreguidelines-avoid-goto` which is effectively a do-nothing `.clang-tidy` file.
if(NOT EXISTS "${PROJECT_BINARY_DIR}/.clang-tidy")
    file(WRITE "${PROJECT_BINARY_DIR}/.clang-tidy" "Checks: '-*,cppcoreguidelines-avoid-goto'")
endif()

# Everything needed to export libraries using our packaging scheme
include("${CMAKE_CURRENT_LIST_DIR}/cmake/packaging.cmake")

# Default compiler flags and warnings
include("${CMAKE_CURRENT_LIST_DIR}/cmake/flags.cmake")

# Enable CTest support without extra targets we don't need: https://stackoverflow.com/q/44949354
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)
