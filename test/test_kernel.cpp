#include "doctest/doctest.h"

#include "pybind11/pybind11.h"

#include <fstream>

namespace py = pybind11;

TEST_CASE("kernel")
{
    auto environ = py::module_::import("os").attr("environ");
    environ["JUPYTER_PATH"] = SOURCE_DIR "/share/jupyter";
    environ["JUPYTER_PLATFORM_DIRS"] = "1";

    auto args = py::list();
    args.append(py::str(SOURCE_DIR));
    args.append(py::str("-s"));
    const auto return_code = py::module::import("pytest").attr("main")(args).cast<int>();
    py::module_::import("sys").attr("stdout").attr("flush")();
    REQUIRE(return_code == 0);
}
