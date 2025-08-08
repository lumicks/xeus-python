/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xeus-python/xutils.hpp"

#include "pybind11/eval.h"

namespace xpyt {

bool holding_gil() { return PyGILState_Check(); }

void exec(const py::object& code, const py::object& scope) {
    py::exec("exec(_code_, _scope_, _scope_)", py::globals(),
             py::dict(py::arg("_code_") = code, py::arg("_scope_") = scope));
}

py::object eval(const py::object& code, const py::object& scope) { return py::eval(code, scope); }

} // namespace xpyt
