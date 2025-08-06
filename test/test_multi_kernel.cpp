#include "doctest/doctest.h"

#include "xeus/xeus_context.hpp"
#include "xeus/xkernel.hpp"
#include "xeus/xinterpreter.hpp"

#include "xeus-zmq/xzmq_context.hpp"
#include "xeus-zmq/xserver_zmq_split.hpp"

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xdebugger.hpp"
#include "xeus-python/xpaths.hpp"

#include <fstream>

void start_stop_kernel() {
    xeus::xkernel kernel(xeus::get_user_name(),
                         xeus::make_zmq_context(),
                         std::make_unique<xpyt::interpreter>(),
                         xeus::make_xserver_shell_main,
                         xeus::make_in_memory_history_manager(),
                         nullptr,
                         xpyt::make_python_debugger);
}

TEST_CASE("multi kernel")
{
    start_stop_kernel();
    start_stop_kernel();
}
