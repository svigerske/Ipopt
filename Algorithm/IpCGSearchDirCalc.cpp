// Copyright (C) 2005 International Business Machines and others.
// All Rights Reserved.
// This code is published under the Common Public License.
//
// $Id$
//
// Authors:  Andreas Waechter                 IBM    2005-10-13
//               derived from IpIpoptAlg.cpp

#include "IpCGSearchDirCalc.hpp"

namespace Ipopt
{

#ifdef IP_DEBUG
  static const Index dbg_verbosity = 0;
#endif

  CGSearchDirCalculator::CGSearchDirCalculator(const SmartPtr<PDSystemSolver>& pd_solver)
      :
      pd_solver_(pd_solver)
  {
    DBG_START_FUN("CGSearchDirCalculator::CGSearchDirCalculator",
                  dbg_verbosity);
    DBG_ASSERT(IsValid(pd_solver_));
  }

  CGSearchDirCalculator::~CGSearchDirCalculator()
  {
    DBG_START_FUN("CGSearchDirCalculator::~CGSearchDirCalculator()",
                  dbg_verbosity);
  }

  void CGSearchDirCalculator::RegisterOptions(SmartPtr<RegisteredOptions> roptions)
  {
    roptions->AddLowerBoundedNumberOption(
      "penalty_init_min",
      "Minimal value for the intial penalty parameter (for Chen-Goldfarb line search).",
      0, true, 1e-3,
      "");
    roptions->AddLowerBoundedNumberOption(
      "penalty_init_max",
      "Maximal value for the intial penalty parameter (for Chen-Goldfarb line search).",
      0, true, 1.,
      "");
  }

  bool CGSearchDirCalculator::InitializeImpl(const OptionsList& options,
      const std::string& prefix)
  {
    options.GetNumericValue("penalty_init_min", penalty_init_min_, prefix);
    options.GetNumericValue("penalty_init_max", penalty_init_max_, prefix);

    return pd_solver_->Initialize(Jnlst(), IpNLP(), IpData(), IpCq(),
                                  options, prefix);
  }

  void CGSearchDirCalculator::ComputeSearchDirection()
  {
    DBG_START_METH("CGSearchDirCalculator::ComputeSearchDirection",
                   dbg_verbosity);

    bool improve_solution = false;

    // So far, the adaptive mu strategies do not yet work with the
    // penalty function line search
    DBG_ASSERT(!IpData().FreeMuMode());

    SmartPtr<IteratesVector> rhs = IpData().curr()->MakeNewContainer();
    rhs->Set_x(*IpCq().curr_grad_lag_with_damping_x());
    rhs->Set_s(*IpCq().curr_grad_lag_with_damping_s());
    rhs->Set_z_L(*IpCq().curr_relaxed_compl_x_L());
    rhs->Set_z_U(*IpCq().curr_relaxed_compl_x_U());
    rhs->Set_v_L(*IpCq().curr_relaxed_compl_s_L());
    rhs->Set_v_U(*IpCq().curr_relaxed_compl_s_U());

    if (!IpData().PenaltyInitialized()) {
      Number penalty_init = Max(penalty_init_max_,
                                Min(Max(IpData().curr()->y_c()->Amax(),
                                        IpData().curr()->y_d()->Amax()),
                                    penalty_init_min_));
      IpData().Set_penalty(penalty_init);
      char spen[40];
      sprintf(spen, " penaltyinit=%8.2e", penalty_init);
      IpData().Append_info_string(spen);
    }

    Number c_over_r = IpCq().curr_cg_pert_fact();
    SmartPtr<Vector> rhs_c = IpData().curr()->y_c()->MakeNew();
    rhs_c->AddTwoVectors(1., *IpCq().curr_c(),
                         -c_over_r, *IpData().curr()->y_c(), 0.);
    rhs->Set_y_c(*rhs_c);
    SmartPtr<Vector> rhs_d = IpData().curr()->y_d()->MakeNew();
    rhs_d->AddTwoVectors(1., *IpCq().curr_d_minus_s(),
                         -c_over_r, *IpData().curr()->y_d(), 0.);
    rhs->Set_y_d(*rhs_d);

    DBG_PRINT_VECTOR(2, "rhs_cgpen", *rhs);

    // Get space for the search direction
    SmartPtr<IteratesVector> delta =
      IpData().curr()->MakeNewIteratesVector(true);

    bool allow_inexact = false;
    pd_solver_->Solve(-1.0, 0.0, *rhs, *delta, allow_inexact,
                      improve_solution);

    // Store the original search direction in the IpData object
    IpData().set_delta(delta);

    // Now solve it again but for the "fast" search direction
    rhs->Set_y_c(*IpCq().curr_c());
    rhs->Set_y_d(*IpCq().curr_d_minus_s());

    // Get space for the search direction
    SmartPtr<IteratesVector> delta_fast =
      IpData().curr()->MakeNewIteratesVector(true);

    allow_inexact = false;
    pd_solver_->Solve(-1.0, 0.0, *rhs, *delta_fast, allow_inexact,
                      improve_solution);

    // Store the fast search direction in the IpData object
    IpData().set_delta_cgpen(delta_fast);
    IpData().SetHaveCgPenDeltas(true);

    // Now we check whether the fast direction is good compatible with
    // the merit function

    bool keep_fast_delta = true;
    // do the || tilde y - hat y ||_2 <= k_dis ||hat y||_2 test
    SmartPtr<const Vector> y_c = IpData().curr()->y_c();
    SmartPtr<const Vector> y_d = IpData().curr()->y_d();
    SmartPtr<const Vector> delta_fast_y_c = IpData().delta_cgpen()->y_c();
    SmartPtr<const Vector> delta_fast_y_d = IpData().delta_cgpen()->y_d();
    SmartPtr<const Vector> delta_y_c = IpData().delta()->y_c();
    SmartPtr<const Vector> delta_y_d = IpData().delta()->y_d();

    Number hat_y_nrm = sqrt(pow(y_c->Nrm2(), 2.) +
                            pow(y_d->Nrm2(), 2.) +
                            2.*y_c->Dot(*delta_y_c) +
                            2.*y_d->Dot(*delta_y_d) +
                            pow(delta_y_c->Nrm2(), 2.) +
                            pow(delta_y_d->Nrm2(), 2.));
    Number diff_y = sqrt(pow(delta_fast_y_c->Nrm2(), 2.) +
                         pow(delta_fast_y_d->Nrm2(), 2.) -
                         2.*delta_y_c->Dot(*delta_fast_y_c) -
                         2.*delta_y_d->Dot(*delta_fast_y_d) +
                         pow(delta_y_c->Nrm2(), 2.) +
                         pow(delta_y_d->Nrm2(), 2.));

    Number kappa_dis_ = 1e1;
    if (diff_y > kappa_dis_*hat_y_nrm) {
      keep_fast_delta = false;
      IpData().Append_info_string("G");
    }

    if (keep_fast_delta) {
      // For now, I just check if the directional derivative for the
      // penalty functions are not too much off
      Number direct_deriv = IpCq().curr_direct_deriv_penalty_function();
      Number fast_direct_deriv = IpCq().curr_fast_direct_deriv_penalty_function();
      Jnlst().Printf(J_MOREDETAILED, J_LINE_SEARCH,
                     "direct_deriv = %23.16e  fast_direct_deriv = %23.15e\n",
                     direct_deriv, fast_direct_deriv);
      Number need_name_ = 1e-1;
      if (fast_direct_deriv > need_name_*direct_deriv) {
        // Discard the fast direction
        //delta_fast = NULL;
        //IpData().set_delta_cgpen(delta_fast);
        keep_fast_delta = false;
        IpData().Append_info_string("g");
      }
    }

    if (!keep_fast_delta) {
      IpData().SetHaveCgPenDeltas(false);
    }

  }

} // namespace Ipopt
