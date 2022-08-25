#include "gtest/gtest.h"

#include "xeus/xeus_context.hpp"
#include "xeus/xkernel.hpp"
#include "xeus/xserver_shell_main.hpp"
#include "xeus/xinterpreter.hpp"

#include "pybind11/embed.h"
#include "pybind11/pybind11.h"

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xdebugger.hpp"
#include "xeus-python/xpaths.hpp"

void start_stop_kernel() {
    xeus::xkernel kernel(xeus::get_user_name(),
                         xeus::make_context<zmq::context_t>(),
                         std::make_unique<xpyt::interpreter>(),
                         xeus::make_xserver_shell_main,
                         xeus::make_in_memory_history_manager(),
                         nullptr,
                         xpyt::make_python_debugger);
}

TEST(multi_kernel, start_stop)
{
    static const std::string executable(xpyt::get_python_path());
    static const std::wstring wexecutable(executable.cbegin(), executable.cend());
    Py_SetProgramName(const_cast<wchar_t*>(wexecutable.c_str()));

    {
        py::scoped_interpreter guard;
        start_stop_kernel();
        start_stop_kernel();
    }
}
