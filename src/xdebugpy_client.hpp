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

#include "xeus-zmq/xdap_tcp_client.hpp"

namespace xpyt {

using xeus::xdap_tcp_client;
using xeus::xdap_tcp_configuration;

class xdebugpy_client : public xdap_tcp_client {
public:
    xdebugpy_client(xeus::xcontext& context, const xeus::xconfiguration& config, int socket_linger,
                    const xdap_tcp_configuration& dap_config, const event_callback& cb)
        : xdap_tcp_client(context, config, socket_linger, dap_config, cb) {}

private:
    void handle_event(nl::json message) override;
    nl::json get_stack_frames(int thread_id, int seq);
};

} // namespace xpyt
