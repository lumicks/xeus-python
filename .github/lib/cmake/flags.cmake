# Always apply our default compiler flags but don't turn warnings into errors if we're building
# a Conan package. For regular development, we do want warnings as errors, however Conan packages
# live for a long time and may end up being compiled with an unknown future compiler that includes
# more default warnings that we originally accounted for. We don't want to block the package build
# in such cases.
option(LMX_BOILERPLATE_FLAGS "This is an escape hatch: turn this off if the flags cause issues" ON)
if(CONAN_EXPORTED)
    option(LMX_WERROR "Make all warnings into errors" OFF)
else()
    option(LMX_WERROR "Make all warnings into errors" ON)
endif()
option(LMX_ASAN "Enable address sanitizer: we usually only enable this for CI builds" OFF)

function(lmx_boilerplate_set_default_flags)
    if(MSVC)
        add_compile_options(/W4 $<$<BOOL:${LMX_WERROR}>:/WX>)

        # Disable Windows legacy
        add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

        # Disable specific linker warnings:
        # - 4075: "ignoring /INCREMENTAL due to /LTCG specification"
        # - 4099: "PDB file not found, linking object as if no debug info"
        add_link_options(-ignore:4075 -ignore:4099)

        # Ignore warnings from third-party headers (matches GCC/clang defaults). Note that CMake
        # already adds `-external:W0` by default. We just say that all <foo> includes are external.
        add_compile_options(-external:anglebrackets)
        add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

        # Disable specific compiler warnings:
        # - C4251 and C4275: "non-dllexported members of classes" and "non-dllexported base class
        #   inheritance". We compile everything with the same compiler and /MD so we don't need to
        #   worry about these: https://stackoverflow.com/q/3793309, https://stackoverflow.com/q/5661738.
        # - C4702: "unreachable code". It's buggy (especially with external headers, e.g. range-v3),
        #   and it's more comprehensively covered by clang-tidy.
        add_compile_options(/wd4251 /wd4275 /wd4702)

        # This category of MSVC warnings frequently suggests non-portable solutions
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)

        # Templated code often exceeds the object file section limit. Since we don't use a
        # pre-msvc 2015 linker we can always use -bigobj.
        add_compile_options(-bigobj)
    else()
        add_compile_options(
            $<$<BOOL:${LMX_WERROR}>:-Werror>
            -Wall -Wextra -Wconversion -Wsign-compare
            -Wno-sign-conversion -Wno-parentheses -Wno-missing-braces
        )
    endif()

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
        # Speed up linking by using the lld linker
        add_link_options("-fuse-ld=lld")
        # enable code coverage for only for clang
        if(LMX_CODE_COVERAGE_ENABLED)
            add_compile_options(-fcoverage-mapping -fprofile-instr-generate)
            add_link_options(-fprofile-instr-generate)
        endif()
    endif()

    # We don't want Qt macro keywords: signals, slots, emit
    add_compile_definitions(QT_NO_KEYWORDS)

    if(LMX_ASAN)
        message(STATUS "LMX: Address sanitizer enabled")
        add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
        add_link_options(-fno-omit-frame-pointer -fsanitize=address)
    endif()

    # Without this, ASIO spams "Please define _WIN32_WINNT or _WIN32_WINDOWS appropriately"
    # Originally from here: https://stackoverflow.com/a/40217291/7189436
    if(WIN32 AND CMAKE_SYSTEM_VERSION)
        if(NOT LMX_BOILERPLATE_WIN_VER)
            set(ver ${CMAKE_SYSTEM_VERSION})
            string(REGEX MATCH "^([0-9]+).([0-9])" ver ${ver})
            string(REGEX MATCH "^([0-9]+)" ver_major ${ver})
            # Check for Windows 10, b/c we'll need to convert to hex 'A'.
            if("${ver_major}" MATCHES "10")
                set(ver_major "A")
                string(REGEX REPLACE "^([0-9]+)" ${ver_major} ver ${ver})
            endif()
            # Remove all remaining '.' characters.
            string(REPLACE "." "" ver ${ver})
            # Prepend each digit with a zero.
            string(REGEX REPLACE "([0-9A-Z])" "0\\1" ver ${ver})
            set(LMX_BOILERPLATE_WIN_VER 0x${ver} CACHE INTERNAL "")
        endif()
        add_compile_definitions(_WIN32_WINNT=${LMX_BOILERPLATE_WIN_VER})
    endif()
endfunction()

if(LMX_BOILERPLATE_FLAGS)
    lmx_boilerplate_set_default_flags()
endif()
