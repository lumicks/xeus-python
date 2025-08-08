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

namespace py = pybind11;

namespace xpyt {

/**
 * Input_redirection is a scope guard implementing the redirection of
 * input() and getpass() to the frontend through an input_request message.
 */
class input_redirection {
public:
    input_redirection(bool allow_stdin);
    ~input_redirection();

private:
    py::object m_sys_input;
    py::object m_sys_getpass;
};

} // namespace xpyt
