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

#include "CubicLowOps.h"

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
#include "EbsdLib/Utilities/ColorTable.h"
#include "EbsdLib/Utilities/ComputeStereographicProjection.h"
#include "EbsdLib/Utilities/ModifiedLambertProjection.h"

namespace CubicLow
{

static const std::array<size_t, 3> OdfNumBins = {36, 36, 36}; // Represents a 5Deg bin
static const std::array<double, 3> OdfDimInitValue = {std::pow((0.75 * (EbsdLib::Constants::k_PiOver2D - std::sin(EbsdLib::Constants::k_PiOver2D))), (1.0 / 3.0)),
                                                      std::pow((0.75 * (EbsdLib::Constants::k_PiOver2D - std::sin(EbsdLib::Constants::k_PiOver2D))), (1.0 / 3.0)),
                                                      std::pow((0.75 * (EbsdLib::Constants::k_PiOver2D - std::sin(EbsdLib::Constants::k_PiOver2D))), (1.0 / 3.0))};
static const std::array<double, 3> OdfDimStepValue = {OdfDimInitValue[0] / static_cast<double>(OdfNumBins[0] / 2), OdfDimInitValue[1] / static_cast<double>(OdfNumBins[1] / 2),
                                                      OdfDimInitValue[2] / static_cast<double>(OdfNumBins[2] / 2)};

static const int symSize0 = 6;
static const int symSize1 = 12;
static const int symSize2 = 8;

static const int k_OdfSize = 46656;
static const int k_MdfSize = 46656;
static const int k_SymOpsCount = 12;
static const int k_NumMdfBins = 18;

static const std::vector<QuatD> QuatSym = {
    QuatD(0.000000000, 0.000000000, 0.000000000, 1.000000000),   QuatD(1.000000000, 0.000000000, 0.000000000, 0.000000000),   QuatD(0.000000000, 1.000000000, 0.000000000, 0.000000000),
    QuatD(0.000000000, 0.000000000, 1.000000000, 0.000000000),   QuatD(0.500000000, 0.500000000, 0.500000000, 0.500000000),   QuatD(-0.500000000, -0.500000000, -0.500000000, 0.500000000),
    QuatD(0.500000000, -0.500000000, 0.500000000, 0.500000000),  QuatD(-0.500000000, 0.500000000, -0.500000000, 0.500000000), QuatD(-0.500000000, 0.500000000, 0.500000000, 0.500000000),
    QuatD(0.500000000, -0.500000000, -0.500000000, 0.500000000), QuatD(-0.500000000, -0.500000000, 0.500000000, 0.500000000), QuatD(0.500000000, 0.500000000, -0.500000000, 0.500000000)};

static const std::vector<OrientationD> RodSym = {{0.0, 0.0, 0.0},  {10000000000.0, 0.0, 0.0}, {0.0, 10000000000.0, 0.0}, {0.0, 0.0, 10000000000.0}, {1.0, 1.0, 1.0},   {-1.0, -1.0, -1.0},
                                                 {1.0, -1.0, 1.0}, {-1.0, 1.0, -1.0},         {-1.0, 1.0, 1.0},          {1.0, -1.0, -1.0},         {-1.0, -1.0, 1.0}, {1.0, 1.0, -1.0}};

// static const double CubicLowSlipDirections[12][3] = {{0.0, 1.0, -1.0},
//  {1.0, 0.0, -1.0},
//  {1.0, -1.0, 0.0},
//  {1.0, -1.0, 0.0},
//  {1.0, 0.0, 1.0},
//  {0.0, 1.0, 1.0},
//  {1.0, 1.0, 0.0},
//  {0.0, 1.0, 1.0},
//  {1.0, 0.0, -1.0},
//  {1.0, 1.0, 0.0},
//  {1.0, 0.0, 1.0},
//  {0.0, 1.0, -1.0}
//};
//
// static const double CubicLowSlipPlanes[12][3] = {{1.0, 1.0, 1.0},
//  {1.0, 1.0, 1.0},
//  {1.0, 1.0, 1.0},
//  {1.0, 1.0, -1.0},
//  {1.0, 1.0, -1.0},
//  {1.0, 1.0, -1.0},
//  {1.0, -1.0, 1.0},
//  {1.0, -1.0, 1.0},
//  {1.0, -1.0, 1.0},
//  { -1.0, 1.0, 1.0},
//  { -1.0, 1.0, 1.0},
//  { -1.0, 1.0, 1.0}
//};

static const double MatSym[k_SymOpsCount][3][3] = {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}},

                                                   {{-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}},

                                                   {{-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}},

                                                   {{0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}, {1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, -1.0}, {1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}},

                                                   {{0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},

                                                   {{0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}},

                                                   {{0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}};
} // namespace CubicLow

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
CubicLowOps::CubicLowOps() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
CubicLowOps::~CubicLowOps() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool CubicLowOps::getHasInversion() const
{
  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicLowOps::getODFSize() const
{
  return CubicLow::k_OdfSize;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicLowOps::getMDFSize() const
{
  return CubicLow::k_MdfSize;
}

// -----------------------------------------------------------------------------
int CubicLowOps::getMdfPlotBins() const
{
  return CubicLow::k_NumMdfBins;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicLowOps::getNumSymOps() const
{
  return CubicLow::k_SymOpsCount;
}

// -----------------------------------------------------------------------------
std::array<size_t, 3> CubicLowOps::getOdfNumBins() const
{
  return CubicLow::OdfNumBins;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
std::string CubicLowOps::getSymmetryName() const
{
  return "Cubic m3 (Tetrahedral)";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationD CubicLowOps::calculateMisorientation(const QuatD& q1, const QuatD& q2) const
{
  return calculateMisorientationInternal(CubicLow::QuatSym, q1, q2);
}

// -----------------------------------------------------------------------------
OrientationF CubicLowOps::calculateMisorientation(const QuatF& q1f, const QuatF& q2f) const

{
  QuatD q1 = q1f.to<double>();
  QuatD q2 = q2f.to<double>();
  OrientationD axisAngle = calculateMisorientationInternal(CubicLow::QuatSym, q1, q2);
  return axisAngle;
}

QuatD CubicLowOps::getQuatSymOp(int32_t i) const
{
  return CubicLow::QuatSym[i];
}

void CubicLowOps::getRodSymOp(int i, double* r) const
{
  r[0] = CubicLow::RodSym[i][0];
  r[1] = CubicLow::RodSym[i][1];
  r[2] = CubicLow::RodSym[i][2];
}

void CubicLowOps::getMatSymOp(int i, double g[3][3]) const
{
  g[0][0] = CubicLow::MatSym[i][0][0];
  g[0][1] = CubicLow::MatSym[i][0][1];
  g[0][2] = CubicLow::MatSym[i][0][2];
  g[1][0] = CubicLow::MatSym[i][1][0];
  g[1][1] = CubicLow::MatSym[i][1][1];
  g[1][2] = CubicLow::MatSym[i][1][2];
  g[2][0] = CubicLow::MatSym[i][2][0];
  g[2][1] = CubicLow::MatSym[i][2][1];
  g[2][2] = CubicLow::MatSym[i][2][2];
}

void CubicLowOps::getMatSymOp(int i, float g[3][3]) const
{
  g[0][0] = static_cast<float>(CubicLow::MatSym[i][0][0]);
  g[0][1] = static_cast<float>(CubicLow::MatSym[i][0][1]);
  g[0][2] = static_cast<float>(CubicLow::MatSym[i][0][2]);
  g[1][0] = static_cast<float>(CubicLow::MatSym[i][1][0]);
  g[1][1] = static_cast<float>(CubicLow::MatSym[i][1][1]);
  g[1][2] = static_cast<float>(CubicLow::MatSym[i][1][2]);
  g[2][0] = static_cast<float>(CubicLow::MatSym[i][2][0]);
  g[2][1] = static_cast<float>(CubicLow::MatSym[i][2][1]);
  g[2][2] = static_cast<float>(CubicLow::MatSym[i][2][2]);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicLowOps::getODFFZRod(const OrientationType& rod) const
{
  return _calcRodNearestOrigin(CubicLow::RodSym, rod);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicLowOps::getMDFFZRod(const OrientationType& inRod) const
{
  double w = 0.0, n1 = 0.0, n2 = 0.0, n3 = 0.0;
  double FZn1 = 0.0, FZn2 = 0.0, FZn3 = 0.0, FZw = 0.0;

  OrientationType rod = _calcRodNearestOrigin(CubicLow::RodSym, inRod);
  OrientationType ax = OrientationTransformation::ro2ax<OrientationType, OrientationType>(rod);

  n1 = ax[0];
  n2 = ax[1], n3 = ax[2], w = ax[3];

  FZw = w;
  n1 = fabs(n1);
  n2 = fabs(n2);
  n3 = fabs(n3);
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

QuatD CubicLowOps::getNearestQuat(const QuatD& q1, const QuatD& q2) const
{
  return _calcNearestQuat(CubicLow::QuatSym, q1, q2);
}

QuatF CubicLowOps::getNearestQuat(const QuatF& q1f, const QuatF& q2f) const
{
  return _calcNearestQuat(CubicLow::QuatSym, q1f.to<double>(), q2f.to<double>()).to<float>();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicLowOps::getMisoBin(const OrientationType& rod) const
{
  double dim[3];
  double bins[3];
  double step[3];

  OrientationType ho = OrientationTransformation::ro2ho<OrientationType, OrientationType>(rod);

  dim[0] = CubicLow::OdfDimInitValue[0];
  dim[1] = CubicLow::OdfDimInitValue[1];
  dim[2] = CubicLow::OdfDimInitValue[2];
  step[0] = CubicLow::OdfDimStepValue[0];
  step[1] = CubicLow::OdfDimStepValue[1];
  step[2] = CubicLow::OdfDimStepValue[2];
  bins[0] = static_cast<double>(CubicLow::OdfNumBins[0]);
  bins[1] = static_cast<double>(CubicLow::OdfNumBins[1]);
  bins[2] = static_cast<double>(CubicLow::OdfNumBins[2]);

  return _calcMisoBin(dim, bins, step, ho);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicLowOps::determineEulerAngles(double random[3], int choose) const
{
  double init[3];
  double step[3];
  int32_t phi[3];
  double h1, h2, h3;

  init[0] = CubicLow::OdfDimInitValue[0];
  init[1] = CubicLow::OdfDimInitValue[1];
  init[2] = CubicLow::OdfDimInitValue[2];
  step[0] = CubicLow::OdfDimStepValue[0];
  step[1] = CubicLow::OdfDimStepValue[1];
  step[2] = CubicLow::OdfDimStepValue[2];
  phi[0] = static_cast<int32_t>(choose % CubicLow::OdfNumBins[0]);
  phi[1] = static_cast<int32_t>((choose / CubicLow::OdfNumBins[0]) % CubicLow::OdfNumBins[1]);
  phi[2] = static_cast<int32_t>(choose / (CubicLow::OdfNumBins[0] * CubicLow::OdfNumBins[1]));

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
OrientationType CubicLowOps::randomizeEulerAngles(const OrientationType& synea) const
{
  size_t symOp = getRandomSymmetryOperatorIndex(CubicLow::k_SymOpsCount);
  QuatD quat = OrientationTransformation::eu2qu<OrientationType, QuatD>(synea);
  QuatD qc = CubicLow::QuatSym[symOp] * quat;
  return OrientationTransformation::qu2eu<QuatD, OrientationType>(qc);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType CubicLowOps::determineRodriguesVector(double random[3], int choose) const
{
  double init[3];
  double step[3];
  int32_t phi[3];
  double h1, h2, h3;

  init[0] = CubicLow::OdfDimInitValue[0];
  init[1] = CubicLow::OdfDimInitValue[1];
  init[2] = CubicLow::OdfDimInitValue[2];
  step[0] = CubicLow::OdfDimStepValue[0];
  step[1] = CubicLow::OdfDimStepValue[1];
  step[2] = CubicLow::OdfDimStepValue[2];
  phi[0] = static_cast<int32_t>(choose % CubicLow::OdfNumBins[0]);
  phi[1] = static_cast<int32_t>((choose / CubicLow::OdfNumBins[0]) % CubicLow::OdfNumBins[1]);
  phi[2] = static_cast<int32_t>(choose / (CubicLow::OdfNumBins[0] * CubicLow::OdfNumBins[1]));

  _calcDetermineHomochoricValues(random, init, step, phi, h1, h2, h3);
  OrientationType ho(h1, h2, h3);
  OrientationType ro = OrientationTransformation::ho2ro<OrientationType, OrientationType>(ho);
  ro = getMDFFZRod(ro);
  return ro;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int CubicLowOps::getOdfBin(const OrientationType& rod) const
{
  double dim[3];
  double bins[3];
  double step[3];

  OrientationType ho = OrientationTransformation::ro2ho<OrientationType, OrientationType>(rod);

  dim[0] = CubicLow::OdfDimInitValue[0];
  dim[1] = CubicLow::OdfDimInitValue[1];
  dim[2] = CubicLow::OdfDimInitValue[2];
  step[0] = CubicLow::OdfDimStepValue[0];
  step[1] = CubicLow::OdfDimStepValue[1];
  step[2] = CubicLow::OdfDimStepValue[2];
  bins[0] = static_cast<double>(CubicLow::OdfNumBins[0]);
  bins[1] = static_cast<double>(CubicLow::OdfNumBins[1]);
  bins[2] = static_cast<double>(CubicLow::OdfNumBins[2]);

  return _calcODFBin(dim, bins, step, ho);
}

void CubicLowOps::getSchmidFactorAndSS(double load[3], double& schmidfactor, double angleComps[2], int& slipsys) const
{
  schmidfactor = 0;
  slipsys = 0;
  angleComps[0] = 0;
  angleComps[1] = 0;
}

void CubicLowOps::getSchmidFactorAndSS(double load[3], double plane[3], double direction[3], double& schmidfactor, double angleComps[2], int& slipsys) const
{
  schmidfactor = 0;
  slipsys = 0;
  angleComps[0] = 0;
  angleComps[1] = 0;

  // compute mags
  double loadMag = sqrt(load[0] * load[0] + load[1] * load[1] + load[2] * load[2]);
  double planeMag = sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
  double directionMag = sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
  planeMag *= loadMag;
  directionMag *= loadMag;

  // loop over symmetry operators finding highest schmid factor
  for(int i = 0; i < CubicLow::k_SymOpsCount; i++)
  {
    // compute slip system
    double slipPlane[3] = {0};
    slipPlane[2] = CubicLow::MatSym[i][2][0] * plane[0] + CubicLow::MatSym[i][2][1] * plane[1] + CubicLow::MatSym[i][2][2] * plane[2];

    // dont consider negative z planes (to avoid duplicates)
    if(slipPlane[2] >= 0)
    {
      slipPlane[0] = CubicLow::MatSym[i][0][0] * plane[0] + CubicLow::MatSym[i][0][1] * plane[1] + CubicLow::MatSym[i][0][2] * plane[2];
      slipPlane[1] = CubicLow::MatSym[i][1][0] * plane[0] + CubicLow::MatSym[i][1][1] * plane[1] + CubicLow::MatSym[i][1][2] * plane[2];

      double slipDirection[3] = {0};
      slipDirection[0] = CubicLow::MatSym[i][0][0] * direction[0] + CubicLow::MatSym[i][0][1] * direction[1] + CubicLow::MatSym[i][0][2] * direction[2];
      slipDirection[1] = CubicLow::MatSym[i][1][0] * direction[0] + CubicLow::MatSym[i][1][1] * direction[1] + CubicLow::MatSym[i][1][2] * direction[2];
      slipDirection[2] = CubicLow::MatSym[i][2][0] * direction[0] + CubicLow::MatSym[i][2][1] * direction[1] + CubicLow::MatSym[i][2][2] * direction[2];

      double cosPhi = fabs(load[0] * slipPlane[0] + load[1] * slipPlane[1] + load[2] * slipPlane[2]) / planeMag;
      double cosLambda = fabs(load[0] * slipDirection[0] + load[1] * slipDirection[1] + load[2] * slipDirection[2]) / directionMag;

      double schmid = cosPhi * cosLambda;
      if(schmid > schmidfactor)
      {
        schmidfactor = schmid;
        slipsys = i;
        angleComps[0] = acos(cosPhi);
        angleComps[1] = acos(cosLambda);
      }
    }
  }
}

double CubicLowOps::getmPrime(const QuatD& q1, const QuatD& q2, double LD[3]) const
{
  return 0.0;
}

double CubicLowOps::getF1(const QuatD& q1, const QuatD& q2, double LD[3], bool maxSF) const
{
  return 0.0;
}

double CubicLowOps::getF1spt(const QuatD& q1, const QuatD& q2, double LD[3], bool maxSF) const
{
  return 0.0;
}

double CubicLowOps::getF7(const QuatD& q1, const QuatD& q2, double LD[3], bool maxSF) const
{
  return 0.0;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

namespace CubicLow
{
class GenerateSphereCoordsImpl
{
  EbsdLib::FloatArrayType* m_Eulers;
  EbsdLib::FloatArrayType* m_xyz001;
  EbsdLib::FloatArrayType* m_xyz011;
  EbsdLib::FloatArrayType* m_xyz111;

public:
  GenerateSphereCoordsImpl(EbsdLib::FloatArrayType* eulerAngles, EbsdLib::FloatArrayType* xyz001Coords, EbsdLib::FloatArrayType* xyz011Coords, EbsdLib::FloatArrayType* xyz111Coords)
  : m_Eulers(eulerAngles)
  , m_xyz001(xyz001Coords)
  , m_xyz011(xyz011Coords)
  , m_xyz111(xyz111Coords)
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
      direction[1] = -EbsdLib::Constants::k_1OverRoot2D;
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
} // namespace CubicLow

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void CubicLowOps::generateSphereCoordsFromEulers(EbsdLib::FloatArrayType* eulers, EbsdLib::FloatArrayType* xyz001, EbsdLib::FloatArrayType* xyz011, EbsdLib::FloatArrayType* xyz111) const
{
  size_t nOrientations = eulers->getNumberOfTuples();

  // Sanity Check the size of the arrays
  if(xyz001->getNumberOfTuples() < nOrientations * CubicLow::symSize0)
  {
    xyz001->resizeTuples(nOrientations * CubicLow::symSize0 * 3);
  }
  if(xyz011->getNumberOfTuples() < nOrientations * CubicLow::symSize1)
  {
    xyz011->resizeTuples(nOrientations * CubicLow::symSize1 * 3);
  }
  if(xyz111->getNumberOfTuples() < nOrientations * CubicLow::symSize2)
  {
    xyz111->resizeTuples(nOrientations * CubicLow::symSize2 * 3);
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  bool doParallel = true;
  if(doParallel)
  {
    tbb::parallel_for(tbb::blocked_range<size_t>(0, nOrientations), CubicLow::GenerateSphereCoordsImpl(eulers, xyz001, xyz011, xyz111), tbb::auto_partitioner());
  }
  else
#endif
  {
    CubicLow::GenerateSphereCoordsImpl serial(eulers, xyz001, xyz011, xyz111);
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

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool CubicLowOps::inUnitTriangle(double eta, double chi) const
{
  double etaDeg = eta * EbsdLib::Constants::k_180OverPiD;
  double chiMax;
  if(etaDeg > 45.0)
  {
    chiMax = sqrt(1.0 / (2.0 + tan(0.5 * EbsdLib::Constants::k_PiD - eta) * tan(0.5 * EbsdLib::Constants::k_PiD - eta)));
  }
  else
  {
    chiMax = sqrt(1.0 / (2.0 + tan(eta) * tan(eta)));
  }
  EbsdLibMath::bound(chiMax, -1.0, 1.0);
  chiMax = acos(chiMax);
  return !(eta < 0.0 || eta > (90.0 * EbsdLib::Constants::k_PiOver180D) || chi < 0.0 || chi > chiMax);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb CubicLowOps::generateIPFColor(double* eulers, double* refDir, bool convertDegrees) const
{
  return generateIPFColor(eulers[0], eulers[1], eulers[2], refDir[0], refDir[1], refDir[2], convertDegrees);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb CubicLowOps::generateIPFColor(double phi1, double phi, double phi2, double refDir0, double refDir1, double refDir2, bool degToRad) const
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
  double chi = 0.0f;
  double eta = 0.0f;
  double _rgb[3] = {0.0, 0.0, 0.0};

  OrientationType eu(phi1, phi, phi2);
  OrientationType om(9); // Reusable for the loop
  QuatD q1 = OrientationTransformation::eu2qu<OrientationType, QuatD>(eu);

  for(int j = 0; j < CubicLow::k_SymOpsCount; j++)
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
    if(!inUnitTriangle(eta, chi))
    {
      continue;
    }

    break;
  }
  double etaMin = 0.0;
  double etaMax = 90.0;
  double etaDeg = eta * EbsdLib::Constants::k_180OverPiD;
  double chiMax;
  if(etaDeg > 45.0)
  {
    chiMax = sqrt(1.0 / (2.0 + tan(0.5 * EbsdLib::Constants::k_PiD - eta) * tan(0.5 * EbsdLib::Constants::k_PiD - eta)));
  }
  else
  {
    chiMax = sqrt(1.0 / (2.0 + tan(eta) * tan(eta)));
  }
  EbsdLibMath::bound(chiMax, -1.0, 1.0);
  chiMax = acos(chiMax);

  _rgb[0] = 1.0 - chi / chiMax;
  _rgb[2] = fabs(etaDeg - etaMin) / (etaMax - etaMin);
  _rgb[1] = 1 - _rgb[2];
  _rgb[1] *= chi / chiMax;
  _rgb[2] *= chi / chiMax;
  _rgb[0] = sqrt(_rgb[0]);
  _rgb[1] = sqrt(_rgb[1]);
  _rgb[2] = sqrt(_rgb[2]);

  return EbsdLib::RgbColor::dRgb(static_cast<int32_t>(_rgb[0] * 255), static_cast<int32_t>(_rgb[1] * 255), static_cast<int32_t>(_rgb[2] * 255), 255);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb CubicLowOps::generateRodriguesColor(double r1, double r2, double r3) const
{
  double range1 = 2.0f * CubicLow::OdfDimInitValue[0];
  double range2 = 2.0f * CubicLow::OdfDimInitValue[1];
  double range3 = 2.0f * CubicLow::OdfDimInitValue[2];
  double max1 = range1 / 2.0f;
  double max2 = range2 / 2.0f;
  double max3 = range3 / 2.0f;
  double red = (r1 + max1) / range1;
  double green = (r2 + max2) / range2;
  double blue = (r3 + max3) / range3;

  // Scale values from 0 to 1.0
  red = red / max1;
  green = green / max1;
  blue = blue / max2;

  return EbsdLib::RgbColor::dRgb(static_cast<int32_t>(red * 255), static_cast<int32_t>(green * 255), static_cast<int32_t>(blue * 255), 255);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
std::vector<EbsdLib::UInt8ArrayType::Pointer> CubicLowOps::generatePoleFigure(PoleFigureConfiguration_t& config) const
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
  EbsdLib::FloatArrayType::Pointer xyz001 = EbsdLib::FloatArrayType::CreateArray(numOrientations * CubicLow::symSize0, dims, label0 + std::string("xyzCoords"), true);
  // this is size for CUBIC ONLY, <011> Family
  EbsdLib::FloatArrayType::Pointer xyz011 = EbsdLib::FloatArrayType::CreateArray(numOrientations * CubicLow::symSize1, dims, label1 + std::string("xyzCoords"), true);
  // this is size for CUBIC ONLY, <111> Family
  EbsdLib::FloatArrayType::Pointer xyz111 = EbsdLib::FloatArrayType::CreateArray(numOrientations * CubicLow::symSize2, dims, label2 + std::string("xyzCoords"), true);

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
  EbsdLib::UInt8ArrayType::Pointer image001 = EbsdLib::UInt8ArrayType::CreateArray(config.imageDim * config.imageDim, dims, label0, true);
  EbsdLib::UInt8ArrayType::Pointer image011 = EbsdLib::UInt8ArrayType::CreateArray(config.imageDim * config.imageDim, dims, label1, true);
  EbsdLib::UInt8ArrayType::Pointer image111 = EbsdLib::UInt8ArrayType::CreateArray(config.imageDim * config.imageDim, dims, label2, true);

  std::vector<EbsdLib::UInt8ArrayType::Pointer> poleFigures(3);
  if(config.order.size() == 3)
  {
    poleFigures[config.order[0]] = image001;
    poleFigures[config.order[1]] = image011;
    poleFigures[config.order[2]] = image111;
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

  return poleFigures;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::UInt8ArrayType::Pointer CubicLowOps::generateIPFTriangleLegend(int imageDim) const
{

  std::vector<size_t> dims(1, 4);
  EbsdLib::UInt8ArrayType::Pointer image = EbsdLib::UInt8ArrayType::CreateArray(imageDim * imageDim, dims, getSymmetryName() + " Triangle Legend", true);
  image->initializeWithValue(255);
  return image;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb CubicLowOps::generateMisorientationColor(const QuatD& q, const QuatD& refFrame) const
{
  throw std::out_of_range("generateMisorientationColor::generateMisorientationColor NOT Implemented");

  // double n1, n2, n3, w;
  double x, x1, x2, x3, x4;
  double y, y1, y2, y3, y4;
  double z, z1, z2, z3, z4;
  double k, h, s, v, c, r, g, b;

  QuatD q1 = q;
  QuatD q2 = refFrame;

  OrientationD axisAngle = calculateMisorientation(q1, q2);

  // eq c7.1
  k = tan(axisAngle[3] / 2.0);
  x = axisAngle[0];
  y = axisAngle[1];
  z = axisAngle[2];
  OrientationType rod(x, y, z, k);
  rod = getMDFFZRod(rod);
  x = rod[0];
  y = rod[1];
  z = rod[2];
  k = rod[3];

  // eq c7.2
  k = atan2(y, x);
  if(k < M_PI_4)
  {
    x1 = z;
    y1 = x;
    z1 = y;
  }
  else
  {
    x1 = y;
    y1 = z;
    z1 = x;
  }

  // eq c7.3
  // 3 rotation matricies (in paper) can be multiplied into one (here) for simplicity / speed
  // g1*g2*g3 = {{sqrt(2/3), 0, 1/sqrt(3)},{-1/sqrt(6), 1/sqrt(2), 1/sqrt(3)},{-1/sqrt(6), 1/sqrt(2), 1/sqrt(3)}}
  x2 = x1 * sqrt(2.0f / 3.0) - (y1 + z1) / sqrt(6.0);
  y2 = (y1 - z1) / sqrt(2.0);
  z2 = (x1 + y1 + z1) / sqrt(3.0);

  // eq c7.4
  k = (sqrt(3.0) * y2 + x2) / (2.0f * pow(x2 * x2 + y2 * y2, 1.5f));
  x3 = x2 * (x2 + sqrt(3.0) * y2) * (x2 - sqrt(3.0) * y2) * k;
  y3 = y2 * (y2 + sqrt(3.0) * x2) * (sqrt(3.0) * x2 - y2) * k;
  z3 = z2 * sqrt(3.0);

  // eq c7.5 these hsv are from 0 to 1 in cartesian coordinates
  x4 = -x3;
  y4 = -y3;
  z4 = z3;

  // convert to traditional hsv (0-1)
  h = fmod(atan2(y4, x4) + 2.0f * M_PI, 2.0f * M_PI) / (2.0f * M_PI);
  s = sqrt(x4 * x4 + y4 * y4);
  v = z4;
  if(v > 0)
  {
    s = s / v;
  }

  // hsv to rgb (from wikipedia hsv/hsl page)
  c = v * s;
  k = c * (1 - fabs(fmod(h * 6, 2) - 1)); // x in wiki article
  h = h * 6;
  r = 0;
  g = 0;
  b = 0;

  if(h >= 0)
  {
    if(h < 1)
    {
      r = c;
      g = k;
    }
    else if(h < 2)
    {
      r = k;
      g = c;
    }
    else if(h < 3)
    {
      g = c;
      b = k;
    }
    else if(h < 4)
    {
      g = k;
      b = c;
    }
    else if(h < 5)
    {
      r = k;
      b = c;
    }
    else if(h < 6)
    {
      r = c;
      b = k;
    }
  }

  // adjust lumosity and invert
  r = (r + (v - c));
  g = (g + (v - c));
  b = (b + (v - c));

  return EbsdLib::RgbColor::dRgb(static_cast<int32_t>(r * 255), static_cast<int32_t>(g * 255), static_cast<int32_t>(b * 255), 255);
}

// -----------------------------------------------------------------------------
CubicLowOps::Pointer CubicLowOps::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::string CubicLowOps::getNameOfClass() const
{
  return std::string("CubicLowOps");
}

// -----------------------------------------------------------------------------
std::string CubicLowOps::ClassName()
{
  return std::string("CubicLowOps");
}

// -----------------------------------------------------------------------------
CubicLowOps::Pointer CubicLowOps::New()
{
  Pointer sharedPtr(new(CubicLowOps));
  return sharedPtr;
}
