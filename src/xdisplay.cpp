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

#include "nlohmann/json.hpp"
#include "pybind11/functional.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_json/pybind11_json.hpp"
#include "xeus-python/xutils.hpp"
#include "xeus/xinterpreter.hpp"
#include "xinternal_utils.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt_ipython {
/******************
 * display module *
 ******************/

py::module get_display_module_impl(xeus::xinterpreter* xint) {
    py::module display_module = xpyt::create_module("display");

    display_module.def(
        "publish_display_data",
        [xint](const py::object& data, const py::object& metadata, const py::object& transient,
               bool update) {
            // Make sure transient is not None
            py::object transient_ = transient;
            if (transient_.is_none()) {
                transient_ = py::dict();
            }

            if (update) {
                xint->update_display_data(data, metadata, transient_);
            } else {
                xint->display_data(data, metadata, transient_);
            }
        },
        "data"_a, "metadata"_a = py::dict(), "transient"_a = py::dict(), "update"_a = false);

    display_module.def(
        "publish_execution_result",
        [xint](const py::int_& execution_count, const py::object& data,
               const py::object& metadata) {
            nl::json cpp_data = data;
            if (cpp_data.size() != 0) {
                xint->publish_execution_result(execution_count, std::move(cpp_data), metadata);
            }
        },
        "execution_count"_a, "data"_a, "metadata"_a = py::dict());

    display_module.def(
        "clear_output", [xint](bool wait = false) { xint->clear_output(wait); }, "wait"_a = false);

    return display_module;
}
} // namespace xpyt_ipython

namespace xpyt {
py::module make_display_module(xeus::xinterpreter* xint) {
    return xpyt_ipython::get_display_module_impl(xint);
}
} // namespace xpyt

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
