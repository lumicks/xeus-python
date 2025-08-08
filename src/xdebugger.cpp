/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xeus-python/xdebugger.hpp"

#include "pybind11/stl.h"
#include "pybind11_json/pybind11_json.hpp"
#include "xdebugpy_client.hpp"
#include "xeus-python/xutils.hpp"
#include "xeus-zmq/xmiddleware.hpp"
#include "xeus/xinterpreter.hpp"
#include "xeus/xsystem.hpp"
#include "xinternal_utils.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace pybind11::literals;
using namespace std::placeholders;

namespace xpyt {

debugger::debugger(xeus::xcontext& context, const xeus::xconfiguration& config,
                   const std::string& user_name, const std::string& session_id,
                   const nl::json& debugger_config)
    : xdebugger_base(context),
      p_debugpy_client(std::make_unique<xdebugpy_client>(
          context, config, xeus::get_socket_linger(),
          xdap_tcp_configuration(xeus::dap_tcp_type::client, xeus::dap_init_type::parallel,
                                 user_name, session_id),
          get_event_callback())),
      m_debugpy_host("127.0.0.1"), m_debugpy_port(xeus::find_free_port(100, 5678, 5900)),
      m_debugger_config(debugger_config) {
    register_request_handler("inspectVariables",
                             std::bind(&debugger::inspect_variables_request, this, _1), false);
    register_request_handler("richInspectVariables",
                             std::bind(&debugger::rich_inspect_variables_request, this, _1), false);
    register_request_handler("attach", std::bind(&debugger::attach_request, this, _1), true);
    register_request_handler("configurationDone",
                             std::bind(&debugger::configuration_done_request, this, _1), true);
    register_request_handler("copyToGlobals",
                             std::bind(&debugger::copy_to_globals_request, this, _1), true);
}

debugger::~debugger() {
    const auto gil = py::gil_scoped_acquire{};
    m_pydebugger = {};
}

nl::json debugger::inspect_variables_request(const nl::json& message) {
    const auto gil = py::gil_scoped_acquire{};
    return m_pydebugger.attr("inspect_variables")(message);
}

nl::json debugger::rich_inspect_variables_request(const nl::json& message) {
    nl::json reply = {{"type", "response"},
                      {"request_seq", message["seq"]},
                      {"success", false},
                      {"command", message["command"]}};

    std::string var_name = message["arguments"]["variableName"].get<std::string>();
    py::str py_var_name = py::str(var_name);
    bool valid_name = PyUnicode_IsIdentifier(py_var_name.ptr()) == 1;
    if (!valid_name) {
        reply["body"] = {{"data", {}}, {"metadata", {}}};
        if (var_name == "special variables" || var_name == "function variables") {
            reply["success"] = true;
        }
        return reply;
    }

    std::string var_repr_data = var_name + "_repr_data";
    std::string var_repr_metadata = var_name + "_repr_metada";

    if (get_stopped_threads().empty()) {
        // The code did not hit a breakpoint, we use the interpreter
        // to get the rich representation of the variable
        std::string code = "from IPython import get_ipython;";
        code += var_repr_data + ',' + var_repr_metadata
                + "= get_ipython().display_formatter.format(" + var_name + ")";
        py::gil_scoped_acquire acquire;
        exec(py::str(code));
    } else {
        // The code has stopped on a breakpoint, we use the setExpression request
        // to get the rich representation of the variable
        std::string code = "get_ipython().display_formatter.format(" + var_name + ")";
        int frame_id = message["arguments"]["frameId"].get<int>();
        int seq = message["seq"].get<int>();
        nl::json request = {
            {"type", "request"},
            {"command", "evaluate"},
            {"seq", seq + 1},
            {"arguments", {{"expression", code}, {"frameId", frame_id}, {"context", "clipboard"}}}};
        nl::json request_reply = forward_message(request);
        std::string result = request_reply["body"]["result"];

        py::gil_scoped_acquire acquire;
        std::string exec_code =
            var_repr_data + ',' + var_repr_metadata + "= eval(str(" + result + "))";
        exec(py::str(exec_code));
    }

    py::gil_scoped_acquire acquire;
    py::object variables = py::globals();
    py::object repr_data = variables[py::str(var_repr_data)];
    py::object repr_metadata = variables[py::str(var_repr_metadata)];
    nl::json body = {{"data", {}}, {"metadata", {}}};
    for (const py::handle& key : repr_data) {
        std::string data_key = py::str(key);
        body["data"][data_key] = repr_data[key];
        if (repr_metadata.contains(key)) {
            body["metadata"][data_key] = repr_metadata[key];
        }
    }
    PyDict_DelItem(variables.ptr(), py::str(var_repr_data).ptr());
    PyDict_DelItem(variables.ptr(), py::str(var_repr_metadata).ptr());
    reply["body"] = body;
    reply["success"] = true;
    return reply;
}

nl::json debugger::attach_request(const nl::json& message) {
    auto new_message = message;
    new_message["arguments"]["connect"] = {{"host", m_debugpy_host},
                                           {"port", std::stoi(m_debugpy_port)}};
    new_message["arguments"]["logToFile"] = true;
    return forward_message(new_message);
}

nl::json debugger::configuration_done_request(const nl::json& message) {
    return {{"seq", message["seq"].get<int>()},
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", true},
            {"command", message["command"]}};
}

nl::json debugger::copy_to_globals_request(const nl::json& message) {
    // It basically runs a setExpression in the globals dictionary of Python.
    const auto& args = message["arguments"];
    return forward_message({
        {"type", "request"},
        {"command", "setExpression"},
        {"seq", message["seq"].get<int>() + 1},
        {
            "arguments",
            {
                {"expression", "globals()['" + args["dstVariableName"].get<std::string>() + "']"},
                {"value", args["srcVariableName"].get<std::string>()},
                {"frameId", args["srcFrameId"].get<int>()},
            },
        },
    });
}

nl::json debugger::variables_request_impl(const nl::json& message) {
    if (get_stopped_threads().empty()) {
        const auto gil = py::gil_scoped_acquire{};
        return m_pydebugger.attr("variables")(message);
    }

    const auto rep = xdebugger_base::variables_request_impl(message);
    const auto gil = py::gil_scoped_acquire{};
    return m_pydebugger.attr("build_variables_response")(message, rep["body"]["variables"]);
}

bool debugger::start_debugpy() {
    if (std::getenv("XEUS_LOG") != nullptr) {
        std::ofstream out("xeus.log", std::ios_base::app);
        out << "===== DEBUGGER CONFIG =====" << std::endl;
        out << m_debugger_config.dump() << std::endl;
    }

    std::string code = "import debugpy;";
    if (auto it = m_debugger_config.find("python"); it != m_debugger_config.end()) {
        code += "debugpy.configure({\'python\': r\'" + it->get<std::string>() + "\'});";
    }
    code += "debugpy.listen((\'" + m_debugpy_host + "\'," + m_debugpy_port + "))";

    const auto rep = get_control_messenger().send_to_shell({"code", code});
    const auto status = rep["status"].get<std::string>();
    if (status != "ok") {
        std::clog << "Exception raised when trying to import debugpy" << std::endl;
        for (const auto& line : rep["traceback"].get<std::vector<std::string>>()) {
            std::clog << line << std::endl;
        }
        std::clog << rep["ename"].get<std::string>() << " - " << rep["evalue"].get<std::string>()
                  << std::endl;
    } else {
        const auto gil = py::gil_scoped_acquire{};
        m_pydebugger = py::module::import("xeus_python_shell.debugger").attr("XDebugger")();
    }
    return status == "ok";
}

bool debugger::start() {
    xeus::create_directory(xeus::get_temp_directory_path() + "/xpython_debug_logs_"
                           + std::to_string(xeus::get_current_pid()));

    static bool debugpy_started = start_debugpy();
    if (!debugpy_started) {
        return false;
    }

    const auto controller_end_point = xeus::get_controller_end_point("debugger");
    const auto controller_header_end_point = xeus::get_controller_end_point("debugger_header");
    bind_sockets(controller_header_end_point, controller_end_point);

    const auto debugpy_end_point = "tcp://" + m_debugpy_host + ':' + m_debugpy_port;
    std::thread(&xdap_tcp_client::start_debugger, p_debugpy_client.get(), debugpy_end_point,
                xeus::get_publisher_end_point(), controller_end_point, controller_header_end_point)
        .detach();

    send_recv_request("REQ");
    xeus::create_directory(get_tmp_prefix());
    return true;
}

void debugger::stop() {
    unbind_sockets(xeus::get_controller_end_point("debugger_header"),
                   xeus::get_controller_end_point("debugger"));
}

xeus::xdebugger_info debugger::get_debugger_info() const {
    return {xeus::get_tmp_hash_seed(), get_tmp_prefix(),
            get_tmp_suffix(),          true,
            {"Python Exceptions"},     true};
}

std::string debugger::get_cell_temporary_file(const std::string& code) const {
    return get_cell_tmp_file(code);
}

std::unique_ptr<xeus::xdebugger> make_python_debugger(xeus::xcontext& context,
                                                      const xeus::xconfiguration& config,
                                                      const std::string& user_name,
                                                      const std::string& session_id,
                                                      const nl::json& debugger_config) {
    return std::make_unique<debugger>(context, config, user_name, session_id, debugger_config);
}

} // namespace xpyt
