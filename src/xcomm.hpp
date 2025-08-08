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

class xcomm_manager {
public:
    explicit xcomm_manager(xeus::xinterpreter* xint) : m_xint(xint) {}

    void register_target(const std::string& target_name, const py::object& callback) const;

private:
    xeus::xinterpreter* m_xint;
};

py::module make_comm_module(xeus::xinterpreter* xint);

} // namespace xpyt
