/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xdisplay.hpp"

#include "pybind11_json/pybind11_json.hpp"
#include "xinternal_utils.hpp"

using namespace pybind11::literals;

namespace xpyt {

py::module make_display_module(xeus::xinterpreter* xint) {
    return create_module("display")
        .def(
            "publish_display_data",
            [xint](nl::json data, nl::json metadata, py::object transient, bool update) {
                if (transient.is_none()) {
                    transient = py::dict();
                }
                if (update) {
                    xint->update_display_data(std::move(data), std::move(metadata), transient);
                } else {
                    xint->display_data(std::move(data), std::move(metadata), transient);
                }
            },
            "data"_a, "metadata"_a = py::dict(), "transient"_a = py::dict(), "update"_a = false)
        .def(
            "publish_execution_result",
            [xint](int execution_count, nl::json data, nl::json metadata) {
                if (data.size() != 0) {
                    xint->publish_execution_result(execution_count, std::move(data),
                                                   std::move(metadata));
                }
            },
            "execution_count"_a, "data"_a, "metadata"_a = py::dict())
        .def("clear_output", [xint](bool wait) { xint->clear_output(wait); }, "wait"_a = false);
}

} // namespace xpyt
