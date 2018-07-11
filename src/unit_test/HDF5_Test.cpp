
// #if 0 // comment to make this file active
#ifdef DAKOTA_HAVE_HDF5

#include <Teuchos_UnitTestHarness.hpp>

#include "hdf5.h"    // C API
#include "hdf5_hl.h" // C API (HDF5 "high-level")
#include "H5Cpp.h"   // C++ API
#include <string>

using namespace H5;

#define FILE "file.h5"
#define G_METHODS "/methods"

Group* HDF5_add_method_group ( H5File* db_file, std::string method_name ) {
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

TEUCHOS_UNIT_TEST(tpl_hdf5, new_hdf5_test) {

	/* FIELDS */
	// TODO In general, the following variables should get passed in from elsewhere in Dakota.

	int  num_evaluations = 2;
	std::string  sampling_method_name = "sampling";

        float lower_bounds_arr[4] =
		{ 2.7604749078e+11, 3.6000000000e+11, 4.0000000000e+11, 4.4000000000e+11 };
        float upper_bounds_arr[4] =
		{ 3.6000000000e+11, 4.0000000000e+11, 4.4000000000e+11, 5.4196114379e+11 };
	
	float* probability_density_arrs[3];
        float probability_density_1_arr[4] =
		{ 5.3601733194e-12, 4.2500000000e-12, 3.7500000000e-12, 2.2557612778e-12 };
        float probability_density_2_arr[4] =
		{ 2.8742313192e-05, 6.4000000000e-05, 4.0000000000e-05, 1.0341896485e-05 };
        float probability_density_3_arr[4] =
		{ 4.2844660868e-06, 8.6000000000e-06, 1.8000000000e-06, 1.8000000000e-06 };

        probability_density_arrs[0] = probability_density_1_arr;
        probability_density_arrs[1] = probability_density_2_arr;
        probability_density_arrs[2] = probability_density_3_arr;
	
	/* LOGIC */

	H5File* file_ptr = new H5File(FILE, H5F_ACC_TRUNC);
	Group* group_method = HDF5_add_method_group(file_ptr, sampling_method_name);

	hsize_t dims_lb[1];
        hsize_t dims_ub[1];

        dims_lb[0] = 4;
        dims_ub[0] = 4;
        
        DataSpace scales_lb_dataspace( 1, dims_lb );
        DataSpace scales_ub_dataspace( 1, dims_ub );

	// Execution groups 
	for(int i = 1; i <= num_evaluations; i++) {
		std::string exec_id_path = "execution_id_" + std::to_string(i);
		Group group_exec_id(group_method->createGroup(exec_id_path));

		Group probability_density_group( group_exec_id.createGroup("probability_density") );
		Group scales_group( probability_density_group.createGroup("_scales") );

		DataSet dim_scale_ds_lower_bounds = scales_group.createDataSet(
			"lower_bounds", PredType::IEEE_F64LE, scales_lb_dataspace
		);
		DataSet dim_scale_ds_upper_bounds = scales_group.createDataSet(
			"upper_bounds", PredType::IEEE_F64LE, scales_ub_dataspace
		);

		dim_scale_ds_lower_bounds.write(lower_bounds_arr, PredType::IEEE_F64LE);
		dim_scale_ds_upper_bounds.write(upper_bounds_arr, PredType::IEEE_F64LE);
		
		// Convert datasets to dimension scales (using C API)
		hid_t  dim_scale_ds_lower_bounds_id = dim_scale_ds_lower_bounds.getId();
		hid_t  dim_scale_ds_upper_bounds_id = dim_scale_ds_upper_bounds.getId();

		herr_t ret_code_lb = H5DSset_scale(dim_scale_ds_lower_bounds_id, "lower_bounds");
		herr_t ret_code_ub = H5DSset_scale(dim_scale_ds_upper_bounds_id, "upper_bounds");
		if(ret_code_lb != 0 || ret_code_ub != 0) {
			//TODO Do something to handle the error.
		}

		dim_scale_ds_lower_bounds.close();
		dim_scale_ds_upper_bounds.close();

		// Probability density datasets
		for(int j = 0; j < 3; j++) {
			hsize_t dims_ds[1];
	                dims_ds[0] = 4;
        	        DataSpace probability_density_dataspace( 1, dims_ds );

			std::string probability_density_dataset_name =
				"resp_desc_" + std::to_string(j+1);
			DataSet probability_density_dataset =
				probability_density_group.createDataSet(
					probability_density_dataset_name,
					PredType::IEEE_F64LE,
					probability_density_dataspace
			);
			
			probability_density_dataset.write(
				probability_density_arrs[j], PredType::IEEE_F64LE
			);

			// Attach dataset to dimension scales (using C API)
			hid_t ds_id = probability_density_dataset.getId();			
			herr_t ret_code;
			ret_code = H5DSattach_scale(dim_scale_ds_lower_bounds_id, ds_id, 0);
			ret_code = H5DSattach_scale(dim_scale_ds_upper_bounds_id, ds_id, 0);

			probability_density_dataset.close();
		}
		
		probability_density_group.close();
		group_exec_id.close();
	}

	group_method->close();
	// group_methods.close(); TODO Do we need to explicitly close the methods group?
	file_ptr->close();

	TEST_ASSERT( true );  // successfully terminated
}

#endif
// #endif // comment to make this file active
