
// #if 0 // comment to make this file active
#ifdef DAKOTA_HAVE_HDF5

#include <Teuchos_UnitTestHarness.hpp>

#include "hdf5.h"    // C API
#include "hdf5_hl.h" // C API (HDF5 "high-level")
#include "H5Cpp.h"   // C++ API

#include <iostream>
#include <math.h>
#include <memory>
#include <string>

using namespace H5;

#define FILE "file.h5"

/**
 *  Insert a new method group into a Dakota HDF5 database at "/methods/<your method>."
 *  If "/methods" does not exist yet, it is automatically created for you.
 */
Group* HDF5_add_method_group ( H5File* db_file, std::string method_name ) {
    const char* G_METHODS = "/methods";

	H5G_stat_t stat_buf;
	herr_t status = H5Gget_objinfo (db_file->getId(), G_METHODS, 0, &stat_buf);

	Group group_methods;
	if( status != 0 ) {
		group_methods = db_file->createGroup(G_METHODS);
	} else {
		group_methods = db_file->openGroup(G_METHODS);
	}

	Group * group_method = new Group(group_methods.createGroup(method_name));
	return group_method;
}

/**
 *  Create a 1D dimension scale with the length and label specified by the arguments.
 *  Returns a pointer to an open DataSet object.  It is up to the caller to then
 *  write to and close the DataSet.
 *
 *  This function uses C API to set dimension scale information, as dimension scales
 *  are not supported by HDF5's C++ API.
 */
DataSet* HDF5_create_1D_dimension_scale ( Group* parent, int size, PredType type, const char* label ) {
	hsize_t ds_dims[1];
	ds_dims[0] = size;

	DataSpace ds_dataspace( 1, ds_dims );
	DataSet  *ds_dataset = new DataSet( parent->createDataSet( label, type, ds_dataspace ));

	// Use C API to set the appropriate dimension scale metadata.
	hid_t  ds_dataset_id = ds_dataset->getId();
	herr_t ret_code      = H5DSset_scale( ds_dataset_id, label );
	if(ret_code != 0) {
	   std::cerr << "H5DSset_scale returned an error\n";	// TODO Do something to handle the error.
    }
    
	return ds_dataset;
}

/**
 *  Attach a dimension scale DataSet to a target DataSet.
 *
 *  This function uses C API to set dimension scale information, as dimension scales
 *  are not supported by HDF5's C++ API.
 */
void HDF5_attach_dimension_scale ( DataSet* target, DataSet* ds_to_attach ) {
	// Use C API to attach a dataset to a dimension scale.
	hid_t target_id = target->getId();
	hid_t ds_id     = ds_to_attach->getId();

	herr_t ret_code = H5DSattach_scale(target_id, ds_id, 0);
	if(ret_code != 0) {
	   std::cerr << "H5DSattach_scale returned an error\n";	// TODO Do something to handle the error.
	}
}

/**
 *  Test writing results from a probability density to an HDF5 database.
 */
TEUCHOS_UNIT_TEST(tpl_hdf5, new_hdf5_test) {

	/* FIELDS */
	// TODO The following variables should get passed in from elsewhere in Dakota.

	int  num_evaluations = 2;
	std::string  sampling_method_name = "sampling";

	std::array<double, 4> lower_bounds_arr = { 2.7604749078e+11, 3.6e+11, 4.0e+11, 4.4e+11 };
	std::array<double, 4> upper_bounds_arr = { 3.6e+11, 4.0e+11, 4.4e+11, 5.4196114379e+11 };
    std::array<std::array<double, 4>, 3> probability_density_arrs =
		{{{ 5.3601733194e-12, 4.25e-12, 3.75e-12, 2.2557612778e-12 },
		  { 2.8742313192e-05, 6.4e-05, 4.0e-05, 1.0341896485e-05 },
		  { 4.2844660868e-06, 8.6e-06, 1.8e-06, 1.8e-06 }}};

	/* LOGIC */
	// Part 1:  Write the data.

	std::unique_ptr<H5File> file( new H5File(FILE, H5F_ACC_TRUNC) );
	std::unique_ptr<Group>  group_method( HDF5_add_method_group(file.get(), sampling_method_name) );

	for(int i = 1; i <= num_evaluations; i++) {
		std::string exec_id_path = "execution_id_" + std::to_string(i);

		Group group_exec_id( group_method->createGroup(exec_id_path) );
		Group probability_density_group( group_exec_id.createGroup("probability_density") );
		Group scales_group( probability_density_group.createGroup("_scales") );

		std::unique_ptr<DataSet> ds_lower_bounds(
			HDF5_create_1D_dimension_scale (
				&scales_group, lower_bounds_arr.size(), PredType::IEEE_F64LE, "lower_bounds"
			)
		);
		std::unique_ptr<DataSet> ds_upper_bounds(
			HDF5_create_1D_dimension_scale (
				&scales_group, upper_bounds_arr.size(), PredType::IEEE_F64LE, "upper_bounds"
			)
		);

		ds_lower_bounds->write( lower_bounds_arr.data(), PredType::NATIVE_DOUBLE );
		ds_upper_bounds->write( upper_bounds_arr.data(), PredType::NATIVE_DOUBLE );

		for(int j = 0; j < probability_density_arrs.size(); j++) {
			hsize_t dims_ds[1];
			dims_ds[0] = probability_density_arrs[j].size();
			DataSpace probability_density_dataspace( 1, dims_ds );

			std::string probability_density_dataset_name = "resp_desc_" + std::to_string(j+1);
			DataSet probability_density_dataset = probability_density_group.createDataSet(
				probability_density_dataset_name,
				PredType::IEEE_F64LE,
				probability_density_dataspace
			);
			probability_density_dataset.write(
				probability_density_arrs[j].data(), PredType::NATIVE_DOUBLE
			);

			HDF5_attach_dimension_scale ( &probability_density_dataset, ds_lower_bounds.get() );
			HDF5_attach_dimension_scale ( &probability_density_dataset, ds_upper_bounds.get() );
			
			// EMR close() happens automatically in object destructors.
			// probability_density_dataspace.close();
			// probability_density_dataset.close();
		}
		// JAS closing these dimension scale datasets invalidates their IDs, so it
		// must be deferred until after they are attached.	
		ds_lower_bounds->close();
		ds_upper_bounds->close();
		
		// probability_density_group.close(); // may not be necessary
		// group_exec_id.close();  // may not be necessary
	}

	// group_method->close();

	// Part 2:  Re-open and verify the data.

	double TOL  = 1.0e-15;
	H5File h5file( FILE, H5F_ACC_RDONLY );

	Group group_methods   = h5file.openGroup("/methods");
	Group group_sampling  = group_methods.openGroup("sampling");
	Group group_exec_id_1 = group_sampling.openGroup("execution_id_1");
	Group group_prob_dens = group_exec_id_1.openGroup("probability_density");
	Group group_scales    = group_prob_dens.openGroup("_scales");

    // Test lower_bounds dimension scale.
	DataSet dataset_lower_bounds = group_scales.openDataSet("lower_bounds");
	double data_out[4];
	hsize_t dimsm[1];  // memory space dimensions
	dimsm[0] = 4;
	DataSpace memspace( 1, dimsm );
	DataSpace dataspace = dataset_lower_bounds.getSpace();

	dataset_lower_bounds.read( data_out, PredType::NATIVE_DOUBLE, memspace, dataspace );

    TEST_FLOATING_EQUALITY( data_out[0], 2.7604749078e+11, TOL );
	TEST_FLOATING_EQUALITY( data_out[1], 3.6e+11, TOL );
	TEST_FLOATING_EQUALITY( data_out[2], 4.0e+11, TOL );
	TEST_FLOATING_EQUALITY( data_out[3], 4.4e+11, TOL );

	// Test upper_bounds dimension scale.
	DataSet dataset_upper_bounds = group_scales.openDataSet("upper_bounds");
	dataspace = dataset_upper_bounds.getSpace();

	dataset_upper_bounds.read( data_out, PredType::NATIVE_DOUBLE, memspace, dataspace );
	TEST_FLOATING_EQUALITY( data_out[0], 3.6e+11, TOL );
	TEST_FLOATING_EQUALITY( data_out[1], 4.0e+11, TOL );
	TEST_FLOATING_EQUALITY( data_out[2], 4.4e+11, TOL );
	TEST_FLOATING_EQUALITY( data_out[3], 5.4196114379e+11, TOL );

	// Test resp_desc datasets.
	DataSet dataset_resp_desc_1 = group_prob_dens.openDataSet("resp_desc_1");
	dataspace = dataset_resp_desc_1.getSpace();
    dataset_resp_desc_1.read( data_out, PredType::NATIVE_DOUBLE, memspace, dataspace );
    TEST_FLOATING_EQUALITY( data_out[0], 5.3601733194e-12, TOL );
    TEST_FLOATING_EQUALITY( data_out[1], 4.25e-12, TOL );
    TEST_FLOATING_EQUALITY( data_out[2], 3.75e-12, TOL );
    TEST_FLOATING_EQUALITY( data_out[3], 2.2557612778e-12, TOL );

	DataSet dataset_resp_desc_2 = group_prob_dens.openDataSet("resp_desc_2");
	dataspace = dataset_resp_desc_2.getSpace();
    dataset_resp_desc_2.read( data_out, PredType::NATIVE_DOUBLE, memspace, dataspace );
    TEST_FLOATING_EQUALITY( data_out[0], 2.8742313192e-05, TOL );
    TEST_FLOATING_EQUALITY( data_out[1], 6.4e-05, TOL );
    TEST_FLOATING_EQUALITY( data_out[2], 4.0e-05, TOL );
    TEST_FLOATING_EQUALITY( data_out[3], 1.0341896485e-05, TOL );

	DataSet dataset_resp_desc_3 = group_prob_dens.openDataSet("resp_desc_3");
	dataspace = dataset_resp_desc_3.getSpace();
    dataset_resp_desc_3.read( data_out, PredType::NATIVE_DOUBLE, memspace, dataspace );
    TEST_FLOATING_EQUALITY( data_out[0], 4.2844660868e-06, TOL );
    TEST_FLOATING_EQUALITY( data_out[1], 8.6e-06, TOL );
    TEST_FLOATING_EQUALITY( data_out[2], 1.8e-06, TOL );
    TEST_FLOATING_EQUALITY( data_out[3], 1.8e-06, TOL );
	
	h5file.close();
	TEST_ASSERT( true );  // successfully terminated
}

#endif
// #endif // comment to make this file active
