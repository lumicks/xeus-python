/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xdebugpy_client.hpp"

namespace xpyt {

void xdebugpy_client::handle_event(nl::json message) { forward_event(std::move(message)); }

nl::json xdebugpy_client::get_stack_frames(int thread_id, int seq) {
    send_dap_request({{"type", "request"},
                      {"seq", seq},
                      {"command", "stackTrace"},
                      {"arguments", {{"threadId", thread_id}}}});

    auto reply = wait_for_message([](const nl::json& message) {
        return message["type"] == "response" && message["command"] == "stackTrace";
    });
    return reply["body"]["stackFrames"];
}

} // namespace xpyt
