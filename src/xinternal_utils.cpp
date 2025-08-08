/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xinternal_utils.hpp"

#include "pybind11_json/pybind11_json.hpp"
#include "xeus/xsystem.hpp"

#include <string>
#include <vector>

using namespace py::literals;

namespace xpyt {
namespace {

xeus::binary_buffer pybytes_to_cpp_message(const py::bytes& bytes) {
    const auto s = static_cast<std::string_view>(bytes);
    return {s.begin(), s.end()};
}

} // namespace

py::module create_module(const std::string& module_name) {
    return py::module_::create_extension_module(module_name.c_str(), nullptr,
                                                new py::module_::module_def);
}

std::string red_text(const std::string& text) { return "\033[0;31m" + text + "\033[0m"; }

std::string green_text(const std::string& text) { return "\033[0;32m" + text + "\033[0m"; }

std::string blue_text(const std::string& text) { return "\033[0;34m" + text + "\033[0m"; }

std::string highlight(const std::string& code) {
    const auto py_highlight = py::module::import("pygments").attr("highlight");
    const auto lexer = py::module::import("pygments.lexers").attr("Python3Lexer");
    const auto formatter = py::module::import("pygments.formatters").attr("TerminalFormatter");
    return py_highlight(code, lexer(), formatter()).cast<std::string>();
}

py::list cpp_buffers_to_pylist(const xeus::buffer_sequence& buffers) {
    auto bufferlist = py::list();
    for (const auto& buffer : buffers) {
        bufferlist.append(py::memoryview(py::bytes(buffer.data(), buffer.size())));
    }
    return bufferlist;
}

xeus::buffer_sequence pylist_to_cpp_buffers(const py::object& bufferlist) {
    // Cannot iterate over NoneType, returning immediately with an empty vector
    if (bufferlist.is_none()) {
        return {};
    }

    auto buffers = xeus::buffer_sequence();
    for (py::handle buffer : bufferlist) {
        if (py::isinstance<py::memoryview>(buffer)) {
            buffers.push_back(pybytes_to_cpp_message(buffer.attr("tobytes")()));
        } else {
            buffers.push_back(pybytes_to_cpp_message(buffer.cast<py::bytes>()));
        }
    }
    return buffers;
}

py::object cppmessage_to_pymessage(const xeus::xmessage& msg) {
    py::dict py_msg;
    py_msg["header"] = msg.header().get<py::object>();
    py_msg["parent_header"] = msg.parent_header().get<py::object>();
    py_msg["metadata"] = msg.metadata().get<py::object>();
    py_msg["content"] = msg.content().get<py::object>();
    py_msg["buffers"] = cpp_buffers_to_pylist(msg.buffers());
    return py_msg;
}

std::string get_tmp_prefix() { return xeus::get_tmp_prefix("xpython"); }

std::string get_tmp_suffix() { return ".py"; }

std::string get_cell_tmp_file(const std::string& content) {
    return xeus::get_cell_tmp_file(get_tmp_prefix(), content, get_tmp_suffix());
}
} // namespace xpyt
