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
#include "xeus/xinterpreter.hpp"

namespace py = pybind11;

namespace xpyt {
py::object make_stream(const std::string& name, xeus::xinterpreter* xint);
py::object make_terminal_stream();
} // namespace xpyt
