// Copyright (C) 2021 COIN-OR Foundation
// All Rights Reserved.
// This code is published under the Eclipse Public License.

// get active asserts also if NDEBUG is defined
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "IpIpoptApplication.hpp"
#include "IpSolveStatistics.hpp"
#include "IpTNLP.hpp"

#include <iostream>
#include <cassert>
#include <cmath>

using namespace Ipopt;

#ifdef IPOPT_SINGLE
#define TESTTOL 1e-6
#else
#define TESTTOL 1e-9
#endif
#define ASSERTEQ(val1, val2) \
   do if( fabs((val1)-(val2)) > TESTTOL*std::max(1.0,std::max(fabs(val1),fabs(val2))) ) \
   { \
      fprintf(stderr, "Wrong %s = %.12g, expected %s = %.12g\n", #val1, val1, #val2, val2); \
      abort(); \
   } while (false)

/** NLP to test TNLP::get_current_iterate() and TNLP::get_current_violations()
 *
 * min x1 + x2 + x3
 * s.t. 1 <= x1^2 + x2^2 + x3^2 <= 2
 *      x1^2 - x3^2 = 0.5
 *      x1 >= -10
 *      x2 = 1
 */
class TestNLP: public TNLP
{
private:
   /// whether fixed variables are treated as constraints
   bool fixedvar_makeconstr_;

public:
   /** constructor */
   TestNLP(
      bool fixedvar_makeconstr
   )
   : fixedvar_makeconstr_(fixedvar_makeconstr)
   { }

   /** Method to return some info about the nlp */
   bool get_nlp_info(
      Index&          n,
      Index&          m,
      Index&          nnz_jac_g,
      Index&          nnz_h_lag,
      IndexStyleEnum& index_style
   )
   {
      n = 3;
      m = 2;
      nnz_jac_g = 5;
      nnz_h_lag = 3;
      index_style = C_STYLE;

      return true;
   }

   /** Method to return the bounds for my problem */
   bool get_bounds_info(
      Index   n,
      Number* x_l,
      Number* x_u,
      Index   m,
      Number* g_l,
      Number* g_u
   )
   {
      assert(n == 3);
      assert(m == 2);

      x_l[0] = -10.0;
      x_u[0] = 1e300;

      x_l[1] = 1.0;
      x_u[1] = 1.0;

      x_l[2] = -1e300;
      x_u[2] = 1e300;

      g_l[0] = 1.0;
      g_u[0] = 2.0;

      g_l[1] = 0.5;
      g_u[1] = 0.5;

      return true;
   }

   /** Method to return the starting point for the algorithm */
   bool get_starting_point(
      Index   n,
      bool    init_x,
      Number* x,
      bool    init_z,
      Number* ,
      Number* ,
      Index   ,
      bool    init_lambda,
      Number*
   )
   {
      assert(n == 3);

      if( init_x )
      {
         x[0] = 1.0;
         x[1] = 1.0;
         x[2] = -1.0;
      }

      assert(!init_z);
      assert(!init_lambda);

      return true;
   }

   /** Method to return the objective value */
   bool eval_f(
      Index         n,
      const Number* x,
      bool          ,
      Number&       obj_value
   )
   {
      assert(n == 3);

      obj_value = x[0] + x[1] + x[2];

      return true;
   }

   /** Method to return the gradient of the objective */
   bool eval_grad_f(
      Index         n,
      const Number* ,
      bool          ,
      Number*       grad_f
   )
   {
      assert(n == 3);

      grad_f[0] = 1.0;
      grad_f[1] = 1.0;
      grad_f[2] = 1.0;

      return true;
   }

   /** Method to return the constraint residuals */
   bool eval_g(
      Index         n,
      const Number* x,
      bool          ,
      Index         m,
      Number*       g
   )
   {
      assert(n == 3);
      assert(m == 2);

      g[0] = x[0]*x[0] + x[1]*x[1] + x[2]*x[2];
      g[1] = x[0]*x[0] - x[2]*x[2];

      return true;
   }

   /** Method to return:
    *   1) The structure of the Jacobian (if "values" is NULL)
    *   2) The values of the Jacobian (if "values" is not NULL)
    */
   bool eval_jac_g(
      Index         n,
      const Number* x,
      bool          ,
      Index         m,
      Index         nele_jac,
      Index*        iRow,
      Index*        jCol,
      Number*       values
   )
   {
      assert(n == 3);
      assert(m == 2);
      assert(nele_jac == 5);
      assert((iRow != NULL) == (jCol != NULL));
      assert((iRow != NULL) == (values == NULL));

      if( iRow != NULL )
      {
         iRow[0] = 0;
         jCol[0] = 0;

         iRow[1] = 0;
         jCol[1] = 1;

         iRow[2] = 0;
         jCol[2] = 2;

         iRow[3] = 1;
         jCol[3] = 0;

         iRow[4] = 1;
         jCol[4] = 2;
      }
      else
      {
         values[0] = 2*x[0];
         values[1] = 2*x[1];
         values[2] = 2*x[2];
         values[3] = 2*x[0];
         values[4] = -2*x[2];
      }

      return true;
   }

   /** Method to return:
    *   1) The structure of the Hessian of the Lagrangian (if "values" is NULL)
    *   2) The values of the Hessian of the Lagrangian (if "values" is not NULL)
    */
   bool eval_h(
      Index         n,
      const Number* ,
      bool          ,
      Number        ,
      Index         m,
      const Number* lambda,
      bool          ,
      Index         nele_hess,
      Index*        iRow,
      Index*        jCol,
      Number*       values
   )
   {
      assert(n == 3);
      assert(m == 2);
      assert(nele_hess == 3);
      assert((iRow != NULL) == (jCol != NULL));
      assert((iRow != NULL) == (values == NULL));

      if( iRow != NULL )
      {
         iRow[0] = 0;
         jCol[0] = 0;

         iRow[1] = 1;
         jCol[1] = 1;

         iRow[2] = 2;
         jCol[2] = 2;
      }
      else
      {
         values[0] = 2*lambda[0] + 2*lambda[1];
         values[1] = 2*lambda[0];
         values[2] = 2*lambda[0] - 2*lambda[1];
      }

      return true;
   }

   /** This method is called when the algorithm is complete so the TNLP can store/write the solution */
   void finalize_solution(
      SolverReturn               ,
      Index                      n,
      const Number*              x,
      const Number*              z_L,
      const Number*              z_U,
      Index                      m,
      const Number*              g,
      const Number*              lambda,
      Number                     ,
      const IpoptData*           ip_data,
      IpoptCalculatedQuantities* ip_cq
   )
   {
      std::cout << "x =";
      for( int i = 0; i < n; ++i )
         std::cout << ' ' << x[i];
      std::cout << std::endl;
      std::cout << "z_L =";
      for( int i = 0; i < n; ++i )
         std::cout << ' ' << z_L[i];
      std::cout << std::endl;
      std::cout << "z_U =";
      for( int i = 0; i < n; ++i )
         std::cout << ' ' << z_U[i];
      std::cout << std::endl;
      std::cout << "lambda =";
      for( int i = 0; i < m; ++i )
         std::cout << ' ' << lambda[i];
      std::cout << std::endl;

      // check that get_curr_iterate() returns the same point
      Number curr_x[3];
      Number curr_z_L[3];
      Number curr_z_U[3];
      Number curr_g[3];
      Number curr_lambda[3];
      assert(get_curr_iterate(ip_data, ip_cq, n, curr_x, curr_z_L, curr_z_U, m, curr_g, curr_lambda));

      ASSERTEQ(curr_x[0], x[0]);
      ASSERTEQ(curr_x[1], x[1]);
      ASSERTEQ(curr_x[2], x[2]);
      ASSERTEQ(curr_z_L[0], z_L[0]);
      ASSERTEQ(curr_z_L[1], z_L[1]);
      ASSERTEQ(curr_z_L[2], z_L[2]);
      ASSERTEQ(curr_z_U[0], z_U[0]);
      ASSERTEQ(curr_z_U[1], z_U[1]);
      ASSERTEQ(curr_z_U[2], z_U[2]);
      ASSERTEQ(curr_g[0], g[0]);
      ASSERTEQ(curr_g[1], g[1]);
      ASSERTEQ(curr_lambda[0], lambda[0]);
      ASSERTEQ(curr_lambda[1], lambda[1]);

      // run checks on current point and violations
      intermediate_callback(RegularMode, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, ip_data, ip_cq);
   }

   bool intermediate_callback(
      AlgorithmMode              mode,
      Index                      ,
      Number                     ,
      Number                     ,
      Number                     ,
      Number                     ,
      Number                     ,
      Number                     ,
      Number                     ,
      Number                     ,
      Index                      ,
      const IpoptData*           ip_data,
      IpoptCalculatedQuantities* ip_cq
   )
   {
      Number x[3];
      Number z_L[3] = { 17, 17, 17 };
      Number z_U[3] = { 19, 19, 19 };
      Number compl_x_L[3] = { 42, 42, 42 };
      Number compl_x_U[3] = { 42, 42, 42 };
      Number grad_lag_x[3] = { 42, 42, 42 };

      Number g[2];
      Number lambda[2];
      Number constraint_violation[2] = { 42, 42 };
      Number compl_g[2] = { 42, 42 };

      bool have_iter = get_curr_iterate(ip_data, ip_cq, 3, x, z_L, z_U, 2, g, lambda);
      bool have_viol = get_curr_violations(ip_data, ip_cq, false, 3, compl_x_L, compl_x_U, grad_lag_x, 2, constraint_violation, compl_g);

      assert(have_iter);
      assert(have_viol);

      printf("Current iterate (%s mode):\n", mode == RegularMode ? "regular" : "restoration");
      printf("  %-12s %-12s %-12s %-12s %-12s %-12s\n", "x", "z_L", "z_U", "compl_x_L", "compl_x_U", "grad_lag_x");
      for( int i = 0; i < 3; ++i )
         printf("  %-12g %-12g %-12g %-12g %-12g %-12g\n", x[i], z_L[i], z_U[i], compl_x_L[i], compl_x_U[i], grad_lag_x[i]);

      printf("  %-12s %-12s %-12s %-12s\n", "g(x)", "lambda", "constr_viol", "compl_g");
      for( int i = 0; i < 2; ++i )
         printf("  %-12g %-12g %-12g %-12g\n", g[i], lambda[i], constraint_violation[i], compl_g[i]);

      // check activity
      ASSERTEQ(g[0], x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
      ASSERTEQ(g[1], x[0]*x[0] - x[2]*x[2]);

      // check complementarity for variable bounds
      ASSERTEQ(compl_x_L[0], z_L[0] * (x[0] + 10.0));
      ASSERTEQ(compl_x_U[0], 0.0);
      ASSERTEQ(compl_x_L[1], z_L[1] * (x[1] - 1.0));
      ASSERTEQ(compl_x_U[1], z_U[1] * (x[1] - 1.0));
      ASSERTEQ(compl_x_L[2], 0.0);
      ASSERTEQ(compl_x_U[2], 0.0);

      // check gradient on Lagrangian
      if( mode == RegularMode )
      {
         ASSERTEQ(grad_lag_x[0], 1.0 + lambda[0] * 2*x[0] + lambda[1] * 2*x[0] - z_L[0]);
         if( fixedvar_makeconstr_ )
            ASSERTEQ(grad_lag_x[1], 1.0 + lambda[0] * 2*x[1] - z_U[1] + z_U[1]);
         else
            ASSERTEQ(grad_lag_x[1], 0.0);
         ASSERTEQ(grad_lag_x[2], 1.0 + lambda[0] * 2*x[2] - lambda[1] * 2*x[2]);
      }
      else
      {
         ASSERTEQ(grad_lag_x[0], 0.0);
         ASSERTEQ(grad_lag_x[1], 0.0);
         ASSERTEQ(grad_lag_x[2], 0.0);
      }

      // check constraint violation
      ASSERTEQ(constraint_violation[0], std::max(0.0, std::max(1.0 - g[0], g[0] - 2.0)));
      ASSERTEQ(constraint_violation[1], std::max(0.0, fabs(g[1]-0.5)));

      ASSERTEQ(compl_g[0], (g[0] - 1.0) * std::max(Number(0.0), -lambda[0]) + (2.0 - g[0]) * std::max(Number(0.0), lambda[0]));
      ASSERTEQ(compl_g[1], -(g[1] - 0.5) * lambda[1]);

      return true;
   }
};

bool run(
   bool fixedvar_makeconstr,
   bool start_resto
   )
{
   // Create an instance of your nlp...
   SmartPtr<TNLP> nlp = new TestNLP(fixedvar_makeconstr);

   // Create an instance of the IpoptApplication
   SmartPtr<IpoptApplication> app = new IpoptApplication();

   // Initialize the IpoptApplication and process the options
   ApplicationReturnStatus status;
   status = app->Initialize();
   if( status != Solve_Succeeded )
   {
      std::cout << std::endl << std::endl << "*** Error during initialization!" << std::endl;
      return false;
   }

   app->Options()->SetStringValue("print_user_options", "yes", true, true);
   app->Options()->SetNumericValue("bound_relax_factor", 0.0);
   app->Options()->SetIntegerValue("print_level", 4);

   if( fixedvar_makeconstr )
      app->Options()->SetStringValue("fixed_variable_treatment", "make_constraint");

   if( start_resto )
      app->Options()->SetStringValue("start_with_resto", "yes");

   status = app->OptimizeTNLP(nlp);

   return EXIT_SUCCESS;
}

int main(
   int,
   char**
)
{
   if( run(false, false) != EXIT_SUCCESS )
      return EXIT_FAILURE;

   if( run(true, false) != EXIT_SUCCESS )
      return EXIT_FAILURE;

   if( run(false, true) != EXIT_SUCCESS )
      return EXIT_FAILURE;

   if( run(true, true) != EXIT_SUCCESS )
      return EXIT_FAILURE;

   std::cout << std::endl << "*** All tests passed" << std::endl;

   return EXIT_SUCCESS;
}