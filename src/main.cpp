/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include <signal.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#ifdef __GNUC__
#  include <execinfo.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <unistd.h>
#endif

#include "pybind11/embed.h"
#include "pybind11/pybind11.h"
#include "xeus-python/xdebugger.hpp"
#include "xeus-python/xeus_python_config.hpp"
#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xinterpreter_raw.hpp"
#include "xeus-python/xpaths.hpp"
#include "xeus-python/xutils.hpp"
#include "xeus-zmq/xserver_zmq_split.hpp"
#include "xeus-zmq/xzmq_context.hpp"
#include "xeus/xhelper.hpp"
#include "xeus/xinterpreter.hpp"
#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"

namespace py = pybind11;

int main(int argc, char* argv[]) {
    if (xeus::should_print_version(argc, argv)) {
        std::clog << "xpython " << XPYT_VERSION << std::endl;
        return 0;
    }

    // If we are called from the Jupyter launcher, silence all logging. This
    // is important for a JupyterHub configured with cleanup_servers = False:
    // Upon restart, spawned single-user servers keep running but without the
    // std* streams. When a user then tries to start a new kernel, xpython
    // will get a SIGPIPE and exit.
    if (std::getenv("JPY_PARENT_PID") != NULL) {
        std::clog.setstate(std::ios_base::failbit);
    }

    // Registering SIGSEGV handler
#ifdef __GNUC__
    std::clog << "registering handler for SIGSEGV" << std::endl;
    signal(SIGSEGV, xpyt::sigsegv_handler);

    // Registering SIGINT and SIGKILL handlers
    signal(SIGKILL, xpyt::sigkill_handler);
#endif
    signal(SIGINT, xpyt::sigkill_handler);

    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    // Setting Python Home
    auto stream = std::ifstream(".embedded_python.home");
    if (!stream.is_open()) {
        stream = std::ifstream("../.embedded_python.home");
    }
    using It = std::istreambuf_iterator<char>;
    const auto embedded_home = std::string{It{stream}, It{}};
    PyConfig_SetBytesString(&config, &config.home, embedded_home.c_str());

    // Setting Program Name
    const auto executable = embedded_home + "/bin/python3";
    PyConfig_SetBytesString(&config, &config.program_name, executable.c_str());

    // Instantiating the Python interpreter
    py::scoped_interpreter guard{&config};

    std::unique_ptr<xeus::xcontext> context = xeus::make_zmq_context();

    // Instantiating the xeus xinterpreter
    bool raw_mode = xpyt::extract_option("-r", "--raw", argc, argv);
    using interpreter_ptr = std::unique_ptr<xeus::xinterpreter>;
    interpreter_ptr interpreter;
    if (raw_mode) {
        interpreter = interpreter_ptr(new xpyt::raw_interpreter());
    } else {
        interpreter = interpreter_ptr(new xpyt::interpreter());
    }

    using history_manager_ptr = std::unique_ptr<xeus::xhistory_manager>;
    history_manager_ptr hist = xeus::make_in_memory_history_manager();

    std::string connection_filename = xeus::extract_filename(argc, argv);

#ifdef XEUS_PYTHON_PYPI_WARNING
    std::clog
        << "WARNING: this instance of xeus-python has been installed from a PyPI wheel.\n"
           "We recommend using a general-purpose package manager instead, such as Conda/Mamba.\n"
        << std::endl;
#endif

    nl::json debugger_config;
    debugger_config["python"] = executable;

    if (!connection_filename.empty()) {
        xeus::xconfiguration config = xeus::load_configuration(connection_filename);

        xeus::xkernel kernel(
            config, xeus::get_user_name(), std::move(context), std::move(interpreter),
            xeus::make_xserver_shell_main, std::move(hist),
            xeus::make_console_logger(xeus::xlogger::msg_type,
                                      xeus::make_file_logger(xeus::xlogger::content, "xeus.log")),
            xpyt::make_python_debugger, debugger_config);

        std::clog << "Starting xeus-python kernel...\n\n"
                     "If you want to connect to this kernel from an other client, you can use"
                     " the "
                         + connection_filename + " file."
                  << std::endl;

        kernel.start();
    } else {
        std::clog << "Instantiating kernel" << std::endl;
        xeus::xkernel kernel(xeus::get_user_name(), std::move(context), std::move(interpreter),
                             xeus::make_xserver_shell_main, std::move(hist), nullptr,
                             xpyt::make_python_debugger, debugger_config);

        std::cout << "Getting config" << std::endl;
        const auto& config = kernel.get_config();
        std::clog << "Starting xeus-python kernel...\n\n"
                     "If you want to connect to this kernel from an other client, just copy"
                     " and paste the following content inside of a `kernel.json` file. And then "
                     "run for example:\n\n"
                     "# jupyter console --existing kernel.json\n\n"
                     "kernel.json\n```\n{\n"
                     "    \"transport\": \""
                         + config.m_transport
                         + "\",\n"
                           "    \"ip\": \""
                         + config.m_ip
                         + "\",\n"
                           "    \"control_port\": "
                         + config.m_control_port
                         + ",\n"
                           "    \"shell_port\": "
                         + config.m_shell_port
                         + ",\n"
                           "    \"stdin_port\": "
                         + config.m_stdin_port
                         + ",\n"
                           "    \"iopub_port\": "
                         + config.m_iopub_port
                         + ",\n"
                           "    \"hb_port\": "
                         + config.m_hb_port
                         + ",\n"
                           "    \"signature_scheme\": \""
                         + config.m_signature_scheme
                         + "\",\n"
                           "    \"key\": \""
                         + config.m_key
                         + "\"\n"
                           "}\n```"
                  << std::endl;

        kernel.start();
    }

    return 0;
}
