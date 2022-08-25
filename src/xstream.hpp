/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_STREAM_HPP
#define XPYT_STREAM_HPP

#include "xeus/xinterpreter.hpp"

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{
    class xstream
    {
    public:

      xstream(std::string stream_name, xeus::xinterpreter* interpreter);
      virtual ~xstream();

      void write(const std::string& message);
      void flush();
      bool isatty();

    private:

      std::string m_stream_name;
      xeus::xinterpreter* m_interpreter;
    };

    py::module get_stream_module();
}

#endif
