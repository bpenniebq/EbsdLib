/* ============================================================================
 * Copyright (c) 2009-2017 BlueQuartz Software, LLC
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

#include <iostream>
#include <memory>
#include <vector>

#include <string>

#include "EbsdLib/Core/EbsdSetGetMacros.h"
#include "EbsdLib/Core/Orientation.hpp"
#include "EbsdLib/Core/OrientationRepresentation.h"
#include "EbsdLib/Core/OrientationTransformation.hpp"
#include "EbsdLib/EbsdLib.h"
#include "EbsdLib/Math/EbsdLibMath.h"

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/partitioner.h>
#endif

#define OC_CLASS_DEFINES(name)                                                                                                                                                                         \
  using DataArrayPointerType = typename DataArrayType::Pointer;                                                                                                                                        \
  using Self = name<DataArrayType, T>;                                                                                                                                                                 \
  using Pointer = std::shared_ptr<Self>;                                                                                                                                                               \
  static Pointer New()                                                                                                                                                                                 \
  {                                                                                                                                                                                                    \
    Pointer sharedPtr(new(Self));                                                                                                                                                                      \
    return sharedPtr;                                                                                                                                                                                  \
  }                                                                                                                                                                                                    \
  std::string getNameOfClass() const override                                                                                                                                                          \
  {                                                                                                                                                                                                    \
    return std::string(#name);                                                                                                                                                                         \
  }

/**
 * @brief This is the top level superclass for doing the conversions between orientation
 * representations
 */
template <class DataArrayType, typename T>
class OrientationConverter
{
public:
  using DataArrayPointerType = typename DataArrayType::Pointer;
  using Self = OrientationConverter<DataArrayType, T>;
  using Pointer = std::shared_ptr<Self>;

  virtual ~OrientationConverter() = default;

  virtual std::string getNameOfClass() const
  {
    return std::string("OrientationConverter<DataArrayType,T>");
  }

  /**
   * @brief getOrientationRepresentation
   * @return
   */
  OrientationRepresentation::Type getOrientationRepresentation()
  {
    return OrientationRepresentation::Type::Unknown;
  }

  /**
   * @brief convertRepresentationTo Converts the data to the desired type
   * @param repType The type of representation to convert to.
   * @return
   */
  void convertRepresentationTo(OrientationRepresentation::Type repType)
  {
    if(repType == OrientationRepresentation::Type::Euler)
    {
      toEulers();
    }
    else if(repType == OrientationRepresentation::Type::OrientationMatrix)
    {
      toOrientationMatrix();
    }
    else if(repType == OrientationRepresentation::Type::Quaternion)
    {
      toQuaternion();
    }
    else if(repType == OrientationRepresentation::Type::AxisAngle)
    {
      toAxisAngle();
    }
    else if(repType == OrientationRepresentation::Type::Rodrigues)
    {
      toRodrigues();
    }
    else if(repType == OrientationRepresentation::Type::Homochoric)
    {
      toHomochoric();
    }
    else if(repType == OrientationRepresentation::Type::Cubochoric)
    {
      toCubochoric();
    }
  }

  /**
   * @brief toEulers Converts the input orientations to Euler Angles
   */
  virtual void toEulers() = 0;

  /**
   * @brief toOrientationMatrix  Converts the input orientations to an Orientation Matrix (3x3)
   */
  virtual void toOrientationMatrix() = 0;

  /**
   * @brief toQuaternion  Converts the input orientations to Quaternions
   */
  virtual void toQuaternion() = 0;

  /**
   * @brief toAxisAngle  Converts the input orientations to Axis Angles
   */
  virtual void toAxisAngle() = 0;

  /**
   * @brief toRodrigues  Converts the input orientations to Rodrigues
   */
  virtual void toRodrigues() = 0;

  /**
   * @brief toHomochoric  Converts the input orientations to Homochoric
   */
  virtual void toHomochoric() = 0;

  /**
   * @brief toCubochoric  Converts the input orientations to Cubochoric
   */
  virtual void toCubochoric() = 0;

  /**
   * @brief sanityCheckInputData Runs basic checks on the input data to ensure
   * the intput data falls within certain data ranges.
   */
  virtual void sanityCheckInputData() = 0;

  /**
   * @brief printRepresentation Prints the values of a single representation to
   * an output stream;
   * @param out An output stream
   * @param a Pointer to print
   * @param label Optional Label
   */
  //   void printRepresentation(std::ostream& out, T* a, const std::string& label = std::string("")) = 0;

  /**
   * @brief Sets/Gets the input orientations
   */
  void setInputData(DataArrayPointerType& input)
  {
    m_InputData = input;
  }
  DataArrayPointerType getInputData() const
  {
    return m_InputData;
  }

  /**
   * @brief Sets/Gets the output orientations
   */
  void setOutputData(DataArrayPointerType& output)
  {
    m_OutputData = output;
  }
  DataArrayPointerType getOutputData() const
  {
    return m_OutputData;
  }

  /**
   * @brief GetOrientationTypeStrings
   * @return
   */
  template <typename ContainerType>
  static ContainerType GetOrientationTypeStrings()
  {
    ContainerType otypes(7);
    otypes[0] = "Euler";
    otypes[1] = "Orientation Matrix";
    otypes[2] = "Quaternion";
    otypes[3] = "Axis-Angle";
    otypes[4] = "Rodrigues";
    otypes[5] = "Homochoric";
    otypes[6] = "Cubochoric";
    return otypes;
  }

  /**
   * @brief GetComponentCounts
   * @return
   */
  template <typename ContainerType>
  static ContainerType GetComponentCounts()
  {
    ContainerType counts(7);
    counts[0] = 3; // Euler
    counts[1] = 9; // Orientation Matrix
    counts[2] = 4; // Quaternion
    counts[3] = 4; // Axis-Angle
    counts[4] = 4; // Rodrigues
    counts[5] = 3; // Homchoric
    counts[6] = 3; // Cubochoric
    return counts;
  }

  /**
   * @brief GetOrientationTypes
   * @return
   */
  static std::vector<OrientationRepresentation::Type> GetOrientationTypes()
  {
    std::vector<OrientationRepresentation::Type> ocTypes(7);
    ocTypes[0] = OrientationRepresentation::Type::Euler;
    ocTypes[1] = OrientationRepresentation::Type::OrientationMatrix;
    ocTypes[2] = OrientationRepresentation::Type::Quaternion;
    ocTypes[3] = OrientationRepresentation::Type::AxisAngle;
    ocTypes[4] = OrientationRepresentation::Type::Rodrigues;
    ocTypes[5] = OrientationRepresentation::Type::Homochoric;
    ocTypes[6] = OrientationRepresentation::Type::Cubochoric;
    return ocTypes;
  }

  /**
   * @brief GetMinIndex
   * @return
   */
  static int GetMinIndex()
  {
    return 0;
  }

  /**
   * @brief Returns the maximum index into the arrays of conversion types and representations
   * @return
   */
  static int GetMaxIndex()
  {
    return 6;
  }

protected:
  OrientationConverter() = default;

public:
  OrientationConverter(const OrientationConverter&) = delete;            // Copy Constructor Not Implemented
  OrientationConverter(OrientationConverter&&) = delete;                 // Move Constructor Not Implemented
  OrientationConverter& operator=(const OrientationConverter&) = delete; // Copy Assignment Not Implemented
  OrientationConverter& operator=(OrientationConverter&&) = delete;      // Move Assignment Not Implemented

private:
  DataArrayPointerType m_InputData;
  DataArrayPointerType m_OutputData;
};

/**
 * @brief This macro is used to create a functor that wraps a paricular conversion
 * method with a functor class so it can be passed to the parallel algorithms
 */

#define OC_CONVERTOR_FUNCTOR(CLASSNAME, INSTRIDE, OUTSTRIDE, CONVERSION_METHOD)                                                                                                                        \
  template <typename InputType>                                                                                                                                                                        \
  class CLASSNAME                                                                                                                                                                                      \
  {                                                                                                                                                                                                    \
  public:                                                                                                                                                                                              \
    CLASSNAME() = default;                                                                                                                                                                             \
    void operator()(InputType* input, InputType* output)                                                                                                                                               \
    {                                                                                                                                                                                                  \
      using OrientationInputType = Orientation<InputType>;                                                                                                                                             \
      OrientationInputType inputOrientation(input, INSTRIDE);                                                                                                                                          \
      OrientationInputType outputOrientation(output, OUTSTRIDE);                                                                                                                                       \
      outputOrientation = OrientationTransformation::CONVERSION_METHOD<OrientationInputType, OrientationInputType>(inputOrientation);                                                                  \
    }                                                                                                                                                                                                  \
  };

#define OC_CONVERTOR_FUNCTOR_2QU(CLASSNAME, INSTRIDE, OUTSTRIDE, CONVERSION_METHOD)                                                                                                                    \
  template <typename NumericType>                                                                                                                                                                      \
  class CLASSNAME                                                                                                                                                                                      \
  {                                                                                                                                                                                                    \
  public:                                                                                                                                                                                              \
    CLASSNAME() = default;                                                                                                                                                                             \
    void operator()(NumericType* input, NumericType* output)                                                                                                                                           \
    {                                                                                                                                                                                                  \
      using InputType = Orientation<NumericType>;                                                                                                                                                      \
      using OutputType = Quaternion<NumericType>;                                                                                                                                                      \
      InputType inputOrientation(input, INSTRIDE);                                                                                                                                                     \
      OrientationTransformation::CONVERSION_METHOD<InputType, OutputType>(inputOrientation).copyInto(output, Quaternion<NumericType>::Order::VectorScalar);                                            \
    }                                                                                                                                                                                                  \
  };

#define OC_QU2_CONVERTOR_FUNCTOR(CLASSNAME, INSTRIDE, OUTSTRIDE, CONVERSION_METHOD)                                                                                                                    \
  template <typename NumericType>                                                                                                                                                                      \
  class CLASSNAME                                                                                                                                                                                      \
  {                                                                                                                                                                                                    \
  public:                                                                                                                                                                                              \
    CLASSNAME() = default;                                                                                                                                                                             \
    void operator()(NumericType* input, NumericType* output)                                                                                                                                           \
    {                                                                                                                                                                                                  \
      using QuaternionType = Quaternion<NumericType>;                                                                                                                                                  \
      using OutputType = Orientation<NumericType>;                                                                                                                                                     \
      QuaternionType inputQuat(input[0], input[1], input[2], input[3]);                                                                                                                                \
      OutputType outputOrientation = OrientationTransformation::CONVERSION_METHOD<QuaternionType, OutputType>(inputQuat);                                                                              \
      outputOrientation.copyInto(output, OUTSTRIDE);                                                                                                                                                   \
    }                                                                                                                                                                                                  \
  };

/**
 * @brief This contains all the functors that represent all possible conversion routines
 * between orientation representations
 */
namespace Convertors
{
/* Euler Functors  */
OC_CONVERTOR_FUNCTOR(Eu2Om, 3, 9, eu2om)
OC_CONVERTOR_FUNCTOR_2QU(Eu2Qu, 3, 4, eu2qu)
OC_CONVERTOR_FUNCTOR(Eu2Ax, 3, 4, eu2ax)
OC_CONVERTOR_FUNCTOR(Eu2Ro, 3, 4, eu2ro)
OC_CONVERTOR_FUNCTOR(Eu2Ho, 3, 3, eu2ho)
OC_CONVERTOR_FUNCTOR(Eu2Cu, 3, 3, eu2cu)

/* OrientationMatrix Functors */
OC_CONVERTOR_FUNCTOR(Om2Eu, 9, 3, om2eu)
OC_CONVERTOR_FUNCTOR_2QU(Om2Qu, 9, 4, om2qu)
OC_CONVERTOR_FUNCTOR(Om2Ax, 9, 4, om2ax)
OC_CONVERTOR_FUNCTOR(Om2Ro, 9, 4, om2ro)
OC_CONVERTOR_FUNCTOR(Om2Ho, 9, 3, om2ho)
OC_CONVERTOR_FUNCTOR(Om2Cu, 9, 3, om2cu)

/* Quaterion Functors */
OC_QU2_CONVERTOR_FUNCTOR(Qu2Eu, 4, 3, qu2eu)
OC_QU2_CONVERTOR_FUNCTOR(Qu2Om, 4, 9, qu2om)
OC_QU2_CONVERTOR_FUNCTOR(Qu2Ax, 4, 4, qu2ax)
OC_QU2_CONVERTOR_FUNCTOR(Qu2Ro, 4, 4, qu2ro)
OC_QU2_CONVERTOR_FUNCTOR(Qu2Ho, 4, 3, qu2ho)
OC_QU2_CONVERTOR_FUNCTOR(Qu2Cu, 4, 3, qu2cu)

/* AxisAngles Functors */
OC_CONVERTOR_FUNCTOR(Ax2Eu, 4, 3, ax2eu)
OC_CONVERTOR_FUNCTOR(Ax2Om, 4, 9, ax2om)
OC_CONVERTOR_FUNCTOR_2QU(Ax2Qu, 4, 4, ax2qu)
OC_CONVERTOR_FUNCTOR(Ax2Ro, 4, 4, ax2ro)
OC_CONVERTOR_FUNCTOR(Ax2Ho, 4, 3, ax2ho)
OC_CONVERTOR_FUNCTOR(Ax2Cu, 4, 3, ax2cu)

/* Rodrigues Functors */
OC_CONVERTOR_FUNCTOR(Ro2Eu, 4, 3, ro2eu)
OC_CONVERTOR_FUNCTOR(Ro2Om, 4, 9, ro2om)
OC_CONVERTOR_FUNCTOR_2QU(Ro2Qu, 4, 4, ro2qu)
OC_CONVERTOR_FUNCTOR(Ro2Ax, 4, 4, ro2ax)
OC_CONVERTOR_FUNCTOR(Ro2Ho, 4, 3, ro2ho)
OC_CONVERTOR_FUNCTOR(Ro2Cu, 4, 3, ro2cu)

/* Rodrigues Functors */
OC_CONVERTOR_FUNCTOR(Ho2Eu, 3, 3, ho2eu)
OC_CONVERTOR_FUNCTOR(Ho2Om, 3, 9, ho2om)
OC_CONVERTOR_FUNCTOR_2QU(Ho2Qu, 3, 4, ho2qu)
OC_CONVERTOR_FUNCTOR(Ho2Ax, 3, 4, ho2ax)
OC_CONVERTOR_FUNCTOR(Ho2Ro, 3, 4, ho2ro)
OC_CONVERTOR_FUNCTOR(Ho2Cu, 3, 3, ho2cu)

/* Rodrigues Functors */
OC_CONVERTOR_FUNCTOR(Cu2Eu, 3, 3, cu2eu)
OC_CONVERTOR_FUNCTOR(Cu2Om, 3, 9, cu2om)
OC_CONVERTOR_FUNCTOR_2QU(Cu2Qu, 3, 4, cu2qu)
OC_CONVERTOR_FUNCTOR(Cu2Ax, 3, 4, cu2ax)
OC_CONVERTOR_FUNCTOR(Cu2Ro, 3, 4, cu2ro)
OC_CONVERTOR_FUNCTOR(Cu2Ho, 3, 3, cu2ho)

} // namespace Convertors

/**
 * @brief This templated class is a functor class that is used for
 * the TBB classes to use to parallelize the conversion of orientation
 * representations
 */
template <typename T, class Converter>
class ConvertRepresentation
{
public:
  ConvertRepresentation(T* inPtr, T* outPtr, size_t inStride, size_t outStride)
  : m_InPtr(inPtr)
  , m_OutPtr(outPtr)
  , m_InStride(inStride)
  , m_OutStride(outStride)
  {
  }
  virtual ~ConvertRepresentation() = default;

  /**
   * @brief This is the main conversion routine
   * @param start Starting index
   * @param end Ending index
   */
  void convert(size_t start, size_t end) const
  {
    Converter conv;
    T* input = m_InPtr + (start * m_InStride);
    T* output = m_OutPtr + (start * m_OutStride);
    for(size_t i = start; i < end; ++i)
    {
      conv(input, output);
      input = input + m_InStride;    /* Increment input pointer */
      output = output + m_OutStride; /* Increment output pointer*/
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    convert(r.begin(), r.end());
  }
#endif

private:
  T* m_InPtr = nullptr;
  T* m_OutPtr = nullptr;
  size_t m_InStride = 0;
  size_t m_OutStride = 0;
};

/**
 * @brief OC_CONVERT_BODY Generates the body of method that will perform the conversion
 */
#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS

#define OC_CONVERT_BODY(OUTSTRIDE, OUT_ARRAY_NAME, CONVERSION_METHOD, FUNCTOR)                                                                                                                         \
  sanityCheckInputData();                                                                                                                                                                              \
  DataArrayPointerType input = this->getInputData();                                                                                                                                                   \
  T* inPtr = input->getPointer(0);                                                                                                                                                                     \
  size_t nTuples = this->getInputData()->getNumberOfTuples();                                                                                                                                          \
  int inStride = input->getNumberOfComponents();                                                                                                                                                       \
  size_t outStride = OUTSTRIDE;                                                                                                                                                                        \
  std::vector<size_t> cDims = {outStride};                                                                                                                                                             \
  DataArrayPointerType output = DataArrayType::CreateArray(nTuples, cDims, #OUT_ARRAY_NAME, true);                                                                                                     \
  output->initializeWithZeros(); /* Intialize the array with Zeros */                                                                                                                                  \
  T* outPtr = output->getPointer(0);                                                                                                                                                                   \
  tbb::parallel_for(tbb::blocked_range<size_t>(0, nTuples), ConvertRepresentation<T, Convertors::FUNCTOR<T>>(inPtr, outPtr, inStride, outStride), tbb::auto_partitioner());                            \
  this->setOutputData(output);

#else

#define OC_CONVERT_BODY(OUTSTRIDE, OUT_ARRAY_NAME, CONVERSION_METHOD, FUNCTOR)                                                                                                                         \
  sanityCheckInputData();                                                                                                                                                                              \
  DataArrayPointerType input = this->getInputData();                                                                                                                                                   \
  T* inPtr = input->getPointer(0);                                                                                                                                                                     \
  size_t nTuples = this->getInputData()->getNumberOfTuples();                                                                                                                                          \
  int inStride = input->getNumberOfComponents();                                                                                                                                                       \
  size_t outStride = OUTSTRIDE;                                                                                                                                                                        \
  std::vector<size_t> cDims = {outStride}; /* Create the n component (nx1) based array.*/                                                                                                              \
  DataArrayPointerType output = DataArrayType::CreateArray(nTuples, cDims, #OUT_ARRAY_NAME, true);                                                                                                     \
  output->initializeWithZeros(); /* Intialize the array with Zeros */                                                                                                                                  \
  T* outPtr = output->getPointer(0);                                                                                                                                                                   \
  ConvertRepresentation<T, Convertors::FUNCTOR<T>> serial(inPtr, outPtr, inStride, outStride);                                                                                                         \
  serial.convert(0, nTuples);                                                                                                                                                                          \
  this->setOutputData(output);

#endif

/* =============================================================================
 *
 * ===========================================================================*/

template <typename T>
class EulerSanityCheck
{
public:
  EulerSanityCheck(T* input, size_t stride)
  : m_Input(input)
  , m_Stride(stride)
  {
  }
  virtual ~EulerSanityCheck() = default;

  void sanityCheck(size_t start, size_t end) const
  {
    T* inPtr = m_Input + (start * m_Stride);

    for(size_t i = start; i < end; ++i)
    {
      inPtr[0] = static_cast<T>(std::fmod(inPtr[0], EbsdLib::Constants::k_2PiD));
      inPtr[1] = static_cast<T>(std::fmod(inPtr[1], EbsdLib::Constants::k_PiD));
      inPtr[2] = static_cast<T>(std::fmod(inPtr[2], EbsdLib::Constants::k_2PiD));

      if(inPtr[0] < 0.0)
      {
        inPtr[0] *= static_cast<T>(-1.0);
      }
      if(inPtr[1] < 0.0)
      {
        inPtr[1] *= static_cast<T>(-1.0);
      }
      if(inPtr[2] < 0.0)
      {
        inPtr[2] *= static_cast<T>(-1.0);
      }

      inPtr = inPtr + m_Stride; // This is Pointer arithmetic!!
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    sanityCheck(r.begin(), r.end());
  }
#endif

private:
  T* m_Input = nullptr;
  size_t m_Stride = 0;
};

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
template <class DataArrayType, typename T>
class EulerConverter : public OrientationConverter<DataArrayType, T>
{
public:
  OC_CLASS_DEFINES(EulerConverter)

  virtual ~EulerConverter() = default;

  OrientationRepresentation::Type getOrientationRepresentation()
  {
    return OrientationRepresentation::Type::Euler;
  }

  void toEulers() override
  {
    DataArrayPointerType input = this->getInputData();
    DataArrayPointerType output = std::dynamic_pointer_cast<DataArrayType>(input->deepCopy());
    this->setOutputData(output);
  }

  void toOrientationMatrix() override
  {
    OC_CONVERT_BODY(9, OrientationMatrix, eu2om, Eu2Om)
  }

  void toQuaternion() override
  {
    OC_CONVERT_BODY(4, Quaternion, eu2qu, Eu2Qu)
  }

  void toAxisAngle() override
  {
    OC_CONVERT_BODY(4, AxisAngle, eu2ax, Eu2Ax)
  }

  void toRodrigues() override
  {
    OC_CONVERT_BODY(4, Rodrigues, eu2ro, Eu2Ro)
  }

  void toHomochoric() override
  {
    OC_CONVERT_BODY(3, Homochoric, eu2ho, Eu2Ho)
  }

  void toCubochoric() override
  {
    OC_CONVERT_BODY(3, Cubochoric, eu2cu, Eu2Cu)
  }

  void sanityCheckInputData() override
  {
    DataArrayPointerType input = this->getInputData();
    T* inPtr = input->getPointer(0);
    size_t nTuples = input->getNumberOfTuples();
    int inStride = input->getNumberOfComponents();

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
    bool doParallel = true;
    if(doParallel)
    {
      tbb::parallel_for(tbb::blocked_range<size_t>(0, nTuples), EulerSanityCheck<T>(inPtr, inStride), tbb::auto_partitioner());
    }
    else
#endif
    {
      EulerSanityCheck<T> serial(inPtr, inStride);
      serial.sanityCheck(0, nTuples);
    }
  }

  /**
   * @brief compareRepresentations
   * @param a
   * @param b
   * @param epsilon
   * @return
   */
  bool compareRepresentations(T* a, T* b, const T& epsilon = std::numeric_limits<T>::epsilon())
  {
    bool close = false;
    for(int i = 0; i < 3; i++)
    {
      close = (epsilon > std::fabs(a[i] - b[i]));
      if(!close)
      {
        return close;
      }
    }
    return close;
  }

  /**
   * @brief printRepresentation
   * @param out
   * @param eu
   * @param label
   */
  void printRepresentation(std::ostream& out, T* eu, const std::string& label = std::string("Eu"))
  {
    out.precision(16);
    out << label << eu[0] << '\t' << eu[1] << '\t' << eu[2] << std::endl;
  }

protected:
  EulerConverter()
  : OrientationConverter<DataArrayType, T>()
  {
  }

  explicit EulerConverter(DataArrayPointerType data)
  : OrientationConverter<DataArrayType, T>()
  {
    this->setInputData(data);
  }

public:
  EulerConverter(const EulerConverter&) = delete;            // Copy Constructor Not Implemented
  EulerConverter(EulerConverter&&) = delete;                 // Move Constructor Not Implemented
  EulerConverter& operator=(const EulerConverter&) = delete; // Copy Assignment Not Implemented
  EulerConverter& operator=(EulerConverter&&) = delete;      // Move Assignment Not Implemented
};

/* =============================================================================
 *
 * ===========================================================================*/
template <typename T>
class OrientationMatrixSanityCheck
{
public:
  OrientationMatrixSanityCheck(T* input, size_t stride)
  : m_Input(input)
  , m_Stride(stride)
  {
  }
  virtual ~OrientationMatrixSanityCheck() = default;

  void sanityCheck(size_t start, size_t end) const
  {
    T* inPtr = m_Input + (start * m_Stride);

    for(size_t i = start; i < end; ++i)
    {
      using Orientation_Type = Orientation<T>;
      using ResultType = OrientationTransformation::ResultType;

      Orientation_Type oaType(inPtr, 9);

      ResultType res = OrientationTransformation::om_check(oaType);
      if(res.result <= 0)
      {
        std::cout << res.msg << std::endl;
        printRepresentation(std::cout, inPtr, std::string("Bad OM"));
      }

      inPtr = inPtr + m_Stride; // This is Pointer arithmetic!!
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    sanityCheck(r.begin(), r.end());
  }
#endif

  /**
   * @brief printRepresentation
   * @param out
   * @param om
   * @param label
   */
  void printRepresentation(std::ostream& out, T* om, const std::string& label = std::string("Om")) const
  {
    out.precision(16);
    out << label << om[0] << '\t' << om[1] << '\t' << om[2] << std::endl;
    out << label << om[3] << '\t' << om[4] << '\t' << om[5] << std::endl;
    out << label << om[6] << '\t' << om[7] << '\t' << om[8] << std::endl;
  }

private:
  T* m_Input = nullptr;
  size_t m_Stride = 0;
};

template <class DataArrayType, typename T>
class OrientationMatrixConverter : public OrientationConverter<DataArrayType, T>
{
public:
  OC_CLASS_DEFINES(OrientationMatrixConverter)

  virtual ~OrientationMatrixConverter() = default;

  OrientationRepresentation::Type getOrientationRepresentation()
  {
    return OrientationRepresentation::Type::OrientationMatrix;
  }

  void toEulers() override
  {
    sanityCheckInputData();
    OC_CONVERT_BODY(3, Eulers, om2eu, Om2Eu)
  }

  void toOrientationMatrix() override
  {
    DataArrayPointerType input = this->getInputData();
    DataArrayPointerType output = std::dynamic_pointer_cast<DataArrayType>(input->deepCopy());
    this->setOutputData(output);
  }

  void toQuaternion() override
  {
    sanityCheckInputData();
    OC_CONVERT_BODY(4, Quaternion, om2qu, Om2Qu)
  }

  void toAxisAngle() override
  {
    sanityCheckInputData();
    OC_CONVERT_BODY(4, AxisAngle, om2ax, Om2Ax)
  }

  void toRodrigues() override
  {
    sanityCheckInputData();
    OC_CONVERT_BODY(4, Rodrigues, om2ro, Om2Ro)
  }

  void toHomochoric() override
  {
    sanityCheckInputData();
    OC_CONVERT_BODY(3, Homochoric, om2ho, Om2Ho)
  }

  void toCubochoric() override
  {
    sanityCheckInputData();
    OC_CONVERT_BODY(3, Cubochoric, om2cu, Om2Cu)
  }

  void sanityCheckInputData() override
  {
    DataArrayPointerType input = this->getInputData();
    T* inPtr = input->getPointer(0);
    size_t nTuples = input->getNumberOfTuples();
    int inStride = input->getNumberOfComponents();

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
    bool doParallel = true;
    if(doParallel)
    {
      tbb::parallel_for(tbb::blocked_range<size_t>(0, nTuples), OrientationMatrixSanityCheck<T>(inPtr, inStride), tbb::auto_partitioner());
    }
    else
#endif
    {
      OrientationMatrixSanityCheck<T> serial(inPtr, inStride);
      serial.sanityCheck(0, nTuples);
    }
  }

  /**
   * @brief compareRepresentations
   * @param a
   * @param b
   * @param epsilon
   * @return
   */
  bool compareRepresentations(T* a, T* b, const T& epsilon = std::numeric_limits<T>::epsilon())
  {
    bool close = false;
    for(int i = 0; i < 9; i++)
    {
      close = (epsilon > std::fabs(a[i] - b[i]));
      if(!close)
      {
        return close;
      }
    }
    return close;
  }

  /**
   * @brief printRepresentation
   * @param out
   * @param om
   * @param label
   */
  void printRepresentation(std::ostream& out, T* om, const std::string& label = std::string("Om"))
  {
    out.precision(16);
    out << label << om[0] << '\t' << om[1] << '\t' << om[2] << std::endl;
    out << label << om[3] << '\t' << om[4] << '\t' << om[5] << std::endl;
    out << label << om[6] << '\t' << om[7] << '\t' << om[8] << std::endl;
  }

protected:
  OrientationMatrixConverter()
  : OrientationConverter<DataArrayType, T>()
  {
  }
  explicit OrientationMatrixConverter(DataArrayPointerType data)
  : OrientationConverter<DataArrayType, T>()
  {
    this->setInputData(data);
  }

public:
  OrientationMatrixConverter(const OrientationMatrixConverter&) = delete;            // Copy Constructor Not Implemented
  OrientationMatrixConverter(OrientationMatrixConverter&&) = delete;                 // Move Constructor Not Implemented
  OrientationMatrixConverter& operator=(const OrientationMatrixConverter&) = delete; // Copy Assignment Not Implemented
  OrientationMatrixConverter& operator=(OrientationMatrixConverter&&) = delete;      // Move Assignment Not Implemented
};

/* =============================================================================
 *
 * ===========================================================================*/

template <typename T>
class QuaternionSanityCheck
{
public:
  QuaternionSanityCheck(T* input, size_t stride)
  : m_Input(input)
  , m_Stride(stride)
  {
  }
  virtual ~QuaternionSanityCheck() = default;

  void sanityCheck(size_t start, size_t end) const
  {
    T* inPtr = m_Input + (start * m_Stride);

    for(size_t i = start; i < end; ++i)
    {
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    sanityCheck(r.begin(), r.end());
  }
#endif

private:
  T* m_Input = nullptr;
  size_t m_Stride = 0;
};

template <class DataArrayType, typename T>
class QuaternionConverter : public OrientationConverter<DataArrayType, T>
{
public:
  OC_CLASS_DEFINES(QuaternionConverter)

  virtual ~QuaternionConverter() = default;

  OrientationRepresentation::Type getOrientationRepresentation()
  {
    return OrientationRepresentation::Type::Quaternion;
  }

  void toEulers() override
  {
    OC_CONVERT_BODY(3, Eulers, qu2eu, Qu2Eu)
  }

  void toOrientationMatrix() override
  {
    OC_CONVERT_BODY(9, OrientationMatrix, qu2om, Qu2Om)
  }

  void toQuaternion() override
  {
    using PointerType = DataArrayPointerType;
    PointerType input = this->getInputData();
    PointerType output = std::dynamic_pointer_cast<DataArrayType>(input->deepCopy());
    this->setOutputData(output);
  }

  void toAxisAngle() override
  {
    OC_CONVERT_BODY(4, AxisAngle, qu2ax, Qu2Ax)
  }

  void toRodrigues() override
  {
    OC_CONVERT_BODY(4, Rodrigues, qu2ro, Qu2Ro)
  }

  void toHomochoric() override
  {
    OC_CONVERT_BODY(3, Homochoric, qu2ho, Qu2Ho)
  }

  void toCubochoric() override
  {
    OC_CONVERT_BODY(3, Cubochoric, qu2cu, Qu2Cu)
  }

  void sanityCheckInputData() override
  {
    /* Apparently there is no sanity check for Quaternions, Odd. We place this
     * code here in case we come up with one, the parallel version is ready to
     * go
     */
#if 0
      DataArrayPointerType input = this->getInputData();
      T* inPtr = input->getPointer(0);
      size_t nTuples = input->getNumberOfTuples();
      int inStride = input->getNumberOfComponents();

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
        tbb::parallel_for(tbb::blocked_range<size_t>(0, nTuples), QuaternionSanityCheck<T>(inPtr, inStride), tbb::auto_partitioner());
#else
        QuaternionSanityCheck<T> serial(inPtr, inStride);
        serial.sanityCheck(0, nTuples);
#endif
#endif
  }

  /**
   * @brief printRepresentation
   * @param out
   * @param qu
   * @param label
   */
  void printRepresentation(std::ostream& out, T* qu, const std::string& label = std::string("Qu"))
  {
    out.precision(16);
    out << label << qu[0] << '\t' << qu[1] << '\t' << qu[2] << '\t' << qu[3] << std::endl;
  }

  /**
   * @brief compareRepresentations
   * @param a
   * @param b
   * @param epsilon
   * @return
   */
  bool compareRepresentations(T* a, T* b, const T& epsilon = std::numeric_limits<T>::epsilon())
  {
    bool close = false;
    for(int i = 0; i < 4; i++)
    {
      close = (epsilon > std::fabs(a[i] - b[i]));
      if(!close)
      {
        return close;
      }
    }
    return close;
  }

protected:
  QuaternionConverter()
  : OrientationConverter<DataArrayType, T>()
  {
  }
  explicit QuaternionConverter(DataArrayPointerType data)
  : OrientationConverter<DataArrayType, T>()
  {
    this->setInputData(data);
  }

public:
  QuaternionConverter(const QuaternionConverter&) = delete;            // Copy Constructor Not Implemented
  QuaternionConverter(QuaternionConverter&&) = delete;                 // Move Constructor Not Implemented
  QuaternionConverter& operator=(const QuaternionConverter&) = delete; // Copy Assignment Not Implemented
  QuaternionConverter& operator=(QuaternionConverter&&) = delete;      // Move Assignment Not Implemented
};

/* =============================================================================
 *
 * ===========================================================================*/
template <typename T>
class AxisAngleSanityCheck
{
public:
  AxisAngleSanityCheck(T* input, size_t stride)
  : m_Input(input)
  , m_Stride(stride)
  {
  }
  virtual ~AxisAngleSanityCheck() = default;

  void sanityCheck(size_t start, size_t end) const
  {
    T* inPtr = m_Input + (start * m_Stride);

    for(size_t i = start; i < end; ++i)
    {
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    sanityCheck(r.begin(), r.end());
  }
#endif

private:
  T* m_Input = nullptr;
  size_t m_Stride = 0;
};

template <class DataArrayType, typename T>
class AxisAngleConverter : public OrientationConverter<DataArrayType, T>
{
public:
  OC_CLASS_DEFINES(AxisAngleConverter)

  virtual ~AxisAngleConverter() = default;

  OrientationRepresentation::Type getOrientationRepresentation()
  {
    return OrientationRepresentation::Type::AxisAngle;
  }

  void toEulers() override
  {
    OC_CONVERT_BODY(3, Eulers, ax2eu, Ax2Eu)
  }

  void toOrientationMatrix() override
  {
    OC_CONVERT_BODY(9, OrientationMatrix, ax2om, Ax2Om)
  }

  void toQuaternion() override
  {
    OC_CONVERT_BODY(4, Quaternions, ax2qu, Ax2Qu)
  }

  void toAxisAngle() override
  {
    using PointerType = DataArrayPointerType;
    PointerType input = this->getInputData();
    PointerType output = std::dynamic_pointer_cast<DataArrayType>(input->deepCopy());
    this->setOutputData(output);
  }

  void toRodrigues() override
  {
    OC_CONVERT_BODY(4, Rodrigues, ax2ro, Ax2Ro)
  }

  void toHomochoric() override
  {
    OC_CONVERT_BODY(3, Homochoric, ax2ho, Ax2Ho)
  }

  void toCubochoric() override
  {
    OC_CONVERT_BODY(3, Cubochoric, ax2cu, Ax2Cu)
  }

  void sanityCheckInputData() override
  {
    /* Apparently there is no sanity check for AxisAngle, Odd. We place this
     * code here in case we come up with one, the parallel version is ready to
     * go
     */
#if 0
      DataArrayPointerType input = this->getInputData();
      T* inPtr = input->getPointer(0);
      size_t nTuples = input->getNumberOfTuples();
      int inStride = input->getNumberOfComponents();
#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
      tbb::parallel_for(tbb::blocked_range<size_t>(0, nTuples), AxisAngleSanityCheck<T>(inPtr, inStride), tbb::auto_partitioner());
#else
        AxisAngleSanityCheck<T> serial(inPtr, inStride);
        serial.sanityCheck(0, nTuples);
#endif
#endif
  }

  /**
   * @brief compareRepresentations
   * @param a
   * @param b
   * @param epsilon
   * @return
   */
  bool compareRepresentations(T* a, T* b, const T& epsilon = std::numeric_limits<T>::epsilon())
  {
    bool close = false;
    for(int i = 0; i < 4; i++)
    {
      close = (epsilon > std::fabs(a[i] - b[i]));
      if(!close)
      {
        return close;
      }
    }
    return close;
  }

  /**
   * @brief printRepresentation
   * @param out
   * @param ax
   * @param label
   */
  void printRepresentation(std::ostream& out, T* ax, const std::string& label = std::string("Ax"))
  {
    out.precision(16);
    out << label << "<" << ax[0] << '\t' << ax[1] << '\t' << ax[2] << ">\t" << ax[3] << std::endl;
  }

protected:
  AxisAngleConverter()
  : OrientationConverter<DataArrayType, T>()
  {
  }

  explicit AxisAngleConverter(DataArrayPointerType data)
  : OrientationConverter<DataArrayType, T>()
  {
    this->setInputData(data);
  }

public:
  AxisAngleConverter(const AxisAngleConverter&) = delete;            // Copy Constructor Not Implemented
  AxisAngleConverter(AxisAngleConverter&&) = delete;                 // Move Constructor Not Implemented
  AxisAngleConverter& operator=(const AxisAngleConverter&) = delete; // Copy Assignment Not Implemented
  AxisAngleConverter& operator=(AxisAngleConverter&&) = delete;      // Move Assignment Not Implemented
};

/* =============================================================================
 *
 * ===========================================================================*/

template <typename T>
class RodriguesSanityCheck
{
public:
  RodriguesSanityCheck(T* input, size_t stride)
  : m_Input(input)
  , m_Stride(stride)
  {
  }
  virtual ~RodriguesSanityCheck() = default;

  void sanityCheck(size_t start, size_t end) const
  {
    T* inPtr = m_Input + (start * m_Stride);

    for(size_t i = start; i < end; ++i)
    {
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    sanityCheck(r.begin(), r.end());
  }
#endif

private:
  T* m_Input = nullptr;
  size_t m_Stride = 0;
};

template <class DataArrayType, typename T>
class RodriguesConverter : public OrientationConverter<DataArrayType, T>
{
public:
  OC_CLASS_DEFINES(RodriguesConverter)

  virtual ~RodriguesConverter() = default;

  OrientationRepresentation::Type getOrientationRepresentation()
  {
    return OrientationRepresentation::Type::Rodrigues;
  }

  void toEulers() override
  {
    OC_CONVERT_BODY(3, Eulers, ro2eu, Ro2Eu)
  }

  void toOrientationMatrix() override
  {
    OC_CONVERT_BODY(9, OrientationMatrix, ro2om, Ro2Om)
  }

  void toQuaternion() override
  {
    OC_CONVERT_BODY(4, Quaternions, ro2qu, Ro2Qu)
  }

  void toAxisAngle() override
  {
    OC_CONVERT_BODY(4, AxisAngle, ro2ax, Ro2Ax)
  }

  void toRodrigues() override
  {
    using PointerType = DataArrayPointerType;
    PointerType input = this->getInputData();
    PointerType output = std::dynamic_pointer_cast<DataArrayType>(input->deepCopy());
    this->setOutputData(output);
  }

  void toHomochoric() override
  {
    OC_CONVERT_BODY(3, Homochoric, ro2ho, Ro2Ho)
  }

  void toCubochoric() override
  {
    OC_CONVERT_BODY(3, Cubochoric, ro2cu, Ro2Cu)
  }

  void sanityCheckInputData() override
  {
    /* Apparently there is no sanity check for Rodrigues, Odd. We place this
     * code here in case we come up with one, the parallel version is ready to
     * go
     */
#if 0
      DataArrayPointerType input = this->getInputData();
      T* inPtr = input->getPointer(0);
      size_t nTuples = input->getNumberOfTuples();
      int inStride = input->getNumberOfComponents();
#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
        tbb::parallel_for(tbb::blocked_range<size_t>(0, nTuples), RodriguesSanityCheck<DataArrayType>(inPtr, inStride), tbb::auto_partitioner());
#else
      RodriguesSanityCheck<T> serial(inPtr, inStride);
      serial.sanityCheck(0, nTuples);
#endif
#endif
  }

  /**
   * @brief compareRepresentations
   * @param a
   * @param b
   * @param epsilon
   * @return
   */
  bool compareRepresentations(T* a, T* b, const T& epsilon = std::numeric_limits<T>::epsilon())
  {
    bool close = false;
    for(int i = 0; i < 4; i++)
    {
      close = (epsilon > std::fabs(a[i] - b[i]));
      if(!close)
      {
        return close;
      }
    }
    return close;
  }

  /**
   * @brief printRepresentation
   * @param out
   * @param ro
   * @param label
   */
  void printRepresentation(std::ostream& out, T* ro, const std::string& label = std::string("Ro"))
  {
    out.precision(16);
    out << label << ro[0] << '\t' << ro[1] << '\t' << ro[2] << "\t" << ro[3] << std::endl;
  }

protected:
  RodriguesConverter()
  : OrientationConverter<DataArrayType, T>()
  {
  }

  explicit RodriguesConverter(DataArrayPointerType data)
  : OrientationConverter<DataArrayType, T>()
  {
    this->setInputData(data);
  }

public:
  RodriguesConverter(const RodriguesConverter&) = delete;            // Copy Constructor Not Implemented
  RodriguesConverter(RodriguesConverter&&) = delete;                 // Move Constructor Not Implemented
  RodriguesConverter& operator=(const RodriguesConverter&) = delete; // Copy Assignment Not Implemented
  RodriguesConverter& operator=(RodriguesConverter&&) = delete;      // Move Assignment Not Implemented
};

/* =============================================================================
 *
 * ===========================================================================*/

template <typename T>
class HomochoricSanityCheck
{
public:
  HomochoricSanityCheck(T* input, size_t stride)
  : m_Input(input)
  , m_Stride(stride)
  {
  }
  virtual ~HomochoricSanityCheck() = default;

  void sanityCheck(size_t start, size_t end) const
  {
    T* inPtr = m_Input + (start * m_Stride);

    for(size_t i = start; i < end; ++i)
    {
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    sanityCheck(r.begin(), r.end());
  }
#endif

private:
  T* m_Input = nullptr;
  size_t m_Stride = 0;
};

template <class DataArrayType, typename T>
class HomochoricConverter : public OrientationConverter<DataArrayType, T>
{
public:
  OC_CLASS_DEFINES(HomochoricConverter)

  virtual ~HomochoricConverter() = default;

  OrientationRepresentation::Type getOrientationRepresentation()
  {
    return OrientationRepresentation::Type::Homochoric;
  }

  void toEulers() override
  {
    OC_CONVERT_BODY(3, Eulers, ho2eu, Ho2Eu)
  }

  void toOrientationMatrix() override
  {
    OC_CONVERT_BODY(9, OrientationMatrix, ho2om, Ho2Om)
  }

  void toQuaternion() override
  {
    OC_CONVERT_BODY(4, Quaternions, ho2qu, Ho2Qu)
  }

  void toAxisAngle() override
  {
    OC_CONVERT_BODY(4, AxisAngle, ho2ax, Ho2Ax)
  }

  void toRodrigues() override
  {
    OC_CONVERT_BODY(4, Rodrigues, ho2ro, Ho2Ro)
  }

  void toHomochoric() override
  {
    using PointerType = DataArrayPointerType;
    PointerType input = this->getInputData();
    PointerType output = std::dynamic_pointer_cast<DataArrayType>(input->deepCopy());
    this->setOutputData(output);
  }

  void toCubochoric() override
  {
    OC_CONVERT_BODY(3, Cubochoric, ho2cu, Ho2Cu)
  }

  void sanityCheckInputData() override
  {
    /* Apparently there is no sanity check for Homochoric, Odd. We place this
     * code here in case we come up with one, the parallel version is ready to
     * go
     */
#if 0
      DataArrayPointerType input = this->getInputData();
      T* inPtr = input->getPointer(0);
      size_t nTuples = input->getNumberOfTuples();
      int inStride = input->getNumberOfComponents();
#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
      tbb::parallel_for(tbb::blocked_range<size_t>(0, nTuples), HomochoricSanityCheck<T>(inPtr, inStride), tbb::auto_partitioner());
      }
#else
        HomochoricSanityCheck<T> serial(inPtr, inStride);
        serial.sanityCheck(0, nTuples);
#endif
#endif
  }

  /**
   * @brief compareRepresentations
   * @param a
   * @param b
   * @param epsilon
   * @return
   */
  bool compareRepresentations(T* a, T* b, const T& epsilon = std::numeric_limits<T>::epsilon())
  {
    bool close = false;
    for(int i = 0; i < 3; i++)
    {
      close = (epsilon > std::fabs(a[i] - b[i]));
      if(!close)
      {
        return close;
      }
    }
    return close;
  }

  /**
   * @brief printRepresentation
   * @param out
   * @param ho
   * @param label
   */
  void printRepresentation(std::ostream& out, T* ho, const std::string& label = std::string("No"))
  {
    out.precision(16);
    out << label << ho[0] << '\t' << ho[1] << '\t' << ho[2] << std::endl;
  }

protected:
  HomochoricConverter()
  : OrientationConverter<DataArrayType, T>()
  {
  }

  explicit HomochoricConverter(DataArrayPointerType data)
  : OrientationConverter<DataArrayType, T>()
  {
    this->setInputData(data);
  }

public:
  HomochoricConverter(const HomochoricConverter&) = delete;            // Copy Constructor Not Implemented
  HomochoricConverter(HomochoricConverter&&) = delete;                 // Move Constructor Not Implemented
  HomochoricConverter& operator=(const HomochoricConverter&) = delete; // Copy Assignment Not Implemented
  HomochoricConverter& operator=(HomochoricConverter&&) = delete;      // Move Assignment Not Implemented
};

/* =============================================================================
 *
 * ===========================================================================*/

template <typename T>
class CubochoricSanityCheck
{
public:
  CubochoricSanityCheck(T* input, size_t stride)
  : m_Input(input)
  , m_Stride(stride)
  {
  }
  virtual ~CubochoricSanityCheck() = default;

  void sanityCheck(size_t start, size_t end) const
  {
    T* inPtr = m_Input + (start * m_Stride);

    for(size_t i = start; i < end; ++i)
    {
    }
  }

#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    sanityCheck(r.begin(), r.end());
  }
#endif

private:
  T* m_Input = nullptr;
  size_t m_Stride = 0;
};

template <class DataArrayType, typename T>
class CubochoricConverter : public OrientationConverter<DataArrayType, T>
{
public:
  OC_CLASS_DEFINES(CubochoricConverter)

  virtual ~CubochoricConverter() = default;

  OrientationRepresentation::Type getOrientationRepresentation()
  {
    return OrientationRepresentation::Type::Cubochoric;
  }

  void toEulers() override
  {
    OC_CONVERT_BODY(3, Eulers, cu2eu, Cu2Eu)
  }

  void toOrientationMatrix() override
  {
    OC_CONVERT_BODY(9, OrientationMatrix, cu2om, Cu2Om)
  }

  void toQuaternion() override
  {
    OC_CONVERT_BODY(4, Quaternions, cu2qu, Cu2Qu)
  }

  void toAxisAngle() override
  {
    OC_CONVERT_BODY(4, AxisAngle, cu2ax, Cu2Ax)
  }

  void toRodrigues() override
  {
    OC_CONVERT_BODY(4, Rodrigues, cu2ro, Cu2Ro)
  }

  void toHomochoric() override
  {
    OC_CONVERT_BODY(3, Homochoric, cu2ho, Cu2Ho)
  }

  void toCubochoric() override
  {
    using PointerType = DataArrayPointerType;
    PointerType input = this->getInputData();
    PointerType output = std::dynamic_pointer_cast<DataArrayType>(input->deepCopy());
    this->setOutputData(output);
  }

  void sanityCheckInputData() override
  {
    /* Apparently there is no sanity check for Cubochoric, Odd. We place this
     * code here in case we come up with one, the parallel version is ready to
     * go
     */
#if 0
      DataArrayPointerType input = this->getInputData();
      T* inPtr = input->getPointer(0);
      size_t nTuples = input->getNumberOfTuples();
      int inStride = input->getNumberOfComponents();
#ifdef EbsdLib_USE_PARALLEL_ALGORITHMS
      tbb::parallel_for(tbb::blocked_range<size_t>(0, nTuples), CubochoricSanityCheck<T>(inPtr, inStride), tbb::auto_partitioner());
#else
      CubochoricSanityCheck<T> serial(inPtr, inStride);
      serial.sanityCheck(0, nTuples);
#endif
#endif
  }

  /**
   * @brief compareRepresentations
   * @param a
   * @param b
   * @param epsilon
   * @return
   */
  bool compareRepresentations(T* a, T* b, const T& epsilon = std::numeric_limits<T>::epsilon())
  {
    bool close = false;
    for(int i = 0; i < 3; i++)
    {
      close = (epsilon > std::fabs(a[i] - b[i]));
      if(!close)
      {
        return close;
      }
    }
    return close;
  }

  /**
   * @brief printRepresentation
   * @param out
   * @param cu
   * @param label
   */
  void printRepresentation(std::ostream& out, T* cu, const std::string& label = std::string("Cu"))
  {
    out.precision(16);
    out << label << cu[0] << '\t' << cu[1] << '\t' << cu[2] << std::endl;
  }

protected:
  CubochoricConverter()
  : OrientationConverter<DataArrayType, T>()
  {
  }

  explicit CubochoricConverter(DataArrayPointerType data)
  : OrientationConverter<DataArrayType, T>()
  {
    this->setInputData(data);
  }

public:
  CubochoricConverter(const CubochoricConverter&) = delete;            // Copy Constructor Not Implemented
  CubochoricConverter(CubochoricConverter&&) = delete;                 // Move Constructor Not Implemented
  CubochoricConverter& operator=(const CubochoricConverter&) = delete; // Copy Assignment Not Implemented
  CubochoricConverter& operator=(CubochoricConverter&&) = delete;      // Move Assignment Not Implemented
};
