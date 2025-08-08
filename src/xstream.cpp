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

#include "pybind11/functional.h"
#include "pybind11/pybind11.h"
#include "xeus/xinterpreter.hpp"
#include "xinternal_utils.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace py = pybind11;

namespace xpyt {

/***********************
 * xstream declaration *
 ***********************/

class xstream {
public:
    xstream(std::string stream_name, xeus::xinterpreter* xint);
    virtual ~xstream();

    py::object get_write();
    void set_write(const py::object& func);
    void flush();
    bool isatty();

private:
    std::string m_stream_name;
    py::object m_write_func;
};

py::handle bind_stream() {
    static auto h = py::class_<xstream>({}, "Stream")
                        .def_property("write", &xstream::get_write, &xstream::set_write)
                        .def("flush", &xstream::flush)
                        .def("isatty", &xstream::isatty)
                        .release();
    return h;
}

/**************************
 * xstream implementation *
 **************************/

xstream::xstream(std::string stream_name, xeus::xinterpreter* xint)
    : m_stream_name(stream_name),
      m_write_func(py::cpp_function([stream_name, xint](const std::string& message) {
          xint->publish_stream(stream_name, message);
      })) {}

xstream::~xstream() {}

py::object xstream::get_write() { return m_write_func; }

void xstream::set_write(const py::object& func) { m_write_func = func; }

void xstream::flush() {}

bool xstream::isatty() { return false; }

/********************************
 * xterminal_stream declaration *
 ********************************/

class xterminal_stream {
public:
    xterminal_stream();
    virtual ~xterminal_stream();

    py::object get_write();
    void set_write(const py::object& func);
    void flush();

private:
    py::object m_write_func;
};

py::handle bind_terminal_stream() {
    static auto h =
        py::class_<xterminal_stream>({}, "TerminalStream")
            .def(py::init<>())
            .def_property("write", &xterminal_stream::get_write, &xterminal_stream::set_write)
            .def("flush", &xterminal_stream::flush)
            .release();
    return h;
}

/***********************************
 * xterminal_stream implementation *
 ***********************************/

xterminal_stream::xterminal_stream()
    : m_write_func(py::cpp_function([](const std::string& message) { std::cout << message; })) {}

xterminal_stream::~xterminal_stream() {}

py::object xterminal_stream::get_write() { return m_write_func; }

void xterminal_stream::set_write(const py::object& func) { m_write_func = func; }

void xterminal_stream::flush() {}

/*****************
 * stream module *
 *****************/

py::module make_stream_module() {
    py::module stream_module = create_module("stream");
    stream_module.attr("Stream") = bind_stream();
    stream_module.attr("TerminalStream") = bind_terminal_stream();
    return stream_module;
}

py::object make_stream(const std::string& name, xeus::xinterpreter* xint) {
    bind_stream();
    return py::cast(xstream(name, xint));
}

} // namespace xpyt
