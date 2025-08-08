/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xeus/xinterpreter.hpp"

#include "pybind11_json/pybind11_json.hpp"
#include "xcomm.hpp"
#include "xdisplay.hpp"
#include "xeus-python/xeus_python_config.hpp"
#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xtraceback.hpp"
#include "xeus-python/xutils.hpp"
#include "xinput.hpp"
#include "xkernel.hpp"
#include "xstream.hpp"

#include <string>
#include <vector>

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt {

interpreter::interpreter(bool redirect_output_enabled)
    : m_redirect_output_enabled{redirect_output_enabled} {}

interpreter::~interpreter() {
    py::gil_scoped_acquire acquire;
    m_ipython_shell = {};
}

void interpreter::configure_impl() {
    const auto gil = py::gil_scoped_acquire{};

    const auto sys = py::module::import("sys");
    const auto comm = make_comm_module(this);
    // Old approach: ipykernel provides the comm
    sys.attr("modules")["ipykernel.comm"] = comm;
    // New approach: we provide our comm module
    sys.attr("modules")["comm"] = comm;

    const auto ipython_shell_app =
        py::module::import("xeus_python_shell.shell").attr("XPythonShellApp")();
    ipython_shell_app.attr("initialize")(true);
    m_ipython_shell = ipython_shell_app.attr("shell");

    // Setting kernel property owning the CommManager and get_parent
    m_ipython_shell.attr("kernel") = make_kernel(this);
    m_ipython_shell.attr("kernel").attr("comm_manager") = xcomm_manager(this);

    const auto display_module = make_display_module(this);
    m_ipython_shell.attr("display_pub").attr("publish_display_data") =
        display_module.attr("publish_display_data");
    m_ipython_shell.attr("display_pub").attr("clear_output") = display_module.attr("clear_output");

    m_ipython_shell.attr("displayhook").attr("publish_execution_result") =
        display_module.attr("publish_execution_result");

    // Needed for redirecting logging to the terminal
    const auto logger = ipython_shell_app.attr("log");
    logger.attr("handlers") = py::list(0);
    logger.attr("addHandler")(
        py::module::import("logging").attr("StreamHandler")(make_terminal_stream()));

    // Initializing the compiler
    const auto traceback_module = make_traceback_module();
    m_ipython_shell.attr("compile").attr("filename_mapper") =
        traceback_module.attr("register_filename_mapping");
    m_ipython_shell.attr("compile").attr("get_filename") = traceback_module.attr("get_filename");

    if (m_redirect_output_enabled) {
        redirect_output();
    }
}

void interpreter::execute_request_impl(send_reply_callback cb, int /*execution_count*/,
                                       const std::string& code, xeus::execute_request_config config,
                                       nl::json user_expressions) {
    py::gil_scoped_acquire acquire;
    nl::json kernel_res;

    // Reset traceback
    m_ipython_shell.attr("last_error") = py::none();

    // Scope guard performing the temporary monkey patching of input and
    // getpass with a function sending input_request messages.
    auto input_guard = input_redirection(config.allow_stdin);

    bool exception_occurred = false;
    try {
        m_ipython_shell.attr("run_cell")(code, "store_history"_a = config.store_history,
                                         "silent"_a = config.silent);
    } catch (std::runtime_error& e) {
        const std::string error_msg = e.what();
        if (!config.silent) {
            publish_execution_error("RuntimeError", error_msg, std::vector<std::string>());
        }
        kernel_res["ename"] = "std::runtime_error";
        kernel_res["evalue"] = error_msg;
        exception_occurred = true;
    } catch (py::error_already_set& e) {
        xerror error = extract_already_set_error(e);
        if (!config.silent) {
            publish_execution_error(error.name, error.value, error.traceback);
        }

        kernel_res["ename"] = error.name;
        kernel_res["evalue"] = error.value;
        kernel_res["traceback"] = error.traceback;
        exception_occurred = true;
    } catch (...) {
        if (!config.silent) {
            publish_execution_error("unknown_error", "", std::vector<std::string>());
        }
        kernel_res["ename"] = "UnknownError";
        kernel_res["evalue"] = "";
        exception_occurred = true;
    }

    const auto payload_manager = m_ipython_shell.attr("payload_manager");
    kernel_res["payload"] = payload_manager.attr("read_payload")();
    payload_manager.attr("clear_payload")();

    if (exception_occurred) {
        kernel_res["status"] = "error";
        kernel_res["traceback"] = std::vector<std::string>();
        cb(kernel_res);
        return;
    }

    if (m_ipython_shell.attr("last_error").is_none()) {
        kernel_res["status"] = "ok";
        kernel_res["user_expressions"] = m_ipython_shell.attr("user_expressions")(user_expressions);
    } else {
        const auto error = extract_error(m_ipython_shell.attr("last_error").cast<py::list>());
        if (!config.silent) {
            publish_execution_error(error.name, error.value, error.traceback);
        }

        kernel_res["status"] = "error";
        kernel_res["ename"] = error.name;
        kernel_res["evalue"] = error.value;
        kernel_res["traceback"] = error.traceback;
    }
    cb(kernel_res);
}

nl::json interpreter::complete_request_impl(const std::string& code, int cursor_pos) {
    const auto gil = py::gil_scoped_acquire{};
    const auto completion =
        m_ipython_shell.attr("complete_code")(code, cursor_pos).cast<py::list>();
    return nl::json{
        {"matches", completion[0]},
        {"cursor_start", completion[1]},
        {"cursor_end", completion[2]},
        {"metadata", nl::json::object()},
        {"status", "ok"},
    };
}

nl::json interpreter::inspect_request_impl(const std::string& code, int cursor_pos,
                                           int detail_level) {
    const auto gil = py::gil_scoped_acquire{};
    const auto name =
        py::module_::import("IPython.utils.tokenutil").attr("token_at_cursor")(code, cursor_pos);
    try {
        return nl::json{
            {"data",
             m_ipython_shell.attr("object_inspect_mime")(name, "detail_level"_a = detail_level)},
            {"metadata", nl::json::object()},
            {"found", true},
            {"status", "ok"},
        };
    } catch (py::error_already_set&) {
        return nl::json{
            {"data", nl::json::object()},
            {"metadata", nl::json::object()},
            {"found", false},
            {"status", "ok"},
        };
    }
}

nl::json interpreter::is_complete_request_impl(const std::string& code) {
    const auto gil = py::gil_scoped_acquire{};

    auto tm = py::getattr(m_ipython_shell, "input_transformer_manager", py::none());
    if (tm.is_none()) {
        tm = m_ipython_shell.attr("input_splitter");
    }

    const auto result = tm.attr("check_complete")(code).cast<py::list>();
    const auto status = result[0].cast<std::string>();

    auto kernel_res = nl::json{{"status", status}};
    if (status == "incomplete") {
        kernel_res["indent"] = std::string(result[1].cast<std::size_t>(), ' ');
    }
    return kernel_res;
}

nl::json interpreter::kernel_info_request_impl() {
    constexpr auto banner = R"(
  __  _____ _   _ ___
  \ \/ / _ \ | | / __|
   >  <  __/ |_| \__ \
  /_/\_\___|\__,_|___/

  xeus-python: a Jupyter kernel for Python
  Python )" PY_VERSION;

    return nl::json{
        {"implementation", "xeus-python"},
        {"implementation_version", XPYT_VERSION},
        {"banner", banner},
        {"debugger", true},
        {"language_info",
         nl::json{
             {"name", "python"},
             {"version", PY_VERSION},
             {"mimetype", "text/x-python"},
             {"file_extension", ".py"},
         }},
        {"help_links", nl::json::array({nl::json{{"text", "Xeus-Python Reference"},
                                                 {"url", "https://xeus-python.readthedocs.io"}}})},
        {"status", "ok"},
    };
}

nl::json interpreter::internal_request_impl(const nl::json& content) {
    const auto gil = py::gil_scoped_acquire{};
    const auto code = content.value("code", "");
    m_ipython_shell.attr("last_error") = py::none();

    try {
        exec(py::str(code));
        return {{"status", "ok"}};
    } catch (py::error_already_set& e) {
        // This will grab the latest traceback and set shell.last_error
        m_ipython_shell.attr("showtraceback")();
        const auto error = extract_error(m_ipython_shell.attr("last_error"));
        publish_execution_error(error.name, error.value, error.traceback);
        return {
            {"status", "error"},
            {"ename", error.name},
            {"evalue", error.value},
            {"traceback", code},
        };
    }
}

void interpreter::redirect_output() {
    const auto sys = py::module::import("sys");
    sys.attr("stdout") = make_stream("stdout", this);
    sys.attr("stderr") = make_stream("stderr", this);
}

} // namespace xpyt
