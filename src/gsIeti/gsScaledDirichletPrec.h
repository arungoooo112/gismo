/** @file gsScaledDirichletPrec.h

    @brief This class represents the scaled Dirichlet preconditioner.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): S. Takacs
*/

#pragma once

#include <gsSolver/gsMatrixOp.h>
#include <gsUtils/gsSortedVector.h>

namespace gismo
{

/** @brief  This class represents the scaled Dirichlet preconditioner for a IETI problem.
 *
 *  Its formal representation is
 *
 *  \f[
 *       \sum_{k=1}^K  \hat B_k  D_k^{-1}  S_k D_k^{-1} \hat B_k^\top
 *  \f]
 *
 *  It is a preconditioner for the Schur complement of the IETI system (as represented
 *  by \a gsIetiSystem)
 *
 *  \f[
 *     \begin{pmatrix}
 *        \tilde A_1 &            &             &            &  \tilde B_1^\top \\
 *                   & \tilde A_2 &             &            &  \tilde B_2^\top \\
 *                   &            &   \ddots    &            &  \vdots          \\
 *                   &            &             & \tilde A_N &  \tilde B_N^\top \\
 *        \tilde B_1 & \tilde B_2 &   \cdots    & \tilde B_N &     0            \\
 *    \end{pmatrix}
 *  \f]
 *
 *  For a standard IETI-dp setup, we additionally have a primal problem, thus
 *  N=K+1. In this case, the matrices \f$ \tilde A_k \f$ and \f$ \tilde B_k \f$
 *  are obtained from the original matrices \f$ A_k \f$ and \f$ B_k \f$ by
 *  eliminating the primal dofs (or by incorporating a constraint that sets them
 *  to zero). This is done by \a gsPrimalSystem.
 *
 *  The matrices \f$ S_k \f$ are stored in a vector accesible via
 *  \ref localSchurOp. As usual, they are stored in form of a vector of
 *  \a gsLinearOperator s. These operators represent the Schur-complements of
 *  the matrices \f$ A_k \f$ with respect to the degrees of freedom on the
 *  skeleton.
 *
 *  The jump matrices \f$ \hat B_k \f$ are accessible via \ref jumpMatrix.
 *  These matrices usually differ from the matrices \f$ \tilde B_k \f$ from the
 *  IETI-system since -- for the preconditioner -- the jump matrices have to be
 *  restricted to the skeleton.
 *
 *  If the matrices \f$ A_k \f$ and \f$ B_k \f$ are given, the function
 *  \ref restrictToSkeleton allows to compute the corresponding matrices
 *  \f$ S_k \f$ and \f$ \hat B_k \f$. The degrees of freedom belonging to the
 *  skeleton can be specified by the caller. The caller can use the function
 *  \ref getSkeletonDofs to extract this information from the jump matrices,
 *  i.e., skeleton dofs are those that are affected by a Lagrange multiplier.
 *  (Alternatively, the caller might use the corresponding function from the
 *  class \a gsIetiMapper, which uses the \a gsDofMapper s and might yield
 *  different results.)
 *
 *  The scaling matrices \f$ D_k \f$ are stored in a vector accessible via
 *  \ref scalingMatrix. They can be provided by the caller or generated by
 *  calling \ref setupMultiplicityScaling.
 *
 *  @ingroup Solver
**/

template< typename T >
class gsScaledDirichletPrec
{
    typedef gsLinearOperator<T>               Op;              ///< Linear operator
    typedef memory::shared_ptr<Op>            OpPtr;           ///< Shared pointer to linear operator
    typedef gsSparseMatrix<T>                 SparseMatrix;    ///< Sparse matrix type
    typedef gsSparseMatrix<T,RowMajor>        JumpMatrix;      ///< Sparse matrix type for jumps
    typedef memory::shared_ptr<JumpMatrix>    JumpMatrixPtr;   ///< Shared pointer to sparse matrix type for jumps
    typedef gsMatrix<T>                       Matrix;          ///< Matrix type
public:

    /// @brief Reserves the memory required to store the given number of subdomain
    /// @param n Number of subdomains
    void reserve( index_t n )
    {
        m_jumpMatrices.reserve(n);
        m_localSchurOps.reserve(n);
        m_localScalingOps.reserve(n);
        m_localScalingTransOps.reserve(n);
    }

    /// @briefs Adds a new subdomain
    ///
    /// Subdomain might be, e.g., a patch-local problem or the primal problem
    ///
    /// @param jumpMatrix    The associated jump matrix
    /// @param localSchurOp  The operator that represents the local Schur
    ///                      complement
    ///
    /// @note These two parameters can also be provided as \a std::pair as provided
    ///       by \ref restrictToSkeleton
    void addSubdomain( JumpMatrixPtr jumpMatrix, OpPtr localSchurOp )
    {
        m_jumpMatrices.push_back(give(jumpMatrix));
        m_localSchurOps.push_back(give(localSchurOp));
        m_localScalingOps.push_back(nullptr);
        m_localScalingTransOps.push_back(nullptr);
    }

    // Adds a new subdomain
    // Documented in comment above
    void addSubdomain( std::pair<JumpMatrix,OpPtr> data )
    { addSubdomain(data.first.moveToPtr(), give(data.second)); }

    /// Access the jump matrix
    JumpMatrixPtr&       jumpMatrix(index_t k)           { return m_jumpMatrices[k];  }
    const JumpMatrixPtr& jumpMatrix(index_t k) const     { return m_jumpMatrices[k];  }

    /// Access the local Schur complements operator
    OpPtr&               localSchurOps(index_t k)        { return m_localSchurOps[k]; }
    const OpPtr&         localSchurOps(index_t k) const  { return m_localSchurOps[k]; }

    /// Access the local scaling operator
    OpPtr&              localScaling(index_t k)         { return m_localScalingOps[k];  }
    const OpPtr&        localScaling(index_t k) const   { return m_localScalingOps[k];  }

    /// @brief Extracts the skeleton dofs from the jump matrix
    ///
    /// @param jumpMatrix    The jump matrix
    ///
    /// This means that a dof is considered to be on the skeleton iff at least
    /// one Lagrange multiplier acts on it. This might lead to other results
    /// than the function that is provided by \a gsIetiMapper.
    static gsSortedVector<index_t> skeletonDofs( const JumpMatrix& jumpMatrix );

    /// @brief Restricts the jump matrix to the given dofs
    ///
    /// @param jumpMatrix    The jump matrix
    /// @param dofs          The corresponding degrees of freedom (usually skeleton dofs)
    static JumpMatrix restrictJumpMatrix( const JumpMatrix& jumpMatrix, const std::vector<index_t> dofs );

    /// Data type that contains four sparse matrices that make
    /// up the blocks, stored in the members A00, A01, A10 and A11.
    struct Blocks { SparseMatrix A00, A01, A10, A11; };

    /// @brief Computes the matrix blocks with respect to the given dofs
    ///
    /// @param localMatrix   The local stiffness matrix
    /// @param dofs          The corresponding degrees of freedom (usually skeleton dofs)
    ///
    /// If 0 corresponds to the list of dofs and 1 remains to the
    /// others, this function returns the blocks A00, A10, A01, A11 of A
    static Blocks matrixBlocks( const SparseMatrix& localMatrix, const std::vector<index_t> dofs );

    /// @brief Computes the Schur complement with respect to the block A11 of matrixBlocks
    ///
    /// @param matrixBlocks  The blocks of the stiffess matrix as returned by \ref matrixBlocks
    /// @param solver        A \a gsLinearOperator that realizes the inverse of `matrixBlocks.A11`
    static OpPtr schurComplement( Blocks matrixBlocks, OpPtr solver );

    /// @brief Computes the Schur complement of the matrix with respect to the given dofs
    /// using a sparse Cholesky solver.
    ///
    /// @param localMatrix   The local stiffness matrix
    /// @param dofs          The degrees of freedom for which the Schur complement is taken
    ///
    /// The implementation is basically:
    /// @code{.cpp}
    ///    auto blocks = gsScaledDirichletPrec<T>::matrixBlocks(mat, dofs);
    ///    return gsScaledDirichletPrec<T>::schurComplement( blocks, makeSparseCholeskySolver(blocks.A11) );
    /// @endcode
    static OpPtr schurComplement( const SparseMatrix& localMatrix, const std::vector<index_t> dofs )
    {
        Blocks blocks = matrixBlocks(localMatrix, dofs);
        return schurComplement( blocks, makeSparseCholeskySolver(blocks.A11) );
    }

    /// Restricts the jump matrix and the local stiffness matrix to the skeleton
    ///
    /// @param jumpMatrix    The jump matrix
    /// @param localMatrix   The local stiffness matrix
    /// @param dofs          The degrees of freedom on the skeleton
    ///
    /// The implementation is basically:
    /// @code{.cpp}
    ///    return std::pair<gsSparseMatrix<T,RowMajor>,typename gsLinearOperator<T>::Ptr>(
    ///        gsScaledDirichletPrec<T>::restrictJumpMatrix( jumpMatrix, dofs ),
    ///        gsScaledDirichletPrec<T>::schurComplement( localMatrix, dofs )
    ///     );
    /// @endcode
    static std::pair<JumpMatrix,OpPtr> restrictToSkeleton(
        const JumpMatrix& jumpMatrix,
        const SparseMatrix& localMatrix,
        const std::vector<index_t>& dofs
    )
    { return std::pair<JumpMatrix,OpPtr>(restrictJumpMatrix(jumpMatrix,dofs),schurComplement(localMatrix,dofs)); }

    /// @brief Returns the number of Lagrange multipliers.
    ///
    /// This requires that at least one subdomain was defined.
    index_t nLagrangeMultipliers() const
    {
        GISMO_ASSERT( !m_jumpMatrices.empty(), "gsScaledDirichletPrec: Number of Lagrange multipliers "
            "can only be determined if there are jump matrices.");
        return m_jumpMatrices[0]->rows();
    }

    /// @brief This sets up the member vector \a localScaling based on
    ///        multiplicity scaling
    ///
    /// This requires that the subdomains have been defined first.
    void setupMultiplicityScaling();

    /// @brief This sets up the member vector \a localScaling based on
    ///        deluxe scaling
    ///
    /// This requires that the subdomains have been defined first.
    void setupDeluxeScaling(const std::vector<boundaryInterface>& ifaces);

    /// @brief This returns the preconditioner as \a gsLinearOperator
    ///
    /// This requires that the subdomains have been defined first.
    OpPtr preconditioner() const;

private:
    std::vector<JumpMatrixPtr>  m_jumpMatrices;         ///< The jump matrices \f$ \hat B_k \f$
    std::vector<OpPtr>          m_localSchurOps;        ///< The local Schur complements \f$ S_k \f$
    std::vector<OpPtr>          m_localScalingOps;      ///< The local scaling operators representing \f$ D_k \f$ as operators
    std::vector<OpPtr>          m_localScalingTransOps; ///< The local scaling operators representing \f$ D_k^\top \f$ as operators
};

} // namespace gismo

#ifndef GISMO_BUILD_LIB
#include GISMO_HPP_HEADER(gsScaledDirichletPrec.hpp)
#endif
