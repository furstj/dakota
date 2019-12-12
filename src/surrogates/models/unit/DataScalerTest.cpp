/*  _______________________________________________________________________

    PECOS: Parallel Environment for Creation Of Stochastics
    Copyright (c) 2011, Sandia National Laboratories.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Pecos directory.
    _______________________________________________________________________ */

/*
 * DataScalerTest.cpp
 * author Elliott Ridgway
 */

/////////////
// Imports //
/////////////

#include <math.h>
#include <Teuchos_UnitTestHarness.hpp>
#include "teuchos_data_types.hpp"
#include "DataScaler.hpp"
#include "Eigen/Dense"

///////////////
// Namespace //
///////////////

using namespace Surrogates;
using namespace Eigen;

///////////////
// Utilities //
///////////////

void error(const std::string msg)
{
  throw(std::runtime_error(msg));
}

bool matrix_equals(const MatrixXd &A, const MatrixXd &B, Real tol)
{
  if ( (A.rows()!=B.rows()) || (A.cols()!=B.cols())){
    std::cout << A.rows() << "," << A.cols() << std::endl;
    std::cout << B.rows() << "," << B.cols() << std::endl;
    error("matrix_equals() matrices sizes are inconsistent");
  }
  for (int j=0; j<A.cols(); j++){
    for (int i=0; i<A.rows(); i++){
      if (std::abs(A(i,j)-B(i,j))>tol)
	    return false;
    }
  }
  return true;
}

MatrixXd create_single_sample_matrix()
{
  MatrixXd single_sample_matrix(1, 7);
  single_sample_matrix << 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7;
  return single_sample_matrix;
}

MatrixXd create_multiple_sample_matrix()
{
  MatrixXd multiple_sample_matrix(3, 7);
  multiple_sample_matrix << 0.1, 1, 10, 100, 1000, 10000, 100000,
                            0.2, 3, 20, 300, 3000, 30000, 500000,
                            0.5, 6, 50, 700, 8000, 40000, 700000;
  return multiple_sample_matrix;
}

/**
 * Calculates the variance of a vector of doubles.
 */
double variance(VectorXd vec)
{
	const double mean = vec.mean();
	double variance = 0;
	for(int i = 0; i < vec.size(); i++)
	{
		variance += std::pow((vec(i) - mean), 2);
	}
	variance = variance / vec.size();
	return variance;
}

////////////////
// Unit tests //
////////////////

TEUCHOS_UNIT_TEST(surrogates, NormalizationScaler_getScaledFeatures_TestMeanNormalizationTrue)
{
  const int norm_factor = 1.0;
  NormalizationScaler ns(create_single_sample_matrix(), true, norm_factor);

  MatrixXd matrix_actual(1, 7);
  MatrixXd matrix_expected(1, 7);
  
  matrix_actual = ns.getScaledFeatures();
  matrix_expected << -0.5, -0.333333, -0.166667, 0.0, 0.166667, 0.333333, 0.5;

  //std::cout << matrix_expected << std::endl;
  //std::cout << matrix_actual << std::endl;

  TEST_ASSERT(matrix_equals(matrix_actual, matrix_expected, 1.0e-4));
}

TEUCHOS_UNIT_TEST(surrogates, NormalizationScaler_getScaledFeatures_TestMeanNormalizationFalse)
{
  const int norm_factor = 1.0;
  NormalizationScaler ns(create_single_sample_matrix(), false, norm_factor);

  MatrixXd matrix_actual(1, 7);
  MatrixXd matrix_expected(1, 7);
  
  matrix_actual = ns.getScaledFeatures();
  matrix_expected << 0, 0.166667, 0.333333, 0.5, 0.666667, 0.833333, 1;

  //std::cout << matrix_expected << std::endl;
  //std::cout << matrix_actual << std::endl;

  TEST_ASSERT(matrix_equals(matrix_actual, matrix_expected, 1.0e-4));
}

TEUCHOS_UNIT_TEST(surrogates, NormalizationScaler_getScaledFeatures_TestMeanNormalizationFalseWithMultipleSamples)
{
  const int norm_factor = 1.0;
  NormalizationScaler ns(create_multiple_sample_matrix(), false, norm_factor);

  MatrixXd matrix_actual(3, 7);
  MatrixXd matrix_expected(3, 7);
  
  matrix_actual = ns.getScaledFeatures();
  matrix_expected << 0, 9.00001e-06, 9.90001e-05, 0.000999001,  0.00999901,   0.0999991,           1,
                     0,     5.6e-06,    3.96e-05,   0.0005996,   0.0059996,   0.0599996,           1,
                     0, 7.85715e-06, 7.07143e-05, 0.000999286,   0.0114279,   0.0571422,           1;

  //std::cout << matrix_expected << std::endl;
  //std::cout << matrix_actual << std::endl;

  TEST_ASSERT(matrix_equals(matrix_actual, matrix_expected, 1.0e-4));
}

TEUCHOS_UNIT_TEST(surrogates, NormalizationScaler_getScaledFeatures_TestNormFactor)
{
  const int norm_factor = 2.0;
  NormalizationScaler ns(create_single_sample_matrix(), true, norm_factor);

  MatrixXd matrix_actual(1, 7);
  MatrixXd matrix_expected(1, 7);
  
  matrix_actual = ns.getScaledFeatures();
  matrix_expected << -1, -0.666667, -0.333333, 1.85037e-16, 0.333333, 0.666667, 1;

  //std::cout << matrix_expected << std::endl;
  //std::cout << matrix_actual << std::endl;

  TEST_ASSERT(matrix_equals(matrix_actual, matrix_expected, 1.0e-4));
}

TEUCHOS_UNIT_TEST(surrogates, StandardizationScaler_getScaledFeatures_TestDefault)
{
  StandardizationScaler ss(create_single_sample_matrix());

  MatrixXd matrix_actual(1, 7);
  MatrixXd matrix_expected(1, 7);
  
  matrix_actual = ss.getScaledFeatures();
  matrix_expected << -1.5, -1, -0.5, 2.77556e-16, 0.5, 1, 1.5;

  //std::cout << matrix_expected << std::endl;
  //std::cout << matrix_actual << std::endl;

  TEST_ASSERT(matrix_equals(matrix_actual, matrix_expected, 1.0e-4));
  // For StandardizationScaler, mean should be effectively zero.
  TEST_ASSERT(matrix_actual.row(0).mean() < 1.0e-14); 
  // For StandardizationScaler, variance should be effectively one.
  const int UNIT_VARIANCE = 1;
  TEST_ASSERT(variance(matrix_actual.row(0)) - UNIT_VARIANCE < 1.0e-14);
}

TEUCHOS_UNIT_TEST(surrogates, StandardizationScaler_getScaledFeatures_TestMultipleSamples)
{
  StandardizationScaler ss(create_multiple_sample_matrix());

  MatrixXd matrix_actual(3, 7);
  MatrixXd matrix_expected(3, 7);
  
  matrix_actual = ss.getScaledFeatures();
  matrix_expected << -0.45993,  -0.459904, -0.459643, -0.457035, -0.430957, -0.170175,   2.43765,
                     -0.439588, -0.439572, -0.439474, -0.437858, -0.42228,  -0.266498,   2.44527,
                     -0.441129, -0.441107, -0.440925, -0.438244, -0.408139, -0.276169,   2.44571;

  //std::cout << matrix_expected << std::endl;
  //std::cout << matrix_actual << std::endl;

  TEST_ASSERT(matrix_equals(matrix_actual, matrix_expected, 1.0e-4));
  // For StandardizationScaler, mean should be effectively zero.
  TEST_ASSERT(matrix_actual.row(0).mean() < 1.0e-14); 
  TEST_ASSERT(matrix_actual.row(1).mean() < 1.0e-14); 
  TEST_ASSERT(matrix_actual.row(2).mean() < 1.0e-14);
  // For StandardizationScaler, variance should be effectively one.
  const int UNIT_VARIANCE = 1;
  TEST_ASSERT(variance(matrix_actual.row(0)) - UNIT_VARIANCE < 1.0e-14);
  TEST_ASSERT(variance(matrix_actual.row(1)) - UNIT_VARIANCE < 1.0e-14);
  TEST_ASSERT(variance(matrix_actual.row(2)) - UNIT_VARIANCE < 1.0e-14);
}

TEUCHOS_UNIT_TEST(surrogates, NoScaler_getScaledFeatures_TestDefault)
{
  NoScaler ns(create_single_sample_matrix());

  MatrixXd matrix_actual(1, 7);
  MatrixXd matrix_expected(1, 7);
  
  matrix_actual = ns.getScaledFeatures();
  matrix_expected = create_single_sample_matrix();

  //std::cout << matrix_expected << std::endl;
  //std::cout << matrix_actual << std::endl;

  TEST_ASSERT(matrix_equals(matrix_actual, matrix_expected, 1.0e-4));
}