/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:        ResultsDBBase
//- Description:  Any-based in-core results database
//- Owner:        Brian Adams
//- Version: $Id:$

#ifndef RESULTS_DB_BASE_H
#define RESULTS_DB_BASE_H

#include <string>
#include <vector>
#include <map>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/tuple/tuple_io.hpp>
#include <boost/any.hpp>
#include "dakota_global_defs.hpp"
#include "dakota_results_types.hpp"

// TODO: try/catch around any_cast

namespace Dakota {

/// Core data storage type: boost::any, with optional metadata
/// (see other types in results_types.hpp)
typedef std::pair<boost::any, MetaDataType> ResultsValueType;


/**
 * Class: ResultsDBBase
 *
 * Description: A map-based container to store DAKOTA Iterator
 * results in underlying boost::anys, with optional metadata
 */
class ResultsDBBase
{

public:

  ResultsDBBase(const String& filename) :
    fileName(filename)
  { }
  
  // Templated allocation, insertion, and retrieval functions

  /// allocate an entry with sized array of the StoredType, e.g.,
  /// array across response functions or optimization results sets
  template<typename StoredType>
  void array_allocate(const StrStrSizet& iterator_id,
                      const std::string& data_name, size_t array_size,
                      const MetaDataType& metadata);

  /// insert sent_data in specified position in previously allocated array
  template<typename StoredType>
  void array_insert(const StrStrSizet& iterator_id,
                    const std::string& data_name, size_t index,
                    const StoredType& sent_data);

  virtual
  void add_metadata_for_method(
                    const StrStrSizet& iterator_id,
                    const AttributeArray &attrs
                    ) = 0;

  virtual
  void add_metadata_for_execution(
                    const StrStrSizet& iterator_id,
                    const AttributeArray &attrs
                    ) = 0;

  // TODO: For the following need const/non-const versions and
  // value/ref versions...

//  /// return requested data by value in StoredType
//  template<typename StoredType>
//  StoredType get_data(const StrStrSizet& iterator_id,
//		      const std::string& data_name) const;
//
//  /// return requested data from array by value in StoredType
//  template<typename StoredType>
//  StoredType get_array_data(const StrStrSizet& iterator_id,
//			    const std::string& data_name,
//			    size_t index) const;
//
//  /// return pointer to stored data entry
//  template<typename StoredType>
//  const StoredType* get_data_ptr(const StrStrSizet& iterator_id,
//				 const std::string& result_key) const;

//  /// return pointer to stored data at given array location
//  template<typename StoredType>
//  const StoredType* get_array_data_ptr(const StrStrSizet& iterator_id,
//				       const std::string& data_name,
//				       size_t index) const;

  /// record addition with metadata map
  virtual void 
  insert(const StrStrSizet& iterator_id,
         const std::string& data_name,
         const boost::any& result,
         const MetaDataType& metadata
         ) = 0;

  /// record addition with metadata map and scales data
  virtual void
  insert(const StrStrSizet& iterator_id,
         const std::string& result_name,
         const std::string& response_name,
         const boost::any& data,
         const HDF5dss &scales = HDF5dss(),
         const AttributeArray &attrs = AttributeArray(),
         const bool &transpose = false
         ) = 0;

  // NOTE: removed accessors to add metadata only or record w/o metadata

  /// coarsely dump the data to the passed output stream
  virtual void
  print_data(std::ostream& output_stream)
    { /* no-op */ }

  const String& filename() const
    { return fileName; }

protected:

//  /// attempt to find the requested data, erroring if not found
//  const ResultsValueType& lookup_data(const StrStrSizet& iterator_id,
//				      const std::string& data_name) const;
//
//  /// cast the reference to the any data to the requested type
//  template<typename StoredType>
//  StoredType cast_data(const boost::any& dataholder) const;
//
//  /// cast the pointer to the any data to the requested type
//  template<typename StoredType>
//  const StoredType* cast_data_ptr(const boost::any* dataholder) const;

  /// filename for database
  std::string fileName;

  /// core data storage (map from key to value type)
  std::map<ResultsKeyType, ResultsValueType> iteratorData;

}; // class ResultsDBBase


// templated function definitions

template<typename StoredType>
void ResultsDBBase::
array_allocate(const StrStrSizet& iterator_id,
               const std::string& data_name, size_t array_size,
               const MetaDataType& metadata) 
{
  // add a vector of the StoredType, with metadata
  insert(iterator_id, data_name, std::vector<StoredType>(array_size),
         metadata);
}

/** insert requires previous allocation, and does not allow metadata update */
template<typename StoredType>
void ResultsDBBase::
array_insert(const StrStrSizet& iterator_id,
             const std::string& data_name, size_t index,
             const StoredType& sent_data)
{
  ResultsKeyType key = make_key(iterator_id, data_name);

  std::map<ResultsKeyType, ResultsValueType>::iterator data_it = 
    iteratorData.find(key);

  if (data_it == iteratorData.end()) {
    Cout << "\nWarning: Skipping unallocated array insert for " 
         << "\n  Iterator ID: " << iterator_id
         << "\n  Data name: " << data_name
         << std::endl;
    //abort_handler(-1);
  }
  else {

    // update the any contained in the result set, which is a vector
    // of the StoredType
    ResultsValueType& result_value = data_it->second;

    // the first is the data, the second, the metadata
    std::vector<StoredType>& stored_data = 
      boost::any_cast<std::vector<StoredType>& >(result_value.first);

    if (stored_data.size() <= index) {
      Cerr << "\nResultsDB: array index exceeds allocated size." << std::endl;
      abort_handler(-1);
    }
    stored_data[index] = sent_data;
  }
}


//template<typename StoredType>
//StoredType ResultsDBBase::
//get_data(const StrStrSizet& iterator_id,
//	 const std::string& data_name) const
//{
//  const ResultsValueType& result_value = lookup_data(iterator_id, data_name);
//
//  // data is in first, discard metadata in second
//  return cast_data<StoredType>(result_value.first);
//}


//template<typename StoredType>
//StoredType ResultsDBBase::
//get_array_data(const StrStrSizet& iterator_id,
//	       const std::string& data_name, size_t index) const
//{
//  const ResultsValueType& result_value = lookup_data(iterator_id, data_name);
//
//  //TODO: bounds checking
//
//  // data is in first, discard metadata in second
//  return cast_data<std::vector<StoredType> >(result_value.first)[index];
//
//}


//template<typename StoredType>
//const StoredType* ResultsDBBase::
//get_data_ptr(const StrStrSizet& iterator_id,
//	     const std::string& data_name) const
//{
//  const ResultsValueType& result_value = lookup_data(iterator_id, data_name);
//
//  // data is in first, discard metadata in second
//  return cast_data_ptr<StoredType>(result_value.first);
//}


//template<typename StoredType>
//const StoredType* ResultsDBBase::
//get_array_data_ptr(const StrStrSizet& iterator_id,
//		   const std::string& data_name, size_t index) const
//{
//  const ResultsValueType& result_value = lookup_data(iterator_id, data_name);
//
//  // data is in first, discard metadata in second
//
//  // pointer to the array of data
//  // const std::vector<StoredType> *array_ptr = 
//  //   cast_data_ptr<const std::vector<StoredType> >(result_value.first);
//  // return (*array_ptr).data() + index;
//
//  //Call cast_data_ptr on pointer to any: any* = &result_value.first
//  const std::vector<StoredType>* array = 
//    cast_data_ptr<std::vector<StoredType> >(&result_value.first);
//  // TODO: FIX!!!
//  return &(*array)[index];
//
//  // get a pointer to the stored any as the reference will go out of scope...
//  // const std::vector<StoredType>* array = 
//  //   cast_data<std::vector<StoredType>*>(result_value.first);
//
//  // the std vector itself is (*array)
//  //return &(*array)[index];
//  //    return NULL;
//
//}


// Can't return const& through boost::any; instead one function to
// return value, another for pointer...
// these will make copies...

//template<typename StoredType>
//StoredType ResultsDBBase::cast_data(const boost::any& dataholder) const
//{
//
//  if (dataholder.type() != typeid(StoredType)) {
//    Cerr << "Couldn't cast retrieved data" << std::endl;
//    abort_handler(-1);
//  }
//   
//  return boost::any_cast<StoredType>(dataholder);
//
//}
//
//template<typename StoredType>
//const StoredType* ResultsDBBase::
//cast_data_ptr(const boost::any* dataholder) const
//{
//
//  if (dataholder->type() != typeid(StoredType)) {
//    Cerr << "Couldn't cast retrieved data" << std::endl;
//    abort_handler(-1);
//  }
//   
//  return boost::any_cast<StoredType>(dataholder);
//
//}


} // namespace Dakota

#endif  // RESULTS_DB_BASE_H
