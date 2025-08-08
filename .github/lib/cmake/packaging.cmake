include(CMakePackageConfigHelpers)

# Declare a library namespace
#
# This function must be called before `lmx_add_library()` or `lmx_export_namespace()`.
function(lmx_declare_namespace namespace)
    set(${namespace}_targets "" CACHE INTERNAL "")
endfunction()

# Call this function once for each library target in order to set up export behavior
#
# For example, `lmx_add_library(bison foo STATIC source_files...)` will create a local
# target called `bison_foo` and an exported target called `bison::foo` (unfortunately,
# CMake does not allow `bison::foo` to be used as a local mutable target).
#
# After adding multiple of these libraries, they can all be packaged up and exported
# using the `lmx_export_namespace()` function. See below.
function(lmx_add_library namespace name)
    set(target_name ${namespace}_${name})
    add_library(${target_name} ${ARGN})
    add_library(${namespace}::${name} ALIAS ${target_name})
    set_target_properties(${target_name} PROPERTIES EXPORT_NAME ${name})

    # Set standard include dir layout. We have to match the target type.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include)
        get_target_property(type ${target_name} TYPE)
        if(type STREQUAL "INTERFACE_LIBRARY")
            set(include_type INTERFACE)
        else()
            set(include_type PUBLIC)
        endif()

        target_include_directories(${target_name} ${include_type}
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include/${name}>
        )
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include/${name})
    endif()

    list(APPEND ${namespace}_targets ${target_name})
    set(${namespace}_targets ${${namespace}_targets} CACHE INTERNAL "")
endfunction()

# Export all namespace libraries in a standard format compatible with Conan
#
# For example, `lmx_export_libraries(bison)`. This will export all libraries which were previously
# added using `lmx_add_library(bison ...)`. A `${namespace}-targets.cmake` file is created by this
# function. Conan's `CMakeDeps` will create a `${namespace}-config.cmake` file.
function(lmx_export_namespace namespace)
    install(TARGETS ${${namespace}_targets} EXPORT ${namespace}-targets OBJECTS DESTINATION obj)
    install(EXPORT ${namespace}-targets NAMESPACE ${namespace}::
            FILE ${namespace}-targets.cmake DESTINATION cmake)
endfunction()
