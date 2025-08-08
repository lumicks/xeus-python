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

#include "pybind11_json/pybind11_json.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

namespace xpyt_ipython {

struct xkernel {
    py::dict get_parent() const {
        return py::dict("header"_a = m_xint->parent_header().get<py::object>());
    }

    xeus::xinterpreter* m_xint;
    py::object m_comm_manager = {};
};

py::handle bind_xkernel() {
    static auto h = py::class_<xkernel>({}, "XKernel")
                        .def("get_parent", &xkernel::get_parent)
                        .def_property_readonly("_parent_header", &xkernel::get_parent)
                        .def_readwrite("comm_manager", &xkernel::m_comm_manager)
                        .release();
    return h;
}

} // namespace xpyt_ipython

namespace xpyt {

py::object make_kernel(xeus::xinterpreter* xint) {
    xpyt_ipython::bind_xkernel();
    return py::cast(xpyt_ipython::xkernel{xint});
}

} // namespace xpyt
