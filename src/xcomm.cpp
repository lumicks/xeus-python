/***************************************************************************
 * Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
 * Wolf Vollprecht                                                          *
 * Copyright (c) 2018, QuantStack                                           *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xeus/xcomm.hpp"

#include "pybind11_json/pybind11_json.hpp"
#include "xcomm.hpp"
#include "xeus-python/xutils.hpp"
#include "xinternal_utils.hpp"

#include <string>
#include <utility>

using namespace pybind11::literals;

namespace xpyt {
namespace {

xeus::xguid get_comm_id(const py::kwargs& kwargs) {
    if (py::hasattr(kwargs, "comm_id")) {
        const auto sv = kwargs["comm_id"].cast<std::string_view>();
        return xeus::xguid(sv.begin(), sv.end());
    }
    return xeus::new_xguid();
}

std::function<void(const xeus::xmessage&)>
cpp_callback(const std::function<void(py::object)>& py_callback) {
    return [py_callback](const xeus::xmessage& msg) {
        XPYT_HOLDING_GIL(py_callback(cppmessage_to_pymessage(msg)))
    };
}

} // namespace

class xcomm {
public:
    xcomm(xeus::xinterpreter* xint, const std::string& target_name, xeus::xguid guid,
          const nl::json& data, const nl::json& metadata, const py::object& buffers)
        : m_comm(xint->comm_manager().target(target_name), guid) {
        m_comm.open(metadata, data, pylist_to_cpp_buffers(buffers));
    }
    explicit xcomm(xeus::xcomm&& comm) : m_comm(std::move(comm)) {}

    std::string comm_id() const { return m_comm.id(); }

    void close(const py::object& data, const py::object& metadata, const py::object& buffers) {
        m_comm.close(metadata, data, pylist_to_cpp_buffers(buffers));
    }
    void send(const py::object& data, const py::object& metadata, const py::object& buffers) {
        m_comm.send(metadata, data, pylist_to_cpp_buffers(buffers));
    }
    void on_msg(const std::function<void(py::object)>& callback) {
        m_comm.on_message(cpp_callback(callback));
    }
    void on_close(const std::function<void(py::object)>& callback) {
        m_comm.on_close(cpp_callback(callback));
    }

private:
    xeus::xcomm m_comm;
};

py::handle bind_comm() {
    static auto h = py::class_<xcomm>({}, "Comm")
                        .def("close", &xcomm::close, "data"_a = py::dict(),
                             "metadata"_a = py::dict(), "buffers"_a = py::list())
                        .def("send", &xcomm::send, "data"_a = py::dict(), "metadata"_a = py::dict(),
                             "buffers"_a = py::list())
                        .def("on_msg", &xcomm::on_msg)
                        .def("on_close", &xcomm::on_close)
                        .def_property_readonly("comm_id", &xcomm::comm_id)
                        .def_property_readonly("kernel", [](xcomm&) { return true; })
                        .release();
    return h;
}

void xcomm_manager::register_target(const std::string& target_name,
                                    const py::object& callback) const {
    auto target_callback = [callback](xeus::xcomm&& comm, const xeus::xmessage& msg) {
        XPYT_HOLDING_GIL(callback(xcomm(std::move(comm)), cppmessage_to_pymessage(msg)));
    };

    m_xint->comm_manager().register_comm_target(target_name, target_callback);
}

py::handle bind_comm_manager() {
    static auto h = py::class_<xcomm_manager>({}, "CommManager")
                        .def("register_target", &xcomm_manager::register_target)
                        .release();
    return h;
}

py::module make_comm_module(xeus::xinterpreter* xint) {
    bind_comm();
    bind_comm_manager();

    return create_module("comm")
        .def(
            "create_comm",
            [xint](const std::string& target_name, const nl::json& data, const nl::json& metadata,
                   const py::object& buffers, const py::kwargs& kwargs) {
                return xcomm(xint, target_name, get_comm_id(kwargs), data, metadata, buffers);
            },
            "target_name"_a = "", "data"_a = py::dict(), "metadata"_a = py::dict(),
            "buffers"_a = py::list())
        .def("get_comm_manager", [xint]() { return xcomm_manager(xint); });
}
} // namespace xpyt
