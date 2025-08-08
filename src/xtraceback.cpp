/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xeus-python/xtraceback.hpp"

#include "pybind11/stl.h"
#include "xinternal_utils.hpp"

#include <map>
#include <string>
#include <vector>

namespace py = pybind11;
using namespace py::literals;

namespace xpyt {

std::map<std::string, int>& get_filename_map() {
    static std::map<std::string, int> fnm;
    return fnm;
}

xerror extract_error(const py::list& error) {
    return {
        .name = error[0].cast<std::string>(),
        .value = error[1].cast<std::string>(),
        .traceback = error[2].cast<std::vector<std::string>>(),
    };
}

xerror extract_already_set_error(py::error_already_set& error) {
    if (error.type().is_none()) { // this should NOT happen
        return {
            .name = "Error",
            .value = error.what(),
            .traceback = {error.what()},
        };
    }

    auto out = xerror{
        .name = py::str(error.type().attr("__name__")),
        .value = py::str(error.value()),
        .traceback = {},
    };

    constexpr auto first_frame_size = std::size_t{75};
    const auto delimiter = std::string(first_frame_size, '-');
    constexpr auto traceback_msg = std::string_view("Traceback (most recent call last)");
    const auto first_frame_padding =
        std::string(first_frame_size - traceback_msg.size() - out.name.size(), ' ');

    std::stringstream first_frame;
    first_frame << red_text(delimiter) << "\n"
                << red_text(out.name) << first_frame_padding << traceback_msg;
    out.traceback.push_back(first_frame.str());

    if (error.trace() && !error.trace().is_none()) {
        const auto extract_tb = py::module::import("traceback").attr("extract_tb");
        for (const auto& py_frame : extract_tb(error.trace())) {
            auto filename = py_frame.attr("filename").cast<std::string>();
            if (filename == "<string>") { // Workaround for py::exec
                continue;
            }

            const auto lineno = py_frame.attr("lineno").cast<std::string>();
            const auto name = py_frame.attr("name").cast<std::string>();
            const auto line = py_frame.attr("line").cast<std::string>();

            std::string file_prefix;
            std::string func_name;

            // If the error occured in a cell code, extract the line from the given code
            if (filename.starts_with(get_tmp_prefix())) {
                file_prefix = "In  ";
                if (const auto it = get_filename_map().find(filename);
                    it != get_filename_map().end()) {
                    filename = '[' + std::to_string(it->second) + ']';
                }
            } else {
                file_prefix = "File ";
                func_name = ", in " + green_text(name);
            }

            std::stringstream cpp_frame;
            std::string padding(6 - lineno.size(), ' ');
            cpp_frame << file_prefix << blue_text(filename) << func_name << ":\n"
                      << "Line " << blue_text(lineno) << ":" << padding << highlight(line);
            out.traceback.push_back(cpp_frame.str());
        }
    }

    std::stringstream last_frame;
    last_frame << red_text(out.name) << ": " << out.value << "\n" << red_text(delimiter);
    out.traceback.push_back(last_frame.str());

    return out;
}

py::module make_traceback_module() {
    return create_module("traceback")
        .def(
            "get_filename", [](const py::str& raw_code) { return get_cell_tmp_file(raw_code); },
            "raw_code"_a)
        .def(
            "register_filename_mapping",
            [](const std::string& filename, int execution_count) {
                get_filename_map()[filename] = execution_count;
            },
            "filename"_a, "execution_count"_a);
}

} // namespace xpyt
