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

#include "TetragonalOps.h"

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
#include "EbsdLib/Utilities/PoleFigureUtilities.h"

namespace TetragonalHigh
{
static const std::array<size_t, 3> OdfNumBins = {36, 36, 18}; // Represents a 5Deg bin

static const std::array<double, 3> OdfDimInitValue = {std::pow((0.75 * ((EbsdLib::Constants::k_PiOver2D)-std::sin((EbsdLib::Constants::k_PiOver2D)))), (1.0 / 3.0)),
                                                      std::pow((0.75 * ((EbsdLib::Constants::k_PiOver2D)-std::sin((EbsdLib::Constants::k_PiOver2D)))), (1.0 / 3.0)),
                                                      std::pow((0.75 * ((EbsdLib::Constants::k_PiOver4D)-std::sin((EbsdLib::Constants::k_PiOver4D)))), (1.0 / 3.0))};
static const std::array<double, 3> OdfDimStepValue = {OdfDimInitValue[0] / static_cast<double>(OdfNumBins[0] / 2), OdfDimInitValue[1] / static_cast<double>(OdfNumBins[1] / 2),
                                                      OdfDimInitValue[2] / static_cast<double>(OdfNumBins[2] / 2)};

static const int symSize0 = 2;
static const int symSize1 = 4;
static const int symSize2 = 4;

static const int k_OdfSize = 23328;
static const int k_MdfSize = 23328;
static const int k_SymOpsCount = 8;
static const int k_NumMdfBins = 20;

static const std::vector<QuatD> QuatSym = {QuatD(0.000000000, 0.000000000, 0.000000000, 1.000000000),
                                           QuatD(1.000000000, 0.000000000, 0.000000000, 0.000000000),
                                           QuatD(0.000000000, 1.000000000, 0.000000000, 0.000000000),
                                           QuatD(0.000000000, 0.000000000, 1.000000000, 0.000000000),
                                           QuatD(0.000000000, 0.000000000, EbsdLib::Constants::k_1OverRoot2D, -EbsdLib::Constants::k_1OverRoot2D),
                                           QuatD(0.000000000, 0.000000000, EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D),
                                           QuatD(EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D, 0.000000000, 0.000000000),
                                           QuatD(-EbsdLib::Constants::k_1OverRoot2D, EbsdLib::Constants::k_1OverRoot2D, 0.000000000, 0.000000000)};

static const std::vector<OrientationD> RodSym = {{0.0, 0.0, 0.0},  {10000000000.0, 0.0, 0.0}, {0.0, 10000000000.0, 0.0},           {0.0, 0.0, 10000000000.0},
                                                 {0.0, 0.0, -1.0}, {0.0, 0.0, 1.0},           {10000000000.0, 10000000000.0, 0.0}, {-10000000000.0, 10000000000.0, 0.0}};

static const double MatSym[k_SymOpsCount][3][3] = {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}},

                                                   {{-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}},

                                                   {{-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{0.0, -1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},

                                                   {{0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}},

                                                   {{0.0, -1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}}};

} // namespace TetragonalHigh

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
TetragonalOps::TetragonalOps() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
TetragonalOps::~TetragonalOps() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool TetragonalOps::getHasInversion() const
{
  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int TetragonalOps::getODFSize() const
{
  return TetragonalHigh::k_OdfSize;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int TetragonalOps::getMDFSize() const
{
  return TetragonalHigh::k_MdfSize;
}

// -----------------------------------------------------------------------------
int TetragonalOps::getMdfPlotBins() const
{
  return TetragonalHigh::k_NumMdfBins;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int TetragonalOps::getNumSymOps() const
{
  return TetragonalHigh::k_SymOpsCount;
}

// -----------------------------------------------------------------------------
std::array<size_t, 3> TetragonalOps::getOdfNumBins() const
{
  return TetragonalHigh::OdfNumBins;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
std::string TetragonalOps::getSymmetryName() const
{
  return "Tetragonal 4/mmm";
  ;
}

// -----------------------------------------------------------------------------
OrientationD TetragonalOps::calculateMisorientation(const QuatD& q1, const QuatD& q2) const
{
  return calculateMisorientationInternal(TetragonalHigh::QuatSym, q1, q2);
}

// -----------------------------------------------------------------------------
OrientationF TetragonalOps::calculateMisorientation(const QuatF& q1f, const QuatF& q2f) const

{
  QuatD q1 = q1f.to<double>();
  QuatD q2 = q2f.to<double>();
  OrientationD axisAngle = calculateMisorientationInternal(TetragonalHigh::QuatSym, q1, q2);
  return axisAngle;
}

QuatD TetragonalOps::getQuatSymOp(int32_t i) const
{
  return TetragonalHigh::QuatSym[i];
}

void TetragonalOps::getRodSymOp(int i, double* r) const
{
  r[0] = TetragonalHigh::RodSym[i][0];
  r[1] = TetragonalHigh::RodSym[i][1];
  r[2] = TetragonalHigh::RodSym[i][2];
}

void TetragonalOps::getMatSymOp(int i, double g[3][3]) const
{
  g[0][0] = TetragonalHigh::MatSym[i][0][0];
  g[0][1] = TetragonalHigh::MatSym[i][0][1];
  g[0][2] = TetragonalHigh::MatSym[i][0][2];
  g[1][0] = TetragonalHigh::MatSym[i][1][0];
  g[1][1] = TetragonalHigh::MatSym[i][1][1];
  g[1][2] = TetragonalHigh::MatSym[i][1][2];
  g[2][0] = TetragonalHigh::MatSym[i][2][0];
  g[2][1] = TetragonalHigh::MatSym[i][2][1];
  g[2][2] = TetragonalHigh::MatSym[i][2][2];
}

void TetragonalOps::getMatSymOp(int i, float g[3][3]) const
{
  g[0][0] = static_cast<float>(TetragonalHigh::MatSym[i][0][0]);
  g[0][1] = static_cast<float>(TetragonalHigh::MatSym[i][0][1]);
  g[0][2] = static_cast<float>(TetragonalHigh::MatSym[i][0][2]);
  g[1][0] = static_cast<float>(TetragonalHigh::MatSym[i][1][0]);
  g[1][1] = static_cast<float>(TetragonalHigh::MatSym[i][1][1]);
  g[1][2] = static_cast<float>(TetragonalHigh::MatSym[i][1][2]);
  g[2][0] = static_cast<float>(TetragonalHigh::MatSym[i][2][0]);
  g[2][1] = static_cast<float>(TetragonalHigh::MatSym[i][2][1]);
  g[2][2] = static_cast<float>(TetragonalHigh::MatSym[i][2][2]);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType TetragonalOps::getODFFZRod(const OrientationType& rod) const
{
  return _calcRodNearestOrigin(TetragonalHigh::RodSym, rod);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType TetragonalOps::getMDFFZRod(const OrientationType& inRod) const
{
  double FZn1 = 0.0, FZn2 = 0.0, FZn3 = 0.0, FZw = 0.0;

  OrientationType rod = _calcRodNearestOrigin(TetragonalHigh::RodSym, inRod);

  OrientationType ax = OrientationTransformation::ro2ax<OrientationType, OrientationType>(rod);

  FZn1 = std::fabs(ax[0]);
  FZn2 = std::fabs(ax[1]);
  FZn3 = std::fabs(ax[2]);
  FZw = ax[3];

  return OrientationTransformation::ax2ro<OrientationType, OrientationType>(OrientationType(FZn1, FZn2, FZn3, FZw));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QuatD TetragonalOps::getNearestQuat(const QuatD& q1, const QuatD& q2) const
{
  return _calcNearestQuat(TetragonalHigh::QuatSym, q1, q2);
}
QuatF TetragonalOps::getNearestQuat(const QuatF& q1f, const QuatF& q2f) const
{
  return _calcNearestQuat(TetragonalHigh::QuatSym, q1f.to<double>(), q2f.to<double>()).to<float>();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int TetragonalOps::getMisoBin(const OrientationType& rod) const
{
  double dim[3];
  double bins[3];
  double step[3];

  OrientationType ho = OrientationTransformation::ro2ho<OrientationType, OrientationType>(rod);

  dim[0] = TetragonalHigh::OdfDimInitValue[0];
  dim[1] = TetragonalHigh::OdfDimInitValue[1];
  dim[2] = TetragonalHigh::OdfDimInitValue[2];
  step[0] = TetragonalHigh::OdfDimStepValue[0];
  step[1] = TetragonalHigh::OdfDimStepValue[1];
  step[2] = TetragonalHigh::OdfDimStepValue[2];
  bins[0] = static_cast<double>(TetragonalHigh::OdfNumBins[0]);
  bins[1] = static_cast<double>(TetragonalHigh::OdfNumBins[1]);
  bins[2] = static_cast<double>(TetragonalHigh::OdfNumBins[2]);

  return _calcMisoBin(dim, bins, step, ho);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType TetragonalOps::determineEulerAngles(double random[3], int choose) const
{
  double init[3];
  double step[3];
  int32_t phi[3];
  double h1, h2, h3;

  init[0] = TetragonalHigh::OdfDimInitValue[0];
  init[1] = TetragonalHigh::OdfDimInitValue[1];
  init[2] = TetragonalHigh::OdfDimInitValue[2];
  step[0] = TetragonalHigh::OdfDimStepValue[0];
  step[1] = TetragonalHigh::OdfDimStepValue[1];
  step[2] = TetragonalHigh::OdfDimStepValue[2];
  phi[0] = static_cast<int32_t>(choose % TetragonalHigh::OdfNumBins[0]);
  phi[1] = static_cast<int32_t>((choose / TetragonalHigh::OdfNumBins[0]) % TetragonalHigh::OdfNumBins[1]);
  phi[2] = static_cast<int32_t>(choose / (TetragonalHigh::OdfNumBins[0] * TetragonalHigh::OdfNumBins[1]));

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
OrientationType TetragonalOps::randomizeEulerAngles(const OrientationType& synea) const
{
  size_t symOp = getRandomSymmetryOperatorIndex(TetragonalHigh::k_SymOpsCount);
  QuatD quat = OrientationTransformation::eu2qu<OrientationType, QuatD>(synea);
  QuatD qc = TetragonalHigh::QuatSym[symOp] * quat;
  return OrientationTransformation::qu2eu<QuatD, OrientationType>(qc);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
OrientationType TetragonalOps::determineRodriguesVector(double random[3], int choose) const
{
  double init[3];
  double step[3];
  int32_t phi[3];
  double h1, h2, h3;

  init[0] = TetragonalHigh::OdfDimInitValue[0];
  init[1] = TetragonalHigh::OdfDimInitValue[1];
  init[2] = TetragonalHigh::OdfDimInitValue[2];
  step[0] = TetragonalHigh::OdfDimStepValue[0];
  step[1] = TetragonalHigh::OdfDimStepValue[1];
  step[2] = TetragonalHigh::OdfDimStepValue[2];
  phi[0] = static_cast<int32_t>(choose % TetragonalHigh::OdfNumBins[0]);
  phi[1] = static_cast<int32_t>((choose / TetragonalHigh::OdfNumBins[0]) % TetragonalHigh::OdfNumBins[1]);
  phi[2] = static_cast<int32_t>(choose / (TetragonalHigh::OdfNumBins[0] * TetragonalHigh::OdfNumBins[1]));

  _calcDetermineHomochoricValues(random, init, step, phi, h1, h2, h3);
  OrientationType ho(h1, h2, h3);
  OrientationType ro = OrientationTransformation::ho2ro<OrientationType, OrientationType>(ho);
  ro = getMDFFZRod(ro);
  return ro;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int TetragonalOps::getOdfBin(const OrientationType& rod) const
{
  double dim[3];
  double bins[3];
  double step[3];

  OrientationType ho = OrientationTransformation::ro2ho<OrientationType, OrientationType>(rod);

  dim[0] = TetragonalHigh::OdfDimInitValue[0];
  dim[1] = TetragonalHigh::OdfDimInitValue[1];
  dim[2] = TetragonalHigh::OdfDimInitValue[2];
  step[0] = TetragonalHigh::OdfDimStepValue[0];
  step[1] = TetragonalHigh::OdfDimStepValue[1];
  step[2] = TetragonalHigh::OdfDimStepValue[2];
  bins[0] = static_cast<double>(TetragonalHigh::OdfNumBins[0]);
  bins[1] = static_cast<double>(TetragonalHigh::OdfNumBins[1]);
  bins[2] = static_cast<double>(TetragonalHigh::OdfNumBins[2]);

  return _calcODFBin(dim, bins, step, ho);
}

void TetragonalOps::getSchmidFactorAndSS(double load[3], double& schmidfactor, double angleComps[2], int& slipsys) const
{
  schmidfactor = 0;
  slipsys = 0;
}

void TetragonalOps::getSchmidFactorAndSS(double load[3], double plane[3], double direction[3], double& schmidfactor, double angleComps[2], int& slipsys) const
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
  for(int i = 0; i < TetragonalHigh::k_SymOpsCount; i++)
  {
    // compute slip system
    double slipPlane[3] = {0};
    slipPlane[2] = TetragonalHigh::MatSym[i][2][0] * plane[0] + TetragonalHigh::MatSym[i][2][1] * plane[1] + TetragonalHigh::MatSym[i][2][2] * plane[2];

    // dont consider negative z planes (to avoid duplicates)
    if(slipPlane[2] >= 0)
    {
      slipPlane[0] = TetragonalHigh::MatSym[i][0][0] * plane[0] + TetragonalHigh::MatSym[i][0][1] * plane[1] + TetragonalHigh::MatSym[i][0][2] * plane[2];
      slipPlane[1] = TetragonalHigh::MatSym[i][1][0] * plane[0] + TetragonalHigh::MatSym[i][1][1] * plane[1] + TetragonalHigh::MatSym[i][1][2] * plane[2];

      double slipDirection[3] = {0};
      slipDirection[0] = TetragonalHigh::MatSym[i][0][0] * direction[0] + TetragonalHigh::MatSym[i][0][1] * direction[1] + TetragonalHigh::MatSym[i][0][2] * direction[2];
      slipDirection[1] = TetragonalHigh::MatSym[i][1][0] * direction[0] + TetragonalHigh::MatSym[i][1][1] * direction[1] + TetragonalHigh::MatSym[i][1][2] * direction[2];
      slipDirection[2] = TetragonalHigh::MatSym[i][2][0] * direction[0] + TetragonalHigh::MatSym[i][2][1] * direction[1] + TetragonalHigh::MatSym[i][2][2] * direction[2];

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

double TetragonalOps::getmPrime(const QuatD& q1, const QuatD& q2, double LD[3]) const
{
  return 0.0;
}

double TetragonalOps::getF1(const QuatD& q1, const QuatD& q2, double LD[3], bool maxS) const
{
  return 0.0;
}

double TetragonalOps::getF1spt(const QuatD& q1, const QuatD& q2, double LD[3], bool maxS) const
{
  return 0.0;
}

double TetragonalOps::getF7(const QuatD& q1, const QuatD& q2, double LD[3], bool maxS) const
{
  return 0.0;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
namespace TetragonalHigh
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

    // Geneate all the Coordinates
    for(size_t i = start; i < end; ++i)
    {
      OrientationType eu(m_Eulers->getValue(i * 3), m_Eulers->getValue(i * 3 + 1), m_Eulers->getValue(i * 3 + 2));
      OrientationTransformation::eu2om<OrientationType, OrientationType>(eu).toGMatrix(g);

      EbsdMatrixMath::Transpose3x3(g, gTranpose);

      // -----------------------------------------------------------------------------
      // 001 Family
      direction[0] = 0.0;
      direction[1] = 0.0;
      direction[2] = 1.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz001->getPointer(i * 6));
      EbsdMatrixMath::Copy3x1(m_xyz001->getPointer(i * 6), m_xyz001->getPointer(i * 6 + 3));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz001->getPointer(i * 6 + 3), -1.0f);

      // -----------------------------------------------------------------------------
      // 011 Family
      direction[0] = 1.0;
      direction[1] = 0.0;
      direction[2] = 0.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz011->getPointer(i * 12));
      EbsdMatrixMath::Copy3x1(m_xyz011->getPointer(i * 12), m_xyz011->getPointer(i * 12 + 3));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz011->getPointer(i * 12 + 3), -1.0f);
      direction[0] = 0.0;
      direction[1] = 1.0;
      direction[2] = 0.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz011->getPointer(i * 12 + 6));
      EbsdMatrixMath::Copy3x1(m_xyz011->getPointer(i * 12 + 6), m_xyz011->getPointer(i * 12 + 9));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz011->getPointer(i * 12 + 9), -1.0f);

      // -----------------------------------------------------------------------------
      // 111 Family
      direction[0] = EbsdLib::Constants::k_1OverRoot2D;
      direction[1] = EbsdLib::Constants::k_1OverRoot2D;
      direction[2] = 0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz111->getPointer(i * 12));
      EbsdMatrixMath::Copy3x1(m_xyz111->getPointer(i * 12), m_xyz111->getPointer(i * 12 + 3));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz111->getPointer(i * 12 + 3), -1.0f);
      direction[0] = -EbsdLib::Constants::k_1OverRoot2D;
      direction[1] = EbsdLib::Constants::k_1OverRoot2D;
      direction[2] = 0.0;
      EbsdMatrixMath::Multiply3x3with3x1(gTranpose, direction, m_xyz111->getPointer(i * 12 + 6));
      EbsdMatrixMath::Copy3x1(m_xyz111->getPointer(i * 12 + 6), m_xyz111->getPointer(i * 12 + 9));
      EbsdMatrixMath::Multiply3x1withConstant(m_xyz111->getPointer(i * 12 + 9), -1.0f);
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    generate(r.begin(), r.end());
  }
#endif
};
} // namespace TetragonalHigh
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void TetragonalOps::generateSphereCoordsFromEulers(EbsdLib::FloatArrayType* eulers, EbsdLib::FloatArrayType* xyz001, EbsdLib::FloatArrayType* xyz011, EbsdLib::FloatArrayType* xyz111) const
{
  size_t nOrientations = eulers->getNumberOfTuples();

  // Sanity Check the size of the arrays
  if(xyz001->getNumberOfTuples() < nOrientations * TetragonalHigh::symSize0)
  {
    xyz001->resizeTuples(nOrientations * TetragonalHigh::symSize0 * 3);
  }
  if(xyz011->getNumberOfTuples() < nOrientations * TetragonalHigh::symSize1)
  {
    xyz011->resizeTuples(nOrientations * TetragonalHigh::symSize1 * 3);
  }
  if(xyz111->getNumberOfTuples() < nOrientations * TetragonalHigh::symSize2)
  {
    xyz111->resizeTuples(nOrientations * TetragonalHigh::symSize2 * 3);
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  bool doParallel = true;
  if(doParallel)
  {
    tbb::parallel_for(tbb::blocked_range<size_t>(0, nOrientations), TetragonalHigh::GenerateSphereCoordsImpl(eulers, xyz001, xyz011, xyz111), tbb::auto_partitioner());
  }
  else
#endif
  {
    TetragonalHigh::GenerateSphereCoordsImpl serial(eulers, xyz001, xyz011, xyz111);
    serial.generate(0, nOrientations);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool TetragonalOps::inUnitTriangle(double eta, double chi) const
{
  return !(eta < 0 || eta > (45.0 * EbsdLib::Constants::k_PiOver180D) || chi < 0 || chi > (90.0 * EbsdLib::Constants::k_PiOver180D));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb TetragonalOps::generateIPFColor(double* eulers, double* refDir, bool convertDegrees) const
{
  return generateIPFColor(eulers[0], eulers[1], eulers[2], refDir[0], refDir[1], refDir[2], convertDegrees);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb TetragonalOps::generateIPFColor(double phi1, double phi, double phi2, double refDir0, double refDir1, double refDir2, bool degToRad) const
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

  for(int j = 0; j < TetragonalHigh::k_SymOpsCount; j++)
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
  double etaMax = 45.0;
  double chiMax = 90.0;
  double etaDeg = eta * EbsdLib::Constants::k_180OverPiD;
  double chiDeg = chi * EbsdLib::Constants::k_180OverPiD;

  _rgb[0] = 1.0 - chiDeg / chiMax;
  _rgb[2] = fabs(etaDeg - etaMin) / (etaMax - etaMin);
  _rgb[1] = 1 - _rgb[2];
  _rgb[1] *= chiDeg / chiMax;
  _rgb[2] *= chiDeg / chiMax;
  _rgb[0] = sqrt(_rgb[0]);
  _rgb[1] = sqrt(_rgb[1]);
  _rgb[2] = sqrt(_rgb[2]);

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
EbsdLib::Rgb TetragonalOps::generateRodriguesColor(double r1, double r2, double r3) const
{
  double range1 = 2.0f * TetragonalHigh::OdfDimInitValue[0];
  double range2 = 2.0f * TetragonalHigh::OdfDimInitValue[1];
  double range3 = 2.0f * TetragonalHigh::OdfDimInitValue[2];
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
std::vector<EbsdLib::UInt8ArrayType::Pointer> TetragonalOps::generatePoleFigure(PoleFigureConfiguration_t& config) const
{
  std::string label0 = std::string("<001>");
  std::string label1 = std::string("<100>");
  std::string label2 = std::string("<110>");
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
  EbsdLib::FloatArrayType::Pointer xyz001 = EbsdLib::FloatArrayType::CreateArray(numOrientations * TetragonalHigh::symSize0, dims, label0 + std::string("xyzCoords"), true);
  // this is size for CUBIC ONLY, <011> Family
  EbsdLib::FloatArrayType::Pointer xyz011 = EbsdLib::FloatArrayType::CreateArray(numOrientations * TetragonalHigh::symSize1, dims, label1 + std::string("xyzCoords"), true);
  // this is size for CUBIC ONLY, <111> Family
  EbsdLib::FloatArrayType::Pointer xyz111 = EbsdLib::FloatArrayType::CreateArray(numOrientations * TetragonalHigh::symSize2, dims, label2 + std::string("xyzCoords"), true);

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
EbsdLib::UInt8ArrayType::Pointer TetragonalOps::generateIPFTriangleLegend(int imageDim) const
{

  std::vector<size_t> dims(1, 4);
  EbsdLib::UInt8ArrayType::Pointer image = EbsdLib::UInt8ArrayType::CreateArray(imageDim * imageDim, dims, getSymmetryName() + " Triangle Legend", true);
  uint32_t* pixelPtr = reinterpret_cast<uint32_t*>(image->getPointer(0));

  double xInc = 1.0f / static_cast<double>(imageDim);
  double yInc = 1.0f / static_cast<double>(imageDim);
  double rad = 1.0f;

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

  EbsdLib::Rgb color;
  size_t idx = 0;
  size_t yScanLineIndex = 0; // We use this to control where the data is drawn. Otherwise the image will come out flipped vertically
  // Loop over every pixel in the image and project up to the sphere to get the angle and then figure out the RGB from
  // there.
  for(int32_t yIndex = 0; yIndex < imageDim; ++yIndex)
  {

    for(int32_t xIndex = 0; xIndex < imageDim; ++xIndex)
    {
      idx = (imageDim * yScanLineIndex) + xIndex;

      x = xIndex * xInc;
      y = yIndex * yInc;

      double sumSquares = (x * x) + (y * y);
      if(x > y || sumSquares > 1.0) // Outside unit circle
      {
        color = 0xFFFFFFFF;
      }
      else if(sumSquares > (rad - 2 * xInc) && sumSquares < (rad + 2 * xInc)) // Black border on the edges
      {
        color = 0xFF000000;
      }
      else if(xIndex == 0 || yIndex == 0 || xIndex == yIndex) // Black border on the edges
      {
        color = 0xFF000000;
      }
      else
      {
        a = (x * x + y * y + 1);
        b = (2 * x * x + 2 * y * y);
        c = (x * x + y * y - 1);

        val = (-b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);
        x1 = (1 + val) * x;
        y1 = (1 + val) * y;
        z1 = val;
        denom = (x1 * x1) + (y1 * y1) + (z1 * z1);
        denom = sqrt(denom);
        x1 = x1 / denom;
        y1 = y1 / denom;
        z1 = z1 / denom;

        color = generateIPFColor(0.0, 0.0, 0.0, x1, y1, z1, false);
      }

      pixelPtr[idx] = color;
    }
    yScanLineIndex++;
  }
  return image;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::Rgb TetragonalOps::generateMisorientationColor(const QuatD& q, const QuatD& refFrame) const
{
  throw std::out_of_range("TetragonalOps::generateMisorientationColor NOT Implemented");

  // double n1, n2, n3, w;
  double xo;
  double xo1;
  double xo2;
  double xo3;
  double x;
  double x1;
  double x2;
  double x3;
  double x4;
  double x5;
  double x6;
  double x7;
  double x8;
  double x9;
  double x10;
  double x11;
  double yo;
  double yo1;
  double yo2;
  double yo3;
  double y;
  double y1;
  double y2;
  double y3;
  double y4;
  double y5;
  double y6;
  double y7;
  double y8;
  double y9;
  double y10;
  double y11;
  double zo;
  double zo1;
  double zo2;
  double zo3;
  double z;
  double z1;
  double z2;
  double z3;
  double z4;
  double z5;
  double z6;
  double z7;
  double z8;
  double z9;
  double z10;
  double z11;
  double k;
  double h;
  double s;
  double v;
  double c;
  double r;
  double g;
  double b;

  QuatD q1 = q;
  QuatD q2 = refFrame;

  // get misorientation
  OrientationD axisAngle = calculateMisorientation(q1, q2);

  axisAngle[0] = fabs(axisAngle[0]);
  axisAngle[1] = fabs(axisAngle[1]);
  axisAngle[2] = fabs(axisAngle[2]);

  // eq c5.1
  k = tan(axisAngle[3] / 2.0);
  xo = axisAngle[0];
  yo = axisAngle[1];
  zo = axisAngle[2];

  OrientationType rod(xo, yo, zo, k);
  rod = getMDFFZRod(rod);
  xo = rod[0];
  yo = rod[1];
  zo = rod[2];
  k = rod[3];

  // eq c3.2
  k = atan2(yo, xo);
  if(k <= M_PI / 8.0)
  {
    k = sqrt(xo * xo + yo * yo);
    if(k == 0)
    {
      k = xo;
    }
    else
    {
      k = xo / k;
    }
  }
  else
  {
    k = (2.0f * sqrt(xo * xo + yo * yo));
    if(k == 0)
    {
      k = (xo + yo);
    }
    else
    {
      k = (xo + yo) / k;
    }
  }
  xo1 = xo * k;
  yo1 = yo * k;
  zo1 = zo / tan(M_PI / 8);

  // eq c3.3
  k = 2.0f * atan2(yo1, xo1);
  xo2 = sqrt(xo1 * xo1 + yo1 * yo1) * cos(k);
  yo2 = sqrt(xo1 * xo1 + yo1 * yo1) * sin(k);
  zo2 = zo1;

  // eq c3.4
  k = sqrt(xo2 * xo2 + yo2 * yo2) / std::max(xo2, yo2);
  xo3 = xo2 * k;
  yo3 = yo2 * k;
  zo3 = zo2;

  // substitute c5.4 results into c1.1
  x = xo3;
  y = yo3;
  z = zo3;

  // eq c1.2
  k = std::max(x, y);
  k = std::max(k, z);
  k = (k * sqrt(3.0)) / (x + y + z);
  x1 = x * k;
  y1 = y * k;
  z1 = z * k;

  // eq c1.3
  // 3 rotation matricies (in paper) can be multiplied into one (here) for simplicity / speed
  // g1*g2*g3 = {{sqrt(2/3), 0, 1/sqrt(3)},{-1/sqrt(6), 1/sqrt(2), 1/sqrt(3)},{-1/sqrt(6), 1/sqrt(2), 1/sqrt(3)}}
  x2 = x1 * sqrt(2.0f / 3.0) - (y1 + z1) / sqrt(6.0);
  y2 = (y1 - z1) / sqrt(2.0);
  z2 = (x1 + y1 + z1) / sqrt(3.0);

  // eq c1.4
  k = fmod(atan2(y2, x2) + 2.0 * EbsdLib::Constants::k_PiD, 2.0 * EbsdLib::Constants::k_PiD);
  x3 = cos(k) * sqrt((x2 * x2 + y2 * y2) / 2.0) * sin(EbsdLib::Constants::k_PiD / 6.0 + std::fmod(k, 2.0 * EbsdLib::Constants::k_PiD / 3.0)) / 0.5;
  y3 = sin(k) * sqrt((x2 * x2 + y2 * y2) / 2.0) * sin(EbsdLib::Constants::k_PiD / 6.0 + std::fmod(k, 2.0 * EbsdLib::Constants::k_PiD / 3.0)) / 0.5;
  z3 = z2 - 1.0f;

  // eq c1.5
  k = (sqrt(x3 * x3 + y3 * y3) - z3) / sqrt(x3 * x3 + y3 * y3 + z3 * z3);
  x4 = x3 * k;
  y4 = y3 * k;
  z4 = z3 * k;

  // eq c1.6, 7, and 8 (from matlab code not paper)
  k = fmod(atan2(y4, x4) + 2 * M_PI, 2 * M_PI);

  int type;
  if(k >= 0.0f && k < 2.0f * M_PI / 3.0)
  {
    type = 1;
    x5 = (x4 + y4 * sqrt(3.0)) / 2.0f;
    y5 = (-x4 * sqrt(3.0) + y4) / 2.0f;
  }
  else if(k >= 2.0f * M_PI / 3.0f && k < 4.0f * M_PI / 3.0)
  {
    type = 2;
    x5 = x4;
    y5 = y4;
  }
  else // k>=4*pi/3 && <2*pi
  {
    type = 3;
    x5 = (x4 - y4 * sqrt(3.0)) / 2.0f;
    y5 = (x4 * sqrt(3.0) + y4) / 2.0f;
  }
  z5 = z4;

  k = 1.5f * atan2(y5, x5);
  x6 = sqrt(x5 * x5 + y5 * y5) * cos(k);
  y6 = sqrt(x5 * x5 + y5 * y5) * sin(k);
  z6 = z5;

  k = 2.0f * atan2(x6, -z6);
  x7 = sqrt(x6 * x6 + z6 * z6) * sin(k);
  y7 = y6;
  z7 = -sqrt(x6 * x6 + z6 * z6) * cos(k);

  k = (2.0f / 3.0) * atan2(y7, x7);
  x8 = sqrt(x7 * x7 + y7 * y7) * cos(k);
  y8 = sqrt(x7 * x7 + y7 * y7) * sin(k);
  z8 = z7;

  if(type == 1)
  {
    x9 = (x8 - y8 * sqrt(3.0)) / 2.0f;
    y9 = (x8 * sqrt(3.0) + y8) / 2.0f;
  }
  else if(type == 2)
  {
    x9 = x8;
    y9 = y8;
  }
  else // type==3;
  {
    x9 = (x8 + y8 * sqrt(3.0)) / 2.0f;
    y9 = (-x8 * sqrt(3.0) + y8) / 2.0f;
  }
  z9 = z8;

  // c1.9
  x10 = (x9 - y9 * sqrt(3.0)) / 2.0f;
  y10 = (x9 * sqrt(3.0) + y9) / 2.0f;
  z10 = z9;

  // cartesian to traditional hsv
  x11 = sqrt(x10 * x10 + y10 * y10 + z10 * z10);                                                                 // r
  y11 = acos(z10 / x11) / M_PI;                                                                                  // theta
  z11 = fmod(fmod(atan2(y10, x10) + 2.0f * M_PI, 2.0f * M_PI) + 4.0f * M_PI / 3.0, 2.0f * M_PI) / (2.0f * M_PI); // rho

  if(x11 == 0)
  {
    y11 = 0;
    z11 = 0;
  }

  h = z11;
  if(y11 >= 0.5)
  {
    s = (1.0f - x11);
    v = 2.0f * x11 * (1.0f - y11) + (1.0f - x11) / 2.0f;
    if(v > 0)
    {
      s = s / (2.0f * v);
    }
    s = 1.0f - s;
  }
  else
  {
    s = (4.0f * x11 * y11) / (1.0f + x11);
    v = 0.5f + x11 / 2;
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
  r = 1 - (r + (v - c));
  g = 1 - (g + (v - c));
  b = 1 - (b + (v - c));

  return EbsdLib::RgbColor::dRgb(static_cast<int32_t>(r * 255), static_cast<int32_t>(g * 255), static_cast<int32_t>(b * 255), 255);
}

// -----------------------------------------------------------------------------
TetragonalOps::Pointer TetragonalOps::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::string TetragonalOps::getNameOfClass() const
{
  return std::string("TetragonalOps");
}

// -----------------------------------------------------------------------------
std::string TetragonalOps::ClassName()
{
  return std::string("TetragonalOps");
}

// -----------------------------------------------------------------------------
TetragonalOps::Pointer TetragonalOps::New()
{
  Pointer sharedPtr(new(TetragonalOps));
  return sharedPtr;
}
