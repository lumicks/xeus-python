############################################################################
# Copyright (c) 2016, Martin Renou, Johan Mabille, Sylvain Corlay, and     #
# Wolf Vollprecht                                                          #
# Copyright (c) 2016, QuantStack                                           #
#                                                                          #
# Distributed under the terms of the BSD 3-Clause License.                 #
#                                                                          #
# The full license is in the file LICENSE, distributed with this software. #
############################################################################

# Unit tests
# ==========

cmake_minimum_required(VERSION 3.20)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(xeus-python-test)

    enable_testing()

    find_package(xeus-python REQUIRED CONFIG)
endif ()

find_package(Threads)
find_package(doctest)

set(XEUS_PYTHON_TESTS
    main.cpp
    ../src/xutils.cpp
#    test_debugger.cpp
    test_kernel.cpp
    test_multi_kernel.cpp
    xeus_client.hpp
    xeus_client.cpp
)

add_executable(test_xeus_python ${XEUS_PYTHON_TESTS})

target_compile_features(test_xeus_python PRIVATE cxx_std_17)

if (APPLE)
    set_target_properties(test_xeus_python PROPERTIES
        MACOSX_RPATH ON
    )
endif()

set_target_properties(test_xeus_python PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

include_directories(${PYTHON_INCLUDE_DIRS})
target_link_libraries(test_xeus_python ${PYTHON_LIBRARIES} xeus-zmq doctest::doctest ${CMAKE_THREAD_LIBS_INIT} pybind11::embed pybind11_json::pybind11_json xeus-python-static)
target_include_directories(test_xeus_python PRIVATE ${XEUS_PYTHON_INCLUDE_DIR})

add_custom_target(xtest COMMAND test_xeus_python DEPENDS test_xeus_python)

target_compile_definitions(test_xeus_python PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_dependencies(test_xeus_python xpython)
