/** @file gsC1Argyris.h

    @brief Creates the C1 Argyris space.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): P. Weinmueller
*/

#pragma once

#include <gsArgyris/gsC1ArgyrisBasis.h>

#include <gsArgyris/gsC1ArgyrisEdge.h>
#include <gsArgyris/gsC1ArgyrisVertex.h>

namespace gismo
{
template<short_t d,class T>
class gsC1Argyris //: public gsGeoTraits<d,T>::GeometryBase
{

private:
    typedef gsC1ArgyrisBasis<d,T> Basis;
    typedef typename std::vector<Basis> ArgyrisBasisContainer;

    /// Shared pointer for gsC1Argyris
    typedef memory::shared_ptr< gsC1Argyris > Ptr;

    /// Unique pointer for gsC1Argyris
    typedef memory::unique_ptr< gsC1Argyris > uPtr;


public:

    /// Empty constructor
    gsC1Argyris() { }

    gsC1Argyris(gsMultiPatch<T> const & mp,
                const gsOptionList & optionList)
                : m_mp(mp), m_optionList(optionList)
    {
        multiBasis = gsMultiBasis<>(m_mp);

        // p-refine
        multiBasis.degreeIncrease(m_optionList.getInt("degreeElevate"));
        multiBasis.uniformRefine();
        multiBasis.uniformRefine();
/*
        multiBasis.basis(0).uniformRefine();
        multiBasis.basis(1).degreeIncrease();
*/
        //init();
    }



    void createPlusMinusSpace(gsKnotVector<T> & kv1, gsKnotVector<T> & kv2,
                           gsKnotVector<T> & kv1_patch, gsKnotVector<T> & kv2_patch,
                           gsKnotVector<T> & kv1_result, gsKnotVector<T> & kv2_result);

    void createGluingDataSpace(gsKnotVector<T> & kv1, gsKnotVector<T> & kv2,
                           gsKnotVector<T> & kv1_patch, gsKnotVector<T> & kv2_patch,
                           gsKnotVector<T> & kv_result);

    void createLokalEdgeSpace(gsKnotVector<T> & kv_plus, gsKnotVector<T> & kv_minus,
                           gsKnotVector<T> & kv_gD_1, gsKnotVector<T> & kv_gD_2,
                           gsKnotVector<T> & kv1_result, gsKnotVector<T> & kv2_result);

    void init()
    {
        m_bases.reserve(m_mp.nPatches()); // For each Patch
        for (size_t np = 0; np < m_mp.nPatches(); np++)
        {
            gsC1ArgyrisBasis<d,T> c1ArgyrisBasis(m_mp, np, m_optionList);
            m_bases.push_back(c1ArgyrisBasis);
        }

        // Create inner spline space
        for (size_t np = 0; np < m_mp.nPatches(); np++)
        {
            gsTensorBSplineBasis<d, T> basis_inner = dynamic_cast<gsTensorBSplineBasis<d, T> &>(multiBasis.basis(np));
            m_bases[np].setInnerBasis(basis_inner);
        }

        // For loop over Interface to construct the spaces
        for (size_t numInt = 0; numInt < m_mp.interfaces().size(); numInt++)
        {
            const boundaryInterface & item = m_mp.interfaces()[numInt];

            const index_t side_1 = item.first().side().index();
            const index_t side_2 = item.second().side().index();
            const index_t patch_1 = item.first().patch;
            const index_t patch_2 = item.second().patch;

            const index_t dir_1 = side_1 > 2 ? 0 : 1;
            const index_t dir_2 = side_2 > 2 ? 0 : 1;

            gsBSplineBasis<> basis_1 = dynamic_cast<gsBSplineBasis<> &>(multiBasis.basis(patch_1).component(dir_1));
            gsBSplineBasis<> basis_2 = dynamic_cast<gsBSplineBasis<> &>(multiBasis.basis(patch_2).component(dir_2));

            gsBSplineBasis<> basis_geo_1 = dynamic_cast<gsBSplineBasis<> &>(multiBasis.basis(patch_1).component(1-dir_1));
            gsBSplineBasis<> basis_geo_2 = dynamic_cast<gsBSplineBasis<> &>(multiBasis.basis(patch_2).component(1-dir_2));

            gsInfo << "Basis geo 1 : " << basis_geo_1.knots().asMatrix() << "\n";
            gsInfo << "Basis geo 2 : " << basis_geo_2.knots().asMatrix() << "\n";

            gsKnotVector<T> kv_1 = basis_1.knots();
            gsKnotVector<T> kv_2 = basis_2.knots();

            gsBSplineBasis<> patch_basis_1 = dynamic_cast<gsBSplineBasis<> &>(m_mp.patch(patch_1).basis().component(dir_1));
            gsKnotVector<T> kv_patch_1 = patch_basis_1.knots();

            gsBSplineBasis<> patch_basis_2 = dynamic_cast<gsBSplineBasis<> &>(m_mp.patch(patch_2).basis().component(dir_2));
            gsKnotVector<T> kv_patch_2 = patch_basis_1.knots();

            gsKnotVector<T> kv_plus, kv_minus, kv_gluingData;
            createPlusMinusSpace(kv_1, kv_2, kv_patch_1, kv_patch_2, kv_plus, kv_minus);

            gsBSplineBasis<> basis_plus(kv_plus);
            gsInfo << "Basis plus : " << basis_plus.knots().asMatrix() << "\n";
            gsBSplineBasis<> basis_minus(kv_minus);
            gsInfo << "Basis minus : " << basis_minus.knots().asMatrix() << "\n";

            createGluingDataSpace(kv_1, kv_2, kv_patch_1, kv_patch_2, kv_gluingData);

            gsBSplineBasis<> basis_gluingData(kv_gluingData);
            gsInfo << "Basis gluingData : " << basis_gluingData.knots().asMatrix() << "\n";

            m_bases[patch_1].setBasisPlus(basis_plus, side_1);
            m_bases[patch_2].setBasisPlus(basis_plus, side_2);

            m_bases[patch_1].setBasisMinus(basis_minus, side_1);
            m_bases[patch_2].setBasisMinus(basis_minus, side_2);

            m_bases[patch_1].setBasisGeo(basis_geo_1, side_1);
            m_bases[patch_2].setBasisGeo(basis_geo_2, side_2);

            m_bases[patch_1].setBasisGluingData(basis_gluingData, side_1);
            m_bases[patch_2].setBasisGluingData(basis_gluingData, side_2);

            if (m_optionList.getSwitch("isogeometric"))
            {
                gsTensorBSplineBasis<d, T> basis_edge_1 = dynamic_cast<gsTensorBSplineBasis<d, real_t> &>(multiBasis.basis(patch_1));
                m_bases[patch_1].setEdgeBasis(basis_edge_1, side_1);

                gsTensorBSplineBasis<d, T> basis_edge_2 = dynamic_cast<gsTensorBSplineBasis<d, real_t> &>(multiBasis.basis(patch_2));
                m_bases[patch_2].setEdgeBasis(basis_edge_2, side_2);
            }
            else
            {
                gsKnotVector<T> kv_geo_1 = basis_geo_1.knots();
                gsKnotVector<T> kv_geo_2 = basis_geo_2.knots();

                gsKnotVector<T> kv_edge_1, kv_edge_2;

                createLokalEdgeSpace(kv_plus, kv_minus, kv_gluingData, kv_gluingData, kv_edge_1, kv_edge_2);
                gsBSplineBasis<> basis_edge(kv_edge_1);
                gsInfo << "Basis edge : " << basis_edge.knots().asMatrix() << "\n";

                gsTensorBSplineBasis<d, T> basis_edge_1(dir_1 == 0 ? kv_edge_1 : kv_geo_1, dir_1 == 0 ? kv_geo_1 : kv_edge_1);
                gsTensorBSplineBasis<d, T> basis_edge_2(dir_2 == 0 ? kv_edge_2 : kv_geo_2, dir_2 == 0 ? kv_geo_2 : kv_edge_2);

                m_bases[patch_1].setEdgeBasis(basis_edge_1, side_1);
                m_bases[patch_2].setEdgeBasis(basis_edge_2, side_2);
            }

        }
        // For loop over the Edge to construct the spaces
        for (size_t numBdy = 0; numBdy < m_mp.boundaries().size(); numBdy++)
        {
            const patchSide & bit = m_mp.boundaries()[numBdy];

            index_t patch_1 = bit.patch;
            index_t side_1 = bit.side().index();

            index_t dir_1 = m_mp.boundaries()[numBdy].m_index < 3 ? 1 : 0;

            // Using Standard Basis for boundary edges
            gsTensorBSplineBasis<d, T> basis_edge_1 = dynamic_cast<gsTensorBSplineBasis<d, real_t> &>(multiBasis.basis(patch_1));

            gsBSplineBasis<> basis_1 = dynamic_cast<gsBSplineBasis<> &>(multiBasis.basis(patch_1).component(dir_1));
            gsBSplineBasis<> basis_geo_1 = dynamic_cast<gsBSplineBasis<> &>(multiBasis.basis(patch_1).component(1-dir_1));

            // Assume that plus/minus space is the same as the inner space
            gsBSplineBasis<> basis_plus = basis_1;
            gsBSplineBasis<> basis_minus = basis_1;

            m_bases[patch_1].setEdgeBasis(basis_edge_1, side_1);

            m_bases[patch_1].setBasisPlus(basis_plus, side_1);
            m_bases[patch_1].setBasisMinus(basis_minus, side_1);

            m_bases[patch_1].setBasisGeo(basis_geo_1, side_1);
        }

        // For loop over the Vertex to construct the spaces
        for (size_t numVer = 0; numVer < m_mp.vertices().size(); numVer++)
        {
            std::vector<patchCorner> allcornerLists = m_mp.vertices()[numVer];
            std::vector<size_t> patchIndex;
            std::vector<size_t> vertIndex;
            for (size_t j = 0; j < allcornerLists.size(); j++)
            {
                patchIndex.push_back(allcornerLists[j].patch);
                vertIndex.push_back(allcornerLists[j].m_index);
            }

            // JUST FOR TWO PATCH TODO
            if (patchIndex.size() == 1) // Boundary vertex
            {
                index_t patch_1 = patchIndex[0];
                index_t vertex_1 = vertIndex[0];

                gsTensorBSplineBasis<d, T> basis_vertex_1 = dynamic_cast<gsTensorBSplineBasis<d, real_t> &>(multiBasis.basis(patch_1));
                m_bases[patch_1].setVertexBasis(basis_vertex_1, vertex_1);
            }
        }

        // Init local Basis
        for (size_t np = 0; np < m_mp.nPatches(); np++)
            m_bases[np].init();


        m_system.clear();
        index_t dim_col = 0, dim_row = 0;
        for (size_t i = 0; i < m_bases.size(); i++)
        {
            dim_col += m_bases[i].size_cols();
            dim_row += m_bases[i].size_rows();
        }

        m_system.resize(dim_row, dim_col);
        const index_t nz = 7*dim_row; // TODO
        m_system.reserve(nz);

    }

    void createArgyrisSpace()
    {
        // Compute Inner Basis functions
        index_t shift_row = 0, shift_col = 0;
        for(size_t np = 0; np < m_mp.nPatches(); ++np)
        {
            index_t dim_u = m_bases[np].getInnerBasis().component(0).size();
            index_t dim_v = m_bases[np].getInnerBasis().component(1).size();

            index_t row_i = 0;
            for (index_t j = 2; j < dim_v-2; ++j)
                for (index_t i = 2; i < dim_u-2; ++i)
                {
                    m_system.insert(shift_row + row_i,shift_col + j*dim_u+i) = 1.0;
                    ++row_i;
                }

            shift_row += m_bases[np].size_rows();
            shift_col += m_bases[np].size_cols();
        }

        // Compute Interface Basis functions
        for (size_t numInt = 0; numInt < m_mp.interfaces().size(); numInt++)
        {
            const boundaryInterface & item = m_mp.interfaces()[numInt];

            gsC1ArgyrisEdge<d, T> c1ArgyrisEdge(m_mp, m_bases, item, numInt, m_optionList);
            c1ArgyrisEdge.saveBasisInterface(m_system);
        }
        // Compute Edge Basis functions
        for (size_t numBdy = 0; numBdy < m_mp.boundaries().size(); numBdy++)
        {
            const patchSide & bit = m_mp.boundaries()[numBdy];

            gsC1ArgyrisEdge<d, T> c1ArgyrisEdge(m_mp, m_bases, bit, numBdy, m_optionList);
            c1ArgyrisEdge.saveBasisBoundary(m_system);
        }
        // Compute Vertex Basis functions
        for (size_t numVer = 0; numVer < m_mp.vertices().size(); numVer++)
        {
            std::vector<patchCorner> allcornerLists = m_mp.vertices()[numVer];
            std::vector<size_t> patchIndex;
            std::vector<size_t> vertIndex;
            for (size_t j = 0; j < allcornerLists.size(); j++)
            {
                patchIndex.push_back(allcornerLists[j].patch);
                vertIndex.push_back(allcornerLists[j].m_index);
            }

            // JUST FOR TWO PATCH TODO
            if (patchIndex.size() == 1) // Boundary vertex
            {
                gsC1ArgyrisVertex<d, T> c1ArgyrisVertex(m_mp, m_bases, patchIndex, vertIndex, numVer, m_optionList);
                c1ArgyrisVertex.saveBasisVertex(m_system);
            }
        }

        m_system.makeCompressed();

        gsInfo << "Dimension of Sparse matrix: " << m_system.dim() << "\n";
        gsInfo << "Non-zeros: " << m_system.nonZeros() << "\n";

        gsInfo << "Dim for Patches: \n";
        for(size_t np = 0; np < m_mp.nPatches(); ++np)
        {
            gsInfo << "(" << m_bases[np].size_rows() << "," << m_bases[np].size_cols() << "), ";
        }
        gsInfo << "\n";

    }

    void uniformRefine()
    {
        /*
        for (size_t np = 0; np < m_mp.nPatches(); np++)
        {
            m_bases[np].uniformRefine();
        }
        */
        multiBasis.uniformRefine();
    }

    void writeParaviewSinglePatch( index_t patchID, std::string type )
    {
        std::string fileName;
        std::string basename = "BasisFunctions_" + type + "_" + util::to_string(patchID);
        gsParaviewCollection collection(basename);

        index_t shift_row = 0, shift_col = 0;
        for (index_t np = 0; np < patchID; ++np)
        {
            shift_row += m_bases[np].size_rows();
            shift_col += m_bases[np].size_cols();
        }

        if (type == "inner")
        {
            index_t ii = 0;
            for (index_t i = m_bases[patchID].rowBegin(type);
                     i < m_bases[patchID].rowEnd(type); i++, ii++) // Single basis function
            {
                index_t start_j = m_bases[patchID].colBegin(type);
                index_t end_j = m_bases[patchID].colEnd(type);

                gsMatrix<> coefs = m_system.block(shift_row + i, shift_col + start_j, 1, end_j - start_j);

                gsGeometry<>::uPtr geo_temp;
                geo_temp = m_bases[patchID].getInnerBasis().makeGeometry(coefs.transpose());

                gsTensorBSpline<d, T> patch_single = dynamic_cast<gsTensorBSpline<d, T> &> (*geo_temp);

                fileName = basename + "_0_" + util::to_string(ii);
                gsField<> temp_field(m_mp.patch(patchID), patch_single);
                gsWriteParaview(temp_field, fileName, 5000);
                collection.addTimestep(fileName, ii, "0.vts");
            }
        }
        else if (type == "edge" || type == "vertex")
        {
            index_t ii = 0;
            for (index_t side = 1; side < 5; ++side) {
                for (index_t i = m_bases[patchID].rowBegin(type, side);
                     i < m_bases[patchID].rowEnd(type, side); i++, ii++) // Single basis function
                {
                    index_t start_j = m_bases[patchID].colBegin(type, side);
                    index_t end_j = m_bases[patchID].colEnd(type, side);

                    gsMatrix<> coefs = m_system.block(shift_row + i, shift_col + start_j, 1, end_j - start_j);

                    gsGeometry<>::uPtr geo_temp;
                    if (type == "edge")
                        geo_temp = m_bases[patchID].getEdgeBasis(side).makeGeometry(coefs.transpose());
                    else if (type == "vertex")
                        geo_temp = m_bases[patchID].getVertexBasis(side).makeGeometry(coefs.transpose());

                    gsTensorBSpline<d, T> patch_single = dynamic_cast<gsTensorBSpline<d, T> &> (*geo_temp);

                    fileName = basename + "_0_" + util::to_string(ii);
                    gsField<> temp_field(m_mp.patch(patchID), patch_single);
                    gsWriteParaview(temp_field, fileName, 5000);
                    collection.addTimestep(fileName, ii, "0.vts");
                }
            }
        }
        collection.save();
    }

public:

    //GISMO_CLONE_FUNCTION(gsC1Argyris)

    /// getter for m_bases
    void getMultiBasis(gsMultiBasis<> & multiBasis_result)
    {
        multiBasis_result.clear();

        std::vector<gsBasis<T> *> basis_temp = std::vector<gsBasis<T> *>(m_mp.nPatches());
        for (size_t np = 0; np < m_mp.nPatches(); np++) {
            gsC1ArgyrisBasis<2, real_t>::uPtr mbasis = gsC1ArgyrisBasis<2, real_t>::make(m_bases[np]);
            basis_temp[np] = static_cast<gsBasis<> *>(mbasis.release());
        }

        multiBasis_result = gsMultiBasis<>(basis_temp, m_mp.topology());
    };

    gsSparseMatrix<T> & getSystem()
    {
        return m_system;
    };

protected:
    /// Multipatch
    gsMultiPatch<T> m_mp;
    gsMultiBasis<> multiBasis;

    /// Optionlist
    gsOptionList m_optionList;

    ArgyrisBasisContainer m_bases;

    gsSparseMatrix<T> m_system;

}; // Class gsC1Argyris


template<short_t d,class T>
void gsC1Argyris<d,T>::createPlusMinusSpace(gsKnotVector<T> & kv1, gsKnotVector<T> & kv2,
                           gsKnotVector<T> & kv1_patch, gsKnotVector<T> & kv2_patch,
                           gsKnotVector<T> & kv1_result, gsKnotVector<T> & kv2_result)
{
    std::vector<real_t> knots_unique_1 = kv1.unique();
    std::vector<real_t> knots_unique_2 = kv2.unique();

    std::vector<real_t> patch_kv_unique_1 = kv1_patch.unique();
    std::vector<index_t> patch_kv_mult_1 = kv1_patch.multiplicities();

    std::vector<real_t> patch_kv_unique_2 = kv2_patch.unique();
    std::vector<index_t> patch_kv_mult_2 = kv2_patch.multiplicities();

    index_t p = math::max(kv1.degree(), kv2.degree());

    std::vector<real_t> knot_vector_plus, knot_vector_minus;
/*
 * TODO Add geometry inner knot regularity
 *
    index_t i_3 = 0, i_4 = 0;

    std::vector<real_t>::iterator it3 = patch_kv_unique_1.begin();
    std::vector<real_t>::iterator it4 = patch_kv_unique_2.begin();
*/
    std::vector<real_t>::iterator it2 = knots_unique_2.begin();
    for(std::vector<real_t>::iterator it = knots_unique_1.begin(); it != knots_unique_1.end(); ++it)
    {
        if (*it == *it2)
        {
            knot_vector_plus.push_back(*it);
            knot_vector_minus.push_back(*it);
            ++it2;
        }
        else if (*it < *it2)
        {
            knot_vector_plus.push_back(*it);
            knot_vector_minus.push_back(*it);
        }
        else if (*it > *it2)
        {
            while (*it > *it2)
            {
                knot_vector_plus.push_back(*it2);
                knot_vector_minus.push_back(*it2);
                ++it2;
            }
            knot_vector_plus.push_back(*it2);
            knot_vector_minus.push_back(*it2);
            ++it2;
        }
    }

    kv1_result = gsKnotVector<>(knot_vector_plus);
    kv1_result.degreeIncrease(p);
    kv2_result = gsKnotVector<>(knot_vector_minus);
    kv2_result.degreeIncrease(p-1);
}

template<short_t d,class T>
void gsC1Argyris<d,T>::createGluingDataSpace(gsKnotVector<T> & kv1, gsKnotVector<T> & kv2,
                           gsKnotVector<T> & kv1_patch, gsKnotVector<T> & kv2_patch,
                           gsKnotVector<T> & kv_result)
{
    index_t p_tilde = math::max(math::max(kv1.degree(), kv2.degree())-2,2);
    //index_t r_tilde = p_tilde - 1;

    std::vector<real_t> knots_unique_1 = kv1.unique();
    std::vector<real_t> knots_unique_2 = kv2.unique();

    std::vector<real_t> knot_vector;

/*
 * TODO Add geometry inner knot regularity
 *
 */

    std::vector<real_t>::iterator it2 = knots_unique_2.begin();
    for(std::vector<real_t>::iterator it = knots_unique_1.begin(); it != knots_unique_1.end(); ++it)
    {
        if (*it == *it2)
        {
            knot_vector.push_back(*it); // r_tilde = p_tilde - 1
            ++it2;
        }
        else if (*it < *it2)
        {
            knot_vector.push_back(*it); // r_tilde = p_tilde - 1
        }
        else if (*it > *it2)
        {
            while (*it > *it2)
            {
                knot_vector.push_back(*it2); // r_tilde = p_tilde - 1
                ++it2;
            }
            knot_vector.push_back(*it2); // r_tilde = p_tilde - 1
            ++it2;
        }
    }

    kv_result = gsKnotVector<>(knot_vector);
    kv_result.degreeIncrease(p_tilde);
} // createGluingDataSpace


template<short_t d,class T>
void gsC1Argyris<d,T>::createLokalEdgeSpace(gsKnotVector<T> & kv_plus, gsKnotVector<T> & kv_minus,
                           gsKnotVector<T> & kv_gD_1, gsKnotVector<T> & kv_gD_2,
                           gsKnotVector<T> & kv1_result, gsKnotVector<T> & kv2_result)
{
    index_t p_1 = math::max(kv_plus.degree()+kv_gD_1.degree()-1, kv_minus.degree()+kv_gD_1.degree() );
    //index_t p_2 = math::max(kv_plus.degree()+kv_gD_2.degree()-1, kv_minus.degree()+kv_gD_2.degree() ); == p_1

    index_t p_plus_diff = p_1 - kv_plus.degree();
    index_t p_gD_diff = p_1 - kv_gD_1.degree();

    std::vector<real_t> knots_unique_plus = kv_plus.unique();
    std::vector<real_t> knots_unique_gD = kv_gD_1.unique();

    std::vector<index_t> patch_kv_mult_plus = kv_plus.multiplicities();
    std::vector<index_t> patch_kv_mult_gD = kv_gD_1.multiplicities();

    if (knots_unique_plus != knots_unique_gD)
        gsInfo << "\n\nERROR: TODO \n\n";

    std::vector<real_t> knot_vector;

/*
 * TODO Add geometry inner knot regularity
 *
 */

    index_t i_plus = 0;
    for(std::vector<real_t>::iterator it = knots_unique_plus.begin(); it != knots_unique_plus.end(); ++it, ++i_plus)
    {
        index_t i_temp = 0;
        while(i_temp < math::max(patch_kv_mult_plus[i_plus]+p_plus_diff, patch_kv_mult_gD[i_plus]+p_gD_diff))
        {
            knot_vector.push_back(*it);
            ++i_temp;
        }
    }

    kv1_result = gsKnotVector<>(knot_vector);
    // ==
    kv2_result = gsKnotVector<>(knot_vector);
} // createLokalEdgeSpace

} // Namespace gismo
