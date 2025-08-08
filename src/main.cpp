/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "pybind11/embed.h"
#include "pybind11/pybind11.h"
#include "xeus-python/xdebugger.hpp"
#include "xeus-python/xeus_python_config.hpp"
#include "xeus-python/xinterpreter.hpp"
#include "xeus-zmq/xserver_zmq_split.hpp"
#include "xeus-zmq/xzmq_context.hpp"
#include "xeus/xhelper.hpp"
#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"

#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (xeus::should_print_version(argc, argv)) {
        std::clog << "xpython " << XPYT_VERSION << std::endl;
        return 0;
    }

    auto stream = std::ifstream(".embedded_python.home");
    if (!stream.is_open()) {
        stream = std::ifstream("../.embedded_python.home");
    }
    using It = std::istreambuf_iterator<char>;
    const auto embedded_home = std::string{It{stream}, It{}};

    auto config = PyConfig{};
    PyConfig_InitPythonConfig(&config);
    PyConfig_SetBytesString(&config, &config.home, embedded_home.c_str());
    const auto executable = embedded_home + "/bin/python3";
    PyConfig_SetBytesString(&config, &config.program_name, executable.c_str());
    auto pyint = py::scoped_interpreter{&config};

    const auto connection_filename = xeus::extract_filename(argc, argv);
    if (!connection_filename.empty()) {
        auto kernel = xeus::xkernel{
            xeus::load_configuration(connection_filename),
            xeus::get_user_name(),
            xeus::make_zmq_context(),
            std::make_unique<xpyt::interpreter>(),
            xeus::make_xserver_shell_main,
            xeus::make_in_memory_history_manager(),
            xeus::make_console_logger(xeus::xlogger::msg_type,
                                      xeus::make_file_logger(xeus::xlogger::content, "xeus.log")),
            xpyt::make_python_debugger,
            nl::json{{"python", executable}},
        };

        std::clog << "Starting xeus-python kernel...\n"
                     "If you want to connect to this kernel from another client, you can use the "
                         + connection_filename + " file."
                  << std::endl;

        kernel.start();
    }
}
