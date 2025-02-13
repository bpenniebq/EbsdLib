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

#include "CubicOps.h"

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/partitioner.h>
#include <tbb/task.h>
#include <tbb/task_group.h>
#endif

// Include this FIRST because there is a needed define for some compiles
// to expose some of the constants needed below
#include "EbsdLib/Core/EbsdMacros.h"
#include "EbsdLib/Core/Orientation.hpp"
#include "EbsdLib/Math/EbsdLibMath.h"
#include "EbsdLib/Math/GeometryMath.h"
#include "EbsdLib/Utilities/ColorUtilities.h"
#include "EbsdLib/Utilities/ComputeStereographicProjection.h"

namespace CubicHigh
{

static const std::array<size_t, 3> OdfNumBins = {18, 18, 18}; // Represents a 5Deg bin
static const std::array<double, 3> OdfDimInitValue = {std::pow((0.75 * (EbsdLib::Constants::k_PiOver4D - std::sin(EbsdLib::Constants::k_PiOver4D))), (1.0 / 3.0)),
                                                      std::pow((0.75 * (EbsdLib::Constants::k_PiOver4D - std::sin(EbsdLib::Constants::k_PiOver4D))), (1.0 / 3.0)),
                                                      std::pow((0.75 * (EbsdLib::Constants::k_PiOver4D - std::sin(EbsdLib::Constants::k_PiOver4D))), (1.0 / 3.0))};

static const std::array<double, 3> OdfDimStepValue = {OdfDimInitValue[0] / static_cast<double>(OdfNumBins[0] / 2), OdfDimInitValue[1] / static_cast<double>(OdfNumBins[1] / 2),
                                                      OdfDimInitValue[2] / static_cast<double>(OdfNumBins[2] / 2)};

static const int symSize0 = 6;
static const int symSize1 = 12;
static const int symSize2 = 8;

static const int k_OdfSize = 5832;
static const int k_MdfSize = 5832;
static const int k_SymOpsCount = 24;
static const int k_NumMdfBins = 13;

static const std::vector<QuatD> QuatSym = {QuatD(0.000000000, 0.000000000, 0.000000000, 1.000000000),
                                           QuatD(1.000000000, 0.000000000, 0.000000000, 0.000000000),
                                           QuatD(0.000000000, 1.000000000, 0.000000000, 0.000000000),
                                           QuatD(0.000000000, 0.000000000, 1.000000000, 0.000000000),
                                           QuatD(EbsdLib::Constants::k_1OverRoot2D, 0.000000000, 0.000000000, EbsdLib::Constants::k_1OverRoot2D),
                                           QuatD(0.000000000, EbsdLib::Constants::k_1OverRoot2D, 0.000000000, EbsdLib::Constants::k_1OverRoot2D),
                                           QuatD(0.000000000, 0.000000000, EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D),
                                           QuatD(-EbsdLib::Constants::k_1OverRoot2D, 0.000000000, 0.000000000, EbsdLib::Constants::k_1OverRoot2D),
                                           QuatD(0.000000000, -EbsdLib::Constants::k_1OverRoot2D, 0.000000000, EbsdLib::Constants::k_1OverRoot2D),
                                           QuatD(0.000000000, 0.000000000, -EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D),
                                           QuatD(EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D, 0.000000000, 0.000000000),
                                           QuatD(-EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D, 0.000000000, 0.000000000),
                                           QuatD(0.000000000, EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D, 0.000000000),
                                           QuatD(0.000000000, -EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D, 0.000000000),
                                           QuatD(EbsdLib::Constants::k_1OverRoot2D, 0.000000000, EbsdLib::Constants::k_1OverRoot2D, 0.000000000),
                                           QuatD(-EbsdLib::Constants::k_1OverRoot2D, 0.000000000, EbsdLib::Constants::k_1OverRoot2D, 0.000000000),
                                           QuatD(0.500000000, 0.500000000, 0.500000000, 0.500000000),
                                           QuatD(-0.500000000, -0.500000000, -0.500000000, 0.500000000),
                                           QuatD(0.500000000, -0.500000000, 0.500000000, 0.500000000),
                                           QuatD(-0.500000000, 0.500000000, -0.500000000, 0.500000000),
                                           QuatD(-0.500000000, 0.500000000, 0.500000000, 0.500000000),
                                           QuatD(0.500000000, -0.500000000, -0.500000000, 0.500000000),
                                           QuatD(-0.500000000, -0.500000000, 0.500000000, 0.500000000),
                                           QuatD(0.500000000, 0.500000000, -0.500000000, 0.500000000)};

static const std::vector<OrientationD> RodSym = {{0.0, 0.0, 0.0},
                                                 {10000000000.0, 0.0, 0.0},
                                                 {0.0, 10000000000.0, 0.0},
                                                 {0.0, 0.0, 10000000000.0},
                                                 {1.0, 0.0, 0.0},
                                                 {0.0, 1.0, 0.0},
                                                 {0.0, 0.0, 1.0},
                                                 {-1.0, 0.0, 0.0},
                                                 {0.0, -1.0, 0.0},
                                                 {0.0, 0.0, -1.0},
                                                 {10000000000.0, 10000000000.0, 0.0},
                                                 {-10000000000.0, 10000000000.0, 0.0},
                                                 {0.0, 10000000000.0, 10000000000.0},
                                                 {0.0, -10000000000.0, 10000000000.0},
                                                 {10000000000.0, 0.0, 10000000000.0},
                                                 {-10000000000.0, 0.0, 10000000000.0},
                                                 {1.0, 1.0, 1.0},
                                                 {-1.0, -1.0, -1.0},
                                                 {1.0, -1.0, 1.0},
                                                 {-1.0, 1.0, -1.0},
                                                 {-1.0, 1.0, 1.0},
                                                 {1.0, -1.0, -1.0},
                                                 {-1.0, -1.0, 1.0},
                                                 {1.0, 1.0, -1.0}};

static const double SlipDirections[12][3] = {{0.0, 1.0, -1.0}, {1.0, 0.0, -1.0}, {1.0, -1.0, 0.0}, {1.0, -1.0, 0.0}, {1.0, 0.0, 1.0}, {0.0, 1.0, 1.0},
                                             {1.0, 1.0, 0.0},  {0.0, 1.0, 1.0},  {1.0, 0.0, -1.0}, {1.0, 1.0, 0.0},  {1.0, 0.0, 1.0}, {0.0, 1.0, -1.0}};

static const double SlipPlanes[12][3] = {{1.0, 1.0, 1.0},  {1.0, 1.0, 1.0},  {1.0, 1.0, 1.0},  {1.0, 1.0, -1.0}, {1.0, 1.0, -1.0}, {1.0, 1.0, -1.0},
                                         {1.0, -1.0, 1.0}, {1.0, -1.0, 1.0}, {1.0, -1.0, 1.0}, {-1.0, 1.0, 1.0}, {-1.0, 1.0, 1.0}, {-1.0, 1.0, 1.0}};

static const double MatSym[k_SymOpsCount][3][3] = {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.0, 1.0, 0.0}},

                                                   {{1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}},

                                                   {{1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, -1.0, 0.0}},

                                                   {{0.0, 0.0, -1.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}},

                                                   {{-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}},

                                                   {{-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{0.0, -1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}},

                                                   {{0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}, {1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, -1.0}, {1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}},

                                                   {{0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},

                                                   {{0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},

                                                   {{0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}},

                                                   {{-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}},

                                                   {{0.0, 0.0, 1.0}, {0.0, -1.0, 0.0}, {1.0, 0.0, 0.0}},

                                                   {{-1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.0, -1.0, 0.0}},

                                                   {{0.0, 0.0, -1.0}, {0.0, -1.0, 0.0}, {-1.0, 0.0, 0.0}},

                                                   {{0.0, -1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}}};

} // namespace CubicHigh

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
CubicOps::CubicOps() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
CubicOps::~CubicOps() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool CubicOps::getHasInversion() const
{
  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicOps::getODFSize() const
{
  return CubicHigh::k_OdfSize;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicOps::getMDFSize() const
{
  return CubicHigh::k_MdfSize;
}

// -----------------------------------------------------------------------------
int CubicOps::getMdfPlotBins() const
{
  return CubicHigh::k_NumMdfBins;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicOps::getNumSymOps() const
{
  return CubicHigh::k_SymOpsCount;
}

// -----------------------------------------------------------------------------
std::array<size_t, 3> CubicOps::getOdfNumBins() const
{
  return CubicHigh::OdfNumBins;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
std::string CubicOps::getSymmetryName() const
{
  return "Cubic m3m";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationD CubicOps::calculateMisorientation(const QuatD& q1, const QuatD& q2) const
{
  return calculateMisorientationInternal(CubicHigh::QuatSym, q1, q2);
}

// -----------------------------------------------------------------------------
OrientationF CubicOps::calculateMisorientation(const QuatF& q1f, const QuatF& q2f) const

{
  QuatD q1 = q1f.to<double>();
  QuatD q2 = q2f.to<double>();
  OrientationD axisAngle = calculateMisorientationInternal(CubicHigh::QuatSym, q1, q2);
  return axisAngle;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationD CubicOps::calculateMisorientationInternal(const std::vector<QuatD>& quatsym, const QuatD& q1, const QuatD& q2) const
{
  double wmin = 9999999.0f; //,na,nb,nc;
  QuatD qco;
  int type = 1;
  double sin_wmin_over_2 = 0.0;

  QuatD qc = q1 * (q2.conjugate());
  qc.elementWiseAbs();

  // if qc.x() is smallest
  if(qc.x() <= qc.y() && qc.x() <= qc.z() && qc.x() <= qc.w())
  {
    qco.x() = qc.x();
    // if qc.y() is next smallest
    if(qc.y() <= qc.z() && qc.y() <= qc.w())
    {
      qco.y() = qc.y();
      if(qc.z() <= qc.w())
      {
        qco.z() = qc.z(), qco.w() = qc.w();
      }
      else
      {
        qco.z() = qc.w(), qco.w() = qc.z();
      }
    }
    // if qc.z() is next smallest
    else if(qc.z() <= qc.y() && qc.z() <= qc.w())
    {
      qco.y() = qc.z();
      if(qc.y() <= qc.w())
      {
        qco.z() = qc.y(), qco.w() = qc.w();
      }
      else
      {
        qco.z() = qc.w(), qco.w() = qc.y();
      }
    }
    // if qc.w() is next smallest
    else
    {
      qco.y() = qc.w();
      if(qc.y() <= qc.z())
      {
        qco.z() = qc.y(), qco.w() = qc.z();
      }
      else
      {
        qco.z() = qc.z(), qco.w() = qc.y();
      }
    }
  }
  // if qc.y() is smallest
  else if(qc.y() <= qc.x() && qc.y() <= qc.z() && qc.y() <= qc.w())
  {
    qco.x() = qc.y();
    // if qc.x() is next smallest
    if(qc.x() <= qc.z() && qc.x() <= qc.w())
    {
      qco.y() = qc.x();
      if(qc.z() <= qc.w())
      {
        qco.z() = qc.z(), qco.w() = qc.w();
      }
      else
      {
        qco.z() = qc.w(), qco.w() = qc.z();
      }
    }
    // if qc.z() is next smallest
    else if(qc.z() <= qc.x() && qc.z() <= qc.w())
    {
      qco.y() = qc.z();
      if(qc.x() <= qc.w())
      {
        qco.z() = qc.x(), qco.w() = qc.w();
      }
      else
      {
        qco.z() = qc.w(), qco.w() = qc.x();
      }
    }
    // if qc.w() is next smallest
    else
    {
      qco.y() = qc.w();
      if(qc.x() <= qc.z())
      {
        qco.z() = qc.x(), qco.w() = qc.z();
      }
      else
      {
        qco.z() = qc.z(), qco.w() = qc.x();
      }
    }
  }
  // if qc.z() is smallest
  else if(qc.z() <= qc.x() && qc.z() <= qc.y() && qc.z() <= qc.w())
  {
    qco.x() = qc.z();
    // if qc.x() is next smallest
    if(qc.x() <= qc.y() && qc.x() <= qc.w())
    {
      qco.y() = qc.x();
      if(qc.y() <= qc.w())
      {
        qco.z() = qc.y(), qco.w() = qc.w();
      }
      else
      {
        qco.z() = qc.w(), qco.w() = qc.y();
      }
    }
    // if qc.y() is next smallest
    else if(qc.y() <= qc.x() && qc.y() <= qc.w())
    {
      qco.y() = qc.y();
      if(qc.x() <= qc.w())
      {
        qco.z() = qc.x(), qco.w() = qc.w();
      }
      else
      {
        qco.z() = qc.w(), qco.w() = qc.x();
      }
    }
    // if qc.w() is next smallest
    else
    {
      qco.y() = qc.w();
      if(qc.x() <= qc.y())
      {
        qco.z() = qc.x(), qco.w() = qc.y();
      }
      else
      {
        qco.z() = qc.y(), qco.w() = qc.x();
      }
    }
  }
  // if qc.w() is smallest
  else
  {
    qco.x() = qc.w();
    // if qc.x() is next smallest
    if(qc.x() <= qc.y() && qc.x() <= qc.z())
    {
      qco.y() = qc.x();
      if(qc.y() <= qc.z())
      {
        qco.z() = qc.y(), qco.w() = qc.z();
      }
      else
      {
        qco.z() = qc.z(), qco.w() = qc.y();
      }
    }
    // if qc.y() is next smallest
    else if(qc.y() <= qc.x() && qc.y() <= qc.z())
    {
      qco.y() = qc.y();
      if(qc.x() <= qc.z())
      {
        qco.z() = qc.x(), qco.w() = qc.z();
      }
      else
      {
        qco.z() = qc.z(), qco.w() = qc.x();
      }
    }
    // if qc.z() is next smallest
    else
    {
      qco.y() = qc.z();
      if(qc.x() <= qc.y())
      {
        qco.z() = qc.x(), qco.w() = qc.y();
      }
      else
      {
        qco.z() = qc.y(), qco.w() = qc.x();
      }
    }
  }
  wmin = qco.w();
  if(((qco.z() + qco.w()) / (EbsdLib::Constants::k_Sqrt2D)) > wmin)
  {
    wmin = ((qco.z() + qco.w()) / (EbsdLib::Constants::k_Sqrt2D));
    type = 2;
  }
  if(((qco.x() + qco.y() + qco.z() + qco.w()) / 2) > wmin)
  {
    wmin = ((qco.x() + qco.y() + qco.z() + qco.w()) / 2);
    type = 3;
  }
  if(wmin < -1.0)
  {
    //  wmin = -1.0;
    wmin = EbsdLib::Constants::k_ACosNeg1D;
    sin_wmin_over_2 = std::sin(wmin);
  }
  else if(wmin > 1.0)
  {
    //   wmin = 1.0;
    wmin = EbsdLib::Constants::k_ACos1D;
    sin_wmin_over_2 = std::sin(wmin);
  }
  else
  {
    wmin = acos(wmin);
    sin_wmin_over_2 = std::sin(wmin);
  }

  double n1 = 0.0;
  double n2 = 0.0;
  double n3 = 0.0;
  if(type == 1)
  {
    n1 = qco.x() / sin_wmin_over_2;
    n2 = qco.y() / sin_wmin_over_2;
    n3 = qco.z() / sin_wmin_over_2;
  }
  if(type == 2)
  {
    n1 = ((qco.x() - qco.y()) / (EbsdLib::Constants::k_Sqrt2D)) / sin_wmin_over_2;
    n2 = ((qco.x() + qco.y()) / (EbsdLib::Constants::k_Sqrt2D)) / sin_wmin_over_2;
    n3 = ((qco.z() - qco.w()) / (EbsdLib::Constants::k_Sqrt2D)) / sin_wmin_over_2;
  }
  if(type == 3)
  {
    n1 = ((qco.x() - qco.y() + qco.z() - qco.w()) / (2.0)) / sin_wmin_over_2;
    n2 = ((qco.x() + qco.y() - qco.z() - qco.w()) / (2.0)) / sin_wmin_over_2;
    n3 = ((-qco.x() + qco.y() + qco.z() - qco.w()) / (2.0)) / sin_wmin_over_2;
  }
  double denom = sqrt((n1 * n1 + n2 * n2 + n3 * n3));
  n1 = n1 / denom;
  n2 = n2 / denom;
  n3 = n3 / denom;
  if(denom == 0)
  {
    n1 = 0.0, n2 = 0.0, n3 = 1.0;
  }
  if(wmin == 0)
  {
    n1 = 0.0, n2 = 0.0, n3 = 1.0;
  }
  wmin = 2.0f * wmin;

  OrientationD axisAngle(n1, n2, n3, wmin);
  return axisAngle;
}

QuatD CubicOps::getQuatSymOp(int32_t i) const
{
  return CubicHigh::QuatSym[i];
}

void CubicOps::getRodSymOp(int i, double* r) const
{
  r[0] = CubicHigh::RodSym[i][0];
  r[1] = CubicHigh::RodSym[i][1];
  r[2] = CubicHigh::RodSym[i][2];
}

void CubicOps::getMatSymOp(int i, double g[3][3]) const
{
  g[0][0] = CubicHigh::MatSym[i][0][0];
  g[0][1] = CubicHigh::MatSym[i][0][1];
  g[0][2] = CubicHigh::MatSym[i][0][2];
  g[1][0] = CubicHigh::MatSym[i][1][0];
  g[1][1] = CubicHigh::MatSym[i][1][1];
  g[1][2] = CubicHigh::MatSym[i][1][2];
  g[2][0] = CubicHigh::MatSym[i][2][0];
  g[2][1] = CubicHigh::MatSym[i][2][1];
  g[2][2] = CubicHigh::MatSym[i][2][2];
}

void CubicOps::getMatSymOp(int i, float g[3][3]) const
{
  g[0][0] = static_cast<float>(CubicHigh::MatSym[i][0][0]);
  g[0][1] = static_cast<float>(CubicHigh::MatSym[i][0][1]);
  g[0][2] = static_cast<float>(CubicHigh::MatSym[i][0][2]);
  g[1][0] = static_cast<float>(CubicHigh::MatSym[i][1][0]);
  g[1][1] = static_cast<float>(CubicHigh::MatSym[i][1][1]);
  g[1][2] = static_cast<float>(CubicHigh::MatSym[i][1][2]);
  g[2][0] = static_cast<float>(CubicHigh::MatSym[i][2][0]);
  g[2][1] = static_cast<float>(CubicHigh::MatSym[i][2][1]);
  g[2][2] = static_cast<float>(CubicHigh::MatSym[i][2][2]);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicOps::getODFFZRod(const OrientationType& rod) const
{
  return _calcRodNearestOrigin(CubicHigh::RodSym, rod);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicOps::getMDFFZRod(const OrientationType& inRod) const
{
  double w, n1, n2, n3;
  double FZw, FZn1, FZn2, FZn3;

  OrientationType rod = _calcRodNearestOrigin(CubicHigh::RodSym, inRod);
  OrientationType ax = OrientationTransformation::ro2ax<OrientationType, OrientationType>(rod);

  n1 = ax[0];
  n2 = ax[1], n3 = ax[2], w = ax[3];

  FZw = w;
  n1 = std::fabs(n1);
  n2 = std::fabs(n2);
  n3 = std::fabs(n3);
  if(n1 > n2)
  {
    if(n1 > n3)
    {
      FZn1 = n1;
      if(n2 > n3)
      {
        FZn2 = n2, FZn3 = n3;
      }
      else
      {
        FZn2 = n3, FZn3 = n2;
      }
    }
    else
    {
      FZn1 = n3, FZn2 = n1, FZn3 = n2;
    }
  }
  else
  {
    if(n2 > n3)
    {
      FZn1 = n2;
      if(n1 > n3)
      {
        FZn2 = n1, FZn3 = n3;
      }
      else
      {
        FZn2 = n3, FZn3 = n1;
      }
    }
    else
    {
      FZn1 = n3, FZn2 = n2, FZn3 = n1;
    }
  }

  return OrientationTransformation::ax2ro<OrientationType, OrientationType>(OrientationType(FZn1, FZn2, FZn3, FZw));
}

QuatD CubicOps::getNearestQuat(const QuatD& q1, const QuatD& q2) const
{
  return _calcNearestQuat(CubicHigh::QuatSym, q1, q2);
}

QuatF CubicOps::getNearestQuat(const QuatF& q1f, const QuatF& q2f) const
{
  return _calcNearestQuat(CubicHigh::QuatSym, q1f.to<double>(), q2f.to<double>()).to<float>();
}

QuatD CubicOps::getFZQuat(const QuatD& qr) const
{
  return _calcQuatNearestOrigin(CubicHigh::QuatSym, qr);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicOps::getMisoBin(const OrientationType& rod) const
{
  double dim[3];
  double bins[3];
  double step[3];

  OrientationType ho = OrientationTransformation::ro2ho<OrientationType, OrientationType>(rod);

  dim[0] = CubicHigh::OdfDimInitValue[0];
  dim[1] = CubicHigh::OdfDimInitValue[1];
  dim[2] = CubicHigh::OdfDimInitValue[2];
  step[0] = CubicHigh::OdfDimStepValue[0];
  step[1] = CubicHigh::OdfDimStepValue[1];
  step[2] = CubicHigh::OdfDimStepValue[2];
  bins[0] = static_cast<double>(CubicHigh::OdfNumBins[0]);
  bins[1] = static_cast<double>(CubicHigh::OdfNumBins[1]);
  bins[2] = static_cast<double>(CubicHigh::OdfNumBins[2]);

  return _calcMisoBin(dim, bins, step, ho);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicOps::determineEulerAngles(double random[3], int choose) const
{
  double init[3];
  double step[3];
  int32_t phi[3];
  double h1, h2, h3;

  init[0] = CubicHigh::OdfDimInitValue[0];
  init[1] = CubicHigh::OdfDimInitValue[1];
  init[2] = CubicHigh::OdfDimInitValue[2];
  step[0] = CubicHigh::OdfDimStepValue[0];
  step[1] = CubicHigh::OdfDimStepValue[1];
  step[2] = CubicHigh::OdfDimStepValue[2];
  phi[0] = static_cast<int32_t>(choose % CubicHigh::OdfNumBins[0]);
  phi[1] = static_cast<int32_t>((choose / CubicHigh::OdfNumBins[0]) % CubicHigh::OdfNumBins[1]);
  phi[2] = static_cast<int32_t>(choose / (CubicHigh::OdfNumBins[0] * CubicHigh::OdfNumBins[1]));

  _calcDetermineHomochoricValues(random, init, step, phi, h1, h2, h3);

  OrientationType ho(h1, h2, h3);
  OrientationType ro = OrientationTransformation::ho2ro<OrientationType, OrientationType>(ho);
  ro = getODFFZRod(ro);
  OrientationType eu = OrientationTransformation::ro2eu<OrientationType, OrientationType>(ro);
  return eu;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicOps::randomizeEulerAngles(const OrientationType& synea) const
{
  size_t symOp = getRandomSymmetryOperatorIndex(CubicHigh::k_SymOpsCount);
  QuatD quat = OrientationTransformation::eu2qu<OrientationType, QuatD>(synea);
  QuatD qc = CubicHigh::QuatSym[symOp] * quat;
  return OrientationTransformation::qu2eu<QuatD, OrientationType>(qc);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicOps::determineRodriguesVector(double random[3], int choose) const
{
  double init[3];
  double step[3];
  int32_t phi[3];
  double h1, h2, h3;

  init[0] = CubicHigh::OdfDimInitValue[0];
  init[1] = CubicHigh::OdfDimInitValue[1];
  init[2] = CubicHigh::OdfDimInitValue[2];
  step[0] = CubicHigh::OdfDimStepValue[0];
  step[1] = CubicHigh::OdfDimStepValue[1];
  step[2] = CubicHigh::OdfDimStepValue[2];
  phi[0] = static_cast<int32_t>(choose % CubicHigh::OdfNumBins[0]);
  phi[1] = static_cast<int32_t>((choose / CubicHigh::OdfNumBins[0]) % CubicHigh::OdfNumBins[1]);
  phi[2] = static_cast<int32_t>(choose / (CubicHigh::OdfNumBins[0] * CubicHigh::OdfNumBins[1]));

  _calcDetermineHomochoricValues(random, init, step, phi, h1, h2, h3);
  OrientationType ho(h1, h2, h3);
  OrientationType ro = OrientationTransformation::ho2ro<OrientationType, OrientationType>(ho);
  ro = getMDFFZRod(ro);
  return ro;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicOps::getOdfBin(const OrientationType& rod) const
{
  double dim[3];
  double bins[3];
  double step[3];

  OrientationType ho = OrientationTransformation::ro2ho<OrientationType, OrientationType>(rod);

  dim[0] = CubicHigh::OdfDimInitValue[0];
  dim[1] = CubicHigh::OdfDimInitValue[1];
  dim[2] = CubicHigh::OdfDimInitValue[2];
  step[0] = CubicHigh::OdfDimStepValue[0];
  step[1] = CubicHigh::OdfDimStepValue[1];
  step[2] = CubicHigh::OdfDimStepValue[2];
  bins[0] = static_cast<double>(CubicHigh::OdfNumBins[0]);
  bins[1] = static_cast<double>(CubicHigh::OdfNumBins[1]);
  bins[2] = static_cast<double>(CubicHigh::OdfNumBins[2]);

  return _calcODFBin(dim, bins, step, ho);
}

void CubicOps::getSchmidFactorAndSS(double load[3], double& schmidfactor, double angleComps[2], int& slipsys) const
{
  schmidfactor = 0.0;
  double theta1, theta2, theta3, theta4;
  double lambda1, lambda2, lambda3, lambda4, lambda5, lambda6;
  double schmid1, schmid2, schmid3, schmid4, schmid5, schmid6, schmid7, schmid8, schmid9, schmid10, schmid11, schmid12;

  double loadx = load[0];
  double loady = load[1];
  double loadz = load[2];

  double mag = loadx * loadx + loady * loady + loadz * loadz;
  mag = std::sqrt(mag);
  theta1 = (loadx + loady + loadz) / (mag * 1.732f);
  theta1 = std::fabs(theta1);
  theta2 = (loadx + loady - loadz) / (mag * 1.732f);
  theta2 = std::fabs(theta2);
  theta3 = (loadx - loady + loadz) / (mag * 1.732f);
  theta3 = std::fabs(theta3);
  theta4 = (-loadx + loady + loadz) / (mag * 1.732f);
  theta4 = std::fabs(theta4);
  lambda1 = (loadx + loady) / (mag * 1.414f);
  lambda1 = std::fabs(lambda1);
  lambda2 = (loadx + loadz) / (mag * 1.414f);
  lambda2 = std::fabs(lambda2);
  lambda3 = (loadx - loady) / (mag * 1.414f);
  lambda3 = std::fabs(lambda3);
  lambda4 = (loadx - loadz) / (mag * 1.414f);
  lambda4 = std::fabs(lambda4);
  lambda5 = (loady + loadz) / (mag * 1.414f);
  lambda5 = std::fabs(lambda5);
  lambda6 = (loady - loadz) / (mag * 1.414f);
  lambda6 = std::fabs(lambda6);
  schmid1 = theta1 * lambda6;
  schmid2 = theta1 * lambda4;
  schmid3 = theta1 * lambda3;
  schmid4 = theta2 * lambda3;
  schmid5 = theta2 * lambda2;
  schmid6 = theta2 * lambda5;
  schmid7 = theta3 * lambda1;
  schmid8 = theta3 * lambda5;
  schmid9 = theta3 * lambda4;
  schmid10 = theta4 * lambda1;
  schmid11 = theta4 * lambda2;
  schmid12 = theta4 * lambda6;
  schmidfactor = schmid1;
  slipsys = 0;
  angleComps[0] = theta1;
  angleComps[1] = lambda6;

  if(schmid2 > schmidfactor)
  {
    schmidfactor = schmid2;
    slipsys = 1;
    angleComps[0] = theta1;
    angleComps[1] = lambda4;
  }
  if(schmid3 > schmidfactor)
  {
    schmidfactor = schmid3;
    slipsys = 2;
    angleComps[0] = theta1;
    angleComps[1] = lambda3;
  }
  if(schmid4 > schmidfactor)
  {
    schmidfactor = schmid4;
    slipsys = 3;
    angleComps[0] = theta2;
    angleComps[1] = lambda3;
  }
  if(schmid5 > schmidfactor)
  {
    schmidfactor = schmid5;
    slipsys = 4;
    angleComps[0] = theta2;
    angleComps[1] = lambda2;
  }
  if(schmid6 > schmidfactor)
  {
    schmidfactor = schmid6;
    slipsys = 5;
    angleComps[0] = theta2;
    angleComps[1] = lambda5;
  }
  if(schmid7 > schmidfactor)
  {
    schmidfactor = schmid7;
    slipsys = 6;
    angleComps[0] = theta3;
    angleComps[1] = lambda1;
  }
  if(schmid8 > schmidfactor)
  {
    schmidfactor = schmid8;
    slipsys = 7;
    angleComps[0] = theta3;
    angleComps[1] = lambda5;
  }
  if(schmid9 > schmidfactor)
  {
    schmidfactor = schmid9;
    slipsys = 8;
    angleComps[0] = theta3;
    angleComps[1] = lambda4;
  }
  if(schmid10 > schmidfactor)
  {
    schmidfactor = schmid10;
    slipsys = 9;
    angleComps[0] = theta4;
    angleComps[1] = lambda1;
  }
  if(schmid11 > schmidfactor)
  {
    schmidfactor = schmid11;
    slipsys = 10;
    angleComps[0] = theta4;
    angleComps[1] = lambda2;
  }
  if(schmid12 > schmidfactor)
  {
    schmidfactor = schmid12;
    slipsys = 11;
    angleComps[0] = theta4;
    angleComps[1] = lambda6;
  }
}

void CubicOps::getSchmidFactorAndSS(double load[3], double plane[3], double direction[3], double& schmidfactor, double angleComps[2], int& slipsys) const
{
  schmidfactor = 0;
  slipsys = 0;
  angleComps[0] = 0;
  angleComps[1] = 0;

  // compute mags
  double loadMag = std::sqrt(load[0] * load[0] + load[1] * load[1] + load[2] * load[2]);
  double planeMag = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
  double directionMag = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
  planeMag *= loadMag;
  directionMag *= loadMag;

  // loop over symmetry operators finding highest schmid factor
  for(int i = 0; i < CubicHigh::k_SymOpsCount; i++)
  {
    // compute slip system
    double slipPlane[3] = {0};
    slipPlane[2] = CubicHigh::MatSym[i][2][0] * plane[0] + CubicHigh::MatSym[i][2][1] * plane[1] + CubicHigh::MatSym[i][2][2] * plane[2];

    // dont consider negative z planes (to avoid duplicates)
    if(slipPlane[2] >= 0)
    {
      slipPlane[0] = CubicHigh::MatSym[i][0][0] * plane[0] + CubicHigh::MatSym[i][0][1] * plane[1] + CubicHigh::MatSym[i][0][2] * plane[2];
      slipPlane[1] = CubicHigh::MatSym[i][1][0] * plane[0] + CubicHigh::MatSym[i][1][1] * plane[1] + CubicHigh::MatSym[i][1][2] * plane[2];

      double slipDirection[3] = {0};
      slipDirection[0] = CubicHigh::MatSym[i][0][0] * direction[0] + CubicHigh::MatSym[i][0][1] * direction[1] + CubicHigh::MatSym[i][0][2] * direction[2];
      slipDirection[1] = CubicHigh::MatSym[i][1][0] * direction[0] + CubicHigh::MatSym[i][1][1] * direction[1] + CubicHigh::MatSym[i][1][2] * direction[2];
      slipDirection[2] = CubicHigh::MatSym[i][2][0] * direction[0] + CubicHigh::MatSym[i][2][1] * direction[1] + CubicHigh::MatSym[i][2][2] * direction[2];

      double cosPhi = std::fabs(load[0] * slipPlane[0] + load[1] * slipPlane[1] + load[2] * slipPlane[2]) / planeMag;
      double cosLambda = std::fabs(load[0] * slipDirection[0] + load[1] * slipDirection[1] + load[2] * slipDirection[2]) / directionMag;

      double schmid = cosPhi * cosLambda;
      if(schmid > schmidfactor)
      {
        schmidfactor = schmid;
        slipsys = i;
        angleComps[0] = std::acos(cosPhi);
        angleComps[1] = std::acos(cosLambda);
      }
    }
  }
}

double CubicOps::getmPrime(const QuatD& q1, const QuatD& q2, double LD[3]) const
{
  double g1[3][3];
  double g2[3][3];
  double g1temp[3][3];
  double g2temp[3][3];
  double hkl1[3], uvw1[3];
  double hkl2[3], uvw2[3];
  double slipDirection[3], slipPlane[3];
  double schmidFactor1 = 0, schmidFactor2 = 0, maxSchmidFactor = 0;
  double directionComponent1 = 0, planeComponent1 = 0;
  double directionComponent2 = 0, planeComponent2 = 0;
  double planeMisalignment = 0, directionMisalignment = 0;
  int ss1 = 0, ss2 = 0;

  OrientationTransformation::qu2om<QuatD, OrientationType>(q1).toGMatrix(g1temp);
  OrientationTransformation::qu2om<QuatD, OrientationType>(q2).toGMatrix(g2temp);
  EbsdMatrixMath::Transpose3x3(g1temp, g1);
  EbsdMatrixMath::Transpose3x3(g2temp, g2);
  for(int i = 0; i < 12; i++)
  {
    slipDirection[0] = CubicHigh::SlipDirections[i][0];
    slipDirection[1] = CubicHigh::SlipDirections[i][1];
    slipDirection[2] = CubicHigh::SlipDirections[i][2];
    slipPlane[0] = CubicHigh::SlipPlanes[i][0];
    slipPlane[1] = CubicHigh::SlipPlanes[i][1];
    slipPlane[2] = CubicHigh::SlipPlanes[i][2];
    EbsdMatrixMath::Multiply3x3with3x1(g1, slipPlane, hkl1);
    EbsdMatrixMath::Multiply3x3with3x1(g1, slipDirection, uvw1);
    EbsdMatrixMath::Normalize3x1(hkl1);
    EbsdMatrixMath::Normalize3x1(uvw1);
    directionComponent1 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, uvw1));
    planeComponent1 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, hkl1));
    schmidFactor1 = directionComponent1 * planeComponent1;
    if(schmidFactor1 > maxSchmidFactor)
    {
      maxSchmidFactor = schmidFactor1;
      ss1 = i;
    }
  }
  slipDirection[0] = CubicHigh::SlipDirections[ss1][0];
  slipDirection[1] = CubicHigh::SlipDirections[ss1][1];
  slipDirection[2] = CubicHigh::SlipDirections[ss1][2];
  slipPlane[0] = CubicHigh::SlipPlanes[ss1][0];
  slipPlane[1] = CubicHigh::SlipPlanes[ss1][1];
  slipPlane[2] = CubicHigh::SlipPlanes[ss1][2];
  EbsdMatrixMath::Multiply3x3with3x1(g1, slipPlane, hkl1);
  EbsdMatrixMath::Multiply3x3with3x1(g1, slipDirection, uvw1);
  EbsdMatrixMath::Normalize3x1(hkl1);
  EbsdMatrixMath::Normalize3x1(uvw1);

  maxSchmidFactor = 0;
  for(int j = 0; j < 12; j++)
  {
    slipDirection[0] = CubicHigh::SlipDirections[j][0];
    slipDirection[1] = CubicHigh::SlipDirections[j][1];
    slipDirection[2] = CubicHigh::SlipDirections[j][2];
    slipPlane[0] = CubicHigh::SlipPlanes[j][0];
    slipPlane[1] = CubicHigh::SlipPlanes[j][1];
    slipPlane[2] = CubicHigh::SlipPlanes[j][2];
    EbsdMatrixMath::Multiply3x3with3x1(g2, slipPlane, hkl2);
    EbsdMatrixMath::Multiply3x3with3x1(g2, slipDirection, uvw2);
    EbsdMatrixMath::Normalize3x1(hkl2);
    EbsdMatrixMath::Normalize3x1(uvw2);
    directionComponent2 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, uvw2));
    planeComponent2 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, hkl2));
    schmidFactor2 = directionComponent2 * planeComponent2;
    if(schmidFactor2 > maxSchmidFactor)
    {
      maxSchmidFactor = schmidFactor2;
      ss2 = j;
    }
  }
  slipDirection[0] = CubicHigh::SlipDirections[ss2][0];
  slipDirection[1] = CubicHigh::SlipDirections[ss2][1];
  slipDirection[2] = CubicHigh::SlipDirections[ss2][2];
  slipPlane[0] = CubicHigh::SlipPlanes[ss2][0];
  slipPlane[1] = CubicHigh::SlipPlanes[ss2][1];
  slipPlane[2] = CubicHigh::SlipPlanes[ss2][2];
  EbsdMatrixMath::Multiply3x3with3x1(g2, slipPlane, hkl2);
  EbsdMatrixMath::Multiply3x3with3x1(g2, slipDirection, uvw2);
  EbsdMatrixMath::Normalize3x1(hkl2);
  EbsdMatrixMath::Normalize3x1(uvw2);
  planeMisalignment = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(hkl1, hkl2));
  directionMisalignment = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(uvw1, uvw2));
  return planeMisalignment * directionMisalignment;
}

double CubicOps::getF1(const QuatD& q1, const QuatD& q2, double LD[3], bool maxSF) const
{
  double g1[3][3];
  double g2[3][3];
  double g1temp[3][3];
  double g2temp[3][3];
  double hkl1[3], uvw1[3];
  double hkl2[3], uvw2[3];
  double slipDirection[3], slipPlane[3];
  double directionMisalignment = 0, totalDirectionMisalignment = 0;
  double schmidFactor1 = 0, maxSchmidFactor = 0;
  double directionComponent1 = 0, planeComponent1 = 0;
  // double directionComponent2 = 0, planeComponent2 = 0;
  double maxF1 = 0.0;
  double F1 = 0.0;

  OrientationTransformation::qu2om<QuatD, OrientationType>(q1).toGMatrix(g1temp);
  OrientationTransformation::qu2om<QuatD, OrientationType>(q2).toGMatrix(g2temp);
  EbsdMatrixMath::Transpose3x3(g1temp, g1);
  EbsdMatrixMath::Transpose3x3(g2temp, g2);

  EbsdMatrixMath::Normalize3x1(LD);

  if(maxSF)
  {
    maxSchmidFactor = 0;
  }
  for(int i = 0; i < 12; i++)
  {
    slipDirection[0] = CubicHigh::SlipDirections[i][0];
    slipDirection[1] = CubicHigh::SlipDirections[i][1];
    slipDirection[2] = CubicHigh::SlipDirections[i][2];
    slipPlane[0] = CubicHigh::SlipPlanes[i][0];
    slipPlane[1] = CubicHigh::SlipPlanes[i][1];
    slipPlane[2] = CubicHigh::SlipPlanes[i][2];
    EbsdMatrixMath::Multiply3x3with3x1(g1, slipPlane, hkl1);
    EbsdMatrixMath::Multiply3x3with3x1(g1, slipDirection, uvw1);
    EbsdMatrixMath::Normalize3x1(hkl1);
    EbsdMatrixMath::Normalize3x1(uvw1);
    directionComponent1 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, uvw1));
    planeComponent1 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, hkl1));
    schmidFactor1 = directionComponent1 * planeComponent1;
    if(schmidFactor1 > maxSchmidFactor || !maxSF)
    {
      totalDirectionMisalignment = 0;
      if(maxSF)
      {
        maxSchmidFactor = schmidFactor1;
      }
      for(int j = 0; j < 12; j++)
      {
        slipDirection[0] = CubicHigh::SlipDirections[j][0];
        slipDirection[1] = CubicHigh::SlipDirections[j][1];
        slipDirection[2] = CubicHigh::SlipDirections[j][2];
        slipPlane[0] = CubicHigh::SlipPlanes[j][0];
        slipPlane[1] = CubicHigh::SlipPlanes[j][1];
        slipPlane[2] = CubicHigh::SlipPlanes[j][2];
        EbsdMatrixMath::Multiply3x3with3x1(g2, slipPlane, hkl2);
        EbsdMatrixMath::Multiply3x3with3x1(g2, slipDirection, uvw2);
        EbsdMatrixMath::Normalize3x1(hkl2);
        EbsdMatrixMath::Normalize3x1(uvw2);
        // directionComponent2 = std::fabs(GeometryMath::CosThetaBetweenVectors(LD, uvw2));
        // planeComponent2 = std::fabs(GeometryMath::CosThetaBetweenVectors(LD, hkl2));
        // schmidFactor2 = directionComponent2 * planeComponent2;
        directionMisalignment = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(uvw1, uvw2));
        totalDirectionMisalignment = totalDirectionMisalignment + directionMisalignment;
      }
      F1 = schmidFactor1 * directionComponent1 * totalDirectionMisalignment;
      if(!maxSF)
      {
        if(F1 < maxF1)
        {
          F1 = maxF1;
        }
        else
        {
          maxF1 = F1;
        }
      }
    }
  }
  return F1;
}

double CubicOps::getF1spt(const QuatD& q1, const QuatD& q2, double LD[3], bool maxSF) const
{
  double g1[3][3];
  double g2[3][3];
  double g1temp[3][3];
  double g2temp[3][3];
  double hkl1[3], uvw1[3];
  double hkl2[3], uvw2[3];
  double slipDirection[3], slipPlane[3];
  double directionMisalignment = 0, totalDirectionMisalignment = 0;
  double planeMisalignment = 0, totalPlaneMisalignment = 0;
  double schmidFactor1 = 0, maxSchmidFactor = 0;
  double directionComponent1 = 0, planeComponent1 = 0;
  // s double directionComponent2 = 0, planeComponent2 = 0;
  double maxF1spt = 0.0;
  double F1spt = 0.0f;
  OrientationTransformation::qu2om<QuatD, OrientationType>(q1).toGMatrix(g1temp);
  OrientationTransformation::qu2om<QuatD, OrientationType>(q2).toGMatrix(g2temp);
  EbsdMatrixMath::Transpose3x3(g1temp, g1);
  EbsdMatrixMath::Transpose3x3(g2temp, g2);

  EbsdMatrixMath::Normalize3x1(LD);

  if(maxSF)
  {
    maxSchmidFactor = 0;
  }
  for(int i = 0; i < 12; i++)
  {
    slipDirection[0] = CubicHigh::SlipDirections[i][0];
    slipDirection[1] = CubicHigh::SlipDirections[i][1];
    slipDirection[2] = CubicHigh::SlipDirections[i][2];
    slipPlane[0] = CubicHigh::SlipPlanes[i][0];
    slipPlane[1] = CubicHigh::SlipPlanes[i][1];
    slipPlane[2] = CubicHigh::SlipPlanes[i][2];
    EbsdMatrixMath::Multiply3x3with3x1(g1, slipPlane, hkl1);
    EbsdMatrixMath::Multiply3x3with3x1(g1, slipDirection, uvw1);
    EbsdMatrixMath::Normalize3x1(hkl1);
    EbsdMatrixMath::Normalize3x1(uvw1);
    directionComponent1 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, uvw1));
    planeComponent1 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, hkl1));
    schmidFactor1 = directionComponent1 * planeComponent1;
    if(schmidFactor1 > maxSchmidFactor || !maxSF)
    {
      totalDirectionMisalignment = 0;
      totalPlaneMisalignment = 0;
      if(maxSF)
      {
        maxSchmidFactor = schmidFactor1;
      }
      for(int j = 0; j < 12; j++)
      {
        slipDirection[0] = CubicHigh::SlipDirections[j][0];
        slipDirection[1] = CubicHigh::SlipDirections[j][1];
        slipDirection[2] = CubicHigh::SlipDirections[j][2];
        slipPlane[0] = CubicHigh::SlipPlanes[j][0];
        slipPlane[1] = CubicHigh::SlipPlanes[j][1];
        slipPlane[2] = CubicHigh::SlipPlanes[j][2];
        EbsdMatrixMath::Multiply3x3with3x1(g2, slipPlane, hkl2);
        EbsdMatrixMath::Multiply3x3with3x1(g2, slipDirection, uvw2);
        EbsdMatrixMath::Normalize3x1(hkl2);
        EbsdMatrixMath::Normalize3x1(uvw2);
        // directionComponent2 = std::fabs(GeometryMath::CosThetaBetweenVectors(LD, uvw2));
        // planeComponent2 = std::fabs(GeometryMath::CosThetaBetweenVectors(LD, hkl2));
        // schmidFactor2 = directionComponent2 * planeComponent2;
        directionMisalignment = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(uvw1, uvw2));
        planeMisalignment = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(hkl1, hkl2));
        totalDirectionMisalignment = totalDirectionMisalignment + directionMisalignment;
        totalPlaneMisalignment = totalPlaneMisalignment + planeMisalignment;
      }
      F1spt = schmidFactor1 * directionComponent1 * totalDirectionMisalignment * totalPlaneMisalignment;
      if(!maxSF)
      {
        if(F1spt < maxF1spt)
        {
          F1spt = maxF1spt;
        }
        else
        {
          maxF1spt = F1spt;
        }
      }
    }
  }
  return F1spt;
}

double CubicOps::getF7(const QuatD& q1, const QuatD& q2, double LD[3], bool maxSF) const
{
  double g1[3][3];
  double g2[3][3];
  double g1temp[3][3];
  double g2temp[3][3];
  double hkl1[3], uvw1[3];
  double hkl2[3], uvw2[3];
  double slipDirection[3], slipPlane[3];
  double directionMisalignment = 0, totalDirectionMisalignment = 0;
  double schmidFactor1 = 0.0, maxSchmidFactor = 0.0;
  double directionComponent1 = 0.0;
  double planeComponent1 = 0.0;

  // double directionComponent2 = 0, planeComponent2 = 0;
  double maxF7 = 0.0;
  double F7 = 0.0f;

  OrientationTransformation::qu2om<QuatD, OrientationType>(q1).toGMatrix(g1temp);
  OrientationTransformation::qu2om<QuatD, OrientationType>(q2).toGMatrix(g2temp);
  EbsdMatrixMath::Transpose3x3(g1temp, g1);
  EbsdMatrixMath::Transpose3x3(g2temp, g2);

  EbsdMatrixMath::Normalize3x1(LD);

  for(int i = 0; i < 12; i++)
  {
    slipDirection[0] = CubicHigh::SlipDirections[i][0];
    slipDirection[1] = CubicHigh::SlipDirections[i][1];
    slipDirection[2] = CubicHigh::SlipDirections[i][2];
    slipPlane[0] = CubicHigh::SlipPlanes[i][0];
    slipPlane[1] = CubicHigh::SlipPlanes[i][1];
    slipPlane[2] = CubicHigh::SlipPlanes[i][2];
    EbsdMatrixMath::Multiply3x3with3x1(g1, slipPlane, hkl1);
    EbsdMatrixMath::Multiply3x3with3x1(g1, slipDirection, uvw1);
    EbsdMatrixMath::Normalize3x1(hkl1);
    EbsdMatrixMath::Normalize3x1(uvw1);
    directionComponent1 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, uvw1));
    planeComponent1 = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(LD, hkl1));
    schmidFactor1 = directionComponent1 * planeComponent1;
    if(schmidFactor1 > maxSchmidFactor || !maxSF)
    {
      totalDirectionMisalignment = 0;
      if(maxSF)
      {
        maxSchmidFactor = schmidFactor1;
      }
      for(int j = 0; j < 12; j++)
      {
        slipDirection[0] = CubicHigh::SlipDirections[j][0];
        slipDirection[1] = CubicHigh::SlipDirections[j][1];
        slipDirection[2] = CubicHigh::SlipDirections[j][2];
        slipPlane[0] = CubicHigh::SlipPlanes[j][0];
        slipPlane[1] = CubicHigh::SlipPlanes[j][1];
        slipPlane[2] = CubicHigh::SlipPlanes[j][2];
        EbsdMatrixMath::Multiply3x3with3x1(g2, slipPlane, hkl2);
        EbsdMatrixMath::Multiply3x3with3x1(g2, slipDirection, uvw2);
        EbsdMatrixMath::Normalize3x1(hkl2);
        EbsdMatrixMath::Normalize3x1(uvw2);
        // directionComponent2 = std::fabs(GeometryMath::CosThetaBetweenVectors(LD, uvw2));
        // planeComponent2 = std::fabs(GeometryMath::CosThetaBetweenVectors(LD, hkl2));
        // schmidFactor2 = directionComponent2 * planeComponent2;
        directionMisalignment = std::fabs(EbsdLib::GeometryMath::CosThetaBetweenVectors(uvw1, uvw2));
        totalDirectionMisalignment = totalDirectionMisalignment + directionMisalignment;
      }
      F7 = directionComponent1 * directionComponent1 * totalDirectionMisalignment;
      if(!maxSF)
      {
        if(F7 < maxF7)
        {
          F7 = maxF7;
        }
        else
        {
          maxF7 = F7;
        }
      }
    }
  }
  return F7;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
namespace CubicHigh
{
class GenerateSphereCoordsImpl
{
  EbsdLib::FloatArrayType* m_Eulers;
  EbsdLib::FloatArrayType* m_xyz001;
  EbsdLib::FloatArrayType* m_xyz011;
  EbsdLib::FloatArrayType* m_xyz111;

public:
  GenerateSphereCoordsImpl(EbsdLib::FloatArrayType* eulers, EbsdLib::FloatArrayType* xyz001, EbsdLib::FloatArrayType* xyz011, EbsdLib::FloatArrayType* xyz111)
  : m_Eulers(eulers)
  , m_xyz001(xyz001)
  , m_xyz011(xyz011)
  , m_xyz111(xyz111)
  {
  }
  virtual ~GenerateSphereCoordsImpl() = default;

  void generate(size_t start, size_t end) const
  {
    double g[3][3];
    double gTranpose[3][3];
    double direction[3] = {0.0, 0.0, 0.0};

    for(size_t i = start; i < end; ++i)
    {
      OrientationType eu(m_Eulers->getValue(i * 3), m_Eulers->getValue(i * 3 + 1), m_Eulers->getValue(i * 3 + 2));
      OrientationTransformation::eu2om<OrientationType, OrientationType>(eu).toGMatrix(g);

      EbsdMatrixMath::Transpose3x3(g, gTranpose);

      // -----------------------------------------------------------------------------
      // 001 Family
      direction[0] = 1.0;
      direction[1] = 0.0;
      direction[2] = 0.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz001->getPointer(i * 18));
      EbsdMatrixMath::Copy3x1(m_xyz001->getPointer(i * 18), m_xyz001->getPointer(i * 18 + 3));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz001->getPointer(i * 18 + 3), -1.0f);
      direction[0] = 0.0;
      direction[1] = 1.0;
      direction[2] = 0.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz001->getPointer(i * 18 + 6));
      EbsdMatrixMath::Copy3x1(m_xyz001->getPointer(i * 18 + 6), m_xyz001->getPointer(i * 18 + 9));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz001->getPointer(i * 18 + 9), -1.0f);
      direction[0] = 0.0;
      direction[1] = 0.0;
      direction[2] = 1.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz001->getPointer(i * 18 + 12));
      EbsdMatrixMath::Copy3x1(m_xyz001->getPointer(i * 18 + 12), m_xyz001->getPointer(i * 18 + 15));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz001->getPointer(i * 18 + 15), -1.0f);

      // -----------------------------------------------------------------------------
      // 011 Family
      direction[0] = EbsdLib::Constants::k_1OverRoot2D;
      direction[1] = EbsdLib::Constants::k_1OverRoot2D;
      direction[2] = 0.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz011->getPointer(i * 36));
      EbsdMatrixMath::Copy3x1(m_xyz011->getPointer(i * 36), m_xyz011->getPointer(i * 36 + 3));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz011->getPointer(i * 36 + 3), -1.0f);
      direction[0] = EbsdLib::Constants::k_1OverRoot2D;
      direction[1] = 0.0;
      direction[2] = EbsdLib::Constants::k_1OverRoot2D;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz011->getPointer(i * 36 + 6));
      EbsdMatrixMath::Copy3x1(m_xyz011->getPointer(i * 36 + 6), m_xyz011->getPointer(i * 36 + 9));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz011->getPointer(i * 36 + 9), -1.0f);
      direction[0] = 0.0;
      direction[1] = EbsdLib::Constants::k_1OverRoot2D;
      direction[2] = EbsdLib::Constants::k_1OverRoot2D;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz011->getPointer(i * 36 + 12));
      EbsdMatrixMath::Copy3x1(m_xyz011->getPointer(i * 36 + 12), m_xyz011->getPointer(i * 36 + 15));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz011->getPointer(i * 36 + 15), -1.0f);
      direction[0] = -EbsdLib::Constants::k_1OverRoot2D;
      direction[1] = EbsdLib::Constants::k_1OverRoot2D;
      direction[2] = 0.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz011->getPointer(i * 36 + 18));
      EbsdMatrixMath::Copy3x1(m_xyz011->getPointer(i * 36 + 18), m_xyz011->getPointer(i * 36 + 21));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz011->getPointer(i * 36 + 21), -1.0f);
      direction[0] = -EbsdLib::Constants::k_1OverRoot2D;
      direction[1] = 0.0;
      direction[2] = EbsdLib::Constants::k_1OverRoot2D;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz011->getPointer(i * 36 + 24));
      EbsdMatrixMath::Copy3x1(m_xyz011->getPointer(i * 36 + 24), m_xyz011->getPointer(i * 36 + 27));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz011->getPointer(i * 36 + 27), -1.0f);
      direction[0] = 0.0;
      direction[1] = -EbsdLib::Constants::k_1OverRoot2D;
      direction[2] = EbsdLib::Constants::k_1OverRoot2D;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz011->getPointer(i * 36 + 30));
      EbsdMatrixMath::Copy3x1(m_xyz011->getPointer(i * 36 + 30), m_xyz011->getPointer(i * 36 + 33));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz011->getPointer(i * 36 + 33), -1.0f);

      // -----------------------------------------------------------------------------
      // 111 Family
      direction[0] = EbsdLib::Constants::k_1OverRoot3D;
      direction[1] = EbsdLib::Constants::k_1OverRoot3D;
      direction[2] = EbsdLib::Constants::k_1OverRoot3D;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz111->getPointer(i * 24));
      EbsdMatrixMath::Copy3x1(m_xyz111->getPointer(i * 24), m_xyz111->getPointer(i * 24 + 3));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz111->getPointer(i * 24 + 3), -1.0f);
      direction[0] = -EbsdLib::Constants::k_1OverRoot3D;
      direction[1] = EbsdLib::Constants::k_1OverRoot3D;
      direction[2] = EbsdLib::Constants::k_1OverRoot3D;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz111->getPointer(i * 24 + 6));
      EbsdMatrixMath::Copy3x1(m_xyz111->getPointer(i * 24 + 6), m_xyz111->getPointer(i * 24 + 9));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz111->getPointer(i * 24 + 9), -1.0f);
      direction[0] = EbsdLib::Constants::k_1OverRoot3D;
      direction[1] = -EbsdLib::Constants::k_1OverRoot3D;
      direction[2] = EbsdLib::Constants::k_1OverRoot3D;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz111->getPointer(i * 24 + 12));
      EbsdMatrixMath::Copy3x1(m_xyz111->getPointer(i * 24 + 12), m_xyz111->getPointer(i * 24 + 15));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz111->getPointer(i * 24 + 15), -1.0f);
      direction[0] = EbsdLib::Constants::k_1OverRoot3D;
      direction[1] = EbsdLib::Constants::k_1OverRoot3D;
      direction[2] = -EbsdLib::Constants::k_1OverRoot3D;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz111->getPointer(i * 24 + 18));
      EbsdMatrixMath::Copy3x1(m_xyz111->getPointer(i * 24 + 18), m_xyz111->getPointer(i * 24 + 21));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz111->getPointer(i * 24 + 21), -1.0f);
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    generate(r.begin(), r.end());
  }
#endif
};
} // namespace CubicHigh

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void CubicOps::generateSphereCoordsFromEulers(EbsdLib::FloatArrayType* eulers, EbsdLib::FloatArrayType* xyz001, EbsdLib::FloatArrayType* xyz011, EbsdLib::FloatArrayType* xyz111) const
{
  size_t nOrientations = eulers->getNumberOfTuples();

  // Sanity Check the size of the arrays
  if(xyz001->getNumberOfTuples() < nOrientations * CubicHigh::symSize0)
  {
    xyz001->resizeTuples(nOrientations * CubicHigh::symSize0 * 3);
  }
  if(xyz011->getNumberOfTuples() < nOrientations * CubicHigh::symSize1)
  {
    xyz011->resizeTuples(nOrientations * CubicHigh::symSize1 * 3);
  }
  if(xyz111->getNumberOfTuples() < nOrientations * CubicHigh::symSize2)
  {
    xyz111->resizeTuples(nOrientations * CubicHigh::symSize2 * 3);
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  bool doParallel = true;
  if(doParallel)
  {
    tbb::parallel_for(tbb::blocked_range<size_t>(0, nOrientations), CubicHigh::GenerateSphereCoordsImpl(eulers, xyz001, xyz011, xyz111), tbb::auto_partitioner());
  }
  else
#endif
  {
    CubicHigh::GenerateSphereCoordsImpl serial(eulers, xyz001, xyz011, xyz111);
    serial.generate(0, nOrientations);
  }
}

/**
 * @brief Sorts the 3 values from low to high
 * @param a
 * @param b
 * @param c
 * @param sorted The array to store the sorted values.
 */
template <typename T>
void _TripletSort(T a, T b, T c, T* sorted)
{
  if(a > b && a > c)
  {
    sorted[2] = a;
    if(b > c)
    {
      sorted[1] = b;
      sorted[0] = c;
    }
    else
    {
      sorted[1] = c;
      sorted[0] = b;
    }
  }
  else if(b > a && b > c)
  {
    sorted[2] = b;
    if(a > c)
    {
      sorted[1] = a;
      sorted[0] = c;
    }
    else
    {
      sorted[1] = c;
      sorted[0] = a;
    }
  }
  else if(a > b)
  {
    sorted[1] = a;
    sorted[0] = b;
    sorted[2] = c;
  }
  else if(a >= c && b >= c)
  {
    sorted[0] = c;
    sorted[1] = a;
    sorted[2] = b;
  }
  else
  {
    sorted[0] = a;
    sorted[1] = b;
    sorted[2] = c;
  }
}

/**
 * @brief Sorts the 3 values from low to high
 * @param a Input
 * @param b Input
 * @param c Input
 * @param x Output
 * @param y Output
 * @param z Output
 */
template <typename T>
void _TripletSort(T a, T b, T c, T& x, T& y, T& z)
{
  if(a > b && a > c)
  {
    z = a;
    if(b > c)
    {
      y = b;
      x = c;
    }
    else
    {
      y = c;
      x = b;
    }
  }
  else if(b > a && b > c)
  {
    z = b;
    if(a > c)
    {
      y = a;
      x = c;
    }
    else
    {
      y = c;
      x = a;
    }
  }
  else if(a > b)
  {
    y = a;
    x = b;
    z = c;
  }
  else if(a >= c && b >= c)
  {
    x = c;
    y = a;
    z = b;
  }
  else
  {
    x = a;
    y = b;
    z = c;
  }
}

bool inUnitTriangleD(double eta, double chi)
{
  double etaDeg = eta * EbsdLib::Constants::k_180OverPiD;
  double chiMax;
  if(etaDeg > 45.0)
  {
    chiMax = sqrt(1.0 / (2.0 + std::tan(0.5 * EbsdLib::Constants::k_PiD - eta) * std::tan(0.5 * EbsdLib::Constants::k_PiD - eta)));
  }
  else
  {
    chiMax = sqrt(1.0 / (2.0 + std::tan(eta) * std::tan(eta)));
  }
  EbsdLibMath::bound(chiMax, -1.0, 1.0);
  chiMax = acos(chiMax);
  return !(eta < 0.0 || eta > (45.0 * EbsdLib::Constants::k_PiOver180D) || chi < 0.0 || chi > chiMax);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool CubicOps::inUnitTriangle(double eta, double chi) const
{
  double etaDeg = eta * EbsdLib::Constants::k_180OverPiD;
  double chiMax;
  if(etaDeg > 45.0)
  {
    chiMax = std::sqrt(1.0 / (2.0 + std::tan(0.5 * EbsdLib::Constants::k_PiD - eta) * std::tan(0.5 * EbsdLib::Constants::k_PiD - eta)));
  }
  else
  {
    chiMax = std::sqrt(1.0 / (2.0 + std::tan(eta) * std::tan(eta)));
  }
  EbsdLibMath::bound(chiMax, -1.0, 1.0);
  chiMax = acos(chiMax);
  return !(eta < 0.0 || eta > (45.0 * EbsdLib::Constants::k_PiOver180D) || chi < 0.0 || chi > chiMax);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb CubicOps::generateIPFColor(double* eulers, double* refDir, bool convertDegrees) const
{
  return generateIPFColor(eulers[0], eulers[1], eulers[2], refDir[0], refDir[1], refDir[2], convertDegrees);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb CubicOps::generateIPFColor(double phi1, double phi, double phi2, double refDir0, double refDir1, double refDir2, bool degToRad) const
{
  if(degToRad)
  {
    phi1 = phi1 * EbsdLib::Constants::k_DegToRadD;
    phi = phi * EbsdLib::Constants::k_DegToRadD;
    phi2 = phi2 * EbsdLib::Constants::k_DegToRadD;
  }

  double g[3][3];
  double p[3];
  double refDirection[3] = {0.0f, 0.0f, 0.0f};
  double chi = 0.0f, eta = 0.0f;
  double _rgb[3] = {0.0, 0.0, 0.0};

  OrientationType eu(phi1, phi, phi2);
  OrientationType om(9); // Reusable for the loop
  QuatD q1 = OrientationTransformation::eu2qu<OrientationType, QuatD>(eu);

  for(int j = 0; j < CubicHigh::k_SymOpsCount; j++)
  {
    QuatD qu = getQuatSymOp(j) * q1;
    OrientationTransformation::qu2om<QuatD, OrientationType>(qu).toGMatrix(g);

    refDirection[0] = refDir0;
    refDirection[1] = refDir1;
    refDirection[2] = refDir2;
    EbsdMatrixMath::Multiply3x3with3x1(g, refDirection, p);
    EbsdMatrixMath::Normalize3x1(p);

    if(!getHasInversion() && p[2] < 0)
    {
      continue;
    }
    if(getHasInversion() && p[2] < 0)
    {
      p[0] = -p[0], p[1] = -p[1], p[2] = -p[2];
    }
    chi = std::acos(p[2]);
    eta = std::atan2(p[1], p[0]);
    if(!inUnitTriangleD(eta, chi))
    {
      continue;
    }
    break;
  }
  double etaMin = 0.0;
  double etaMax = 45.0;
  double etaDeg = eta * EbsdLib::Constants::k_180OverPiD;
  double chiMax;
  if(etaDeg > 45.0)
  {
    chiMax = std::sqrt(1.0 / (2.0 + std::tan(0.5 * EbsdLib::Constants::k_PiD - eta) * std::tan(0.5 * EbsdLib::Constants::k_PiD - eta)));
  }
  else
  {
    chiMax = std::sqrt(1.0 / (2.0 + std::tan(eta) * std::tan(eta)));
  }
  EbsdLibMath::bound(chiMax, -1.0, 1.0);
  chiMax = std::acos(chiMax);

  _rgb[0] = 1.0 - chi / chiMax;
  _rgb[2] = std::fabs(etaDeg - etaMin) / (etaMax - etaMin);
  _rgb[1] = 1 - _rgb[2];
  _rgb[1] *= chi / chiMax;
  _rgb[2] *= chi / chiMax;
  _rgb[0] = std::sqrt(_rgb[0]);
  _rgb[1] = std::sqrt(_rgb[1]);
  _rgb[2] = std::sqrt(_rgb[2]);

  double max = _rgb[0];
  if(_rgb[1] > max)
  {
    max = _rgb[1];
  }
  if(_rgb[2] > max)
  {
    max = _rgb[2];
  }

  _rgb[0] = _rgb[0] / max;
  _rgb[1] = _rgb[1] / max;
  _rgb[2] = _rgb[2] / max;

  return EbsdLib::RgbColor::dRgb(static_cast<int32_t>(_rgb[0] * 255), static_cast<int32_t>(_rgb[1] * 255), static_cast<int32_t>(_rgb[2] * 255), 255);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb CubicOps::generateRodriguesColor(double r1, double r2, double r3) const
{
  double range1 = 2.0f * CubicHigh::OdfDimInitValue[0];
  double range2 = 2.0f * CubicHigh::OdfDimInitValue[1];
  double range3 = 2.0f * CubicHigh::OdfDimInitValue[2];
  double max1 = range1 / 2.0f;
  double max2 = range2 / 2.0f;
  double max3 = range3 / 2.0f;
  double red = (r1 + max1) / range1;
  double green = (r2 + max2) / range2;
  double blue = (r3 + max3) / range3;

  return EbsdLib::RgbColor::dRgb(static_cast<int32_t>(red * 255), static_cast<int32_t>(green * 255), static_cast<int32_t>(blue * 255), 255);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
std::vector<EbsdLib::UInt8ArrayType::Pointer> CubicOps::generatePoleFigure(PoleFigureConfiguration_t& config) const
{
  std::string label0 = std::string("<001>");
  std::string label1 = std::string("<011>");
  std::string label2 = std::string("<111>");
  if(!config.labels.empty())
  {
    label0 = config.labels.at(0);
  }
  if(config.labels.size() > 1)
  {
    label1 = config.labels.at(1);
  }
  if(config.labels.size() > 2)
  {
    label2 = config.labels.at(2);
  }

  size_t numOrientations = config.eulers->getNumberOfTuples();

  // Create an Array to hold the XYZ Coordinates which are the coords on the sphere.
  // this is size for CUBIC ONLY, <001> Family
  std::vector<size_t> dims(1, 3);
  EbsdLib::FloatArrayType::Pointer xyz001 = EbsdLib::FloatArrayType::CreateArray(numOrientations * CubicHigh::symSize0, dims, label0 + std::string("xyzCoords"), true);
  // this is size for CUBIC ONLY, <011> Family
  EbsdLib::FloatArrayType::Pointer xyz011 = EbsdLib::FloatArrayType::CreateArray(numOrientations * CubicHigh::symSize1, dims, label1 + std::string("xyzCoords"), true);
  // this is size for CUBIC ONLY, <111> Family
  EbsdLib::FloatArrayType::Pointer xyz111 = EbsdLib::FloatArrayType::CreateArray(numOrientations * CubicHigh::symSize2, dims, label2 + std::string("xyzCoords"), true);

  config.sphereRadius = 1.0f;

  // Generate the coords on the sphere **** Parallelized
  generateSphereCoordsFromEulers(config.eulers, xyz001.get(), xyz011.get(), xyz111.get());

  // These arrays hold the "intensity" images which eventually get converted to an actual Color RGB image
  // Generate the modified Lambert projection images (Squares, 2 of them, 1 for northern hemisphere, 1 for southern hemisphere
  EbsdLib::DoubleArrayType::Pointer intensity001 = EbsdLib::DoubleArrayType::CreateArray(config.imageDim * config.imageDim, label0 + "_Intensity_Image", true);
  EbsdLib::DoubleArrayType::Pointer intensity011 = EbsdLib::DoubleArrayType::CreateArray(config.imageDim * config.imageDim, label1 + "_Intensity_Image", true);
  EbsdLib::DoubleArrayType::Pointer intensity111 = EbsdLib::DoubleArrayType::CreateArray(config.imageDim * config.imageDim, label2 + "_Intensity_Image", true);

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  bool doParallel = true;

  if(doParallel)
  {
    std::shared_ptr<tbb::task_group> g(new tbb::task_group);
    g->run(ComputeStereographicProjection(xyz001.get(), &config, intensity001.get()));
    g->run(ComputeStereographicProjection(xyz011.get(), &config, intensity011.get()));
    g->run(ComputeStereographicProjection(xyz111.get(), &config, intensity111.get()));
    g->wait(); // Wait for all the threads to complete before moving on.
  }
  else
#endif
  {
    ComputeStereographicProjection m001(xyz001.get(), &config, intensity001.get());
    m001();
    ComputeStereographicProjection m011(xyz011.get(), &config, intensity011.get());
    m011();
    ComputeStereographicProjection m111(xyz111.get(), &config, intensity111.get());
    m111();
  }

  // Find the Max and Min values based on ALL 3 arrays so we can color scale them all the same
  double max = std::numeric_limits<double>::min();
  double min = std::numeric_limits<double>::max();

  double* dPtr = intensity001->getPointer(0);
  size_t count = intensity001->getNumberOfTuples();
  for(size_t i = 0; i < count; ++i)
  {
    if(dPtr[i] > max)
    {
      max = dPtr[i];
    }
    if(dPtr[i] < min)
    {
      min = dPtr[i];
    }
  }

  dPtr = intensity011->getPointer(0);
  count = intensity011->getNumberOfTuples();
  for(size_t i = 0; i < count; ++i)
  {
    if(dPtr[i] > max)
    {
      max = dPtr[i];
    }
    if(dPtr[i] < min)
    {
      min = dPtr[i];
    }
  }

  dPtr = intensity111->getPointer(0);
  count = intensity111->getNumberOfTuples();
  for(size_t i = 0; i < count; ++i)
  {
    if(dPtr[i] > max)
    {
      max = dPtr[i];
    }
    if(dPtr[i] < min)
    {
      min = dPtr[i];
    }
  }

  config.minScale = min;
  config.maxScale = max;

  dims[0] = 4;
  EbsdLib::UInt8ArrayType::Pointer image001 = EbsdLib::UInt8ArrayType::CreateArray(static_cast<size_t>(config.imageDim * config.imageDim), dims, label0, true);
  EbsdLib::UInt8ArrayType::Pointer image011 = EbsdLib::UInt8ArrayType::CreateArray(static_cast<size_t>(config.imageDim * config.imageDim), dims, label1, true);
  EbsdLib::UInt8ArrayType::Pointer image111 = EbsdLib::UInt8ArrayType::CreateArray(static_cast<size_t>(config.imageDim * config.imageDim), dims, label2, true);

  std::vector<EbsdLib::UInt8ArrayType::Pointer> poleFigures(3);
  if(config.order.size() == 3)
  {
    poleFigures[static_cast<int>(config.order[0])] = image001;
    poleFigures[static_cast<int>(config.order[1])] = image011;
    poleFigures[static_cast<int>(config.order[2])] = image111;
  }
  else
  {
    poleFigures[0] = image001;
    poleFigures[1] = image011;
    poleFigures[2] = image111;
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS

  if(doParallel)
  {
    std::shared_ptr<tbb::task_group> g(new tbb::task_group);
    g->run(GeneratePoleFigureRgbaImageImpl(intensity001.get(), &config, image001.get()));
    g->run(GeneratePoleFigureRgbaImageImpl(intensity011.get(), &config, image011.get()));
    g->run(GeneratePoleFigureRgbaImageImpl(intensity111.get(), &config, image111.get()));
    g->wait(); // Wait for all the threads to complete before moving on.
  }
  else
#endif
  {
    GeneratePoleFigureRgbaImageImpl m001(intensity001.get(), &config, image001.get());
    m001();
    GeneratePoleFigureRgbaImageImpl m011(intensity011.get(), &config, image011.get());
    m011();
    GeneratePoleFigureRgbaImageImpl m111(intensity111.get(), &config, image111.get());
    m111();
  }

#if 0
  size_t dim[3] = {config.imageDim, config.imageDim, 1};
  FloatVec3Type res = {1.0, 1.0, 1.0};
  VtkRectilinearGridWriter::WriteDataArrayToFile("/tmp/" + intensity001->getName() + ".vtk",
                                                 intensity001.get(), dim, res, "double", true );
  VtkRectilinearGridWriter::WriteDataArrayToFile("/tmp/" + intensity011->getName() + ".vtk",
                                                 intensity011.get(), dim, res, "double", true );
  VtkRectilinearGridWriter::WriteDataArrayToFile("/tmp/" + intensity111->getName() + ".vtk",
                                                 intensity111.get(), dim, res, "double", true );
#endif
  return poleFigures;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::UInt8ArrayType::Pointer CubicOps::generateIPFTriangleLegend(int imageDim) const
{

  std::vector<size_t> dims(1, 4);
  EbsdLib::UInt8ArrayType::Pointer image = EbsdLib::UInt8ArrayType::CreateArray(imageDim * imageDim, dims, getSymmetryName() + " Triangle Legend", true);
  uint32_t* pixelPtr = reinterpret_cast<uint32_t*>(image->getPointer(0));

  double indexConst1 = 0.414f / static_cast<double>(imageDim);
  double indexConst2 = 0.207f / static_cast<double>(imageDim);
  double red1 = 0.0f;

  double x = 0.0f;
  double y = 0.0f;
  double a = 0.0f;
  double b = 0.0f;
  double c = 0.0f;

  double val = 0.0f;
  double x1 = 0.0f;
  double y1 = 0.0f;
  double z1 = 0.0f;
  double denom = 0.0f;
  double phi = 0.0f;
  double x1alt = 0.0f;
  double theta = 0.0f;
  double k_RootOfHalf = sqrtf(0.5f);
  double cd[3];

  EbsdLib::Rgb color;
  size_t idx = 0;
  size_t yScanLineIndex = imageDim; // We use this to control where the data is drawn. Otherwise the image will come out flipped vertically
  // Loop over every pixel in the image and project up to the sphere to get the angle and then figure out the RGB from
  // there.
  for(int32_t yIndex = 0; yIndex < imageDim; ++yIndex)
  {
    yScanLineIndex--;
    for(int32_t xIndex = 0; xIndex < imageDim; ++xIndex)
    {
      idx = (imageDim * yScanLineIndex) + xIndex;

      x = xIndex * indexConst1 + indexConst2;
      y = yIndex * indexConst1 + indexConst2;
      //     z = -1.0;
      a = (x * x + y * y + 1);
      b = (2 * x * x + 2 * y * y);
      c = (x * x + y * y - 1);

      val = (-b + std::sqrt(b * b - 4.0f * a * c)) / (2.0f * a);
      x1 = (1 + val) * x;
      y1 = (1 + val) * y;
      z1 = val;
      denom = (x1 * x1) + (y1 * y1) + (z1 * z1);
      denom = std::sqrt(denom);
      x1 = x1 / denom;
      y1 = y1 / denom;
      z1 = z1 / denom;

      red1 = x1 * (-k_RootOfHalf) + z1 * k_RootOfHalf;
      phi = acos(red1);
      x1alt = x1 / k_RootOfHalf;
      x1alt = x1alt / sqrt((x1alt * x1alt) + (y1 * y1));
      theta = acos(x1alt);

      if(phi < (45.0f * EbsdLib::Constants::k_PiOver180D) || phi > (90.0f * EbsdLib::Constants::k_PiOver180D) || theta > (35.26f * EbsdLib::Constants::k_PiOver180D))
      {
        color = 0xFFFFFFFF;
      }
      else
      {
        // 3) move that direction to a single standard triangle - using the 001-011-111 triangle)
        cd[0] = std::fabs(x1);
        cd[1] = std::fabs(y1);
        cd[2] = std::fabs(z1);

        // Sort the cd array from smallest to largest
        _TripletSort(cd[0], cd[1], cd[2], cd);

        color = generateIPFColor(0.0, 0.0, 0.0, cd[0], cd[1], cd[2], false);
      }
      pixelPtr[idx] = color;
    }
  }
  return image;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb CubicOps::generateMisorientationColor(const QuatD& q, const QuatD& refFrame) const
{
  double n1, n2, n3, w;
  double x, x1, x2, x3, x4, x5, x6, x7;
  double y, y1, y2, y3, y4, y5, y6, y7;
  double z, z1, z2, z3, z4, z5, z6, z7;
  double k, h, s, v;

  // get disorientation
  OrientationD axisAngle = calculateMisorientation(q, refFrame);
  n1 = std::fabs(axisAngle[0]);
  n2 = std::fabs(axisAngle[1]);
  n3 = std::fabs(axisAngle[2]);
  w = axisAngle[3];

  _TripletSort(n1, n2, n3, z, y, x);

  // eq c9.1
  k = tan(w / 2.0);
  x = x * k;
  y = y * k;
  z = z * k;

  // eq c9.2
  x1 = x;
  y1 = y;
  z1 = z;
  if(x >= EbsdLib::Constants::k_1Over3D && atan2(z, y) >= ((1.0f - 2.0f * x) / x))
  {
    y1 = (x * (y + z)) / (1.0f - x);
    z1 = (x * z * (y + z)) / (y * (1.0f - x));
  }

  // eq c9.3
  x2 = x1 - EbsdLib::Constants::k_Tan_OneEigthPiD;
  y2 = y1 * EbsdLib::Constants::k_Cos_ThreeEightPiD - z1 * EbsdLib::Constants::k_Sin_ThreeEightPiD;
  z2 = y1 * EbsdLib::Constants::k_Sin_ThreeEightPiD + z1 * EbsdLib::Constants::k_Cos_ThreeEightPiD;

  // eq c9.4
  x3 = x2;
  y3 = y2 * (1 + (y2 / z2) * EbsdLib::Constants::k_Tan_OneEigthPiD);
  z3 = z2 + y2 * EbsdLib::Constants::k_Tan_OneEigthPiD;

  // eq c9.5
  x4 = x3;
  y4 = (y3 * EbsdLib::Constants::k_Cos_OneEigthPiD) / EbsdLib::Constants::k_Tan_OneEigthPiD;
  z4 = z3 - x3 / EbsdLib::Constants::k_Cos_OneEigthPiD;

  // eq c9.6
  k = atan2(-x4, y4);
  x5 = x4 * (sin(k) + std::fabs(cos(k)));
  y5 = y4 * (sin(k) + std::fabs(cos(k)));
  z5 = z4;

  // eq c9.7
  k = atan2(-x5, y5);
  x6 = -sqrt(x5 * x5 + y5 * y5) * sin(2.0f * k);
  y6 = sqrt(x5 * x5 + y5 * y5) * cos(2.0f * k);
  z6 = z5;

  // eq c9.8 these hsv are from 0 to 1 in cartesian coordinates
  x7 = (x6 * EbsdLib::Constants::k_Sqrt3D - y6) / (2.0f * EbsdLib::Constants::k_Tan_OneEigthPiD);
  y7 = (x6 + y6 * EbsdLib::Constants::k_Sqrt3D) / (2.0f * EbsdLib::Constants::k_Tan_OneEigthPiD);
  z7 = z6 * (EbsdLib::Constants::k_Cos_OneEigthPiD / EbsdLib::Constants::k_Tan_OneEigthPiD);

  // convert to traditional hsv (0-1)
  h = std::fmod(std::atan2(y7, x7) + M_2PI, M_2PI) / M_2PI;
  s = std::sqrt(x7 * x7 + y7 * y7);
  v = z7;
  if(v > 0)
  {
    s = s / v;
  }

  EbsdLib::Rgb rgb = EbsdLib::ColorUtilities::ConvertHSVtoRgb(static_cast<float>(h), static_cast<float>(s), static_cast<float>(v));

  // now standard 0-255 rgb, needs rotation
  return EbsdLib::RgbColor::dRgb(255 - EbsdLib::RgbColor::dGreen(rgb), EbsdLib::RgbColor::dBlue(rgb), EbsdLib::RgbColor::dRed(rgb), 0);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::UInt8ArrayType::Pointer CubicOps::generateMisorientationTriangleLegend(double angle, int n1, int n2, int imageDim) const
{
  std::vector<size_t> dims(1, 4);
  EbsdLib::UInt8ArrayType::Pointer image = EbsdLib::UInt8ArrayType::CreateArray(imageDim * imageDim, dims, "Cubic High Misorientation Triangle Legend", true);
#if 0
  //uint32_t* pixelPtr = reinterpret_cast<uint32_t*>(image->getPointer(0));

  double maxk = EbsdLib::Constants::k_Sqrt2 - 1;
  double maxdeg = 2 * atan(sqrt(6 * maxk * maxk - 4 * maxk + 1));
  double deg1 = 2 * atan(sqrt(2 * maxk * maxk));

  double A = angle * M_PI / 360;
  std::vector<double> B;
  std::vector< std::vector<double> > C;

  ///Generate regularly spaced array of points in misorientation space
  if(A <= M_PI / 8)
  {
    double theta1 = atan(1 / sin(M_PI_4));
    double theta2 = M_PI_2;
    B = EbsdLibMath::linspace(theta1, theta2, n2);
    for(int i = 0; i < n2; i++)
    {
      C.push_back(EbsdLibMath::linspace(asin(1 / tan(B[i])), M_PI_4, n1));
    }
  }
  else if(A > M_PI / 8 && A <= M_PI / 6)
  {
    double theta1 = atan(1 / sin(M_PI_4));
    double theta2 = acos(-(maxk * (1 / tan(A))) * (maxk * (1 / tan(A)))) / 2;
    double theta3 = M_PI_2;
    int frac1 = floor((n2 - 3) * (theta2 - theta1) / (theta3 - theta1));
    int frac2 = (n2 - 3) - frac1;
    std::vector<double> temptheta1 = EbsdLibMath::linspace(theta1, theta2, frac1 + 2);
    std::vector<double> temptheta2 = EbsdLibMath::linspace(theta2, theta3, frac2 + 2);
    temptheta2.erase(temptheta2.begin());
    B.insert(B.end(), temptheta1.begin(), temptheta1.end());
    B.insert(B.end(), temptheta2.begin(), temptheta2.end());
    for(int i = 0; i < n2; i++)
    {
      double theta = B[i];
      if(theta >= theta1 && theta <= theta2)
      {
        C.push_back(EbsdLibMath::linspace(asin(1 / tan(theta)), M_PI_4, n1));
      }
      else
      {
        C.push_back(EbsdLibMath::linspace(acos(maxk / (tan(A)*sin(theta))), M_PI_4, n1));
      }
    }
  }
  else if(A > M_PI / 6 && A <= deg1 / 2)
  {
    std::vector<double> thetaSort;
    double thetaa = acos((1 - sqrt(6 * tan(A) * tan(A) - 2)) / (3 * tan(A)));
    thetaSort.push_back(thetaa);
    thetaSort.push_back(M_PI_2);
    thetaSort.push_back(acos((2 - sqrt(6 * tan(A)*tan(A) - 2)) / (6 * tan(A))));
    double thetad = (acos(-(maxk / tan(A)) * (maxk / tan(A)))) / 2;
    thetaSort.push_back(thetad);
    std::sort(thetaSort.begin(), thetaSort.end());
    double theta1 = thetaSort[0];
    double theta2 = thetaSort[1];
    double theta3 = thetaSort[2];
    double theta4 = thetaSort[3];
    int frac1 = (floor((n2 - 4) * (theta2 - theta1) / (theta4 - theta1)));
    int frac2 = (floor((n2 - 4) * (theta3 - theta2) / (theta4 - theta1)));
    int frac3 = n2 - 4 - frac1 - frac2;
    std::vector<double> temptheta3 = EbsdLibMath::linspace(theta1, theta2, (frac1 + 2));
    std::vector<double> temptheta4 = EbsdLibMath::linspace(theta2, theta3, (frac2 + 2));
    temptheta4.erase(temptheta4.begin());
    std::vector<double>temptheta5 = EbsdLibMath::linspace(theta3, theta4, (frac3 + 2));
    temptheta5.erase(temptheta5.begin());
    B.insert(B.end(), temptheta3.begin(), temptheta3.end());
    B.insert(B.end(), temptheta4.begin(), temptheta4.end());
    B.insert(B.end(), temptheta5.begin(), temptheta5.end());

    for(int i = 0; i < n2; i++)
    {
      double theta = B[i];
      double phi1, phi2;
      if(thetaa <= thetad)
      {
        if(theta <= theta2)
        {
          phi1 = asin(1 / tan(theta));
          double k = (1 - tan(A) * cos(theta)) / (EbsdLib::Constants::k_Sqrt2 * (tan(A) * sin(theta)));
          if(k > 1)
          {
            k = 1;
          }
          if(k < -1)
          {
            k = -1;
          }
          phi2 = asin(k) - M_PI_4;
        }
        else if(theta > theta2 && theta < theta3)
        {
          phi1 = acos((EbsdLib::Constants::k_Sqrt2 - 1) / (tan(A) * sin(theta)));
          phi2 = M_PI_4;
        }
      }
      else
      {
        if(theta <= theta2)
        {
          phi1 = asin(1 / tan(theta));
        }
        else if(theta > theta2 && theta <= theta3)
        {
          phi1 = acos(maxk / (tan(A) * sin(theta)));
        }

        double d3 = tan(A) * cos(theta);
        double k = ((1 - d3) + sqrt(2 * (tan(A) * tan(A) - d3 * d3) - (1 - d3) * (1 - d3))) / (2 * tan(A) * sin(theta));
        if(k > 1)
        {
          k = 1;
        }
        if(k < -1)
        {
          k = -1;
        }
        phi2 = acos(k);
      }
      if(theta > theta3)
      {
        phi1 = acos(maxk / (tan(A) * sin(theta)));
        phi2 = M_PI_4;
      }
      C.push_back(EbsdLibMath::linspace(phi1, phi2, n1));
    }
  }
  else if(A >= deg1 / 2 && A <= maxdeg / 2)
  {
    double theta1 = acos(((1 - maxk) - sqrt(2 * (tan(A) * tan(A) - maxk * maxk) - (1 - maxk) * (1 - maxk))) / (2 * tan(A)));
    double theta2 = acos((1 - sqrt(6 * (tan(A) * tan(A)) - 2)) / (3 * tan(A)));
    double theta3 = acos((sqrt(tan(A) * tan(A) - 2 * (maxk * maxk))) / (tan(A)));
    int frac1 = (floor((n2 - 3) * (theta2 - theta1) / (theta3 - theta1)));
    int frac2 = (n2 - 3) - frac1;
    std::vector<double> temptheta1 = EbsdLibMath::linspace(theta1, theta2, (frac1 + 2));
    std::vector<double> temptheta2 = EbsdLibMath::linspace(theta2, theta3, (frac2 + 2));
    temptheta2.erase(temptheta2.begin());
    B.insert(B.end(), temptheta1.begin(), temptheta1.end());
    B.insert(B.end(), temptheta2.begin(), temptheta2.end());
    for(int i = 0; i < n2; i++)
    {
      double theta = B[i];
      double phi1, phi2;
      phi1 = acos(maxk / (tan(A) * sin(theta)));
      if(theta >= theta1 && theta < theta2)
      {
        double d3 = tan(A) * cos(theta);
        double k = ((1 - d3) + sqrt(2 * (tan(A) * tan(A) - d3 * d3) - (1 - d3) * (1 - d3))) / (2 * tan(A) * sin(theta));
        phi2 = acos(k);
      }
      else
      {
        phi2 = M_PI_4;
      }
      C.push_back(EbsdLibMath::linspace(phi1, phi2, n1));
    }
  }

  ///create image, fill with empty pixels, setup painter
  // int width = 1000;
  //double scale = width / tan(M_PI / 8);
  /*
  int height = ceil(0.349159 * scale);
  QPainter painter;
  image = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(0x00000000);
  painter.begin(&image);
  painter.setRenderHint(QPainter::HighQualityAntialiasing, true);
  */

  ///draw standard sterographic triangle border (dashed)
  /*
  QPen pen;
  pen.setColor(Qt::black);
  pen.setWidth(2);
  pen.setStyle(Qt::DotLine);
  painter.setPen(pen);
  */
  double r = tan(A);
  std::vector<double> x, y, z;
  y = EbsdLibMath::linspace(0, r / EbsdLib::Constants::k_Sqrt2, 100);
  for(std::vector<double>::size_type i = 0; i < y.size(); i++)
  {
    double k = r * r - y[i] * y[i];
    if(k < 0)
    {
      k = 0;
    }
    x.push_back(sqrt(k));
    z.push_back(0);
  }
  std::vector< std::pair<double, double> > ptsa = rodri2pair(x, y, z);
  //std::vector< std::pair<int, int> > pointlist=scalePoints(ptsa, scale);
  //std::vector<QPoint> qpointlist = pairToQPoint(pointlist);
  //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

  x.clear();
  y.clear();
  z.clear();
  x = EbsdLibMath::linspace(r / EbsdLib::Constants::k_Sqrt3, r, 100);
  for(std::vector<double>::size_type i = 0; i < x.size(); i++)
  {
    double k = r * r - x[i] * x[i];
    if(k < 0)
    {
      k = 0;
    }
    y.push_back(sqrt((k) / 2));
    z.push_back(y[i]);
  }
  ptsa = rodri2pair(x, y, z);
  //pointlist=scalePoints(ptsa, scale);
  //qpointlist = pairToQPoint(pointlist);
  //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

  x.clear();
  y.clear();
  z.clear();
  x = EbsdLibMath::linspace(r / EbsdLib::Constants::k_Sqrt3, r / EbsdLib::Constants::k_Sqrt2, 100);
  for(std::vector<double>::size_type i = 0; i < x.size(); i++)
  {
    y.push_back(x[i]);
    double k = r * r - 2 * x[i] * x[i];
    if(k < 0)
    {
      k = 0;
    }
    z.push_back(sqrt(k));
  }
  ptsa = rodri2pair(x, y, z);
  //pointlist=scalePoints(ptsa, scale);
  //qpointlist = pairToQPoint(pointlist);
  //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

  ///find triangle vertices by converting misorientation space grid to pixels
  std::vector< std::pair<double, double> > ba;
  std::vector<double> d0, d1, d2, d3;
  //int k = 0;
  for(int i = 0; i < n1; i++)
  {
    for(int j = 0; j < n2; j++)
    {
      d3.push_back(tan(A)*cos(B[j]));
      d2.push_back(tan(A)*sin(B[j])*sin(C[j][i]));
      d1.push_back(tan(A)*sin(B[j])*cos(C[j][i]));
      //      double d = 1 - d1[k] * d1[k] - d2[k] * d2[k] - d3[k] * d3[k];
      //      k++;
    }
  }

  ///add corner vertices if the sst isn't filled
  if(A > M_PI / 8)
  {
    d1.push_back(0);
    d2.push_back(0);
    d3.push_back(0);

    if(A > M_PI / 8 && A <= M_PI / 6)
    {
    }
    else
    {
      d1.push_back(r / EbsdLib::Constants::k_Sqrt3);
      d2.push_back(r / EbsdLib::Constants::k_Sqrt3);
      d3.push_back(r / EbsdLib::Constants::k_Sqrt3);
    }
  }
  ba = rodri2pair(d1, d2, d3);

  ///find triangles
  std::vector< std::pair<int, int> > scaledba;//=scalePoints(ba, scale);
  //std::vector<QPoint> qpointba = pairToQPoint(scaledba);


  std::vector< std::vector<int> > triList;
  /*
  tpp::Delaunay::Point tempP;
  vector<tpp::Delaunay::Point> v;
  for(int i=0; i<ba.size(); i++)
  {
    tempP[0]=scaledba[i].first;
    tempP[1]=scaledba[i].second;
    v.push_back(tempP);
  }
  tpp::Delaunay delobject(v);
  delobject.Triangulate();

  for(tpp::Delaunay::fIterator fit = delobject.fbegin(); fit != delobject.fend(); ++fit)
  {
    std::vector<int> triangle;
    triangle.push_back(delobject.Org(fit));
    triangle.push_back(delobject.Dest(fit));
    triangle.push_back(delobject.Apex(fit));
    triList.push_back(triangle);
  }
  */

  ///fill triangles
  for(std::vector< std::pair<double, double> >::size_type i = 0; i < ba.size(); i++)
  {
    QuatD quat, refQuat;
    refQuat.x() = 0;
    refQuat.y() = 0;
    refQuat.z(), () = 0;
    refQuat.w() = 1;
    //have rodrigues vector, need quat
    double tanAng = sqrt(d1[i] * d1[i] + d2[i] * d2[i] + d3[i] * d3[i]);
    double cosAng = std::cos(atanf(tanAng));
    quat.x() = d1[i] * cosAng * tanAng;
    quat.y() = d2[i] * cosAng * tanAng;
    quat.z(), () = d3[i] * cosAng * tanAng;
    quat.w() = cosAng;
    EbsdLib::Rgb pix = generateMisorientationColor(quat, refQuat);
    //image.setPixel(qpointba[i].x(), qpointba[i].y(), pix);
  }

  std::pair<int, int> vert1, vert2, vert3;
  std::vector<int> triangle;

  const double k_PiOver8 = M_PI / 8.0;
  const double k_PiOver6 = M_PI / 6.0;
  int32_t baSizeMinus1 = static_cast<int32_t>(ba.size() - 1);
  int32_t baSizeMinus2 = static_cast<int32_t>(ba.size() - 2);

  for(std::vector< std::vector<int> >::size_type k = 0; k < triList.size(); k++)
  {
    triangle = triList[k];
    vert1 = scaledba[triangle[0]];
    vert2 = scaledba[triangle[1]];
    vert3 = scaledba[triangle[2]];

    //check that none of verticies are special spots
    bool color = true;
    if(A > k_PiOver8)
    {
      if( A <= k_PiOver6)
      {
        //1 extra point at end
        if(triangle[0] == baSizeMinus1)
        {
          color = false;
        }
        if(triangle[1] == baSizeMinus1)
        {
          color = false;
        }
        if(triangle[2] == baSizeMinus1)
        {
          color = false;
        }
      }
      else
      {
        //2 extra points at end
        if(triangle[0] == baSizeMinus1)
        {
          color = false;
        }
        if(triangle[1] == baSizeMinus1)
        {
          color = false;
        }
        if(triangle[2] == baSizeMinus1)
        {
          color = false;
        }
        if(triangle[0] == baSizeMinus2)
        {
          color = false;
        }
        if(triangle[1] == baSizeMinus2)
        {
          color = false;
        }
        if(triangle[2] == baSizeMinus2)
        {
          color = false;
        }
      }
    }

    if(color)
    {
      double x1 = 0.0, x2 = 0.0, x3 = 0.0, y1 = 0.0, y2 = 0.0, y3 = 0.0;
      int r1 = 0, r2 = 0, r3 = 0, g1 = 0, g2 = 0, g3 = 0, b1 = 0, b2 = 0, b3 = 0;
      x1 = vert1.first;
      x2 = vert2.first;
      x3 = vert3.first;
      y1 = vert1.second;
      y2 = vert2.second;
      y3 = vert3.second;

      //find rectangle bounding triangle
      int xMin, xMax, yMin, yMax;
      xMin = std::min(std::min(x1, x2), x3);
      xMax = std::max(std::max(x1, x2), x3);
      yMin = std::min(std::min(y1, y2), y3);
      yMax = std::max(std::max(y1, y2), y3);

      //get colors of vertices
      /*
        QRgb pixval1=image.pixel(x1, y1);
        QRgb pixval2=image.pixel(x2, y2);
        QRgb pixval3=image.pixel(x3, y3);
        r1 = qRed(pixval1);
        r2 = qRed(pixval2);
        r3 = qRed(pixval3);
        g1 = qGreen(pixval1);
        g2 = qGreen(pixval2);
        g3 = qGreen(pixval3);
        b1 = qBlue(pixval1);
        b2 = qBlue(pixval2);
        b3 = qBlue(pixval3);
        */

      //loop over pixels in rectangle
      for(int i = xMin; i <= xMax; i++)
      {
        for(int j = yMin; j <= yMax; j++)
        {
          //determine barycentric coordinates
          double gamma1, gamma2, gamma3;
          double det = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);
          gamma1 = ((y2 - y3) * ((double)i - x3) + (x3 - x2) * ((double)j - y3)) / det;
          gamma2 = ((y3 - y1) * ((double)i - x3) + (x1 - x3) * ((double)j - y3)) / det;
          gamma3 = 1.0 - gamma1 - gamma2;

          //check if pixel is inside triangle
          double minval = -0.0000000000000000001;//misses some boundary points for >=0
          if(gamma1 >= minval && gamma2 >= minval && gamma3 >= minval && gamma1 <= 1.0 && gamma2 <= 1.0 && gamma3 <= 1.0 )
          {
            if(gamma1 == 1.0 || gamma2 == 1.0 || gamma3 == 1.0)
            {
              //vertex
            }
            else
            {
              //edge or inside
#if 0
              uint8_t red = r1 * gamma1 + r2 * gamma2 + r3 * gamma3;
              uint8_t green = g1 * gamma1 + g2 * gamma2 + g3 * gamma3;
              uint8_t blue = b1 * gamma1 + b2 * gamma2 + b3 * gamma3;
              uint8_t alpha = 255;
              unsigned int pix = (alpha << 24) | (red << 16) | (green << 8) | blue;
              image.setPixel(i, j, pix);
#endif
            }
          }
          else
          {
            //outside triangle
          }
        }
      }
    }
  }

  ///Draw Solid Border
  //pen.setStyle(Qt::SolidLine);
  //painter.setPen(pen);

  if(A <= M_PI / 8)
  {
    double k = 0.0;
    x.clear();
    y.clear();
    z.clear();
    y = EbsdLibMath::linspace(0.0, r / sqrt(2.0), 100);
    for(std::vector<double>::size_type i = 0; i < y.size(); i++)
    {
      z.push_back(0);
      k = r * r - y[i] * y[i];
      if(k < 0)
      {
        k = 0;
      }
      x.push_back(sqrt(k));
    }
    ptsa = rodri2pair(x, y, z);
    //std::vector< std::pair<int, int> >pointlist=scalePoints(ptsa, scale);
    //std::vector<QPoint> qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    x = EbsdLibMath::linspace(r / EbsdLib::Constants::k_Sqrt3, r, 100);
    for(std::vector<double>::size_type i = 0; i < x.size(); i++)
    {
      k = (r * r - x[i] * x[i]) / 2;
      if(k < 0)
      {
        k = 0;
      }
      y.push_back(sqrt(k));
      z.push_back(y[i]);
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    x = EbsdLibMath::linspace(r / EbsdLib::Constants::k_Sqrt3, r / EbsdLib::Constants::k_Sqrt2, 100);
    for(std::vector<double>::size_type i = 0; i < x.size(); i++)
    {
      y.push_back(x[i]);
      k = r * r - 2 * x[i] * x[i];
      if(k < 0)
      {
        k = 0;
      }
      z.push_back(sqrt(k));
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));
  }

  else if(A > M_PI / 8 && A <= M_PI / 6)
  {
    x.clear();
    y.clear();
    z.clear();
    double theta1 = atan(1 / sin(M_PI_4));
    double theta2 = acos(-(maxk * maxk) / (tan(A) * tan(A))) / 2;
    double theta3 = M_PI_2;
    double phi3 = acos(maxk / (tan(A) * sin(theta3)));

    y = EbsdLibMath::linspace(r * sin(phi3), r / (EbsdLib::Constants::k_Sqrt2), 100);
    for(std::vector<double>::size_type i = 0; i < y.size(); i++)
    {
      x.push_back(sqrt(r * r - y[i]*y[i]));
      z.push_back(0);
    }
    ptsa = rodri2pair(x, y, z);
    //std::vector< std::pair<int, int> >pointlist=scalePoints(ptsa, scale);
    //std::vector<QPoint> qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    z = EbsdLibMath::linspace(r * cos(theta1), r * cos(theta2), 100);
    for(std::vector<double>::size_type i = 0; i < z.size(); i++)
    {
      y.push_back(z[i]);
      x.push_back(sqrt(r * r - 2 * z[i]*z[i]));
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    z = EbsdLibMath::linspace(r * cos(theta1), r * cos(theta3), 100);
    for(std::vector<double>::size_type i = 0; i < z.size(); i++)
    {
      x.push_back(sqrt((r * r - z[i]*z[i]) / 2));
      y.push_back(x[i]);
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    z = EbsdLibMath::linspace(r * cos(theta2), r * cos(theta3), 100);
    for(std::vector<double>::size_type i = 0; i < z.size(); i++)
    {
      x.push_back(maxk);
      y.push_back(sqrt(r * r - maxk * maxk - z[i]*z[i]));
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

  }
  else if(A > M_PI / 6 && A <= deg1 / 2)
  {
    x.clear();
    y.clear();
    z.clear();
    double thetac = acos((2 - sqrt(6 * (tan(A) * tan(A)) - 2)) / (6 * tan(A)));
    double thetaa = acos((1 - sqrt(6 * (tan(A) * tan(A)) - 2)) / (3 * tan(A)));
    double thetad = acos(-(maxk * maxk) / (tan(A) * tan(A))) / 2;
    double thetab = M_PI_2;
    double phi3 = acos(maxk / (tan(A) * sin(thetab)));

    y = EbsdLibMath::linspace(r * sin(phi3), r / (EbsdLib::Constants::k_Sqrt2), 100);
    for(std::vector<double>::size_type i = 0; i < y.size(); i++)
    {
      z.push_back(0);
      x.push_back(sqrt(r * r - y[i]*y[i]));
    }
    ptsa = rodri2pair(x, y, z);
    //std::vector< std::pair<int, int> >pointlist=scalePoints(ptsa, scale);
    //std::vector<QPoint> qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    z = EbsdLibMath::linspace(r * cos(thetac), r * cos(thetad), 100);
    for(std::vector<double>::size_type i = 0; i < z.size(); i++)
    {
      y.push_back(z[i]);
      double k = r * r - 2 * (z[i] * z[i]);
      if(k < 0.0)
      {
        k = 0.0;
      }
      x.push_back(sqrt(k));
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    z = EbsdLibMath::linspace(r * cos(thetaa), r * cos(thetab), 100);
    for(std::vector<double>::size_type i = 0; i < z.size(); i++)
    {
      x.push_back(sqrt((r * r - (z[i]*z[i])) / 2));
      y.push_back(x[i]);
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    z = EbsdLibMath::linspace(r * cos(thetad), r * cos(thetab), 100);
    for(std::vector<double>::size_type i = 0; i < z.size(); i++)
    {
      x.push_back(maxk);
      y.push_back(sqrt(r * r - maxk * maxk - z[i]*z[i]));
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    std::vector<double> theta, tempd3, phi;
    theta = EbsdLibMath::linspace(thetac, thetaa, 100);
    for(std::vector<double>::size_type i = 0; i < theta.size(); i++)
    {
      tempd3.push_back(tan(A)*cos(theta[i]));
      double k = 2 * ((tan(A) * tan(A)) - tempd3[i] * tempd3[i]) - (1 - tempd3[i]) * (1 - tempd3[i]);
      if(k < 0.0)
      {
        k = 0.0;
      }
      phi.push_back(acos((((1 - tempd3[i]) + (sqrt(k))) / 2) / (tan(A)*sin(theta[i]))));
      z.push_back(r * cos(theta[i]));
      x.push_back(r * sin(theta[i])*cos(phi[i]));
      y.push_back(sqrt(r * r - x[i]*x[i] - z[i]*z[i]));
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));
  }
  else if(A >= deg1 / 2 && A <= maxdeg / 2)
  {
    x.clear();
    y.clear();
    z.clear();
    double theta1 = acos(((1 - maxk) - sqrt(2 * (tan(A) * tan(A) - maxk * maxk) - (1 - maxk) * (1 - maxk))) / (2 * tan(A)));
    double theta2 = acos((1 - sqrt(6 * tan(A) * tan(A) - 2)) / (3 * tan(A)));
    double theta3 = acos((sqrt(tan(A) * tan(A) - 2 * maxk * maxk)) / (tan(A)));

    z = EbsdLibMath::linspace(r * cos(theta2), r * cos(theta3), 100);
    for(std::vector<double>::size_type i = 0; i < z.size(); i++)
    {
      x.push_back(sqrt((r * r - z[i]*z[i]) / 2));
      y.push_back(x[i]);
    }
    ptsa = rodri2pair(x, y, z);
    //std::vector< std::pair<int, int> >pointlist=scalePoints(ptsa, scale);
    //std::vector<QPoint> qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    std::vector<double> theta, tempd3, phi;
    theta = EbsdLibMath::linspace(theta1, theta2, 100);
    for(std::vector<double>::size_type i = 0; i < theta.size(); i++)
    {
      tempd3.push_back(tan(A)*cos(theta[i]));
      double k = 2 * (tan(A) * tan(A) - tempd3[i] * tempd3[i]) - (1 - tempd3[i]) * (1 - tempd3[i]);
      if(k < 0.0)
      {
        k = 0.0;
      }
      phi.push_back(acos((((1 - tempd3[i]) + (sqrt(k))) / 2) / (tan(A)*sin(theta[i]))));
      x.push_back(r * sin(theta[i])*cos(phi[i]));
      z.push_back(r * cos(theta[i]));
      y.push_back(sqrt(r * r - x[i]*x[i] - z[i]*z[i]));
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));

    x.clear();
    y.clear();
    z.clear();
    z = EbsdLibMath::linspace(r * cos(theta1), r * cos(theta3), 100);
    for(std::vector<double>::size_type i = 0; i < z.size(); i++)
    {
      x.push_back(maxk);
      y.push_back(sqrt(r * r - maxk * maxk - z[i]*z[i]));
    }
    ptsa = rodri2pair(x, y, z);
    //pointlist=scalePoints(ptsa, scale);
    //qpointlist = pairToQPoint(pointlist);
    //painter.drawPolyline(qpointlist.data(), static_cast<int>(qpointlist.size()));
  }
#endif
  return image;
}

std::vector<std::pair<double, double>> CubicOps::rodri2pair(std::vector<double> x, std::vector<double> y, std::vector<double> z)
{
  std::vector<std::pair<double, double>> result;
  double q0, q1, q2, q3, ang, r, x1, y1, z1, rad, xPair, yPair, k;

  for(std::vector<double>::size_type i = 0; i < x.size(); i++)
  {
    // rodri2volpreserv
    q0 = sqrt(1 / (1 + x[i] * x[i] + y[i] * y[i] + z[i] * z[i]));
    q1 = x[i] * q0;
    q2 = y[i] * q0;
    q3 = z[i] * q0;
    ang = acos(q0);
    r = pow(1.5 * (ang - sin(ang) * cos(ang)), (1.0 / 3.0));
    x1 = q1 * r;
    y1 = q2 * r;
    z1 = q3 * r;
    if(sin(ang) != 0)
    {
      x1 = x1 / sin(ang);
      y1 = y1 / sin(ang);
      z1 = z1 / sin(ang);
    }

    // areapreservingx
    rad = sqrt(x1 * x1 + y1 * y1 + z1 * z1);
    if(rad == 0)
    {
      rad++;
    }
    k = 2 * (1 - std::fabs(x1 / rad));
    if(k < 0)
    {
      k = 0;
    }
    k = rad * sqrt(k);
    xPair = y1 * k;
    yPair = z1 * k;
    k = rad * rad - x1 * x1;
    if(k > 0)
    {
      xPair = xPair / sqrt(k);
      yPair = yPair / sqrt(k);
    }
    result.push_back(std::make_pair(xPair, yPair));
  }
  return result;
}

// -----------------------------------------------------------------------------
CubicOps::Pointer CubicOps::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::string CubicOps::getNameOfClass() const
{
  return std::string("CubicOps");
}

// -----------------------------------------------------------------------------
std::string CubicOps::ClassName()
{
  return std::string("CubicOps");
}

// -----------------------------------------------------------------------------
CubicOps::Pointer CubicOps::New()
{
  Pointer sharedPtr(new(CubicOps));
  return sharedPtr;
}
