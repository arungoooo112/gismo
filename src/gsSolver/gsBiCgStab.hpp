/** @file gsBiCgStab.hpp

    @brief Biconjugate gradient stabilized solver

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): R. Tielen, S. Takacs
*/

namespace gismo
{

//#define DEBUGVAR( x ) gsInfo << #x " = " << x << "\n";

template<class T>
bool gsBiCgStab<T>::initIteration( const typename gsBiCgStab<T>::VectorType& rhs,
                                         typename gsBiCgStab<T>::VectorType& x )
{
    if (Base::initIteration(rhs,x))
        return true;

    // m_res = rhs - A * x;
    m_mat->apply(x,m_tmp);
    m_res = rhs - m_tmp;

    m_r0 = m_res;

    m_p.resize( m_mat->cols(), 1); m_p.setZero();
    m_v.resize( m_mat->cols(), 1); m_v.setZero();

    m_alpha = 1;
    m_rho = 1;
    m_w = 1;

    m_error = m_res.norm() / m_rhs_norm;

    if (m_error < m_tol)
        return true;

    return false;
}

template<class T>
const T& as_const(T&t) {return t;}

template<class T>
bool gsBiCgStab<T>::step( typename gsBiCgStab<T>::VectorType& x )
{
    T rho_old = m_rho;
    m_rho = m_r0.col(0).dot(m_res.col(0));

    if (abs(m_rho) < 1e-32 * m_r0.col(0).dot(m_r0.col(0)) ) // TODO
    {
        gsInfo << "Residual too orthogonal, restart with new r0 \n";
        m_r0 = m_res;
        m_rho = m_r0.col(0).dot(m_r0.col(0)); //= r0_sqnorm
    }

    T beta = (m_rho/rho_old)*(m_alpha/m_w);
    m_p = m_res + beta*(m_p - m_w * m_v);

    // Apply preconditioning by solving Ahat m_y = m_p
    m_precond->apply(as_const(m_p), m_y);
    // m_v = A * m_y;
    m_mat->apply(as_const(m_y), m_v);
    m_alpha = m_rho/(m_r0.col(0).dot(m_v.col(0)));

    m_s = m_res - m_alpha * m_v;
    // Apply preconditioning by solving Ahat m_z = m_s
    m_precond->apply(as_const(m_s), m_z);

    // m_t = A * m_z;
    m_mat->apply(as_const(m_z), m_t);

    if (m_t.col(0).dot(m_t.col(0)) > 0)
        m_w = m_t.col(0).dot(m_s.col(0))/m_t.col(0).dot(m_t.col(0));
    else
        m_w = 0;

    // Update iterate and residual
    x += m_alpha * m_y + m_w * m_z;
    m_res -= m_alpha * m_v + m_w * m_t;

    m_error = m_res.norm() / m_rhs_norm;
    if (m_error < m_tol)
        return true;

    return false;
}

} // end namespace gismo
