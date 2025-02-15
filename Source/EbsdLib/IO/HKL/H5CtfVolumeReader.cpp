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

#include "H5CtfVolumeReader.h"

#include <cmath>

#include "H5Support/H5Lite.h"
#include "H5Support/H5Utilities.h"

#include "EbsdLib/Core/EbsdLibConstants.h"
#include "EbsdLib/IO/HKL/H5CtfReader.h"
#include "EbsdLib/Utilities/EbsdStringUtils.hpp"

using namespace H5Support;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
H5CtfVolumeReader::H5CtfVolumeReader() = default;
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
H5CtfVolumeReader::~H5CtfVolumeReader()
{
  deletePointers();
}

#define H5CTFREADER_ALLOCATE_ARRAY(name, type)                                                                                                                                                         \
  if(readAllArrays == true || arrayNames.find(EbsdLib::Ctf::name) != arrayNames.end())                                                                                                                 \
  {                                                                                                                                                                                                    \
    auto _##name = allocateArray<type>(numElements);                                                                                                                                                   \
    if(nullptr != _##name)                                                                                                                                                                             \
    {                                                                                                                                                                                                  \
      ::memset(_##name, 0, numBytes);                                                                                                                                                                  \
    }                                                                                                                                                                                                  \
    set##name##Pointer(_##name);                                                                                                                                                                       \
  }

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void H5CtfVolumeReader::initPointers(size_t numElements)
{
  setNumberOfElements(numElements);
  size_t numBytes = numElements * sizeof(float);
  bool readAllArrays = getReadAllArrays();
  std::set<std::string> arrayNames = getArraysToRead();

  H5CTFREADER_ALLOCATE_ARRAY(Phase, int)
  H5CTFREADER_ALLOCATE_ARRAY(X, float)
  H5CTFREADER_ALLOCATE_ARRAY(Y, float)
  H5CTFREADER_ALLOCATE_ARRAY(Z, float)
  H5CTFREADER_ALLOCATE_ARRAY(Bands, int)
  H5CTFREADER_ALLOCATE_ARRAY(Error, int)
  H5CTFREADER_ALLOCATE_ARRAY(Euler1, float)
  H5CTFREADER_ALLOCATE_ARRAY(Euler2, float)
  H5CTFREADER_ALLOCATE_ARRAY(Euler3, float)
  H5CTFREADER_ALLOCATE_ARRAY(MAD, float)
  H5CTFREADER_ALLOCATE_ARRAY(BC, int)
  H5CTFREADER_ALLOCATE_ARRAY(BS, int)
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void H5CtfVolumeReader::deletePointers()
{
  this->deallocateArrayData<int>(m_Phase);
  this->deallocateArrayData<float>(m_X);
  this->deallocateArrayData<float>(m_Y);
  this->deallocateArrayData<int>(m_Bands);
  this->deallocateArrayData<int>(m_Error);
  this->deallocateArrayData<float>(m_Euler1);
  this->deallocateArrayData<float>(m_Euler2);
  this->deallocateArrayData<float>(m_Euler3);
  this->deallocateArrayData<float>(m_MAD);
  this->deallocateArrayData<int>(m_BC);
  this->deallocateArrayData<int>(m_BS);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void* H5CtfVolumeReader::getPointerByName(const std::string& featureName)
{
  if(featureName == EbsdLib::Ctf::Phase)
  {
    return static_cast<void*>(m_Phase);
  }
  if(featureName == EbsdLib::Ctf::X)
  {
    return static_cast<void*>(m_X);
  }
  if(featureName == EbsdLib::Ctf::Y)
  {
    return static_cast<void*>(m_Y);
  }
  if(featureName == EbsdLib::Ctf::Bands)
  {
    return static_cast<void*>(m_Bands);
  }
  if(featureName == EbsdLib::Ctf::Error)
  {
    return static_cast<void*>(m_Error);
  }
  if(featureName == EbsdLib::Ctf::Euler1)
  {
    return static_cast<void*>(m_Euler1);
  }
  if(featureName == EbsdLib::Ctf::Euler2)
  {
    return static_cast<void*>(m_Euler2);
  }
  if(featureName == EbsdLib::Ctf::Euler3)
  {
    return static_cast<void*>(m_Euler3);
  }
  if(featureName == EbsdLib::Ctf::MAD)
  {
    return static_cast<void*>(m_MAD);
  }
  if(featureName == EbsdLib::Ctf::BC)
  {
    return static_cast<void*>(m_BC);
  }
  if(featureName == EbsdLib::Ctf::BS)
  {
    return static_cast<void*>(m_BS);
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
EbsdLib::NumericTypes::Type H5CtfVolumeReader::getPointerType(const std::string& featureName)
{
  if(featureName == EbsdLib::Ctf::Phase)
  {
    return EbsdLib::NumericTypes::Type::Int32;
  }
  if(featureName == EbsdLib::Ctf::X)
  {
    return EbsdLib::NumericTypes::Type::Float;
  }
  if(featureName == EbsdLib::Ctf::Y)
  {
    return EbsdLib::NumericTypes::Type::Float;
  }
  if(featureName == EbsdLib::Ctf::Bands)
  {
    return EbsdLib::NumericTypes::Type::Int32;
  }
  if(featureName == EbsdLib::Ctf::Error)
  {
    return EbsdLib::NumericTypes::Type::Int32;
  }
  if(featureName == EbsdLib::Ctf::Euler1)
  {
    return EbsdLib::NumericTypes::Type::Float;
  }
  if(featureName == EbsdLib::Ctf::Euler2)
  {
    return EbsdLib::NumericTypes::Type::Float;
  }
  if(featureName == EbsdLib::Ctf::Euler3)
  {
    return EbsdLib::NumericTypes::Type::Float;
  }
  if(featureName == EbsdLib::Ctf::MAD)
  {
    return EbsdLib::NumericTypes::Type::Float;
  }
  if(featureName == EbsdLib::Ctf::BC)
  {
    return EbsdLib::NumericTypes::Type::Int32;
  }
  if(featureName == EbsdLib::Ctf::BS)
  {
    return EbsdLib::NumericTypes::Type::Int32;
  }
  return EbsdLib::NumericTypes::Type::UnknownNumType;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
std::vector<CtfPhase::Pointer> H5CtfVolumeReader::getPhases()
{
  m_Phases.clear();

  // Get the first valid index of a z slice
  std::string index = EbsdStringUtils::number(getZStart());

  // Open the hdf5 file and read the data
  hid_t fileId = H5Utilities::openFile(getFileName(), true);
  if(fileId < 0)
  {
    std::cout << "Error" << std::endl;
    return m_Phases;
  }
  herr_t err = 0;

  hid_t gid = H5Gopen(fileId, index.c_str(), H5P_DEFAULT);
  H5CtfReader::Pointer reader = H5CtfReader::New();
  reader->setHDF5Path(index);
  err = reader->readHeader(gid);
  if(err < 0)
  {
    std::cout << "Error reading the header information from the .h5ebsd file" << std::endl;
    err = H5Gclose(gid);
    err = H5Utilities::closeFile(fileId);
    return m_Phases;
  }
  m_Phases = reader->getPhases();
  err = H5Gclose(gid);
  err = H5Utilities::closeFile(fileId);
  return m_Phases;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int H5CtfVolumeReader::loadData(int64_t xpoints, int64_t ypoints, int64_t zpoints, uint32_t ZDir)
{
  int index = 0;
  int err = -1;
  // Initialize all the pointers
  initPointers(xpoints * ypoints * zpoints);

  int readerIndex = 0;
  int64_t xpointsslice = 0;
  int64_t ypointsslice = 0;
  int64_t xpointstemp = 0;
  int64_t ypointstemp = 0;
  int zval = 0;

  int xstartspot = 0;
  int ystartspot = 0;

  err = readVolumeInfo();

  for(int64_t slice = 0; slice < zpoints; ++slice)
  {
    H5CtfReader::Pointer reader = H5CtfReader::New();
    reader->setFileName(getFileName());
    reader->setHDF5Path(EbsdStringUtils::number(slice + getSliceStart()));
    reader->setUserZDir(getStackingOrder());
    reader->setSampleTransformationAngle(getSampleTransformationAngle());
    reader->setSampleTransformationAxis(getSampleTransformationAxis());
    reader->setEulerTransformationAngle(getEulerTransformationAngle());
    reader->setEulerTransformationAxis(getEulerTransformationAxis());
    reader->readAllArrays(getReadAllArrays());
    reader->setArraysToRead(getArraysToRead());

    err = reader->readFile();
    if(err < 0)
    {
      std::cout << "H5CtfVolumeReader Error: There was an issue loading the data from the hdf5 file." << std::endl;
      return -77000;
    }
    readerIndex = 0;
    xpointsslice = reader->getXCells();
    ypointsslice = reader->getYCells();
    int* phasePtr = reader->getPhasePointer();
    float* xPtr = reader->getXPointer();
    float* yPtr = reader->getYPointer();
    int* bandPtr = reader->getBandCountPointer();
    int* errorPtr = reader->getErrorPointer();
    float* euler1Ptr = reader->getEuler1Pointer();
    float* euler2Ptr = reader->getEuler2Pointer();
    float* euler3Ptr = reader->getEuler3Pointer();
    float* madPtr = reader->getMeanAngularDeviationPointer();
    int* bcPtr = reader->getBandContrastPointer();
    int* bsPtr = reader->getBandSlopePointer();

    xpointstemp = xpoints;
    ypointstemp = ypoints;
    xstartspot = static_cast<int>((xpointstemp - xpointsslice) / 2);
    ystartspot = static_cast<int>((ypointstemp - ypointsslice) / 2);

    // If no stacking order preference was passed, read it from the file and use that value
    if(ZDir == EbsdLib::RefFrameZDir::UnknownRefFrameZDirection)
    {
      ZDir = getStackingOrder();
    }
    if(ZDir == 0)
    {
      zval = static_cast<int>(slice);
    }
    if(ZDir == 1)
    {
      zval = static_cast<int>((zpoints - 1) - slice);
    }

    // Copy the data from the current storage into the Storage Location
    for(int64_t j = 0; j < ypointsslice; j++)
    {
      for(int64_t i = 0; i < xpointsslice; i++)
      {
        index = static_cast<int32_t>((zval * xpointstemp * ypointstemp) + ((j + ystartspot) * xpointstemp) + (i + xstartspot));
        if(nullptr != phasePtr)
        {
          m_Phase[index] = phasePtr[readerIndex];
        }
        if(nullptr != xPtr)
        {
          m_X[index] = xPtr[readerIndex];
        }
        if(nullptr != yPtr)
        {
          m_Y[index] = yPtr[readerIndex];
        }
        if(nullptr != bandPtr)
        {
          m_Bands[index] = bandPtr[readerIndex];
        }
        if(nullptr != errorPtr)
        {
          m_Error[index] = errorPtr[readerIndex];
        }
        if(nullptr != euler1Ptr)
        {
          m_Euler1[index] = euler1Ptr[readerIndex];
        }
        if(nullptr != euler2Ptr)
        {
          m_Euler2[index] = euler2Ptr[readerIndex];
        }
        if(nullptr != euler3Ptr)
        {
          m_Euler3[index] = euler3Ptr[readerIndex];
        }
        if(nullptr != madPtr)
        {
          m_MAD[index] = madPtr[readerIndex];
        }
        if(nullptr != bcPtr)
        {
          m_BC[index] = bcPtr[readerIndex];
        }
        if(nullptr != bsPtr)
        {
          m_BS[index] = bsPtr[readerIndex];
        }

        /* For HKL OIM Files if there is a single phase then the value of the phase
         * data is one (1). If there are 2 or more phases then the lowest value
         * of phase is also one (1). However, if there are "zero solutions" in the data
         * then those points are assigned a phase of zero.  Since those points can be identified
         * by other methods, the phase of these points should be changed to one since in the rest
         * of the reconstruction code we follow the convention that the lowest value is One (1)
         * even if there is only a single phase. The next if statement converts all zeros to ones
         * if there is a single phase in the OIM data.
         */
        //        if(nullptr != phasePtr && m_Phase[index] < 1)
        //        {
        //          m_Phase[index] = 1;
        //        }

        ++readerIndex;
      }
    }
  }
  return err;
}

// -----------------------------------------------------------------------------
H5CtfVolumeReader::Pointer H5CtfVolumeReader::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::string H5CtfVolumeReader::getNameOfClass() const
{
  return std::string("H5CtfVolumeReader");
}

// -----------------------------------------------------------------------------
std::string H5CtfVolumeReader::ClassName()
{
  return std::string("H5CtfVolumeReader");
}
