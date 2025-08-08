/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xkernel.hpp"

#include "nlohmann/json.hpp"
#include "pybind11/eval.h"
#include "pybind11/functional.h"
#include "pybind11/pybind11.h"
#include "pybind11_json/pybind11_json.hpp"
#include "xcomm.hpp"
#include "xeus-python/xutils.hpp"
#include "xeus/xinterpreter.hpp"
#include "xinternal_utils.hpp"

#include <string>
#include <utility>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt_ipython {
/***********************
 * xkernel declaration *
 **********************/

struct xkernel {
    py::dict get_parent();

    xeus::xinterpreter* m_xint;
    py::object m_comm_manager = {};
};

/**************************
 * xkernel implementation *
 **************************/

py::dict xkernel::get_parent() {
    return py::dict(py::arg("header") = m_xint->parent_header().get<py::object>());
}

py::handle bind_xkernel() {
    static auto h = py::class_<xkernel>({}, "XKernel")
                        .def("get_parent", &xkernel::get_parent)
                        .def_property_readonly("_parent_header", &xkernel::get_parent)
                        .def_readwrite("comm_manager", &xkernel::m_comm_manager)
                        .release();
    return h;
}

/*****************
 * kernel module *
 *****************/

py::module get_kernel_module_impl() {
    py::module kernel_module = xpyt::create_module("kernel");
    kernel_module.attr("XKernel") = bind_xkernel();
    return kernel_module;
}
} // namespace xpyt_ipython

namespace xpyt {
py::module make_kernel_module() { return xpyt_ipython::get_kernel_module_impl(); }

py::object make_kernel(xeus::xinterpreter* xint) {
    xpyt_ipython::bind_xkernel();
    return py::cast(xpyt_ipython::xkernel{xint});
}
} // namespace xpyt

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
