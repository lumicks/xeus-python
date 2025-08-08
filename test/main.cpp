/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "pybind11/embed.h"

namespace py = pybind11;

int main(int argc, char** argv) {
    auto config = PyConfig{};
    PyConfig_InitPythonConfig(&config);

    auto stream = std::ifstream(".embedded_python.home");
    if (!stream.is_open()) {
        stream = std::ifstream("../.embedded_python.home");
    }
    using It = std::istreambuf_iterator<char>;
    const auto embedded_home = std::string{It{stream}, It{}};
    PyConfig_SetBytesString(&config, &config.home, embedded_home.c_str());

    auto pyint = py::scoped_interpreter{&config};

    return doctest::Context(argc, argv).run();
}
