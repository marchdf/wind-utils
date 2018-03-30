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

#include "HexBlockMesh.h"
#include "core/YamlUtils.h"
#include "core/PerfUtils.h"

#include "stk_mesh/base/TopologyDimensions.hpp"
#include "stk_mesh/base/FEMHelpers.hpp"
#include "stk_mesh/base/Field.hpp"

namespace sierra{
namespace nalu {

REGISTER_DERIVED_CLASS(HexBlockBase, HexBlockMesh, "generate_ablmesh");

bool check_input_coords(
    std::vector<std::vector<double>> crdVec,
    size_t ncoords,
    size_t ndim=3)
{
    bool coordsOK = true;

    if (crdVec.size() != ncoords) {
        coordsOK = false;
        return coordsOK;
    }

    for (auto crds: crdVec)
        if (crds.size() != ndim) {
            coordsOK = false;
            break;
        }
    return coordsOK;
}

void bbox_to_vertices(
    std::vector<std::vector<double>>& vertices,
    std::vector<std::vector<double>>& bbox)
{
    // S-W corner (0 vertex)
    vertices[0][0] = bbox[0][0];
    vertices[0][1] = bbox[0][1];
    vertices[0][2] = bbox[0][2];
    // S-E corner (1 vertex)
    vertices[1][0] = bbox[1][0];
    vertices[1][1] = bbox[0][1];
    vertices[1][2] = bbox[0][2];
    // N-E corner (2 vertex)
    vertices[2][0] = bbox[1][0];
    vertices[2][1] = bbox[1][1];
    vertices[2][2] = bbox[0][2];
    // N-W corner (3 vertex)
    vertices[3][0] = bbox[0][0];
    vertices[3][1] = bbox[1][1];
    vertices[3][2] = bbox[0][2];

    // S-W corner (4 vertex)
    vertices[4][0] = bbox[0][0];
    vertices[4][1] = bbox[0][1];
    vertices[4][2] = bbox[1][2];
    // S-E corner (5 vertex)
    vertices[5][0] = bbox[1][0];
    vertices[5][1] = bbox[0][1];
    vertices[5][2] = bbox[1][2];
    // N-E corner (6 vertex)
    vertices[6][0] = bbox[1][0];
    vertices[6][1] = bbox[1][1];
    vertices[6][2] = bbox[1][2];
    // N-W corner (7 vertex)
    vertices[7][0] = bbox[0][0];
    vertices[7][1] = bbox[1][1];
    vertices[7][2] = bbox[1][2];
}

HexBlockMesh::HexBlockMesh(
    CFDMesh& mesh,
    const YAML::Node& node
) : HexBlockBase(mesh),
    vertices_(8, std::vector<double>(3, 0.0))
{
    load(node);
}

HexBlockMesh::~HexBlockMesh()
{}

void HexBlockMesh::load(const YAML::Node& node)
{
    using namespace sierra::nalu::wind_utils;
    HexBlockBase::load(node);

    if (node["spec_type"]) {
        std::string spec_type = node["spec_type"].as<std::string>();

        if (spec_type == "bounding_box")
            vertexDef_ = BOUND_BOX;
        else if (spec_type == "vertices")
            vertexDef_ = VERTICES;
        else
            throw std::runtime_error("HexBlockMesh: Invalid option for spec_type = "
                                     + spec_type);
    }

    if (vertexDef_ == BOUND_BOX) {
        auto bbox = node["vertices"].as<std::vector<std::vector<double>>>();
        auto coordsOK = check_input_coords(bbox, 2, 3);
        if (!coordsOK)
            throw std::runtime_error("HexBlockMesh: Inconsistent coordinates provided");

        bbox_to_vertices(vertices_, bbox);

    } else {
        vertices_ = node["vertices"].as<std::vector<std::vector<double>>>();

        auto coordsOK = check_input_coords(vertices_, 8, 3);
        if (!coordsOK)
            throw std::runtime_error("HexBlockMesh: Inconsistent coordinates provided");
    }

    meshDims_ = node["mesh_dimensions"].as<std::vector<int>>();

    // Process mesh spacing inputs

    if (node["x_spacing"]) {
        auto& xsnode = node["x_spacing"];
        get_optional(xsnode, "spacing_type", xspacing_type_);

        xSpacing_.reset(MeshSpacing::create(meshDims_[0]+1, xsnode, xspacing_type_));
    } else {
        xSpacing_.reset(MeshSpacing::create(meshDims_[0]+1, node, xspacing_type_));
    }

    if (node["y_spacing"]) {
        auto& ysnode = node["y_spacing"];
        get_optional(ysnode, "spacing_type", yspacing_type_);

        ySpacing_.reset(MeshSpacing::create(meshDims_[1]+1, ysnode, yspacing_type_));
    } else {
        ySpacing_.reset(MeshSpacing::create(meshDims_[1]+1, node, yspacing_type_));
    }

    if (node["z_spacing"]) {
        auto& zsnode = node["z_spacing"];
        get_optional(zsnode, "spacing_type", zspacing_type_);

        zSpacing_.reset(MeshSpacing::create(meshDims_[2]+1, zsnode, zspacing_type_));
    } else {
        zSpacing_.reset(MeshSpacing::create(meshDims_[2]+1, node, zspacing_type_));
    }
}

void HexBlockMesh::generate_coordinates(const std::vector<stk::mesh::EntityId>& nodeVec)
{
    const std::string timerName("HexBlockMesh::generate_coordinates");
    auto timeMon = get_stopwatch(timerName);
    int nx = meshDims_[0] + 1;
    int ny = meshDims_[1] + 1;
    int nz = meshDims_[2] + 1;

    VectorFieldType* coords = meta_.get_field<VectorFieldType>(
        stk::topology::NODE_RANK, "coordinates");

    std::cout << "\t Generating x spacing: " << xspacing_type_ << std::endl;
    xSpacing_->init_spacings();
    std::cout << "\t Generating y spacing: " << yspacing_type_ <<  std::endl;
    ySpacing_->init_spacings();
    std::cout << "\t Generating z spacing: " << zspacing_type_ << std::endl;
    zSpacing_->init_spacings();

    auto& rxvec = xSpacing_->ratios();
    auto& ryvec = ySpacing_->ratios();
    auto& rzvec = zSpacing_->ratios();

    for (int k=0; k < nz; k++) {
        double rz = rzvec[k];
        for (int j=0; j < ny; j++) {
            double ry = ryvec[j];
            for (int i=0; i < nx; i++) {
                double rx = rxvec[i];
                int idx = k * (nx * ny) + j * nx + i;
                auto node = bulk_.get_entity(stk::topology::NODE_RANK, nodeVec[idx]);
                double* pt = stk::mesh::field_data(*coords, node);

                pt[0] = ((1.0 - rx) * (1.0 - ry) * (1.0 - rz) * vertices_[0][0] +
                         rx * (1.0 - ry) * (1.0 - rz) * vertices_[1][0] +
                         rx * ry * (1.0 - rz) * vertices_[2][0] +
                         (1.0 - rx) * ry * (1.0 - rz) * vertices_[3][0] +
                         (1.0 - rx) * (1.0 - ry) * (rz) * vertices_[4][0] +
                         rx * (1.0 - ry) * (rz) * vertices_[5][0] +
                         rx * ry * (rz) * vertices_[6][0] +
                         (1.0 - rx) * ry * (rz) * vertices_[7][0]);

                pt[1] = ((1.0 - rx) * (1.0 - ry) * (1.0 - rz) * vertices_[0][1] +
                         rx * (1.0 - ry) * (1.0 - rz) * vertices_[1][1] +
                         rx * ry * (1.0 - rz) * vertices_[2][1] +
                         (1.0 - rx) * ry * (1.0 - rz) * vertices_[3][1] +
                         (1.0 - rx) * (1.0 - ry) * (rz) * vertices_[4][1] +
                         rx * (1.0 - ry) * (rz) * vertices_[5][1] +
                         rx * ry * (rz) * vertices_[6][1] +
                         (1.0 - rx) * ry * (rz) * vertices_[7][1]);

                pt[2] = ((1.0 - rx) * (1.0 - ry) * (1.0 - rz) * vertices_[0][2] +
                         rx * (1.0 - ry) * (1.0 - rz) * vertices_[1][2] +
                         rx * ry * (1.0 - rz) * vertices_[2][2] +
                         (1.0 - rx) * ry * (1.0 - rz) * vertices_[3][2] +
                         (1.0 - rx) * (1.0 - ry) * (rz) * vertices_[4][2] +
                         rx * (1.0 - ry) * (rz) * vertices_[5][2] +
                         rx * ry * (rz) * vertices_[6][2] +
                         (1.0 - rx) * ry * (rz) * vertices_[7][2]);
            }
        }
    }
}

}  // nalu
}  // sierra
