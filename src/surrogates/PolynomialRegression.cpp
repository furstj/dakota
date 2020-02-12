/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

#include "PolynomialRegression.hpp"

namespace dakota {
namespace surrogates {

// Constructor

PolynomialRegression::PolynomialRegression(const int num_terms_) : num_terms(num_terms_) {}

// Destructor

PolynomialRegression::~PolynomialRegression() {}

// Getters

const MatrixXd & PolynomialRegression::get_samples() const { return *samples; }

const VectorXd & PolynomialRegression::get_response() const { return *response; }

int PolynomialRegression::get_polynomial_order() const { return polynomial_order; }

bool PolynomialRegression::get_scaling() const { return scaling; }

const VectorXd & PolynomialRegression::get_polynomial_coeffs() const { return *polynomial_coeffs; }

double PolynomialRegression::get_polynomial_intercept() const { return polynomial_intercept; }

const util::LinearSolverBase & PolynomialRegression::get_solver() const { return *solver; }

// Setters

void PolynomialRegression::set_samples(const MatrixXd & samples_) { samples = std::make_shared<MatrixXd>(samples_); }

void PolynomialRegression::set_response(const VectorXd & response_) { response = std::make_shared<VectorXd>(response_); }

void PolynomialRegression::set_polynomial_order(int polynomial_order_) { polynomial_order = polynomial_order_; }

void PolynomialRegression::set_scaling(bool scaling_) { scaling = scaling_; }

void PolynomialRegression::set_solver(util::SOLVER_TYPE solver_type_) { solver = solver_factory(solver_type_); }

// Surrogate

void PolynomialRegression::build_surrogate() {
  const int num_samples = get_samples().size();
  const int num_responses = get_response().size();

  if(num_samples == 0 || num_responses == 0) {
  	return;
  }

  // Generate the basis matrix
  MatrixXd unscaled_basis_matrix = MatrixXd::Zero(num_samples, num_terms);
  for(int i = 0; i < num_samples; i++) {
    for(int j = 0; j < num_terms; j++) {
      unscaled_basis_matrix(i, j) = std::pow(get_samples()(i), (double)j);
    }
  }
  //std::cout << "unscaled_basis_matrix:" << std::endl;
  //std::cout << unscaled_basis_matrix << std::endl;

  // Standardize the basis matrix
  MatrixXd scaled_basis_matrix;
  if(scaling) {
    //TODO:
    // scaler = StandardScaler()
    // scaled_basis_matrix = scaler.fit_transform(unscaled_basis_matrix)
  } else {
  	scaled_basis_matrix = unscaled_basis_matrix;
  }

  // These have not been constructed and so am doing it here - RWH
  // Is there a better place to initialize the polynomial_coeffs?
  polynomial_coeffs = std::make_shared<VectorXd>(*response);

  // Compute the singular value decomposition of the basis matrix using SVD.
  solver->solve(scaled_basis_matrix, *response, *polynomial_coeffs);

  // Compute the intercept
  polynomial_intercept = 0.0;
  if(scaling) {
  	polynomial_intercept = get_response().mean() - (scaled_basis_matrix*(get_polynomial_coeffs())).mean();
  }
}

void PolynomialRegression::surrogate_value(const MatrixXd &eval_points, MatrixXd &approx_values) {
  const int num_eval_points = eval_points.size();

  // Generate the basis matrix for the eval points
  MatrixXd unscaled_basis_matrix = MatrixXd::Zero(num_eval_points, num_terms);
  for(int i = 0; i < num_eval_points; i++) {
    for(int j = 0; j < num_terms; j++) {
      unscaled_basis_matrix(i, j) = std::pow(eval_points(i), (double)j);
    }
  }

  // Scale sample points if scaling was used.
  //TODO
  MatrixXd scaled_basis_matrix = unscaled_basis_matrix;

  // Find the polynomial regression values.
  approx_values = scaled_basis_matrix * (get_polynomial_coeffs());
  approx_values = (approx_values.array() + polynomial_intercept).matrix();
}

} // namespace surrogates
} // namespace dakota
