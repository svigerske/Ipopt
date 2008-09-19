// Copyright (C) 2007 Peter Carbonetto. All Rights Reserved.
// This code is published under the Common Public License.
//
// Author: Peter Carbonetto
//         Dept. of Computer Science
//         University of British Columbia
//         August 25, 2008

#include "mex.h"
#include "iterate.h"
#include "options.h"
#include "matlabexception.h"
#include "callbackfunctions.h"
#include "matlabinfo.h"
#include "matlabjournal.h"
#include "matlabprogram.h"
#include "coin/IpRegOptions.hpp"
#include "coin/IpJournalist.hpp"
#include "coin/IpIpoptApplication.hpp"

using Ipopt::IsValid;
using Ipopt::RegisteredOption;
using Ipopt::EJournalLevel;
using Ipopt::Journal;
using Ipopt::MatlabJournal;
using Ipopt::IpoptApplication;
using Ipopt::SmartPtr;
using Ipopt::TNLP;
using Ipopt::ApplicationReturnStatus;

extern void _main();

// Function definitions.
// -----------------------------------------------------------------
void mexFunction (int nlhs, mxArray *plhs[], 
		  int nrhs, const mxArray *prhs[]) 
  try {

    // Check to see if we have the correct number of input and output
    // arguments.
    if (nrhs != 3)
      throw MatlabException("Incorrect number of input arguments");
    if (nlhs != 2)
      throw MatlabException("Incorrect number of output arguments");

    // Get the first input which specifies the initial iterate.
    Iterate x0(mxDuplicateArray(prhs[0]));

    // Get the second input which specifies the callback functions.
    CallbackFunctions funcs(prhs[1]);

    // Create a new IPOPT application object and process the options.
    IpoptApplication app(false);
    Options          options(x0,app,prhs[2]);

    // The first output argument is the value of the optimization
    // variables obtained at the solution.
    plhs[0] = mxDuplicateArray(x0);
    Iterate x(plhs[0]);

    // The second output argument stores other information, such as
    // the exit status, the value of the Lagrange multipliers upon
    // termination, the final state of the auxiliary data, and so on.
    MatlabInfo info(plhs[1]);

    // Check to see whether the user provided a callback function for
    // computing the Hessian. This is not needed in the special case
    // when a quasi-Newton approximation to the Hessian is being used.
    if (!options.ipoptOptions().useQuasiNewton() && 
	!funcs.hessianFuncIsAvailable())
      throw MatlabException("You must supply a callback function for \
computing the Hessian unless you decide to um knse a quasi-Newton \
approximation to the Hessian");

    // If the user tried to use her own scaling, report an error.
    if (options.ipoptOptions().userScaling())
      throw MatlabException("The user-defined scaling option does not \
work in the MATLAB interface for IPOPT");

    // If the user supplied initial values for the Lagrange
    // multipliers, activate the "warm start" option in IPOPT.
    if (options.multlb() && options.multub() && options.multconstr())
      app.Options()->SetStringValue("warm_start_init_point","yes");

    // Set up the IPOPT console.
    EJournalLevel printLevel = (EJournalLevel) 
      options.ipoptOptions().printLevel();
    SmartPtr<Journal> console = new MatlabJournal(printLevel);
    app.Jnlst()->AddJournal(console);

    // Intialize the IpoptApplication object and process the options.
    ApplicationReturnStatus exitstatus;
    exitstatus = app.Initialize();
    if (exitstatus != Ipopt::Solve_Succeeded)
      throw MatlabException("IPOPT solver initialization failed");

    // Create a new instance of the constrained, nonlinear program.
    MatlabProgram* matlabProgram 
      = new MatlabProgram(x0,funcs,options,x,options.getAuxData(),info);
    SmartPtr<TNLP> program = matlabProgram;

    // Ask Ipopt to solve the problem.
    exitstatus = app.OptimizeTNLP(program);
    info.setExitStatus(exitstatus);

    // Free the dynamically allocated memory.
    mxDestroyArray(x0);

//     // OLD STUFF
//     // ---------
//     // If requested, create the output that will store the values of
//     // the multipliers obtained when IPOPT converges to a stationary
//     // point. This output is a MATLAB structure with three fields, two
//     // for the final values of the upper and lower bound multipliers,
//     // and one for the constraint multipliers.
//     Multipliers* multipliers = 0;
//     if (nlhs > l)
//       multipliers = new Multipliers(plhs[l++],x.numelems(),
// 				    constraintlb.length());

//     // Get the iterative callback function.
//     MatlabFunctionHandle* iterFunc = new MatlabFunctionHandle();
//     ptr = prhs[k++];
//     if (nrhs > 11)
//       if (!mxIsEmpty(ptr)) {
// 	delete iterFunc;
// 	iterFunc = new MatlabFunctionHandle(ptr);
//       }

//     // Get the initial Lagrange multipliers, if provided.
//     Multipliers* initialMultipliers = 0;
//     ptr = prhs[k++];
//     app.Options()->SetStringValue("warm_start_init_point","no");
//     if (nrhs > 12) 
//       if (!mxIsEmpty(ptr)) {
// 	initialMultipliers = new Multipliers(ptr);

// 	// Notify the IPOPT algorithm that we will provide our own
// 	// values for the initial Lagrange multipliers.
// 	app.Options()->SetStringValue("warm_start_init_point","yes");
//       }

  } catch (std::exception& error) {
    mexErrMsgTxt(error.what());
  }
