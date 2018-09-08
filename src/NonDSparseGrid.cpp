/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:	 NonDSparseGrid
//- Description: Implementation code for NonDSparseGrid class
//- Owner:       Mike Eldred
//- Revised by:  
//- Version:

#include "dakota_data_io.hpp"
#include "dakota_system_defs.hpp"
#include "NonDSparseGrid.hpp"
#include "IncrementalSparseGridDriver.hpp"
#include "HierarchSparseGridDriver.hpp"
#include "DakotaModel.hpp"
#include "DiscrepancyCorrection.hpp"
#include "ProblemDescDB.hpp"
#include "PolynomialApproximation.hpp"

static const char rcsId[]="@(#) $Id: NonDSparseGrid.cpp,v 1.57 2004/06/21 19:57:32 mseldre Exp $";

//#define DEBUG

namespace Dakota {


/** This constructor is called for a standard letter-envelope iterator
    instantiation.  In this case, set_db_list_nodes has been called
    and probDescDB can be queried for settings from the method
    specification.  It is not currently used, as there is not a
    separate sparse_grid method specification. */
NonDSparseGrid::NonDSparseGrid(ProblemDescDB& problem_db, Model& model):
  NonDIntegration(problem_db, model),  
  ssgLevelSpec(probDescDB.get_ushort("method.nond.sparse_grid_level"))
{
  short exp_basis_type
    = probDescDB.get_short("method.nond.expansion_basis_type");
  short refine_type
    = probDescDB.get_short("method.nond.expansion_refinement_type");
  short refine_control
    = probDescDB.get_short("method.nond.expansion_refinement_control");
  if (exp_basis_type == Pecos::HIERARCHICAL_INTERPOLANT)
    ssgDriverType = Pecos::HIERARCHICAL_SPARSE_GRID;
  else
    ssgDriverType = (refine_control) ? Pecos::INCREMENTAL_SPARSE_GRID
                                     : Pecos::COMBINED_SPARSE_GRID;

  // initialize the numerical integration driver
  numIntDriver = Pecos::IntegrationDriver(ssgDriverType);
  ssgDriver = (Pecos::SparseGridDriver*)numIntDriver.driver_rep();

  // initialize_random_variables() called in NonDIntegration ctor
  check_variables(natafTransform.x_random_variables());

  // define ExpansionConfigOptions
  Pecos::ExpansionConfigOptions ec_options(ssgDriverType, exp_basis_type,
    iteratedModel.correction_type(),
    probDescDB.get_short("method.nond.multilevel_discrepancy_emulation"),
    outputLevel, probDescDB.get_bool("method.variance_based_decomp"),
    probDescDB.get_ushort("method.nond.vbd_interaction_order"), //refine_type,
    refine_control, //maxIterations,
    probDescDB.get_int("method.nond.max_refinement_iterations"),
    probDescDB.get_int("method.nond.max_solver_iterations"), convergenceTol,
    probDescDB.get_ushort("method.soft_convergence_limit"));

  // define BasisConfigOptions
  bool nested_rules = (probDescDB.get_short("method.nond.nesting_override")
		       != Pecos::NON_NESTED);
  bool piecewise_basis = (probDescDB.get_bool("method.nond.piecewise_basis") ||
			  refine_type == Pecos::H_REFINEMENT);
  bool equidist_rules = true; // NEWTON_COTES pts for piecewise interpolants
  Pecos::BasisConfigOptions
    bc_options(nested_rules, piecewise_basis, equidist_rules,
	       probDescDB.get_bool("method.derivative_usage"));

  // initialize ssgDriver
  short growth_rate;
  short growth_override = probDescDB.get_short("method.nond.growth_override");
  // moderate growth is helpful for iso and aniso sparse grids, but not
  // necessary for generalized grids
  if (growth_override == Pecos::UNRESTRICTED ||
      refine_control  == Pecos::DIMENSION_ADAPTIVE_CONTROL_GENERALIZED)
    // unstructured index set evolution: no motivation to restrict
    growth_rate = Pecos::UNRESTRICTED_GROWTH;
  /* piecewise bases can be MODERATE now that we distinguish INTERPOLATION_MODE
  else if (piecewise_basis)
    // no reason to match Gaussian precision, but restriction still useful:
    // use SLOW i=2l+1 since it is more natural for NEWTON_COTES,CLENSHAW_CURTIS
    // and is more consistent with UNRESTRICTED generalized sparse grids.
    growth_rate = Pecos::SLOW_RESTRICTED_GROWTH;
  */
  else
    // INTEGRATION_MODE:   standardize on precision: i = 2m-1 = 2(2l+1)-1 = 4l+1
    // INTERPOLATION_MODE: standardize on number of interp pts: m = 2l+1
    growth_rate = Pecos::MODERATE_RESTRICTED_GROWTH;

  switch (ssgDriverType) {
  case Pecos::COMBINED_SPARSE_GRID: {
    bool track_colloc = false, track_uniq_prod_wts = false; // defaults
    ((Pecos::CombinedSparseGridDriver*)ssgDriver)->
      initialize_grid(ssgLevelSpec, dimPrefSpec, natafTransform.u_types(),
		      ec_options, bc_options, growth_rate, track_colloc,
		      track_uniq_prod_wts);
    break;
  }
  case Pecos::INCREMENTAL_SPARSE_GRID: {
    bool track_uniq_prod_wts = false; // default
    ((Pecos::IncrementalSparseGridDriver*)ssgDriver)->
      initialize_grid(ssgLevelSpec, dimPrefSpec, natafTransform.u_types(),
		      ec_options, bc_options, growth_rate, track_uniq_prod_wts);
    break;
  }
  case Pecos::HIERARCHICAL_SPARSE_GRID: {
    bool track_colloc = false; // non-default
    ((Pecos::HierarchSparseGridDriver*)ssgDriver)->
      initialize_grid(ssgLevelSpec, dimPrefSpec, natafTransform.u_types(),
		      ec_options, bc_options, growth_rate, track_colloc);
    break;
  }
  default: // SparseGridDriver
    ssgDriver->
      initialize_grid(ssgLevelSpec, dimPrefSpec, natafTransform.u_types(),
		      ec_options, bc_options, growth_rate);
    break;
  }
  ssgDriver->initialize_grid_parameters(natafTransform.u_types(),
    iteratedModel.aleatory_distribution_parameters());

  maxEvalConcurrency *= ssgDriver->grid_size(); // requires polyParams
}


/** This alternate constructor is used for on-the-fly generation and
    evaluation of sparse grids within PCE and SC. */
NonDSparseGrid::
NonDSparseGrid(Model& model, unsigned short ssg_level,
	       const RealVector& dim_pref, short exp_coeffs_soln_approach,
	       short driver_mode, short growth_rate, short refine_control,
	       bool track_uniq_prod_wts): 
  NonDIntegration(SPARSE_GRID_INTEGRATION, model, dim_pref),
  ssgDriverType(exp_coeffs_soln_approach), ssgLevelSpec(ssg_level)
{
  // initialize the numerical integration driver
  numIntDriver = Pecos::IntegrationDriver(ssgDriverType);
  ssgDriver = (Pecos::SparseGridDriver*)numIntDriver.driver_rep();

  // propagate general settings (not inferrable from the basis of polynomials)
  // prior to initialize_grid()
  // Note: could utilize Pecos::ExpansionConfigOptions to group some of these,
  // but only a few are relevant.
  ssgDriver->mode(driver_mode);
  ssgDriver->growth_rate(growth_rate);
  ssgDriver->refinement_control(refine_control);
  switch (ssgDriverType) {
  case Pecos::COMBINED_SPARSE_GRID: {
    Pecos::CombinedSparseGridDriver* csg_driver
      = (Pecos::CombinedSparseGridDriver*)ssgDriver;
    csg_driver->track_collocation_details(true); // SC & sparse-grid PCE (SPAM)
    csg_driver->track_unique_product_weights(track_uniq_prod_wts);
    break;
  }
  case Pecos::INCREMENTAL_SPARSE_GRID:
    ((Pecos::IncrementalSparseGridDriver*)ssgDriver)->
      track_unique_product_weights(track_uniq_prod_wts);
    break;
  case Pecos::HIERARCHICAL_SPARSE_GRID:
    if (refine_control == Pecos::DIMENSION_ADAPTIVE_CONTROL_GENERALIZED)
      ((Pecos::HierarchSparseGridDriver*)ssgDriver)->
	track_collocation_indices(true);
    break;
  }

  // local natafTransform not yet updated: x_ran_vars would have to be passed in
  // from NonDExpansion if check_variables() needed to be called here.  Instead,
  // it is deferred until run time in NonDIntegration::core_run().
  //check_variables(x_ran_vars);
}


void NonDSparseGrid::
initialize_grid(const std::vector<Pecos::BasisPolynomial>& poly_basis)
{
  numIntDriver.initialize_grid(poly_basis);
  ssgDriver->level(ssgLevelSpec);
  ssgDriver->dimension_preference(dimPrefSpec);

  // Precompute quadrature rules (e.g., by defining maximal order for
  // NumGenOrthogPolynomial::solve_eigenproblem()):
  ssgDriver->precompute_rules(); // efficiency optimization

  maxEvalConcurrency *= ssgDriver->grid_size(); // requires polyParams
}


NonDSparseGrid::~NonDSparseGrid()
{ }


void NonDSparseGrid::get_parameter_sets(Model& model)
{
  // capture any run-time updates to distribution parameters
  if (subIteratorFlag)
    ssgDriver->initialize_grid_parameters(natafTransform.u_types(),
      iteratedModel.aleatory_distribution_parameters());

  // compute grid and retrieve point/weight sets
  ssgDriver->compute_grid(allSamples);
  Cout << "\nSparse grid level = " << ssgDriver->level() << "\nTotal number "
       << "of integration points: " << allSamples.numCols() << '\n';

  if (outputLevel > NORMAL_OUTPUT &&
      ssgDriverType != Pecos::HIERARCHICAL_SPARSE_GRID)// t1 wts not implemented
    print_points_weights("dakota_sparse_tabular.dat");
}


/** used by DataFitSurrModel::build_global() to publish the minimum
    number of points needed from the sparse grid routine in order to
    build a particular global approximation. */
void NonDSparseGrid::
sampling_reset(int min_samples, bool all_data_flag, bool stats_flag)
{
  // Determine minimum sparse grid level that provides at least min_samples
  //  
  // Simplifing assumption: uniform/adaptive refinements do not need to interact
  // with sampling_reset() --> the current ssgDriver->level() is sufficient as
  // lower bound for enforcing min_samples
  //
  // If ssgLevelSpec used as lower bound, SparseGridDriver::ssgLevel could
  // be increased ***or decreased*** to provide at least min_samples subject
  // to this lower bound, but this would overwrite any previous grid increments
  //
  // If ssgLevelRef tracked separately from ssgDriver->level() for use as
  // lower bound, then uniform/adaptive refinements could be respected within
  // min_samples enforcement, but this would require activeKey management to
  // track multiple reference points.  This complication does not appear
  // necessary at this time (could be if sparse grids used for regression).

  // if resetting lower bound (take care to not overwrite increments):
  //ssgDriver->level(ssgLevelSpec);//(ssgLevelRef);
  //
  // use current level as lower bound (one sided: level cannot decrease if
  // min_samples decreases):
  unsigned short ssg_level = ssgDriver->level();
  while (ssgDriver->grid_size() < min_samples)
    ssgDriver->level(++ssg_level);

  // maxEvalConcurrency must not be updated since parallel config management
  // depends on having the same value at ctor/run/dtor times.

  // not currently used by this class:
  //allDataFlag = all_data_flag;
  //statsFlag   = stats_flag;
}


void NonDSparseGrid::increment_grid()
{
  // Pecos::SparseGridDriver manages active keys; pull current level from Driver
  unsigned short ssg_lev = ssgDriver->level();
  int orig_size = ssgDriver->grid_size();
  ssgLevelPrev  = ssg_lev; // for restoration in decrement_grid()

  // enforce a change in grid size
  ssgDriver->level(++ssg_lev);
  // with restricted growth/delayed sequences in nested rules, an increment in
  // level will not always change the grid.  Anisotropy (if present) is fixed.
  while (ssgDriver->grid_size() == orig_size)
    ssgDriver->level(++ssg_lev);
}


void NonDSparseGrid::decrement_grid()
{
  // adaptive increment logic is not reversible, so use ssgLevelPrev
  // (assumes no change in active key between increment-decrement pairs)
  ssgDriver->level(ssgLevelPrev);
}


void NonDSparseGrid::increment_grid_weights(const RealVector& aniso_wts)
{
  // Pecos::SparseGridDriver manages active keys; pull current level from Driver
  size_t ssg_lev = ssgDriver->level();
  int orig_size  = ssgDriver->grid_size();
  ssgDriver->update_axis_lower_bounds();
  // initial increment and anisotropy update
  ssgDriver->level(++ssg_lev);
  ssgDriver->anisotropic_weights(aniso_wts); // enforce axis LB's --> wt UB's
  // Enforce constraints of retaining all previous collocation sets and adding
  // at least one new set.  Given the former constraint, the same grid size
  // must logically be the same grid irregardless of changes in anisotropy.
  while (ssgDriver->grid_size() == orig_size) {
    ssgDriver->level(++ssg_lev);
    ssgDriver->anisotropic_weights(aniso_wts); // re-enforce LB's for new level
  }
}

} // namespace Dakota
