// Copyright (C) 2005, International Business Machines and others.
// All Rights Reserved.
// This code is published under the Common Public License.
//
// $Id$
//
// Authors:  Andreas Waechter              IBM    2005-08-04

#include "IpPDPerturbationHandler.hpp"

namespace Ipopt
{
  DBG_SET_VERBOSITY(0);

  PDPerturbationHandler::PDPerturbationHandler()
      :
      delta_xs_first_inc_fact_(100.),
      delta_xs_inc_fact_(8.),
      delta_xs_dec_fact_(1./3.),
      delta_xs_init_(1e-4),
      delta_cd_val_(1e-9),
      always_perturb_cd_(false),
      reset_last_(false),
      degen_iters_max_(3)
  {}

  void
  PDPerturbationHandler::RegisterOptions(SmartPtr<RegisteredOptions> roptions)
  {
    roptions->AddLowerBoundedNumberOption(
      "max_hessian_perturbation",
      "Maximum value of regularization parameter for handling negative curvature.",
      0, true,
      1e40,
      "In order to guarantee that the search directions are indeed proper "
      "descent directions, Ipopt requires that the inertia of the "
      "(augmented) linear system for the step computation has the "
      "correct number of negative and positive eigenvalues. The idea "
      "is that this guides the algorithm away from maximizers and makes "
      "Ipopt more likely converge to first order optimal points that "
      "are minimizers. If the inertia is not correct, a multiple of the "
      "identity matrix is added to the Hessian of the Lagrangian in the "
      "augmented system. This parameter gives the maximum value of the "
      "regularization parameter. If a regularization of that size is "
      "not enough, the algorithm skips this iteration and goes to the "
      "restoration phase. (This is delta_w^max in implementation paper.)");
    roptions->AddLowerBoundedNumberOption(
      "min_hessian_perturbation",
      "Smallest value by which Hessian block is perturbed.",
      0., false, 1e-20,
      "The size of the perturbation of the Hessian block is never chosen "
      "smaller than this value, unless no perturbation is necessary. (This "
      "is delta_w^min in implementation paper.)");
    roptions->AddLowerBoundedNumberOption(
      "perturb_inc_fact_first",
      "Increase factor for x-s perturbation for very first perturbation.",
      1., true, 100.,
      "The factor by which the perturbation is increased when a trial value "
      "was not sufficient - this value is used for the computation of the "
      "very first perturbation. (This is bar_kappa_w^+ in implementation "
      "paper.)");
    roptions->AddLowerBoundedNumberOption(
      "perturb_inc_fact",
      "Increase factor for x-s perturbation.",
      1., true, 8.,
      "The factor by which the perturbation is increased when a trial value "
      "was not sufficient - this value is used for the computation of "
      "later perturbations. (This is kappa_w^+ in implementation paper.)");
    roptions->AddBoundedNumberOption(
      "perturb_dec_fact",
      "Decrease factor for x-s perturbation.",
      0., true, 1., true, 1./3.,
      "The factor by which the perturbation is decreased when a trial value "
      "is deduced from the size of the most recent successful perturbation. "
      "(This is kappa_w^- in implementation paper.)");
    roptions->AddLowerBoundedNumberOption(
      "first_hessian_perturbation",
      "First size of first x-s perturbation tried.",
      0., true, 1e-4,
      "The first value for an x-s perturbation factors tried. "
      "(This is delta_0 in implementation paper.)");
    roptions->AddLowerBoundedNumberOption(
      "jacobian_regularization",
      "Size of the regularization for rank-deficient constraint Jacobians.",
      0., false, 1e-9,
      "(This is delta_c in implementation paper - assuming that kappa_c=0.)");
  }

  bool PDPerturbationHandler::InitializeImpl(const OptionsList& options,
      const std::string& prefix)
  {
    options.GetNumericValue("max_hessian_perturbation", delta_xs_max_, prefix);
    options.GetNumericValue("min_hessian_perturbation", delta_xs_min_, prefix);
    options.GetNumericValue("perturb_inc_fact_first", delta_xs_first_inc_fact_, prefix);
    options.GetNumericValue("perturb_inc_fact", delta_xs_inc_fact_, prefix);
    options.GetNumericValue("perturb_dec_fact", delta_xs_dec_fact_, prefix);
    options.GetNumericValue("first_hessian_perturbation", delta_xs_init_, prefix);
    options.GetNumericValue("jacobian_regularization", delta_cd_val_, prefix);

    hess_degenerate_ = NOT_YET_DETERMINED;
    jac_degenerate_ = NOT_YET_DETERMINED;
    degen_iters_ = 0;

    delta_x_curr_ = 0.;
    delta_s_curr_ = 0.;
    delta_c_curr_ = 0.;
    delta_d_curr_ = 0.;
    delta_x_last_ = 0.;
    delta_s_last_ = 0.;
    delta_c_last_ = 0.;
    delta_d_last_ = 0.;

    test_status_ = NO_TEST;

    return true;
  }

  bool
  PDPerturbationHandler::ConsiderNewSystem(Number& delta_x, Number& delta_s,
      Number& delta_c, Number& delta_d)
  {
    DBG_START_METH("PDPerturbationHandler::ConsiderNewSystem",dbg_verbosity);

    // Check if we can conclude that some components of the system are
    // structurally degenerate
    finalize_test();

    // Store the perturbation from the previous matrix
    if (reset_last_) {
      delta_x_last_ = delta_x_curr_;
      delta_s_last_ = delta_s_curr_;
      delta_c_last_ = delta_c_curr_;
      delta_d_last_ = delta_d_curr_;
    }
    else {
      if (delta_x_curr_ > 0.) {
        delta_x_last_ = delta_x_curr_;
      }
      if (delta_s_curr_ > 0.) {
        delta_s_last_ = delta_s_curr_;
      }
      if (delta_c_curr_ > 0.) {
        delta_c_last_ = delta_c_curr_;
      }
      if (delta_d_curr_ > 0.) {
        delta_d_last_ = delta_d_curr_;
      }
    }

    DBG_ASSERT((hess_degenerate_ != NOT_YET_DETERMINED ||
                jac_degenerate_ != DEGENERATE) &&
               (jac_degenerate_ != NOT_YET_DETERMINED ||
                hess_degenerate_ != DEGENERATE));

    if (hess_degenerate_ == NOT_YET_DETERMINED ||
        jac_degenerate_ == NOT_YET_DETERMINED) {
      test_status_ = TEST_DELTA_C_EQ_0_DELTA_X_EQ_0;
    }
    else {
      test_status_ = NO_TEST;
    }

    if (jac_degenerate_ == DEGENERATE) {
      delta_c = delta_c_curr_ = delta_cd_val_;
      IpData().Append_info_string("l");
    }
    else {
      delta_c = delta_c_curr_ = 0.;
    }
    delta_d = delta_d_curr_ = delta_c;

    if (hess_degenerate_ == DEGENERATE) {
      PerturbForWrongInertia(delta_x, delta_s,
                             delta_c, delta_d);
    }
    else {
      delta_x = 0.;
      delta_s = delta_x;
    }

    delta_x_curr_ = delta_x;
    delta_s_curr_ = delta_s;
    delta_c_curr_ = delta_c;
    delta_d_curr_ = delta_d;

    IpData().Set_info_regu_x(delta_x);

    return true;
  }

  bool
  PDPerturbationHandler::PerturbForSingularity(
    Number& delta_x, Number& delta_s,
    Number& delta_c, Number& delta_d)
  {
    DBG_START_METH("PDPerturbationHandler::PerturbForSingularity",
                   dbg_verbosity);

    // Check for structural degeneracy
    if (hess_degenerate_ == NOT_YET_DETERMINED ||
        jac_degenerate_ == NOT_YET_DETERMINED) {
      switch (test_status_) {
        case TEST_DELTA_C_EQ_0_DELTA_X_EQ_0:
        DBG_ASSERT(delta_x_curr_ == 0. && delta_c_curr_ == 0.);
        // in this case we haven't tried anything for this matrix yet
        if (jac_degenerate_ == NOT_YET_DETERMINED) {
          delta_d_curr_ = delta_c_curr_ = delta_cd_val_;
          test_status_ = TEST_DELTA_C_GT_0_DELTA_X_EQ_0;
        }
        else {
          DBG_ASSERT(hess_degenerate_ == NOT_YET_DETERMINED);
          PerturbForWrongInertia(delta_x, delta_s,
                                 delta_c, delta_d);
          DBG_ASSERT(delta_c = delta_cd_val_ && delta_d == delta_cd_val_);
          test_status_ = TEST_DELTA_C_EQ_0_DELTA_X_GT_0;
        }
        break;
        case TEST_DELTA_C_GT_0_DELTA_X_EQ_0:
        DBG_ASSERT(delta_x_curr_ > 0. && delta_c_curr_ == 0.);
        DBG_ASSERT(jac_degenerate_ == NOT_YET_DETERMINED);
        delta_d_curr_ = delta_c_curr_ = 0.;
        PerturbForWrongInertia(delta_x, delta_s,
                               delta_c, delta_d);
        DBG_ASSERT(delta_c = 0. && delta_d == 0.);
        test_status_ = TEST_DELTA_C_EQ_0_DELTA_X_GT_0;
        break;
        case TEST_DELTA_C_EQ_0_DELTA_X_GT_0:
        DBG_ASSERT(delta_x_curr_ > 0. && delta_c_curr_ == 0.);
        delta_d_curr_ = delta_c_curr_ = delta_cd_val_;
        PerturbForWrongInertia(delta_x, delta_s,
                               delta_c, delta_d);
        test_status_ = TEST_DELTA_C_GT_0_DELTA_X_GT_0;
        break;
        case TEST_DELTA_C_GT_0_DELTA_X_GT_0:
        PerturbForWrongInertia(delta_x, delta_s,
                               delta_c, delta_d);
        break;
        case NO_TEST:
        DBG_ASSERT(false && "we should not get here.");
      }
    }
    else {
      if (delta_c_curr_ > 0.) {
        // If we already used a perturbation for the constraints, we do
        // the same thing as if we were encountering negative curvature
        PerturbForWrongInertia(delta_x, delta_s,
                               delta_c, delta_d);
      }
      else {
        // Otherwise we now perturb the lower right corner
        delta_d_curr_ = delta_c_curr_ = delta_cd_val_;

        // ToDo - also perturb Hessian?
        IpData().Append_info_string("L");
      }
    }

    delta_x = delta_x_curr_;
    delta_s = delta_s_curr_;
    delta_c = delta_c_curr_;
    delta_d = delta_d_curr_;

    IpData().Set_info_regu_x(delta_x);

    return true;
  }

  bool
  PDPerturbationHandler::PerturbForWrongInertia(
    Number& delta_x, Number& delta_s,
    Number& delta_c, Number& delta_d)
  {
    DBG_START_METH("PDPerturbationHandler::PerturbForWrongInertia",
                   dbg_verbosity);

    // Check if we can conclude that components of the system are
    // structurally degenerate (we only get here if the most recent
    // perturbation for a test did not result in a singular system)
    finalize_test();

    if (delta_x_curr_ == 0.) {
      if (delta_x_last_ == 0.) {
        delta_x_curr_ = delta_xs_init_;
      }
      else {
        delta_x_curr_ = Max(delta_xs_min_,
                            delta_x_last_*delta_xs_dec_fact_);
      }
    }
    else {
      if (delta_x_last_ == 0. || 1e5*delta_x_last_<delta_x_curr_) {
        delta_x_curr_ = delta_xs_first_inc_fact_*delta_x_curr_;
      }
      else {
        delta_x_curr_ = delta_xs_inc_fact_*delta_x_curr_;
      }
    }
    if (delta_x_curr_ > delta_xs_max_) {
      // Give up trying to solve the linear system
      return false;
    }

    delta_s_curr_ = delta_x_curr_;

    delta_x = delta_x_curr_;
    delta_s = delta_s_curr_;
    delta_c = delta_c_curr_;
    delta_d = delta_d_curr_;

    IpData().Set_info_regu_x(delta_x);

    return true;
  }

  void
  PDPerturbationHandler::CurrentPerturbation(
    Number& delta_x, Number& delta_s,
    Number& delta_c, Number& delta_d)
  {
    delta_x = delta_x_curr_;
    delta_s = delta_s_curr_;
    delta_c = delta_c_curr_;
    delta_d = delta_d_curr_;
  }

  void
  PDPerturbationHandler::finalize_test()
  {
    switch (test_status_) {
      case NO_TEST:
      return;
      break;
      case TEST_DELTA_C_EQ_0_DELTA_X_EQ_0:
      if (hess_degenerate_ == NOT_YET_DETERMINED &&
          jac_degenerate_ == NOT_YET_DETERMINED) {
        hess_degenerate_ = NOT_DEGENERATE;
        jac_degenerate_ = NOT_DEGENERATE;
        IpData().Append_info_string("Nhj ");
      }
      else if (hess_degenerate_ == NOT_YET_DETERMINED) {
        hess_degenerate_ = NOT_DEGENERATE;
        IpData().Append_info_string("Nh ");
      }
      else if (jac_degenerate_ == NOT_YET_DETERMINED) {
        jac_degenerate_ = NOT_DEGENERATE;
        IpData().Append_info_string("Nj ");
      }
      break;
      case TEST_DELTA_C_GT_0_DELTA_X_EQ_0:
      if (hess_degenerate_ == NOT_YET_DETERMINED) {
        hess_degenerate_ = NOT_DEGENERATE;
        IpData().Append_info_string("Nh ");
      }
      if (jac_degenerate_ == NOT_YET_DETERMINED) {
        degen_iters_++;
        if (degen_iters_ >= degen_iters_max_) {
          jac_degenerate_ = DEGENERATE;
          IpData().Append_info_string("Dj ");
        }
        IpData().Append_info_string("L");
      }
      break;
      case TEST_DELTA_C_EQ_0_DELTA_X_GT_0:
      if (jac_degenerate_ == NOT_YET_DETERMINED) {
        jac_degenerate_ = NOT_DEGENERATE;
        IpData().Append_info_string("Nj ");
      }
      if (hess_degenerate_ == NOT_YET_DETERMINED) {
        degen_iters_++;
        if (degen_iters_ >= degen_iters_max_) {
          hess_degenerate_ = DEGENERATE;
          IpData().Append_info_string("Dh ");
        }
      }
      break;
      case TEST_DELTA_C_GT_0_DELTA_X_GT_0:
      degen_iters_++;
      if (degen_iters_ >= degen_iters_max_) {
        hess_degenerate_ = DEGENERATE;
        jac_degenerate_ = DEGENERATE;
        IpData().Append_info_string("Dhj ");
      }
      IpData().Append_info_string("L");
      break;
    }
  }

} // namespace Ipopt
