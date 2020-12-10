/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014-2020 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */


/** \file
    Python module wrapping top-level Dakota
 */


#include "ExecutableEnvironment.hpp"
#include "LibraryEnvironment.hpp"

//#include "Eigen/Dense"

//#include <pybind11/eigen.h> 
#include <pybind11/pybind11.h>
//#include <pybind11/stl.h>

namespace py = pybind11;

namespace Dakota {
namespace python {

  // demonstrate a little wrapper to get Dakota version info without
  // making an Environment
  void print_version() {
    OutputManager out_mgr;
    out_mgr.output_version();
  }

  // Eigen::VectorXd final_response(const Environment& dakenv) {
  // const Response& resp = dakenv.response_results();
  // size_t num_fns = resp.num_functions();
  // return Eigen::Map<Eigen::VectorXd>(resp.function_values().values(), num_fns);
  // }

  Real get_response_fn_val(const Dakota::LibraryEnvironment & env) {
    // retrieve the final response values
    const Response& resp  = env.response_results();
    return resp.function_value(0);
  }
    
}
}


/// Define a Python module that wraps a few top-level dakota functions
/// Module name is really generic due to overly simple Python
///  packaging scheme we're using
PYBIND11_MODULE(dakpy, m) {

  // demo a module-level function
  m.def("version",
	&Dakota::python::print_version,
	"Print Dakota version to console"
	);

  // demo a Dakota command-line wrapper
  // probably don't need this at all, but demoing...
  py::class_<Dakota::ExecutableEnvironment>(m, "CommandLine")
    .def(py::init
	 ([](const std::string& input_filename)
	  {
	    int argc = 2;
            std::string infile_copy(input_filename);
	    char* argv[] = { "dakota",
			     const_cast<char*>(infile_copy.data()),
			     NULL
	    };
            return new Dakota::ExecutableEnvironment(argc, argv);
	  })
	 )

    .def("execute", &Dakota::ExecutableEnvironment::execute)
    ;

  // demo a Response wrapper
  py::class_<Dakota::Response>(m, "Response")
    .def(py::init
	 ([]()
	  {
	    return new Dakota::Response(); 
	  }))

    .def("function_value", static_cast<const Dakota::Real & (Dakota::Response::*)(size_t) const>(&Dakota::Response::function_value)
         , "Return function value by index"
         , py::arg("i"))
    ;

  // demo a library environment that models opt_tpl_test semantics
  py::class_<Dakota::LibraryEnvironment>(m, "LibEnv")
    .def(py::init
	 ([](const std::string& input_file,
	     const std::string& input_string)
	  {
	    assert(input_file.empty() || input_string.empty());

	    Dakota::ProgramOptions opts;
	    opts.echo_input(false);
	    opts.input_string(input_string);

	    return new Dakota::LibraryEnvironment(opts); 
	  })
	 , py::arg("input"), py::arg("input_string"))

    .def("execute", &Dakota::LibraryEnvironment::execute)
    .def("response_results", &Dakota::LibraryEnvironment::response_results)
    ;

  m.def("get_response_fn_val",
        &Dakota::python::get_response_fn_val,
	"Get final Response function value"
	);

}
