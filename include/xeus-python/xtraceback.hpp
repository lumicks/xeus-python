/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#pragma once

#include "pybind11/pybind11.h"
#include "xeus_python_config.hpp"

#include <string>
#include <vector>

namespace py = pybind11;

namespace xpyt {

struct XEUS_PYTHON_API xerror {
    std::string name;
    std::string value;
    std::vector<std::string> traceback;
};

XEUS_PYTHON_API py::module make_traceback_module();

XEUS_PYTHON_API XPYT_FORCE_PYBIND11_EXPORT xerror extract_error(const py::list& error);
xerror extract_already_set_error(py::error_already_set& error);

} // namespace xpyt
