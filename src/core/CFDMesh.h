//  Copyright 2016 National Renewable Energy Laboratory
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#ifndef CFDMESH_H
#define CFDMESH_H

#include "stk_util/parallel/Parallel.hpp"
#include "stk_mesh/base/MetaData.hpp"
#include "stk_mesh/base/BulkData.hpp"
#include "stk_mesh/base/Entity.hpp"
#include "stk_mesh/base/Field.hpp"
#include "stk_mesh/base/CoordinateSystems.hpp"
#include "stk_search/Point.hpp"
#include "stk_search/Box.hpp"
#include "stk_io/StkMeshIoBroker.hpp"

#include <string>
#include <memory>
#include <unordered_set>

namespace sierra {
namespace nalu {

typedef stk::mesh::Field<double, stk::mesh::Cartesian> VectorFieldType;
typedef stk::mesh::Field<double> ScalarFieldType;
typedef stk::search::Point<double> PointType;
typedef stk::search::Box<double> BoxType;

/** STK Mesh interface
 *
 *  This class provides a thin wrapper around the STK mesh objects (MetaData,
 *  BulkData, and StkMeshIoBroker) for use with various preprocessing utilities.
 */
class CFDMesh
{
public:
    explicit CFDMesh(stk::ParallelMachine&,
            const std::string filename);

    explicit CFDMesh(stk::ParallelMachine&,
                     const int ndim);

    ~CFDMesh() {}

    void init();

    inline stk::mesh::MetaData& meta() { return meta_; }

    inline stk::mesh::BulkData& bulk() { return bulk_; }

    inline stk::io::StkMeshIoBroker& stkio() { return stkio_; }

    /** Register a field for output during write
     */
    inline void add_output_field(const std::string field)
    {
        output_fields_.insert(field);
    }

    /** Write the Exodus results database with modifications
     */
    void write_database(std::string output_db, double time=0.0);

    /** Calculate the bounding box of the mesh
     *
     *  The selector can pick parts that are not contiguous. However, the
     *  bounding box returned will be the biggest box that encloses all parts
     *  selected.
     *
     *  \param selector An instance of stk::mesh::Selector to filter parts of
     *  the mesh where bounding box is calculated.
     *  \param verbose If true, then print out the bounding box to standard output.
     *
     *  \return An stk::search::Box instance containing the min and max points (3-D).
     */
    BoxType calc_bounding_box(const stk::mesh::Selector, bool verbose=true);

private:
    CFDMesh() = delete;
    CFDMesh(const CFDMesh&) = delete;
    //! Instance of the parallel MPI_COMM object
    stk::ParallelMachine& comm_;

    //! STK Metadata object
    stk::mesh::MetaData meta_;

    //! STK Bulkdata object
    stk::mesh::BulkData bulk_;

    //! Filename for the mesh input database
    std::string input_db_;

    //! STK Mesh I/O handler
    stk::io::StkMeshIoBroker stkio_;

    //! List of fields registered for output
    std::unordered_set<std::string> output_fields_;
};

}  // nalu
}  // sierra

#endif /* CFDMESH_H */
