/* ============================================================================
 * Copyright (c) 2009-2016 BlueQuartz Software, LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of BlueQuartz Software, the US Air Force, nor the names of its
 * contributors may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The code contained herein was partially funded by the following contracts:
 *    United States Air Force Prime Contract FA8650-07-D-5800
 *    United States Air Force Prime Contract FA8650-10-D-5210
 *    United States Prime Contract Navy N00173-07-C-2068
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#pragma once

#include <algorithm>
#include <cassert> /* assert */
#include <complex>
#include <iostream>
#include <string>

#if __APPLE__
#include <Accelerate/Accelerate.h>
#endif

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Eigen>

#include "EbsdLib/Core/Quaternion.hpp"
#include "EbsdLib/EbsdLib.h"
#include "EbsdLib/Math/EbsdMatrixMath.h"
#include "EbsdLib/Utilities/ModifiedLambertProjection3D.hpp"

/* This comment block is commented as Markdown. if you paste this into a text
 * editor then render it with a Markdown aware system a nice table should show
 * up for you.

 ## Function Mapping Check List ##

#### Master Table of Conversions ####

| From/To |  e   |  o   |  a   |  r   |  q   |  h   |  c   |
|  -      |  -   |  -   |  -   |  -   |  -   |  -   |  -   |
|  e      |  -   |  X   |  X   |  X   |  X   |  a   | ah   |
|  o      |  X   |  --  |  X   |  e   |  X   |  a   | ah   |
|  a      |  o   |  X   | --   |  X   |  X   |  X   |  h   |
|  r      |  o   |  a   |  X   | --   |  a   |  X   |  h   |
|  q      |  X   |  X   |  X   |  X   | --   |  X   |  h   |
|  h      |  ao  |  a   |  X   |  a   |  a   | --   |  X   |
|  c      | hao  |  ha  |  h   |  ha  | ha   |  X   | --   |


#### DREAM3D Implemented ####


| From/To |  e   |  o   |  a   |  r   |  q   |  h   |  c   |
|  -      |  -   |  -   |  -   |  -   |  -   |  -   |  -   |
|  e      |  #   |  X   |  X   |  X   |  X   |  a   |  X   |
|  o      |  X   |  #   |  @   |  X   |  X   |  a   |  -   |
|  a      |  X   |  X   |  #   |  X   |  X   |  X   |  -   |
|  r      |  X   |  X   |  X   |  #   |  X   |  X   |  -   |
|  q      |  X   |  X   |  X   |  X   |  #   |  X   |  -   |
|  h      |  X   |  X   |  X   |  X   |  X   |  #   |  X   |
|  c      |  -   |  -   |  -   |  -   |  -   |  X   |  #   |
*/

/**
 * The Orientation codes are written in such a way that the value of -1 indicates
 * an Active Rotation and +1 indicates a passive rotation.
 *
 * DO NOT UNDER ANY CIRCUMSTANCE CHANGE THESE VARIABLES. THERE WILL BE BAD
 * CONSEQUENCES IF THESE ARE CHANGED. EVERY PIECE OF CODE THAT RELIES ON THESE
 * FUNCTIONS WILL BREAK. IN ADDITION, THE QUATERNION ARITHMETIC WILL NO LONGER
 * BE CONSISTENT WITH ROTATION ARITHMETIC.
 *
 * YOU HAVE BEEN WARNED.
 *
 * Adam  Morawiec's book uses Passive rotations.
 **/
#ifndef DREAM3D_PASSIVE_ROTATION
// #define DREAM3D_ACTIVE_ROTATION               -1.0
#define DREAM3D_PASSIVE_ROTATION 1
#endif

#ifndef ROTATIONS_CONSTANTS
#define ROTATIONS_CONSTANTS
namespace Rotations::Constants
{
#if DREAM3D_PASSIVE_ROTATION
static const float epsijk = 1.0f;
static const double epsijkd = 1.0;
#elif DREAM3D_ACTIVE_ROTATION
static const float epsijk = -1.0f;
static const double epsijkd = -1.0;
#endif
} // namespace Rotations::Constants

#endif

// Add some shortened namespace alias
// Condense some of the namespaces to same some typing later on.
namespace LPs = EbsdLib::LambertParametersType;
namespace RConst = Rotations::Constants;
namespace DConst = EbsdLib::Constants;

/**
 * @brief The OrientationTransformation namespace
 * template parameter InputType can be one of std::vector<T>, std::vector<T> or OrientationArray<T>
 * and template parameter typename OutputType::value_type is the type specified in T. For example if InputType is std::vector<float>
 * then typename OutputType::value_type is float.
 */
namespace OrientationTransformation
{

// using RotationMatrixType = Eigen::Matrix<K, 3, 3, Eigen::RowMajor>;
// using RotationMatrixMapType = Eigen::Map<RotationMatrixType>;
// using OMHelperType = ArrayHelpers<T, K>;

struct ResultType
{
  int result;
  std::string msg;
};

// static void FatalError(const std::string& func, const std::string& msg)
//{
//  std::cout << func << "::" << msg << std::endl;
//}
/* ###################################################################
 Original Fotran codes written by Dr. Marc De Graef.

* MODULE: rotations
*
* @brief everything that has to do with rotations and conversions between rotations
*
* @details This file relies a lot on the relations listed in the book "Orientations
* and Rotations" by Adam Morawiec [Springer 2004].  I've tried to implement every
* available representation for rotations in a way that makes it easy to convert
* between any pair.  Needless to say, this needs extensive testing and debugging...
*
* Instead of converting all the time between representations, I've opted to
* "waste" a little more memory and time and provide the option to precompute all the representations.
* This way all representations are available via a single data structure.
*
* Obviously, the individual conversion routines also exist and can be called either in
* single or in double precision (using a function interface for each call, so that only
* one function name is used).  The conversion routines use the following format for their
* call name:  ab2cd, where (ab and cd are two-characters strings selected from the following
* possibilities: [the number in parenthesis lists the number of entries that need to be provided]
*
* eu : euler angle representation (3)
* om : orientation matrix representation (3x3)
* ax : axis angle representation (4)
* ro : Rodrigues vector representation (3)
* qu : unit quaternion representation (4)
* ho : homochoric representation (3)
* cu : cubochoric representation (3).
*
* hence, conversion from homochoric to euler angle is called as ho2eu(); the argument of
* each routine must have the correct number of dimensions and entries.
* All 42 conversion routines exist in both single and double precision.
*
* Some routines were modified in July 2014, to simplify the paths in case the direct conversion
* routine does not exist.  Given the complexity of the cubochoric transformations, all routines
* going to and from this representation will require at least one and sometimes two or three
* intermediate representations.  cu2eu and qu2cu currently represent the longest computation
* paths with three intermediate steps each.
*
* In August 2014, all routines were modified to account for active vs. passive rotations,
* after some inconsistencies were discovered that could be traced back to that distinction.
* The default is for a rotation to be passive, and only those transformation rules have been
* implemented.  For active rotations, the user needs to explicitly take action in the calling
* program by setting the correct option in the ApplyRotation function.
*
* Testing: the program rotationtest.f90 was generated by an IDL script and contains all possible
* pairwise and triplet transformations, using a series of input angle combinations; for now, these
* are potentially problematic Euler combinations.
*
* The conventions of this module are:
*
* - all reference frames are right-handed and orthonormal (except for the Bravais frames)
* - a rotation angle is positive for a counterclockwise rotation when viewing along the positive rotation axis towards the origin
* - all rotations are interpreted in the passive way
* - Euler angles follow the Bunge convention, with phi1 in [0,2pi], Phi in [0,pi], and phi2 in [0,2pi]
* - rotation angles (in axis-angle derived representations) are limited to the range [0,pi]
*
* To make things easier for the user, this module provides a routine to create a rotation
* representation starting from an axis, described by a unit axis vector, and a rotation angle.
* This routine properly takes the sign of epsijk into account, and always produces a passive rotation.
* The user must explicitly take action to interpret a rotation a being active.
*
* @date 08/04/13 MDG 1.0 original
* @date 07/08/14 MDG 2.0 modifications to several routines (mostly simplifications)
* @date 08/08/14 MDG 3.0 added active/passive handling (all routines passive)
* @date 08/11/14 MDG 3.1 modified Rodrigues vector to 4 components (n and length) to accomodate Infinity
* @date 08/18/14 MDG 3.2 added RotateVector, RotateTensor2 routines with active/passive switch
* @date 08/20/14 MDG 3.3 completed extensive testing of epsijk<0 mode; all tests passed for the first time !
* @date 08/21/14 MDG 3.4 minor correction in om2ax to get things to work for epsijk>0 mode; all tests passed!
* @date 09/30/14 MDG 3.5 added routines to make rotation definitions easier
* @date 09/30/14 MDG 3.6 added strict range checking routines for all representations (tested on 10/1/14)
//--------------------------------------------------------------------------
//--------------------------------
* routines to check the validity range of rotation representations
//--------------------------------
* general rotation creation routine, to make sure that a rotation representation is
* correctly initialized, takes an axis and an angle as input, returns an orientationtype structure
* general interface routine to populate the orientation type
//--------------------------------
* convert Euler angles to 3x3 orientation matrix
* convert Euler angles to axis angle
* convert Euler angles to Rodrigues vector
* convert Euler angles to quaternion
* convert Euler angles to homochoric
* convert Euler angles to cubochoric
//--------------------------------
* convert 3x3 orientation matrix to Euler angles
* convert 3x3 orientation matrix to axis angle
* convert 3x3 orientation matrix to Rodrigues
* convert 3x3 rotation matrix to quaternion
* convert 3x3 rotation matrix to homochoric
* convert 3x3 rotation matrix to cubochoric
//--------------------------------
* convert axis angle pair to euler
* convert axis angle pair to orientation matrix
* convert axis angle pair to Rodrigues
* convert axis angle pair to quaternion
* convert axis angle pair to homochoric representation
* convert axis angle pair to cubochoric
//--------------------------------
* convert Rodrigues vector to Euler angles
* convert Rodrigues vector to orientation matrix
* convert Rodrigues vector to axis angle pair
* convert Rodrigues vector to quaternion
* convert Rodrigues vector to homochoric
* convert Rodrigues vector to cubochoric
//--------------------------------
* convert quaternion to Euler angles
* convert quaternion to orientation matrix
* convert quaternion to axis angle
* convert quaternion to Rodrigues
* convert quaternion to homochoric
* convert quaternion to cubochoric
//--------------------------------
* convert homochoric to euler
* convert homochoric to orientation matrix
* convert homochoric to axis angle pair
* convert homochoric to Rodrigues
* convert homochoric to quaternion
* convert homochoric to cubochoric
//--------------------------------
* convert cubochoric to euler
* convert cubochoric to orientation matrix
* convert cubochoric to axis angle
* convert cubochoric to Rodrigues
* convert cubochoric to quaternion
* convert cubochoric to homochoric
* apply a rotation to a vector
* apply a rotation to a second rank tensor
//--------------------------------
* print quaternion and equivalent 3x3 rotation matrix
*/

/**: eu_check
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief verify that the Euler angles are in the correct respective ranges
 *
 * @param eu 3-component vector
 *
 *
 * @date 9/30/14   MDG 1.0 original
 */
template <typename T>
ResultType eu_check(const T& eu)
{
  ResultType res;
  res.result = 1;

  if((eu[0] < 0.0) || (eu[0] > (EbsdLib::Constants::k_2PiD)))
  {
    res.msg = "rotations:eu_check:: phi1 Euler angle outside of valid range [0,2pi]";
    res.result = -1;
  }
  if((eu[1] < 0.0) || (eu[1] > EbsdLib::Constants::k_PiD))
  {
    res.msg = "rotations:eu_check:: Phi Euler angle outside of valid range [0,pi]";
    res.result = -2;
  }
  if((eu[2] < 0.0) || (eu[2] > (EbsdLib::Constants::k_2PiD)))
  {
    res.msg = "rotations:eu_check:: phi2 Euler angle outside of valid range [0,2pi]";
    res.result = -3;
  }
  return res;
}

/**: ro_check
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief verify that the Rodrigues vector has positive length and unit axis vector
 *
 * @param ro 4-component vector ( <v0, v1, v2>, L )
 *
 *
 * @date 9/30/14   MDG 1.0 original
 */
template <typename InputType>
ResultType ro_check(const InputType& ro)
{
  typename InputType::value_type eps = static_cast<typename InputType::value_type>(1.0E-6L);
  ResultType res;
  res.result = 1;
  if(ro[3] < 0.0L)
  {
    res.msg = "rotations:ro_check:: Rodrigues-Frank vector has negative length: ";
    res.result = -1;
    return res;
  }
  typename InputType::value_type ttl = std::sqrt(ro[0] * ro[0] + ro[1] * ro[1] + ro[2] * ro[2]);

  if(std::fabs(ttl - 1.0) > eps)
  {
    res.msg = "rotations:ro_check:: Rodrigues-Frank axis vector not normalized";
    res.result = -2;
  }
  return res;
}

/**: ho_check
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief verify that the homochoric vector is inside or on the homochoric ball
 *
 * @param ho 3-component vector
 *
 *
 * @date 9/30/14   MDG 1.0 original
 */
template <typename InputType>
ResultType ho_check(const InputType& ho)
{
  using value_type = typename InputType::value_type;
  ResultType res;
  res.result = 1;

  value_type r = std::sqrt(ho[0] * ho[0] + ho[1] * ho[1] + ho[2] * ho[2]);

  if(r > static_cast<float>(LPs::R1))
  {
    res.msg = "rotations:ho_check: homochoric vector outside homochoric ball";
    res.result = -1;
  }
  return res;
}

/**: cu_check
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief verify that the cubochoric vector is inside or on the cube
 *
 * @param cu 3-component vector
 *
 *
 * @date 9/30/14   MDG 1.0 original
 */
template <typename InputType>
ResultType cu_check(const InputType& cu)
{
  using ValueType = typename InputType::value_type;
  ResultType res;
  res.result = 1;

  ValueType maxValue = static_cast<ValueType>(LPs::ap / 2.0);
  bool maxValueHit = false;

  std::for_each(cu.begin(), cu.end(), [&](const ValueType& v) {
    ValueType value = std::fabs(v);
    if(value > maxValue)
    {
      maxValueHit = true;
    }
  });

  if(maxValueHit)
  {
    res.msg = "rotations:cu_check: cubochoric vector outside cube";
    res.result = -1;
  }
  return res;
}

/**: qu_check
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief verify that the quaternion has unit length and positive scalar part
 *
 * @param qu 4-component vector (w, <v0, v1, v2> )
 *
 *
 * @date 9/30/14   MDG 1.0 original
 */
template <typename InputType>
ResultType qu_check(const InputType& qu, typename Quaternion<typename InputType::value_type>::Order layout = Quaternion<typename InputType::value_type>::Order::VectorScalar)
{
  using SizeType = typename InputType::size_type;
  SizeType w = 0;
  SizeType x = 1;
  SizeType y = 2;
  SizeType z = 3;
  if(layout == Quaternion<typename InputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }
  ResultType res;
  res.result = 1;

  if(qu[w] < 0.0)
  {
    res.msg = "rotations:qu_check: quaternion must have positive scalar part";
    res.result = -1;
    return res;
  }

  typename InputType::value_type eps = std::numeric_limits<typename InputType::value_type>::epsilon();
  typename InputType::value_type r = std::sqrt(qu[x] * qu[x] + qu[y] * qu[y] + qu[z] * qu[z] + qu[w] * qu[w]);
  if(fabs(r - 1.0) > eps)
  {
    res.msg = "rotations:qu_check: quaternion must have unit norm";
    res.result = -2;
  }
  return res;
}

/**: ax_check
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief verify that the axis angle pair has a unit vector and angle in the correct range
 *
 * @param ax 4-component vector (<ax0, ax1, ax2>, angle )
 *
 *
 * @date 9/30/14   MDG 1.0 original
 */
template <typename InputType>
ResultType ax_check(const InputType& ax)
{
  using value_type = typename InputType::value_type;
  ResultType res;
  res.result = 1;
  if((ax[3] < 0.0) || (ax[3] > EbsdLib::Constants::k_PiD))
  {
    res.msg = "rotations:ax_check: angle must be in range [0,pi]";
    res.result = -1;
    return res;
  }
  typename InputType::value_type eps = std::numeric_limits<value_type>::epsilon();

  typename InputType::value_type r = std::sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
  typename InputType::value_type absv = static_cast<typename InputType::value_type>(fabs(r - 1.0));

  if(absv > eps)
  {
    res.msg = "rotations:ax_check: axis-angle axis vector must have unit norm";
    res.result = -2;
  }
  return res;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
template <typename T>
void Print_OM(const T& om)
{
  printf("OM: /    % 3.16f    % 3.16f    % 3.16f    \\\n", om[0], om[1], om[2]);
  printf("OM: |    % 3.16f    % 3.16f    % 3.16f    |\n", om[3], om[4], om[5]);
  printf("OM: \\    % 3.16f    % 3.16f    % 3.16f    /\n", om[6], om[7], om[8]);
}

/**: om_check
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief verify that the rotation matrix is actually a proper rotation matrix
 *
 * @param om 3x3-component matrix
 *
 *
 * @date 9/30/14   MDG 1.0 original
 */
template <typename InputType>
ResultType om_check(const InputType& om)
{
  ResultType res;
  res.result = 1;
  using ValueType = typename InputType::value_type;
  ValueType threshold = static_cast<ValueType>(1.0E-5L);
  using RotationMatrixType = Eigen::Matrix<ValueType, 3, 3, Eigen::RowMajor>;
  using RotationMatrixMapType = Eigen::Map<RotationMatrixType>;
  RotationMatrixMapType omE(const_cast<ValueType*>(om.data()));

  ValueType det = omE.determinant();

  std::stringstream ss;
  if(det < 0.0)
  {
    ss << "rotations:om_check: Determinant of rotation matrix must be positive: " << det;
    res.msg = ss.str();
    res.result = -1;
    return res;
  }

  ValueType r = fabs(det - static_cast<ValueType>(1.0L));
  if(!EbsdLibMath::closeEnough(r, static_cast<ValueType>(0.0L), threshold))
  {
    ss << "rotations:om_check: Determinant (" << det << ") of rotation matrix must be unity (1.0)";
    res.msg = ss.str();
    res.result = -2;
    return res;
  }

  RotationMatrixType abv = (omE * omE.transpose()).cwiseAbs();

  RotationMatrixType identity;
  identity.setIdentity();

  identity = identity - abv;
  identity = identity.cwiseAbs();

  for(int c = 0; c < 3; c++)
  {
    for(int rIndex = 0; r < 3; r++)
    {
      if(identity(rIndex, c) > threshold)
      {
        std::stringstream ss_error;
        ss_error << "rotations:om_check: rotation matrix times transpose must be identity matrix: (";
        ss_error << rIndex << ", " << c << ") = " << abv(rIndex, c);
        res.msg = ss_error.str();
        res.result = -3;
      }
    }
  }

  return res;
}

#if 0
    /**: genrot
    *
    * @author Marc De Graef, Carnegie Mellon University
    *
    * @brief generate a passive rotation representation, given the unit axis vector and the rotation angle
    *
    * @param av 3-component vector
    * @param omega rotation angle (radians)
    *
    *
    * @date 9/30/14   MDG 1.0 original
    */
    template <typename InputType, typename OutputType> void genrot(const T& av, typename OutputType::value_type omega, OutputType& res)
    {
      //*** use local
      //*** use constants
      //*** use error
      //*** IMPLICIT NONE
      //real(kind=sgl),INTENT(IN)       :: av[2]
      //real(kind=sgl),INTENT(IN)       :: omega
      type(orientationtype)           :: res;
      typename OutputType::value_type axang[4];
      typename OutputType::value_type s;

      if ((omega < 0.0) || (omega > M_PI))
      {
        assert(false);
      }

      axang[0] = -RConst::epsijk * av[0];
      axang[1] = -RConst::epsijk * av[1];
      axang[2] = -RConst::epsijk * av[2];
      axang[3] = omega;
      s = sqrt(sumofSquares(av));

      if (s != 0.0)
      {
        axang[0] = axang[0] / s;
        axang[1] = axang[1] / s;
        axang[2] = axang[2] / s;
      }
      else
      {
        assert(false);
      }
      init_orientation(axang, 'ax', res);
    }



    /**: init_orientation
    *
    * @author Marc De Graef, Carnegie Mellon University
    *
    * @brief take an orientation representation with 3 components and init all others
    *
    * @param orient 3-component vector
    * @param intype input type ['eu', 'ro', 'ho', 'cu']
    * @param rotcheck  optional parameter to enforce strict range checking
    *
    * @date 8/04/13   MDG 1.0 original
    * @date 9/30/14   MDG 1.1 added testing of valid ranges
    */

    template <typename InputType, typename OutputType>
void init_orientation(const T& orient, char intype[2], bool rotcheck, OutputType& res)
    {

    }

    /**: init_orientation_om
    *
    * @author Marc De Graef, Carnegie Mellon University
    *
    * @brief take an orientation representation with 3x3 components and init all others
    *
    * @param orient r-component vector
    * @param intype input type ['om']
    * @param rotcheck  optional parameter to enforce strict range checking
    *
    *
    * @date 8/04/13   MDG 1.0 original
    */

    void init_orientation_om(float* orient, char intype[2], bool rotcheck, float* res);
#endif

/**: eu2om
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Euler angles to orientation matrix  [Morawiec, page 28]
 * also from Appendix 1, equation 1 in
 *
 * Consistent representations of and conversions between 3D rotations
 * D Rowenhorst, A D Rollett, G S Rohrer, M Groeber, M Jackson, P J Konijnenberg, and M De Graef
 * Published 5 October 2015 IOP Publishing Ltd
 * Modelling and Simulation in Materials Science and Engineering, Volume 23, Number 8
 *
 * The output orientation matrix is laid out in memory such that the following is true:
 *       |  c1c2-s1cs2      s1c2+c1cs2    ss2 |
 * OM =  | -c1s2-s1cc2     -s1s2+c1cc2    sc2 |
 *       |      s1s           -c1s         c  |
 *
 *       | res[0]   res[1]  res[2] |
 * OM =  | res[3]   res[4]  res[5] |
 *       | res[6]   res[7]  res[8] |
 *
 * @param e 3 Euler angles in radians
 *
 *
 * @date 8/04/13   MDG 1.0 original
 * @date 7/23/14   MDG 1.1 verified
 */
template <typename InputType, typename OutputType>
OutputType eu2om(const InputType& e)
{
  OutputType om(9);
  // typename OutputType::value_type eps = std::numeric_limits<typename OutputType::value_type>::epsilon();
  using ValueType = typename OutputType::value_type;

  ValueType eps = 1.0E-7f;

  ValueType c1 = cos(e[0]);
  ValueType c = cos(e[1]);
  ValueType c2 = cos(e[2]);
  ValueType s1 = sin(e[0]);
  ValueType s = sin(e[1]);
  ValueType s2 = sin(e[2]);
  om[0] = c1 * c2 - s1 * s2 * c;
  om[1] = s1 * c2 + c1 * s2 * c;
  om[2] = s2 * s;
  om[3] = -c1 * s2 - s1 * c2 * c;
  om[4] = -s1 * s2 + c1 * c2 * c;
  om[5] = c2 * s;
  om[6] = s1 * s;
  om[7] = -c1 * s;
  om[8] = c;
  for(size_t i = 0; i < 9; i++)
  {
    if(fabs(om[i]) < eps)
    {
      om[i] = 0.0;
    }
  }
  return om;
}

/**: eu2ax
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert euler to axis angle
 *
 * @param e 3 euler angles
 *
 *
 * @date 8/12/13   MDG 1.0 original
 * @date 7/23/14   MDG 2.0 explicit implementation
 * @date 7/23/14   MDG 2.1 exception for zero rotation angle
 */
template <typename InputType, typename OutputType>
OutputType eu2ax(const InputType& e)
{
  OutputType res(4);
  using value_type = typename OutputType::value_type;
  value_type thr = static_cast<value_type>(1.0E-6);
  value_type alpha = static_cast<value_type>(0.0);
  value_type t = static_cast<value_type>(tan(e[1] * 0.5));
  value_type sig = static_cast<value_type>(0.5 * (e[0] + e[2]));
  value_type del = static_cast<value_type>(0.5 * (e[0] - e[2]));
  value_type tau = static_cast<value_type>(std::sqrt(t * t + sin(sig) * sin(sig)));
  if(EbsdLibMath::closeEnough(sig, static_cast<typename OutputType::value_type>(EbsdLib::Constants::k_PiOver2D), static_cast<typename OutputType::value_type>(1.0E-6L)))
  {
    alpha = static_cast<value_type>(EbsdLib::Constants::k_PiD);
  }
  else
  {
    alpha = static_cast<value_type>(2.0 * atan(tau / cos(sig))); //! return a default identity axis-angle pair
  }

  if(fabs(alpha) < thr)
  {
    res[0] = 0.0;
    res[1] = 0.0;
    res[2] = 1.0;
    res[3] = 0.0;
  }
  else
  {
    //! passive axis-angle pair so a minus sign in front
    res[0] = static_cast<value_type>(-RConst::epsijkd * t * cos(del) / tau);
    res[1] = static_cast<value_type>(-RConst::epsijkd * t * sin(del) / tau);
    res[2] = static_cast<value_type>(-RConst::epsijkd * sin(sig) / tau);
    res[3] = alpha;

    if(alpha < 0.0)
    {
      res[0] = -res[0];
      res[1] = -res[1];
      res[2] = -res[2];
      res[3] = -res[3];
    }
  }

  return res;
}

/**: eu2ro
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Euler angles to Rodrigues vector  [Morawiec, page 40]
 *
 * @param e 3 Euler angles in radians
 *
 *
 * @date 8/04/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType eu2ro(const InputType& e)
{
  typename OutputType::value_type thr = 1.0E-6f;

  OutputType res = eu2ax<InputType, OutputType>(e);
  typename OutputType::value_type t = res[3];
  if(std::fabs(t - EbsdLib::Constants::k_PiD) < thr)
  {
    res[3] = std::numeric_limits<typename OutputType::value_type>::infinity();
    return res;
  }

  if(t == 0.0)
  {
    res[0] = 0.0;
    res[1] = 0.0;
    res[2] = 0.0;
    res[3] = 0.0;
  }
  else
  {
    res[3] = static_cast<typename OutputType::value_type>(tan(t * 0.5));
  }
  return res;
}

/**: eu2qu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Euler angles to quaternion  [Morawiec, page 40]
 *
 * @note verified 8/5/13
 *
 * @param e 3 Euler angles in radians
 * @param Quaternion can be of form Scalar<Vector> or <Vector>Scalar in memory. The
 * default is (Scalar, <Vector>)
 *
 * @date 8/04/13   MDG 1.0 original
 * @date 8/07/14   MDG 1.1 verified
 */

template <typename InputType, typename OutputType>
OutputType eu2qu(const InputType& e, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  OutputType res(4);
  using SizeType = typename OutputType::size_type;
  SizeType w = 0;
  SizeType x = 1;
  SizeType y = 2;
  SizeType z = 3;
  if(layout == Quaternion<typename OutputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }
  using OutputValueType = typename OutputType::value_type;
  std::array<OutputValueType, 3> ee = {0.0f, 0.0f, 0.0f};
  OutputValueType cPhi = 0.0f;
  OutputValueType cp = 0.0f;
  OutputValueType cm = 0.0f;
  OutputValueType sPhi = 0.0f;
  OutputValueType sp = 0.0f;
  OutputValueType sm = 0.0f;

  ee[0] = static_cast<OutputValueType>(0.5 * e[0]);
  ee[1] = static_cast<OutputValueType>(0.5 * e[1]);
  ee[2] = static_cast<OutputValueType>(0.5 * e[2]);

  cPhi = cos(ee[1]);
  sPhi = sin(ee[1]);
  cm = cos(ee[0] - ee[2]);
  sm = sin(ee[0] - ee[2]);
  cp = cos(ee[0] + ee[2]);
  sp = sin(ee[0] + ee[2]);
  res[w] = cPhi * cp;
  res[x] = -RConst::epsijk * sPhi * cm;
  res[y] = -RConst::epsijk * sPhi * sm;
  res[z] = -RConst::epsijk * cPhi * sp;

  if(res[w] < 0.0)
  {
    res[w] = -res[w];
    res[x] = -res[x];
    res[y] = -res[y];
    res[z] = -res[z];
  }
  return res;
}

/**: om2eu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief orientation matrix to euler angles
 *
 * @note verified 8/19/14 using Mathematica
 *
 * @param o orientation matrix
 * @param res Euler Angles
 *
 * @date 8/04/13   MDG 1.0 original
 * @date 8/19/14   MDG 1.1 verification using Mathematica
 */
template <typename InputType, typename OutputType>
OutputType om2eu(const InputType& o)
{
  using OutputValueType = typename OutputType::value_type;
  OutputType res(3);
  typename OutputType::value_type zeta = 0.0;
  bool close = EbsdLibMath::closeEnough(std::fabs(o[8]), static_cast<typename OutputType::value_type>(1.0), static_cast<typename OutputType::value_type>(1.0E-6));
  if(!close)
  {
    res[1] = acos(o[8]);
    zeta = static_cast<typename OutputType::value_type>(1.0 / sqrt(1.0 - o[8] * o[8]));
    res[0] = atan2(o[6] * zeta, -o[7] * zeta);
    res[2] = atan2(o[2] * zeta, o[5] * zeta);
  }
  else
  {
    close = EbsdLibMath::closeEnough(o[8], static_cast<typename OutputType::value_type>(1.0), static_cast<typename OutputType::value_type>(1.0E-6));
    if(close)
    {
      res[0] = atan2(o[1], o[0]);
      res[1] = 0.0;
      res[2] = 0.0;
    }
    else
    {
      res[0] = static_cast<OutputValueType>(-atan2(-o[1], o[0]));
      res[1] = static_cast<OutputValueType>(EbsdLib::Constants::k_PiD);
      res[2] = 0.0;
    }
  }

  if(res[0] < 0.0)
  {
    res[0] = static_cast<typename OutputType::value_type>(fmod(res[0] + 100.0 * DConst::k_PiD, DConst::k_2PiD));
  }
  if(res[1] < 0.0)
  {
    res[1] = static_cast<typename OutputType::value_type>(fmod(res[1] + 100.0 * DConst::k_PiD, DConst::k_PiD));
  }
  if(res[2] < 0.0)
  {
    res[2] = static_cast<typename OutputType::value_type>(fmod(res[2] + 100.0 * DConst::k_PiD, DConst::k_2PiD));
  }
  return res;
}

/**: ax2om
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Axis angle pair to orientation matrix
 *
 * @note verified 8/5/13.
 *
 * @param a axis angle pair
 *
 *
 * @date 8/04/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ax2om(const InputType& a)
{
  OutputType res(9);
  using value_type = typename OutputType::value_type;
  value_type q = 0.0L;
  value_type c = 0.0L;
  value_type s = 0.0L;
  value_type omc = 0.0L;

  c = cos(a[3]);
  s = sin(a[3]);

  omc = static_cast<value_type>(1.0 - c);

  res[0] = a[0] * a[0] * omc + c;
  res[4] = a[1] * a[1] * omc + c;
  res[8] = a[2] * a[2] * omc + c;
  int _01 = 1;
  int _10 = 3;
  int _12 = 5;
  int _21 = 7;
  int _02 = 2;
  int _20 = 6;
  // Check to see if we need to transpose
  if(Rotations::Constants::epsijk == 1.0L)
  {
    _01 = 3;
    _10 = 1;
    _12 = 7;
    _21 = 5;
    _02 = 6;
    _20 = 2;
  }

  q = omc * a[0] * a[1];
  res[_01] = q + s * a[2];
  res[_10] = q - s * a[2];
  q = omc * a[1] * a[2];
  res[_12] = q + s * a[0];
  res[_21] = q - s * a[0];
  q = omc * a[2] * a[0];
  res[_02] = q - s * a[1];
  res[_20] = q + s * a[1];

  return res;
}

/**: qu2eu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Quaternion to Euler angles  [Morawiec page 40, with errata !!!! ]
 *
 * @param q quaternion
 *
 *
 * @date 8/04/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType qu2eu(const InputType& q, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  OutputType res(3);
  size_t w = 0;
  size_t x = 1;
  size_t y = 2;
  size_t z = 3;
  if(layout == Quaternion<typename OutputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }

  InputType qq(4);
  using OutputValueType = typename OutputType::value_type;
  OutputValueType q12 = 0.0f;
  OutputValueType q03 = 0.0f;
  OutputValueType chi = 0.0f;
  OutputValueType Phi = 0.0f;
  OutputValueType phi1 = 0.0f;
  OutputValueType phi2 = 0.0f;

  qq = q;

  q03 = qq.w() * qq.w() + qq.z() * qq.z();
  q12 = qq.x() * qq.x() + qq.y() * qq.y();
  chi = sqrt(q03 * q12);
  if(chi == 0.0)
  {
    if(q12 == 0.0)
    {
      if(RConst::epsijk == 1.0)
      {
        Phi = 0.0;
        phi2 = 0.0; // arbitrarily due to degeneracy
        phi1 = static_cast<OutputValueType>(atan2(-2.0 * qq.w() * qq.z(), qq.w() * qq.w() - qq.z() * qq.z()));
      }
      else
      {
        Phi = 0.0;
        phi2 = 0.0; // arbitrarily due to degeneracy
        phi1 = static_cast<OutputValueType>(atan2(2.0 * qq.w() * qq.z(), qq.w() * qq.w() - qq.z() * qq.z()));
      }
    }
    else
    {
      Phi = static_cast<OutputValueType>(EbsdLib::Constants::k_PiD);
      phi2 = 0.0; // arbitrarily due to degeneracy
      phi1 = static_cast<OutputValueType>(atan2(2.0 * qq.x() * qq.y(), qq.x() * qq.x() - qq.y() * qq.y()));
    }
  }
  else
  {
    if(RConst::epsijk == 1.0)
    {
      Phi = static_cast<OutputValueType>(atan2(2.0 * chi, q03 - q12));
      chi = static_cast<OutputValueType>(1.0 / chi);
      phi1 = atan2((-qq.w() * qq.y() + qq.x() * qq.z()) * chi, (-qq.w() * qq.x() - qq.y() * qq.z()) * chi);
      phi2 = atan2((qq.w() * qq.y() + qq.x() * qq.z()) * chi, (-qq.w() * qq.x() + qq.y() * qq.z()) * chi);
    }
    else
    {
      Phi = static_cast<OutputValueType>(atan2(2.0 * chi, q03 - q12));
      chi = static_cast<OutputValueType>(1.0 / chi);
      typename OutputType::value_type y1 = (qq.w() * qq.y() + qq.x() * qq.z()) * chi;
      typename OutputType::value_type x1 = (qq.w() * qq.x() - qq.y() * qq.z()) * chi;
      phi1 = atan2(y1, x1);
      y1 = (-qq.w() * qq.y() + qq.x() * qq.z()) * chi;
      x1 = (qq.w() * qq.x() + qq.y() * qq.z()) * chi;
      phi2 = atan2(y1, x1);
    }
  }

  res[0] = phi1;
  res[1] = Phi;
  res[2] = phi2;

  if(res[0] < 0.0)
  {
    res[0] = static_cast<OutputValueType>(fmod(res[0] + 100.0 * DConst::k_PiD, DConst::k_2PiD));
  }
  if(res[1] < 0.0)
  {
    res[1] = static_cast<OutputValueType>(fmod(res[1] + 100.0 * DConst::k_PiD, DConst::k_PiD));
  }
  if(res[2] < 0.0)
  {
    res[2] = static_cast<OutputValueType>(fmod(res[2] + 100.0 * DConst::k_PiD, DConst::k_2PiD));
  }

  return res;
}

/**: ax2ho
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Axis angle pair to homochoric
 *
 * @param a axis-angle pair
 *
 * !
 * @date 8/04/13   MDG 1.0 originaleu2ho
 */
template <typename InputType, typename OutputType>
OutputType ax2ho(const InputType& a)
{
  OutputType res(3);
  typename OutputType::value_type f = static_cast<typename OutputType::value_type>(0.75 * (a[3] - sin(a[3])));
  f = static_cast<typename OutputType::value_type>(pow(f, (1.0 / 3.0)));
  res[0] = a[0] * f;
  res[1] = a[1] * f;
  res[2] = a[2] * f;
  return res;
}

/**: ho2ax
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Homochoric to axis angle pair
 *
 * @param h homochoric coordinates
 *
 *
 *
 * @date 8/04/13  MDG 1.0 original
 * @date 07/21/14 MDG 1.1 double precision fit coefficients
 */
template <typename InputType, typename OutputType>
OutputType ho2ax(const InputType& h)
{
  OutputType res(4);
  using value_type = typename OutputType::value_type;
  using OMHelperType = ArrayHelpers<OutputType, value_type>;

  value_type thr = 1.0E-8f;

  typename OutputType::value_type hmag = ArrayHelpers<InputType, value_type>::sumofSquares(h);
  if(hmag == 0.0)
  {
    res[0] = 0.0;
    res[1] = 0.0;
    res[2] = 1.0;
    res[3] = 0.0;
  }
  else
  {
    using OutputValueType = typename OutputType::value_type;
    OutputValueType hm = hmag;
    InputType hn = h;
    OutputValueType sqrRtHMag = static_cast<OutputValueType>(1.0 / sqrt(hmag));
    OMHelperType::scalarMultiply(hn, sqrRtHMag); // In place scalar multiply
    OutputValueType s = static_cast<OutputValueType>(LPs::tfit[0] + LPs::tfit[1] * hmag);
    for(int i = 2; i < 16; i++)
    {
      hm = hm * hmag;
      s = static_cast<OutputValueType>(s + LPs::tfit[i] * hm);
    }
    s = static_cast<OutputValueType>(2.0 * acos(s));
    res[0] = hn[0];
    res[1] = hn[1];
    res[2] = hn[2];
    OutputValueType delta = static_cast<OutputValueType>(std::fabs(s - EbsdLib::Constants::k_PiD));
    if(delta < thr)
    {
      res[3] = static_cast<value_type>(EbsdLib::Constants::k_PiD);
    }
    else
    {
      res[3] = s;
    }
  }
  return res;
}

/**: om2qu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert a 3x3 rotation matrix to a unit quaternion (see Morawiec, page 37)
 *
 * @param x 3x3 matrix to be converted
 *
 *
 * @date 8/12/13   MDG 1.0 original
 * @date 8/18/14   MDG 2.0 new version
 */
template <typename InputType, typename OutputType>
OutputType om2qu(const InputType& om, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  OutputType res(4);
  using SizeType = typename OutputType::size_type;
  SizeType w = 0;
  SizeType x = 1;
  SizeType y = 2;
  SizeType z = 3;
  if(layout == Quaternion<typename OutputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }
  using OutputValueType = typename OutputType::value_type;
  OutputValueType thr = static_cast<typename OutputType::value_type>(1.0E-10L);
  if(sizeof(typename InputType::value_type) == 4)
  {
    thr = static_cast<typename OutputType::value_type>(1.0E-6L);
  }
  OutputValueType s = 0.0;
  OutputValueType s1 = 0.0;
  OutputValueType s2 = 0.0;
  OutputValueType s3 = 0.0;

  s = static_cast<OutputValueType>(om[0] + om[4] + om[8] + 1.0);
  if(EbsdLibMath::closeEnough(std::fabs(s), static_cast<typename OutputType::value_type>(0.0), thr)) // Are we close to Zero
  {
    s = 0.0;
  }
  s = sqrt(s);
  s1 = static_cast<OutputValueType>(om[0] - om[4] - om[8] + 1.0);
  if(EbsdLibMath::closeEnough(std::fabs(s1), static_cast<typename OutputType::value_type>(0.0), thr)) // Are we close to Zero
  {
    s1 = 0.0;
  }
  s1 = sqrt(s1);
  s2 = static_cast<OutputValueType>(-om[0] + om[4] - om[8] + 1.0);
  if(EbsdLibMath::closeEnough(std::fabs(s2), static_cast<typename OutputType::value_type>(0.0), thr)) // Are we close to Zero
  {
    s2 = 0.0;
  }
  s2 = sqrt(s2);
  s3 = static_cast<OutputValueType>(-om[0] - om[4] + om[8] + 1.0);
  if(EbsdLibMath::closeEnough(std::fabs(s3), static_cast<typename OutputType::value_type>(0.0), thr)) // Are we close to Zero
  {
    s3 = 0.0;
  }
  s3 = sqrt(s3);
  res[w] = static_cast<OutputValueType>(s * 0.5);
  res[x] = static_cast<OutputValueType>(s1 * 0.5);
  res[y] = static_cast<OutputValueType>(s2 * 0.5);
  res[z] = static_cast<OutputValueType>(s3 * 0.5);
  // printf("res[z]: % 3.16f \n", res[z]);

  // verify the signs (q0 always positive)
  if(om[7] < om[5])
  {
    res[x] = -Rotations::Constants::epsijk * res[x];
  }
  if(om[2] < om[6])
  {
    res[y] = -Rotations::Constants::epsijk * res[y];
  }
  if(om[3] < om[1])
  {
    res[z] = -Rotations::Constants::epsijk * res[z];
  }
  // printf("res[z]: % 3.16f \n", res[z]);

  s = EbsdMatrixMath::Magnitude4x1(&(res[0]));

  if(s != 0.0)
  {
    EbsdMatrixMath::Divide4x1withConstant<typename OutputType::value_type>(&(res[0]), s);
  }

  /* we need to do a quick test here to make sure that the
  ! sign of the vector part is the same as that of the
  ! corresponding vector in the axis-angle representation;
  ! these two can end up being different, presumably due to rounding
  ! issues, but this needs to be further analyzed...
  ! This adds a little bit of computation overhead but for now it
  ! is the easiest way to make sure the signs are correct.
  */
  // om2ax(om, oax);

  InputType eu = om2eu<InputType, InputType>(om);
  InputType oax = eu2ax<InputType, InputType>(eu);

  if(oax[0] * res[x] < 0.0)
  {
    res[x] = -res[x];
  }
  if(oax[1] * res[y] < 0.0)
  {
    res[y] = -res[y];
  }
  if(oax[2] * res[z] < 0.0)
  {
    res[z] = -res[z];
  }
  return res;
}

/**: qu2ax
 *
 * @author Dr. David Rowenhorst, NRL
 *
 * @brief convert quaternion to axis angle
 *
 * @param q quaternion
 * @param res Result Axis-Angle
 * @param layout The ordering of the data: Vector-Scalar or Scalar-Vector
 */
template <typename InputType, typename OutputType>
OutputType qu2ax(const InputType& q, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  using OutputValueType = typename OutputType::value_type;
  OutputType res(4);
  using SizeType = typename OutputType::size_type;
  SizeType w = 0;
  SizeType x = 1;
  SizeType y = 2;
  SizeType z = 3;
  if(layout == Quaternion<typename OutputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }

  OutputValueType epsijk = RConst::epsijkd;
  InputType qo(q);
  // make sure q[0] is >= 0.0
  typename OutputType::value_type sign = 1.0;
  if(q[w] < 0.0)
  {
    sign = -1.0;
  }
  for(int i = 0; i < 4; i++)
  {
    qo[i] = sign * q[i];
  }
  OutputValueType eps = static_cast<OutputValueType>(1.0e-12L);
  OutputValueType omega = static_cast<OutputValueType>(2.0 * acos(qo[w]));
  if(omega < eps)
  {
    res[0] = 0.0;
    res[1] = 0.0;
    res[2] = static_cast<OutputValueType>(1.0 * epsijk);
    res[3] = 0.0;
  }
  else
  {
    typename OutputType::value_type mag = 0.0;
    mag = static_cast<OutputValueType>(1.0 / sqrt(q[x] * q[x] + q[y] * q[y] + q[z] * q[z]));
    res[0] = q[x] * mag;
    res[1] = q[y] * mag;
    res[2] = q[z] * mag;
    res[3] = omega;
  }
  return res;
}

/**: om2ax
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert orientation matrix to axis angle
 *
 * @details this assumes that the matrix represents a passive rotation.
 *
 * @param om 3x3 orientation matrix
 *
 *
 * @date 8/12/13  MDG 1.0 original
 * @date 07/08/14 MDG 2.0 replaced by direct solution
 */
template <typename InputType, typename OutputType>
OutputType om2ax(const InputType& om)
{
  OutputType qu = om2qu<InputType, OutputType>(om);
  return qu2ax<OutputType, OutputType>(qu);
}

/**: ro2ax
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Rodrigues vector to axis angle pair
 *
 * @param r Rodrigues vector
 *
 *
 * @date 8/04/13   MDG 1.0 original
 * @date 8/11/14   MDG 1.1 added infty handling
 */
template <typename InputType, typename OutputType>
OutputType ro2ax(const InputType& r)
{
  using OutputValueType = typename OutputType::value_type;
  OutputType res(4);
  OutputValueType ta = 0.0L;
  OutputValueType angle = 0.0L;

  ta = r[3];
  if(ta == 0.0L)
  {
    res[0] = 0.0L;
    res[1] = 0.0L;
    res[2] = 1.0L;
    res[3] = 0.0L;
    return res;
  }
  if(ta == std::numeric_limits<typename OutputType::value_type>::infinity())
  {
    res[0] = r[0];
    res[1] = r[1];
    res[2] = r[2];
    res[3] = static_cast<OutputValueType>(DConst::k_PiD);
  }
  else
  {
    angle = static_cast<OutputValueType>(2.0L * atan(ta));
    ta = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
    ta = sqrt(ta);
    ta = static_cast<OutputValueType>(1.0L / ta);
    res[0] = r[0] * ta;
    res[1] = r[1] * ta;
    res[2] = r[2] * ta;
    res[3] = angle;
  }
  return res;
}

/**: ax2ro
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert axis angle to Rodrigues
 *
 * @param a axis angle pair
 *
 *
 * @date 8/12/13 MDG 1.0 original
 * @date 7/6/14  MDG 2.0 simplified
 * @date 8/11/14 MDG 2.1 added infty handling
 */
template <typename InputType, typename OutputType>
OutputType ax2ro(const InputType& ax)
{
  OutputType res(4);
  using OutputValueType = typename OutputType::value_type;

  OutputValueType thr = 1.0E-7f;

  if(ax[3] == 0.0)
  {
    res[0] = 0.0;
    res[1] = 0.0;
    res[2] = 0.0;
    res[3] = 0.0;
    return res;
  }
  res[0] = ax[0];
  res[1] = ax[1];
  res[2] = ax[2];
  if(fabs(ax[3] - EbsdLib::Constants::k_PiD) < thr)
  {
    res[3] = std::numeric_limits<typename OutputType::value_type>::infinity();
  }
  else
  {
    res[3] = static_cast<OutputValueType>(tan(ax[3] * 0.5));
  }
  return res;
}

/**: ax2qu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert axis angle to quaternion
 *
 * @param a axis angle pair
 *
 *
 * @date 8/12/13   MDG 1.0 original
 * @date 7/23/14   MDG 1.1 explicit transformation
 */
template <typename InputType, typename OutputType>
OutputType ax2qu(const InputType& r, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  using OutputValueType = typename OutputType::value_type;
  OutputType res(4);
  using SizeType = typename OutputType::size_type;
  SizeType w = 0;
  SizeType x = 1;
  SizeType y = 2;
  SizeType z = 3;
  if(layout == Quaternion<typename OutputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }
  if(r[3] == 0.0)
  {
    res[w] = 1.0;
    res[x] = 0.0;
    res[y] = 0.0;
    res[z] = 0.0;
  }
  else
  {
    typename OutputType::value_type c = static_cast<OutputValueType>(cos(r[3] * 0.5));
    typename OutputType::value_type s = static_cast<OutputValueType>(sin(r[3] * 0.5));
    res[w] = c;
    res[x] = r[0] * s;
    res[y] = r[1] * s;
    res[z] = r[2] * s;
  }
  return res;
}

/**: ro2ho
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert rodrigues to homochoric
 *
 * @param r Rodrigues vector
 *
 *
 * @date 8/12/13   MDG 1.0 original
 * @date 7/24/14   MDG 2.0 explicit transformation
 * @date 8/11/14   MDG 3.0 added infty handling
 */
template <typename InputType, typename OutputType>
OutputType ro2ho(const InputType& r)
{
  OutputType res(3);
  using value_type = typename OutputType::value_type;
  using OMHelperType = ArrayHelpers<OutputType, value_type>;

  value_type f = 0.0;
  value_type rv = OMHelperType::sumofSquares(r);
  if(rv == 0.0)
  {
    OMHelperType::splat(res, 0.0);
    return res;
  }
  if(r[3] == std::numeric_limits<typename OutputType::value_type>::infinity())
  {
    f = static_cast<value_type>(0.75 * EbsdLib::Constants::k_PiD);
  }
  else
  {
    value_type t = static_cast<value_type>(2.0 * std::atan(r[3]));
    f = static_cast<value_type>(0.75 * (t - std::sin(t)));
  }
  f = static_cast<value_type>(pow(f, 1.0 / 3.0));
  res[0] = r[0] * f;
  res[1] = r[1] * f;
  res[2] = r[2] * f;
  return res;
}

/**: qu2om
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert a quaternion to a 3x3 matrix
 *
 * @param q quaternion
 *
 *
 * @note verified 8/5/13
 *
 * @date 6/03/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType qu2om(const InputType& r, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  using OutputValueType = typename OutputType::value_type;
  OutputType res(9);
  using SizeType = typename OutputType::size_type;
  SizeType w = 0;
  SizeType x = 1;
  SizeType y = 2;
  SizeType z = 3;
  if(layout == Quaternion<typename OutputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }
  OutputValueType qq = r[w] * r[w] - (r[x] * r[x] + r[y] * r[y] + r[z] * r[z]);
  res[0] = static_cast<OutputValueType>(qq + 2.0 * r[x] * r[x]);
  res[4] = static_cast<OutputValueType>(qq + 2.0 * r[y] * r[y]);
  res[8] = static_cast<OutputValueType>(qq + 2.0 * r[z] * r[z]);
  res[1] = static_cast<OutputValueType>(2.0 * (r[x] * r[y] - r[w] * r[z]));
  res[5] = static_cast<OutputValueType>(2.0 * (r[y] * r[z] - r[w] * r[x]));
  res[6] = static_cast<OutputValueType>(2.0 * (r[z] * r[x] - r[w] * r[y]));
  res[3] = static_cast<OutputValueType>(2.0 * (r[y] * r[x] + r[w] * r[z]));
  res[7] = static_cast<OutputValueType>(2.0 * (r[z] * r[y] + r[w] * r[x]));
  res[2] = static_cast<OutputValueType>(2.0 * (r[x] * r[z] + r[w] * r[y]));
  if(Rotations::Constants::epsijk != 1.0)
  {
    using value_type = typename OutputType::value_type;
    using RotationMatrixType = Eigen::Matrix<value_type, 3, 3, Eigen::RowMajor>;
    using RotationMatrixMapType = Eigen::Map<RotationMatrixType>;

    RotationMatrixMapType resWrap(const_cast<value_type*>(res.data()));
    resWrap.transpose();
  }
  return res;
}

/**: qu2ro
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert quaternion to Rodrigues
 *
 * @param q quaternion
 *
 * @date 8/12/13   MDG 1.0 original
 * @date 7/23/14   MDG 2.0 direct transformation
 * @date 8/11/14   MDG 2.1 added infty handling
 */
template <typename InputType, typename OutputType>
OutputType qu2ro(const InputType& q, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  OutputType res(4);
  using SizeType = typename OutputType::size_type;
  SizeType w = 0;
  SizeType x = 1;
  SizeType y = 2;
  SizeType z = 3;
  if(layout == Quaternion<typename OutputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }
  typename OutputType::value_type thr = static_cast<typename OutputType::value_type>(1.0E-8L);
  res[0] = q[x];
  res[1] = q[y];
  res[2] = q[z];
  res[3] = 0.0;

  if(q[w] < thr)
  {
    res[3] = std::numeric_limits<typename OutputType::value_type>::infinity();
    return res;
  }
  typename OutputType::value_type s = EbsdMatrixMath::Magnitude3x1(&(res[0]));
  if(s < thr)
  {
    res[0] = 0.0;
    res[1] = 0.0;
    res[2] = 0.0;
    res[3] = 0.0;
    return res;
  }

  res[0] = res[0] / s;
  res[1] = res[1] / s;
  res[2] = res[2] / s;
  res[3] = tan(acos(q[w]));
  return res;
}

/**: qu2ho
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert quaternion to homochoric
 *
 * @param q quaternion
 *
 *
 * @date 8/12/13   MDG 1.0 original
 * @date 7/23/14   MDG 2.0 explicit transformation
 */
template <typename InputType, typename OutputType>
OutputType qu2ho(const InputType& q, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  OutputType res(3);
  using value_type = typename OutputType::value_type;
  using OMHelperType = ArrayHelpers<OutputType, value_type>;

  using SizeType = typename OutputType::size_type;
  SizeType w = 0;
  SizeType x = 1;
  SizeType y = 2;
  SizeType z = 3;
  if(layout == Quaternion<typename OutputType::value_type>::Order::VectorScalar)
  {
    w = 3;
    x = 0;
    y = 1;
    z = 2;
  }
  value_type s;
  value_type f;

  value_type omega = static_cast<value_type>(2.0 * std::acos(q[w]));
  if(omega == 0.0)
  {
    OMHelperType::splat(res, 0.0);
    // res.assign(0.0);
  }
  else
  {
    res[0] = q[x];
    res[1] = q[y];
    res[2] = q[z];
    s = static_cast<value_type>(1.0 / std::sqrt(OMHelperType::sumofSquares(res)));
    OMHelperType::scalarMultiply(res, s);
    f = static_cast<value_type>(0.75 * (omega - std::sin(omega)));
    f = static_cast<value_type>(std::pow(f, 1.0 / 3.0));
    OMHelperType::scalarMultiply(res, f);
  }
  return res;
}

/**: ho2cu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert homochoric to cubochoric
 *
 * @param h homochoric coordinates
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ho2cu(const InputType& q)
{
  int ierr = -1;
  OutputType res(3);
  res = ModifiedLambertProjection3D<InputType, typename InputType::value_type>::LambertBallToCube(q, ierr);
  return res;
}

/**: cu2ho
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert cubochoric to homochoric
 *
 * @param c cubochoric coordinates
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType cu2ho(const InputType& cu)
{
  int ierr = 0;
  OutputType res(3);
  res = ModifiedLambertProjection3D<InputType, typename InputType::value_type>::LambertCubeToBall(cu, ierr);
  return res;
}

/**: ro2om
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert rodrigues to orientation matrix
 *
 * @param r Rodrigues vector
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ro2om(const InputType& ro)
{
  OutputType ax = ro2ax<InputType, OutputType>(ro);
  return ax2om<OutputType, OutputType>(ax);
}

/**: ro2eu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief Rodrigues vector to Euler angles
 *
 * @param r Rodrigues vector
 *
 *
 * @date 8/04/13   MDG 1.0 original
 * @date 8/11/14   MDG 1.1 added infty handling
 */
template <typename InputType, typename OutputType>
OutputType ro2eu(const InputType& ro)
{
  OutputType om = ro2om<InputType, OutputType>(ro);
  return om2eu<OutputType, OutputType>(om);
}

/**: eu2ho
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert euler to homochoric
 *
 * @param e 3 euler angles
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType eu2ho(const InputType& eu)
{
  OutputType ax = eu2ax<InputType, OutputType>(eu);
  return ax2ho<OutputType, OutputType>(ax);
}

/**: om2ro
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert orientation matrix to Rodrigues
 *
 * @param om 3x3 orientation matrix
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType om2ro(const InputType& om)
{
  OutputType eu = om2eu<InputType, OutputType>(om); // Convert the OM to Euler
  return eu2ro<OutputType, OutputType>(eu);         // Convert Euler to Rodrigues
}

/**: om2ho
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert orientation matrix to homochoric
 *
 * @param om 3x3 orientation matrix
 *
 *
 * @date 8/12/13   MDG 1.0 original
 * @date 07/08/14 MDG 2.0 simplification via ax (shorter path)
 */
template <typename InputType, typename OutputType>
OutputType om2ho(const InputType& om)
{
  OutputType ax = om2ax<InputType, OutputType>(om); // Convert the OM to Axis-Angles
  return ax2ho<OutputType, OutputType>(ax);         // Convert Axis-Angles to Homochoric
}

/**: ax2eu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert axis angle to euler
 *
 * @param a axis angle pair
 *
 *
 * @date 8/12/13   MDG 1.0 original
 * @date 07/08/14 MDG 2.0 simplification via ro (shorter path)
 */
template <typename InputType, typename OutputType>
OutputType ax2eu(const InputType& ax)
{
  OutputType om = ax2om<InputType, OutputType>(ax);
  return om2eu<OutputType, OutputType>(om);
}

/**: ro2qu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert rodrigues to quaternion
 *
 * @param r Rodrigues vector
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ro2qu(const InputType& ro, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  OutputType ax = ro2ax<InputType, OutputType>(ro);
  return ax2qu<OutputType, OutputType>(ax, layout);
}

/**: ho2eu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert homochoric to euler
 *
 * @param h homochoric coordinates
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ho2eu(const InputType& ho)
{
  OutputType ax = ho2ax<InputType, OutputType>(ho);
  return ax2eu<OutputType, OutputType>(ax);
}

/**: ho2om
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert homochoric to orientation matrix
 *
 * @param h homochoric coordinates
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ho2om(const InputType& ho)
{
  OutputType ax = ho2ax<InputType, OutputType>(ho);
  return ax2om<OutputType, OutputType>(ax);
}

/**: ho2ro
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert homochoric to Rodrigues
 *
 * @param h homochoric coordinates
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ho2ro(const InputType& ho)
{
  OutputType ax = ho2ax<InputType, OutputType>(ho);
  return ax2ro<OutputType, OutputType>(ax);
}

/**: ho2qu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert homochoric to quaternion
 *
 * @param r homochoric coordinates
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ho2qu(const InputType& ho, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  InputType ax = ho2ax<InputType, InputType>(ho);
  return ax2qu<InputType, OutputType>(ax, layout);
}

/**: eu2cu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert euler angles to cubochoric
 *
 * @param e euler angles
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType eu2cu(const InputType& eu)
{
  OutputType ho = eu2ho<InputType, OutputType>(eu);
  return ho2cu<OutputType, OutputType>(ho);
}

/**: om2cu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert orientation matrix to cubochoric
 *
 * @param o orientation matrix
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType om2cu(const InputType& om)
{
  OutputType ho = om2ho<InputType, OutputType>(om);
  return ho2cu<OutputType, OutputType>(ho);
}

/**: ax2cu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert axis angle to cubochoric
 *
 * @param a axis angle
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */

template <typename InputType, typename OutputType>
OutputType ax2cu(const InputType& ax)
{
  OutputType ho = ax2ho<InputType, OutputType>(ax);
  return ho2cu<OutputType, OutputType>(ho);
}

/**: ro2cu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert Rodrigues to cubochoric
 *
 * @param r Rodrigues
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType ro2cu(const InputType& ro)
{
  OutputType ho = ro2ho<InputType, OutputType>(ro);
  return ho2cu<OutputType, OutputType>(ho);
}

/**: qu2cu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert quaternion to cubochoric
 *
 * @param q quaternion
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */
template <typename InputType, typename OutputType>
OutputType qu2cu(const InputType& qu, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  OutputType ho = qu2ho<InputType, OutputType>(qu, layout);
  return ho2cu<OutputType, OutputType>(ho);
}

/**: cu2eu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert cubochoric to euler angles
 *
 * @param c cubochoric coordinates
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */

template <typename InputType, typename OutputType>
OutputType cu2eu(const InputType& cu)
{
  OutputType ho = cu2ho<InputType, OutputType>(cu);
  return ho2eu<OutputType, OutputType>(ho);
}

/**: cu2om
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert cubochoric to orientation matrix
 *
 * @param c cubochoric coordinates
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */

template <typename InputType, typename OutputType>
OutputType cu2om(const InputType& cu)
{
  OutputType ho = cu2ho<InputType, OutputType>(cu);
  return ho2om<OutputType, OutputType>(ho);
}

/**: cu2ax
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert cubochoric to axis angle
 *
 * @param c cubochoric coordinates
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */

template <typename InputType, typename OutputType>
OutputType cu2ax(const InputType& cu)
{
  OutputType ho = cu2ho<InputType, OutputType>(cu);
  return ho2ax<OutputType, OutputType>(ho);
}

/**: cu2ro
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert cubochoric to Rodrigues
 *
 * @param c cubochoric coordinates
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */

template <typename InputType, typename OutputType>
OutputType cu2ro(const InputType& cu)
{
  OutputType ho = cu2ho<InputType, OutputType>(cu);
  return ho2ro<OutputType, OutputType>(ho);
}

/**: cu2qu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief convert cubochoric to quaternion
 *
 * @param c cubochoric coordinates
 *
 *
 *
 * @date 8/12/13   MDG 1.0 original
 */

template <typename InputType, typename OutputType>
OutputType cu2qu(const InputType& cu, typename Quaternion<typename OutputType::value_type>::Order layout = Quaternion<typename OutputType::value_type>::Order::VectorScalar)
{
  InputType ho = cu2ho<InputType, InputType>(cu); // Convert the Cuborchoric to Homochoric
  return ho2qu<InputType, OutputType>(ho);        // Convert Homochoric to Quaternion
}

/**: RotVec_om
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief rotate a vector using a rotation matrix, active or passive
 *
 * @details This routine provides a way for the user to transform a vector
 * and it returns the new vector components.  The user can use either a
 * rotation matrix or a quaternion to define the transformation, and must
 * also specifiy whether an active or passive result is needed.
 *
 *
 * @param vec input vector components
 * @param om orientation matrix
 * @param ap active/passive switch
 *
 *
 * @date 8/18/14   MDG 1.0 original
 */

void RotVec_om(float* vec, float* om, char ap, float* res);

/**: RotVec_qu
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief rotate a vector using a quaternion, active or passive
 *
 * @details This routine provides a way for the user to transform a vector
 * and it returns the new vector components.  The user can use either a
 * rotation matrix or a quaternion to define the transformation, and must
 * also specifiy whether an active or passive result is needed.
 *
 *
 * @param vec input vector components
 * @param qu quaternion
 * @param ap active/passive switch
 *
 *
 * @date 8/18/14   MDG 1.0 original
 */

void RotVec_qu(float* vec, float* qu, char ap, float* res);

/**: RotTensor2_om
 *
 * @author Marc De Graef, Carnegie Mellon University
 *
 * @brief rotate a second rank tensor using a rotation matrix, active or passive
 *
 * @param tensor input tensor components
 * @param om orientation matrix
 * @param ap active/passive switch
 *
 *
 * @date 8/18/14   MDG 1.0 original
 */

void RotTensor2_om(float* tensor, float* om, char ap, float* res);

/**
* SUBROUTINE: print_orientation
              *
              * @author Marc De Graef, Carnegie Mellon University
              *
              * @brief  prints a complete orientationtype record or a single entry
              *
              * @param o orientationtype record
              * @param outtype (optional) indicates which representation to print
* @param pretext (optional) up to 10 characters that will precede each line
*
* @date  8/4/13   MDG 1.0 original
print the entire record with all representations

* SUBROUTINE: print_orientation_d
*
* @author Marc De Graef, Carnegie Mellon University
                         *
                         * @brief  prints a complete orientationtype record or a single entry (double precision)
*
* @param o orientationtype record
* @param outtype (optional) indicates which representation to print
* @param pretext (optional) up to 10 characters that will precede each line
*
* @date  8/4/13   MDG 1.0 original
print the entire record with all representations
*/

} // namespace OrientationTransformation
