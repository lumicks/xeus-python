/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#pragma once

#include "nlohmann/json.hpp"
#include "pybind11/pybind11.h"
#include "xeus/xinterpreter.hpp"
#include "xeus_python_config.hpp"

#include <string>

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt {

class XEUS_PYTHON_API interpreter final : public xeus::xinterpreter {
public:
    // If redirect_output_enabled is true (default) then this interpreter will
    // capture outputs and send them using publish_stream.
    // Disable this if your interpreter uses custom output redirection.
    interpreter(bool redirect_output_enabled = true);
    virtual ~interpreter();

protected:
    void configure_impl() override;

    void execute_request_impl(send_reply_callback cb, int execution_counter,
                              const std::string& code, xeus::execute_request_config config,
                              nl::json user_expressions) override;

    nl::json complete_request_impl(const std::string& code, int cursor_pos) override;

    nl::json inspect_request_impl(const std::string& code, int cursor_pos,
                                  int detail_level) override;

    nl::json is_complete_request_impl(const std::string& code) override;

    nl::json kernel_info_request_impl() override;

    void shutdown_request_impl() override {}

    nl::json internal_request_impl(const nl::json& content) override;

    void redirect_output();

    py::object m_ipython_shell;
    bool m_redirect_output_enabled;
};

} // namespace xpyt
