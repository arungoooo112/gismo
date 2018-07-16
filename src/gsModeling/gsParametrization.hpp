/** @file gsParametrization.hpp

    @brief Provides implementation gsParametrization class.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): L. Groiss, J. Vogl

*/

#include <gsIO/gsOptionList.h>
#include <gsModeling/gsLineSegment.h>

namespace gismo
{

bool rangeCheck(const std::vector<size_t> &corners, const size_t minimum, const size_t maximum)
{
    for (std::vector<size_t>::const_iterator it = corners.begin(); it != corners.end(); it++)
    {
        if (*it < minimum || *it > maximum)
        { return false; }
    }
    return true;
}

real_t findLengthOfPositionPart(const size_t position,
                                const size_t numberOfPositions,
                                const std::vector<size_t> &bounds,
                                const std::vector<real_t> &lengths)
{
    if (position < 1 || position > numberOfPositions)
        gsInfo << "Error: [" << __PRETTY_FUNCTION__ << "] The position " << position
                  << " is not a valid input. There are only " << numberOfPositions << " possible positions.\n";
    if (!rangeCheck(bounds, 1, numberOfPositions))
        gsInfo << "Error: [" << __PRETTY_FUNCTION__
                  << "] The bounds are not a valid input. They have to be out of the possible positions, which only are "
                  << numberOfPositions << ".\n";
    size_t numberOfBounds = bounds.size();
    size_t s = lengths.size();
    if (position > bounds[numberOfBounds - 1] || position <= bounds[0])
        return lengths[s - 1];
    for (size_t i = 0; i < numberOfBounds; i++)
    {
        if (position - bounds[0] + 1 > bounds[i] - bounds[0] + 1
            && position - bounds[0] + 1 <= bounds[(i + 1) % numberOfBounds] - bounds[0] + 1)
            return lengths[i];
    }
    return 0;
}

template<class T>
gsOptionList gsParametrization<T>::defaultOptions()
{
    gsOptionList opt;
    opt.addInt("boundaryMethod", "boundary methodes: {1:chords, 2:corners, 3:smallest, 4:restrict, 5:opposite, 6:distributed}", 4);
    opt.addInt("parametrizationMethod", "parametrization methods: {1:shape, 2:uniform, 3:distance}", 1);
    std::vector<size_t> corners;
    opt.addMultiInt("corners", "vector for corners", corners);
    opt.addReal("range", "in case of restrict or opposite", 0.1);
    opt.addInt("number", "number of corners, in case of corners", 4);
    opt.addReal("precision", "precision to calculate", 1E-8);
    return opt;
}

template<class T>
gsParametrization<T>::gsParametrization(gsMesh<T> &mesh, gsOptionList list) : m_mesh(gsHalfEdgeMesh<T>(mesh))
{
    calculate(list.getString("boundaryMethod"),
              list.getString("parametrizationMethod"),
              list.getMultiInt("corners"),
              list.getReal("range"),
              list.getInt("number"));
}

template<class T>
gsParametrization<T>::gsParametrization(gsMesh<T> &mesh,
                                     const std::string &boundaryMethod,
                                     const std::string &paraMethod,
                                     const std::vector<size_t> &cornersInput,
                                     const real_t rangeInput,
                                     const size_t numberInput)
    : m_mesh(gsHalfEdgeMesh<T>(mesh))
{
    calculate(boundaryMethod, paraMethod, cornersInput, rangeInput, numberInput);
}

template<class T>
void gsParametrization<T>::calculate(const std::string &boundaryMethod,
                                  const std::string &paraMethod,
                                  const std::vector<size_t> &cornersInput,
                                  const real_t rangeInput,
                                  const size_t numberInput)
{
    if (boundaryMethod != "chords" && boundaryMethod != "corners" && boundaryMethod != "smallest"
        && boundaryMethod != "opposite" && boundaryMethod != "restrict" && boundaryMethod != "distributed")
    {
        gsInfo << "Error: [" << __PRETTY_FUNCTION__ << "] The boundary method " << boundaryMethod << " is not valid.\n";
        gsInfo << "Boundary method can be chosen out of 'chords', 'corners', 'smallest', 'opposite' and 'restrict'.\n";
    }
    if (paraMethod != "uniform" && paraMethod != "shape" && paraMethod != "distance")
    {
        gsInfo << "Error: [" << __PRETTY_FUNCTION__ << "] The parametrization method " << paraMethod
                  << " is not valid.\n";
        gsInfo << "gsParametrization method can be chosen out of 'uniform', 'shape' and 'distance'.\n";
    }
    size_t n = m_mesh.getNumberOfInnerVertices();
    size_t N = m_mesh.getNumberOfVertices();
    size_t B = m_mesh.getNumberOfBoundaryVertices();
    Neighbourhood neighbourhood(m_mesh, paraMethod);
    for (size_t i = 1; i <= n; i++)
    {
        m_parameterPoints.push_back(gsPoint2D(0, 0, i));
    }

    if (boundaryMethod == "chords")
    {
        real_t l = m_mesh.getBoundaryLength();
        real_t lInv = 1. / l;
        real_t w = 0;
        m_parameterPoints.push_back(gsPoint2D(0, 0, n + 1));
        std::vector<real_t> halfedgeLengths = m_mesh.getBoundaryChordLengths();
        for (size_t i = 0; i < neighbourhood.getNumberOfBoundaryHalfedges() - 1; i++)
        {
            w += halfedgeLengths[i] * lInv * 4;
            m_parameterPoints.push_back(Neighbourhood::findPointOnBoundary(w, n + i + 2));
        }
    }
    else if (boundaryMethod == "corners" || boundaryMethod == "smallest" || boundaryMethod == "opposite"
        || boundaryMethod == "restrict" || boundaryMethod == "distributed")
    {
        for (size_t i = n + 1; i <= N; i++)
        {
            m_parameterPoints.push_back(gsPoint2D(0, 0, i));
        }
        std::vector<real_t> halfedgeLengths = m_mesh.getBoundaryChordLengths();

        std::vector<size_t> corners;
        if (boundaryMethod == "corners")
            corners = cornersInput;
        else if (boundaryMethod == "smallest" || boundaryMethod == "opposite" || boundaryMethod == "restrict"
            || boundaryMethod == "distributed")
            corners = neighbourhood.getBoundaryCorners(boundaryMethod, rangeInput, numberInput);
        std::vector<real_t> lengths = m_mesh.getCornerLengths(corners);
        real_t w = 0;
        m_parameterPoints[n + corners[0] - 1] = gsPoint2D(0, 0, n + corners[0]);

        for (size_t i = corners[0] + 1; i < corners[0] + B; i++)
        {
            w += halfedgeLengths[(i - 2) % B]
                / findLengthOfPositionPart(i > B ? i - B : i, B, corners, lengths);
            m_parameterPoints[(n + i - 1) > N - 1 ? n + i - 1 - B : n + i - 1] =
                Neighbourhood::findPointOnBoundary(w, n + i > N ? n + i - B : n + i);
        }
    }
    constructAndSolveEquationSystem(neighbourhood, n, N);
}

template<class T>
void gsParametrization<T>::constructAndSolveEquationSystem(const Neighbourhood &neighbourhood,
                                                        const size_t n,
                                                        const size_t N)
{
    Eigen::MatrixXd A;
    A.resize(n, n);
    std::vector<real_t> lambdas;
    Eigen::VectorXd b1;
    b1.resize(n);
    Eigen::VectorXd b2;
    b2.resize(n);
    for (size_t i = 0; i < n; i++)
    {
        b1(i) = 0;
        b2(i) = 0;
    }
    for (size_t i = 0; i < n; i++)
    {
        lambdas = neighbourhood.getLambdas(i);
        for (size_t j = 0; j < n; j++)
        {
            if (i == j)
                A(i, j) = 1;
            else
            {
                A(i, j) = -(lambdas[j]);
            }
        }
    }
    for (size_t i = 0; i < n; i++)
    {
        lambdas = neighbourhood.getLambdas(i);
        for (size_t j = n; j < N; j++)
        {
            b1(i) += (lambdas[j]) * (m_parameterPoints[j][0]);
            b2(i) += (lambdas[j]) * (m_parameterPoints[j][1]);
        }
    }
    Eigen::VectorXd u;
    u.resize(n);
    Eigen::VectorXd v;
    v.resize(n);
    Eigen::PartialPivLU<Eigen::MatrixXd> LU = A.partialPivLu();
    u = LU.solve(b1);
    v = LU.solve(b2);

    u = A.lu().solve(b1);
    v = A.lu().solve(b2);
    for (size_t i = 0; i < n; i++)
    {
        m_parameterPoints[i].moveToPosition(u(i), v(i));
    }
}

template<class T>
const gsPoint2D &gsParametrization<T>::getParameterPoint(size_t vertexIndex) const
{
    return m_parameterPoints[vertexIndex - 1];
}

//*****************************************************************************************************
//*****************************************************************************************************
//*******************THE******INTERN******FUNCTIONS******ARE******NOW******FOLLOWING*******************
//*****************************************************************************************************
//*****************************************************************************************************
template<class T>
gsMatrix<> gsParametrization<T>::createUVmatrix()
{
    gsMatrix<> m;
    m.resize(2, m_mesh.getNumberOfVertices());
    for (size_t i = 1; i <= m_mesh.getNumberOfVertices(); i++)
    {
        m.col(i - 1) << this->getParameterPoint(i)[0], this->getParameterPoint(i)[1];
    }
    return m;
}

template<class T>
gsMatrix<> gsParametrization<T>::createXYZmatrix()
{
    gsMatrix<> m;
    m.resize(3, m_mesh.getNumberOfVertices());
    for (size_t i = 1; i <= m_mesh.getNumberOfVertices(); i++)
    {
        m.col(i - 1) << m_mesh.getVertex(i)->x(), m_mesh.getVertex(i)->y(), m_mesh.getVertex(i)->z();
    }
    return m;
}

template<class T>
gsMesh<> gsParametrization<T>::createFlatMesh()
{
    gsMesh<> mesh;
    for (size_t i = 0; i < m_mesh.getNumberOfTriangles(); i++)
    {
        for (int j = 1; j <= 3; ++j)
        {
            mesh.addVertex(getParameterPoint(m_mesh.getGlobalVertexIndex(j, i))[0],
                           getParameterPoint(m_mesh.getGlobalVertexIndex(j, i))[1], 0);
        }
        mesh.addFace(i * 3, i * 3 + 1, i * 3 + 2);
    }
    return mesh;
}

//******************************************************************************************
//******************************* nested class Neighbourhood *******************************
//******************************************************************************************
template<class T>
gsParametrization<T>::Neighbourhood::Neighbourhood(const gsHalfEdgeMesh<T>& meshInfo, const std::string parametrizationMethod) : m_basicInfos(meshInfo)
{
    //hier geht die ganz zeit verloren
    for(size_t i=1; i <= m_basicInfos.getNumberOfInnerVertices(); i++)
    {
        m_localParametrizations.push_back(LocalParametrization(m_basicInfos, LocalNeighbourhood(m_basicInfos, i), parametrizationMethod));
    }
    //ab hier geht etwas zeit verloren, aber nur ein bruchteil
    for(size_t i=m_basicInfos.getNumberOfInnerVertices()+1; i<= m_basicInfos.getNumberOfVertices(); i++)
    {
        m_localBoundaryNeighbourhoods.push_back(LocalNeighbourhood(m_basicInfos, i,0));
    }
}

template<class T>
real_t gsParametrization<T>::Neighbourhood::getBoundaryLength() const
{
    return m_basicInfos.getBoundaryLength();
}

template<class T>
size_t gsParametrization<T>::Neighbourhood::getNumberOfBoundaryHalfedges() const
{
    return m_basicInfos.getNumberOfBoundaryVertices();
}

template<class T>
size_t gsParametrization<T>::Neighbourhood::getNumberOfInnerVertices() const
{
    return m_basicInfos.getNumberOfInnerVertices();
}

template<class T>
const std::vector<real_t>& gsParametrization<T>::Neighbourhood::getLambdas(const size_t i) const
{
    return m_localParametrizations[i].getLambdas();
}

template<class T>
const std::vector<size_t> gsParametrization<T>::Neighbourhood::getBoundaryCorners(const std::string method, const real_t range, const size_t number) const
{
    std::vector<std::pair<real_t , size_t> > angles;
    std::vector<size_t> corners;
    for(typename std::vector<LocalNeighbourhood>::const_iterator it=m_localBoundaryNeighbourhoods.begin(); it!=m_localBoundaryNeighbourhoods.end(); it++)
    {
        angles.push_back(std::pair<real_t , size_t>(it->getInnerAngle(), it->getVertexIndex() - this->getNumberOfInnerVertices()));
    }
    std::sort(angles.begin(), angles.end());
    if(method == "smallest")
    {
        this->takeCornersWithSmallestAngles(4, angles, corners);
        std::sort(corners.begin(), corners.end());
        gsDebug << "According to the method 'smallest inner angles' the following corners were chosen:\n";
        for(std::vector<size_t>::iterator it=corners.begin(); it!=corners.end(); it++)
        {
            gsDebug << (*it) << "\n";
        }
    }
    else if(method == "opposite")
    {
        searchAreas(range, angles, corners);
        gsDebug << "According to the method 'nearly opposite corners' the following corners were chosen:\n";
        for(size_t i=0; i<corners.size(); i++)
        {
            gsDebug << corners[i] << "\n";
        }
    }
    else if(method == "restrict")
    {
        bool flag = true;
        corners.push_back(angles.front().second);
        angles.erase(angles.begin());
        while(corners.size() < 4)
        {
            flag = true;
            for(std::vector<size_t>::iterator it=corners.begin(); it!=corners.end(); it++)
            {
                if(m_basicInfos.getShortestBoundaryDistanceBetween(angles.front().second, *it) < range*getBoundaryLength())
                    flag = false;
            }
            if(flag)
                corners.push_back(angles.front().second);
            angles.erase(angles.begin());
        }
        std::sort(corners.begin(), corners.end());
        for(size_t i=0; i<corners.size(); i++)
        {
            gsDebug << corners[i] << "\n";
        }
    }
    else if(method == "distributed")
    {
        real_t oldDifference = 0;
        real_t newDifference = 0;
        std::vector<size_t> newCorners;
        std::vector<real_t> lengths;
        angles.erase(angles.begin()+number, angles.end());
        gsDebug << "Angles:\n";
        for(size_t i=0; i<angles.size(); i++)
        {
            gsDebug << angles[i].first << ", " << angles[i].second << "\n";
        }
        for(size_t i=0; i<angles.size(); i++)
        {
            for(size_t j=i+1; j<angles.size(); j++)
            {
                for(size_t k=j+1; k<angles.size(); k++)
                {
                    for(size_t l=k+1; l<angles.size(); l++)
                    {
                        newCorners.push_back(angles[i].second);
                        newCorners.push_back(angles[j].second);
                        newCorners.push_back(angles[k].second);
                        newCorners.push_back(angles[l].second);
                        std::sort(newCorners.begin(), newCorners.end());
                        lengths = m_basicInfos.getCornerLengths(newCorners);
                        std::sort(lengths.begin(), lengths.end());
                        newDifference = std::abs(lengths[0] - lengths[3]);
                        if(oldDifference == 0 || newDifference < oldDifference)
                        {
                            corners.erase(corners.begin(), corners.end());
                            corners.push_back(angles[i].second);
                            corners.push_back(angles[j].second);
                            corners.push_back(angles[k].second);
                            corners.push_back(angles[l].second);
                            std::sort(corners.begin(), corners.end());
                        }
                        newCorners.erase(newCorners.begin(), newCorners.end());
                        oldDifference = newDifference;
                    }
                }
            }
        }
        gsDebug << "According to the method 'evenly distributed corners' the following corners were chosen:\n";
        for(size_t i=0; i<corners.size(); i++)
        {
            gsDebug << corners[i] << "\n";
        }
    }
    return corners;
}

template<class T>
const gsPoint2D gsParametrization<T>::Neighbourhood::findPointOnBoundary(const real_t w, size_t vertexIndex)
{
    if(w < 0 || w > 4)
        gsInfo << "Error: [" << __PRETTY_FUNCTION__ << "] wrong value for w\n";
    else if(0 <= w && w <=1)
        return gsPoint2D(w,0, vertexIndex);
    else if(1<w && w<=2)
        return gsPoint2D(1,w-1, vertexIndex);
    else if(2<w && w<=3)
        return gsPoint2D(1-w+2,1, vertexIndex);
    else if(3<w && w<=4)
        return gsPoint2D(0,1-w+3, vertexIndex);
    return gsPoint2D();
}

//*****************************************************************************************************
//*****************************************************************************************************
//*******************THE******INTERN******FUNCTIONS******ARE******NOW******FOLLOWING*******************
//*****************************************************************************************************
//*****************************************************************************************************

template<class T>
void gsParametrization<T>::Neighbourhood::takeCornersWithSmallestAngles(size_t number, std::vector<std::pair<real_t , size_t> >& sortedAngles, std::vector<size_t>& corners) const
{
    sortedAngles.erase(sortedAngles.begin()+number, sortedAngles.end());
    for(std::vector<std::pair<real_t , size_t> >::iterator it=sortedAngles.begin(); it!=sortedAngles.end(); it++)
    {
        corners.push_back(it->second);
    }
}

template<class T>
std::vector<real_t> gsParametrization<T>::Neighbourhood::midpoints(const size_t numberOfCorners, const real_t length) const
{
    std::vector<real_t> midpoints;
    real_t n = 1./numberOfCorners;
    for(size_t i=1; i<numberOfCorners; i++)
    {
        midpoints.push_back(i*length*n);
    }
    return midpoints;
}

template<class T>
void gsParametrization<T>::Neighbourhood::searchAreas(const real_t range, std::vector<std::pair<real_t , size_t> >& sortedAngles, std::vector<size_t>& corners) const
{
    real_t l = m_basicInfos.getBoundaryLength();
    std::vector<real_t> h = m_basicInfos.getBoundaryChordLengths();
    this->takeCornersWithSmallestAngles(1,sortedAngles, corners);
    std::vector<std::vector<std::pair<real_t , size_t> > > areas;
    for(size_t i=0; i<3; i++)
    {
        areas.push_back(std::vector<std::pair<real_t , size_t> >());
    }
    std::vector<real_t> midpoints = this->midpoints(4, l);

    real_t walkAlong = 0;
    for(size_t i=0; i<h.size(); i++)
    {
        walkAlong += h[(corners[0]+i-1)%h.size()];
        for(int j = 2; j>=0; j--)
        {
            if(std::abs(walkAlong-midpoints[j]) <= l*range)
            {
                areas[j].push_back(std::pair<real_t , size_t>(m_localBoundaryNeighbourhoods[(corners[0]+i)%(h.size())].getInnerAngle(), (corners[0]+i)%h.size() + 1));
                break;
            }
        }
    }
    std::sort(areas[0].begin(), areas[0].end());
    std::sort(areas[1].begin(), areas[1].end());
    std::sort(areas[2].begin(), areas[2].end());
    bool smaller = false;
    for(size_t i=0; i<areas[0].size(); i++)
    {
        if(areas[0][i].second > corners[0] || areas[0][i].second < corners[0])
        {
            corners.push_back(areas[0][i].second);
            if(areas[0][i].second < corners[0])
            {
                smaller = true;
            }
            break;
        }
    }
    for(size_t i=0; i<areas[1].size(); i++)
    {
        if(smaller)
        {
            if(areas[1][i].second > corners[1] && areas[1][i].second < corners[0])
            {
                corners.push_back(areas[1][i].second);
                break;
            }
        }
        else if(areas[1][i].second > corners[1] || areas[1][i].second < corners[0])
        {
            corners.push_back(areas[1][i].second);
            if(areas[1][i].second < corners[0])
            {
                smaller = true;
            }
            break;
        }
    }
    for(size_t i=0; i<areas[2].size(); i++)
    {
        if(smaller)
        {
            if(areas[2][i].second > corners[2] && areas[2][i].second < corners[0])
            {
                corners.push_back(areas[2][i].second);
                break;
            }
        }
        else if(areas[2][i].second > corners[2] || areas[2][i].second < corners[0])
        {
            corners.push_back(areas[2][i].second);
            break;
        }
    }
}

//*******************************************************************************************
//**************************** nested class LocalParametrization ****************************
//*******************************************************************************************

template<class T>
gsParametrization<T>::LocalParametrization::LocalParametrization(const gsHalfEdgeMesh<T>& meshInfo, const LocalNeighbourhood& localNeighbourhood, const std::string parametrizationMethod)
{
    m_vertexIndex = localNeighbourhood.getVertexIndex();
    std::list<size_t> indices = localNeighbourhood.getVertexIndicesOfNeighbours();
    size_t d = localNeighbourhood.getNumberOfNeighbours();
    if(parametrizationMethod == "uniform")
    {
        for(size_t j=1; j <= meshInfo.getNumberOfVertices(); j++)
        {
            m_lambdas.push_back(0); // Lambda(m_vertexIndex, j, 0)
        }
        while(!indices.empty())
        {
            m_lambdas[indices.front()-1] += (1./d);
            indices.pop_front();
        }
    }
    else if(parametrizationMethod == "shape")
    {
        std::list<real_t> angles = localNeighbourhood.getAngles();
        std::vector<gsPoint2D> points;
        real_t theta = 0;
        real_t nextAngle = 0;
        for(std::list<real_t>::iterator it = angles.begin(); it!=angles.end(); ++it)
        {
            theta += *it;
        }
        gsPoint2D p(0, 0, 0);
        real_t length = (*meshInfo.getVertex(indices.front()) - *meshInfo.getVertex(m_vertexIndex)).norm();
        gsPoint2D nextPoint(length, 0, indices.front());
        points.push_back(nextPoint);
        gsVector<> actualVector = nextPoint - p;
        indices.pop_front();
        real_t thetaInv = 1./theta;
        while(!indices.empty())
        {
            length = (*meshInfo.getVertex(indices.front()) - *meshInfo.getVertex(m_vertexIndex)).norm();
            //length =  (meshInfo.getVertex(indices.front()) - meshInfo.getVertex(m_vertexIndex) ).norm();
            nextAngle = angles.front()*thetaInv * 2 * EIGEN_PI;
            gsVector<> nextVector = ((Eigen::Rotation2D<T>(nextAngle) * actualVector).normalized()*length) + p;
            nextPoint = gsPoint2D(nextVector[0], nextVector[1], indices.front());
            points.push_back(nextPoint);
            actualVector = nextPoint - p;
            angles.pop_front();
            indices.pop_front();
        }
        calculateLambdas(meshInfo.getNumberOfVertices(), points);
    }
    else if(parametrizationMethod == "distance")
    {
        std::list<real_t> neighbourDistances = localNeighbourhood.getNeighbourDistances();
        real_t sumOfDistances = 0;
        for(std::list<real_t>::iterator it = neighbourDistances.begin(); it != neighbourDistances.end(); it++)
        {
            sumOfDistances += *it;
        }
        real_t sumOfDistancesInv = 1./sumOfDistances;
        for(size_t j=1; j <= meshInfo.getNumberOfVertices(); j++)
        {
            m_lambdas.push_back(0); //Lambda(m_vertexIndex, j, 0)
        }
        for(std::list<real_t>::iterator it = neighbourDistances.begin(); it != neighbourDistances.end(); it++)
        {
            m_lambdas[indices.front()-1] += ((*it)*sumOfDistancesInv);
            indices.pop_front();
        }
    }
}

template<class T>
const std::vector<real_t>& gsParametrization<T>::LocalParametrization::getLambdas() const
{
    return m_lambdas;
}

//*****************************************************************************************************
//*****************************************************************************************************
//*******************THE******INTERN******FUNCTIONS******ARE******NOW******FOLLOWING*******************
//*****************************************************************************************************
//*****************************************************************************************************

template<class T>
void gsParametrization<T>::LocalParametrization::calculateLambdas(const size_t N, std::vector<gsPoint2D>& points)
{
    for(size_t j=1; j <= N; j++)
    {
        m_lambdas.push_back(0); //Lambda(m_vertexIndex, j, 0)
    }
    gsPoint2D p(0, 0, 0);
    size_t d = points.size();
    std::vector<real_t> my(d, 0);
    gsLineSegment2D actualLine;
    size_t l=1;
    size_t steps = 0;
    //size_t checkOption = 0;
    for(std::vector<gsPoint2D>::const_iterator it=points.begin(); it != points.end(); it++)
    {
        actualLine = gsLineSegment2D(p, *it);
        for(size_t i=1; i < d-1; i++)
        {
            if(l+i == d)
                steps = d - 1;
            else
                steps = (l+i)%d - 1;
            //checkoption is set to another number, in case mu is negativ
            if(actualLine.intersectSegment(*(points.begin()+steps), *(points.begin()+(steps+1)%d)/*, checkOption*/))
            {
                //BarycentricCoordinates b(p, *it, *(points.begin()+steps), *(points.begin()+(steps+1)%d));
                // calculating Barycentric Coordinates
                gsMatrix<real_t, 3, 3> matrix;
                matrix.topRows(2).col(0) = *it;
                matrix.topRows(2).col(1) = *(points.begin()+steps);
                matrix.topRows(2).col(2) = *(points.begin()+(steps+1)%d);
                matrix.row(2).setOnes();

                gsVector3d<T> vector3d;
                vector3d << p, 1;
                gsVector3d<T> delta = matrix.partialPivLu().solve(vector3d);
                my[l-1] = delta(0);
                my[steps] = delta(1);
                my[(steps + 1)%d] = delta(2);
                break;
            }
        }
        for(size_t k = 1; k <= d; k++)
        {
            m_lambdas[points[k-1].getVertexIndex()-1] += (my[k-1]);
        }
        std::fill(my.begin(), my.end(), 0);
        l++;
    }
    for(std::vector<real_t>::iterator it=m_lambdas.begin(); it != m_lambdas.end(); it++)
    {
        *it /= d;
    }
    for(std::vector<real_t>::iterator it=m_lambdas.begin(); it != m_lambdas.end(); it++)
    {
        if(*it < 0)
            gsInfo << *it << "\n";
    }
}

//*******************************************************************************************
//***************************** nested class LocalNeighbourhood *****************************
//*******************************************************************************************

template<class T>
gsParametrization<T>::LocalNeighbourhood::LocalNeighbourhood(const gsHalfEdgeMesh<T>& meshInfo, const size_t vertexIndex, const bool innerVertex)
{
    if((innerVertex && vertexIndex > meshInfo.getNumberOfInnerVertices()) || vertexIndex < 1)
        gsInfo << "Error: [" << __PRETTY_FUNCTION__ << "] Vertex with index " << vertexIndex << " does either not exist (< 1) or is not an inner vertex (> " << meshInfo.getNumberOfInnerVertices() << ").\n";
    else
    {
        m_vertexIndex = vertexIndex;
        std::queue<typename gsHalfEdgeMesh<T>::Halfedge> allHalfedges = meshInfo.getOppositeHalfedges(m_vertexIndex, innerVertex);
        std::queue<typename gsHalfEdgeMesh<T>::Halfedge> nonFittingHalfedges;
        m_neighbours.appendNextHalfedge(allHalfedges.front());
        m_angles.push_back((*meshInfo.getVertex(allHalfedges.front().getOrigin()) - *meshInfo.getVertex(m_vertexIndex)).angle((*meshInfo.getVertex(allHalfedges.front().getEnd()) - *meshInfo.getVertex(vertexIndex))));
        m_neighbourDistances.push_back(allHalfedges.front().getLength());
        allHalfedges.pop();
        while(!allHalfedges.empty())
        {
            if(m_neighbours.isAppendableAsNext(allHalfedges.front()))
            {
                m_neighbours.appendNextHalfedge(allHalfedges.front());
                m_angles.push_back((*meshInfo.getVertex(allHalfedges.front().getOrigin()) - *meshInfo.getVertex(m_vertexIndex)).angle( (*meshInfo.getVertex(allHalfedges.front().getEnd())-*meshInfo.getVertex(m_vertexIndex))));
                m_neighbourDistances.push_back(allHalfedges.front().getLength());
                allHalfedges.pop();
                while(!nonFittingHalfedges.empty())
                {
                    allHalfedges.push(nonFittingHalfedges.front());
                    nonFittingHalfedges.pop();
                }
            }
            else if(m_neighbours.isAppendableAsPrev(allHalfedges.front()))
            {
                m_neighbours.appendPrevHalfedge(allHalfedges.front());
                m_angles.push_front((*meshInfo.getVertex(allHalfedges.front().getOrigin())-*meshInfo.getVertex(m_vertexIndex)).angle( (*meshInfo.getVertex(allHalfedges.front().getEnd())-*meshInfo.getVertex(m_vertexIndex))));
                m_neighbourDistances.push_back(allHalfedges.front().getLength());
                allHalfedges.pop();
                while(!nonFittingHalfedges.empty())
                {
                    allHalfedges.push(nonFittingHalfedges.front());
                    nonFittingHalfedges.pop();
                }
            }
            else
            {
                nonFittingHalfedges.push(allHalfedges.front());
                allHalfedges.pop();
            }
        }
    }
}

template<class T>
size_t gsParametrization<T>::LocalNeighbourhood::getVertexIndex() const
{
    return m_vertexIndex;
}

template<class T>
size_t gsParametrization<T>::LocalNeighbourhood::getNumberOfNeighbours() const
{
    return m_neighbours.getNumberOfVertices();
}

template<class T>
const std::list<size_t> gsParametrization<T>::LocalNeighbourhood::getVertexIndicesOfNeighbours() const
{
    return m_neighbours.getVertexIndices();
}

template<class T>
const std::list<real_t>& gsParametrization<T>::LocalNeighbourhood::getAngles() const
{
    return m_angles;
}

template<class T>
real_t gsParametrization<T>::LocalNeighbourhood::getInnerAngle() const
{
    real_t angle = 0;
    for(std::list<real_t>::const_iterator it=m_angles.begin(); it!=m_angles.end(); it++)
    {
        angle += (*it);
    }
    return angle;
}

template<class T>
std::list<real_t> gsParametrization<T>::LocalNeighbourhood::getNeighbourDistances() const
{
    return m_neighbourDistances;
}

} // namespace gismo