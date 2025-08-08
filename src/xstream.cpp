/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xstream.hpp"

#include <iostream>

namespace py = pybind11;

namespace xpyt {

struct xstream {
    py::object write;
};

py::handle bind_stream() {
    static auto h = py::class_<xstream>({}, "Stream")
                        .def_readwrite<>("write", &xstream::write)
                        .def("flush", [](xstream&) {})
                        .def("isatty", [](xstream&) { return false; })
                        .release();
    return h;
}

py::object make_stream(const std::string& name, xeus::xinterpreter* xint) {
    bind_stream();
    return py::cast(xstream{py::cpp_function(
        [name, xint](const std::string& msg) { xint->publish_stream(name, msg); })});
}

py::object make_terminal_stream() {
    bind_stream();
    return py::cast(
        xstream{py::cpp_function([](const std::string& message) { std::cout << message; })});
}

} // namespace xpyt
