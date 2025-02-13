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

#include <memory>
#include <vector>

#include <string>

#include "EbsdLib/Core/EbsdLibConstants.h"
#include "EbsdLib/EbsdLib.h"

/**
 * @brief The TexturePreset class
 */
class EbsdLib_EXPORT TexturePreset
{
public:
  using Self = TexturePreset;
  using Pointer = std::shared_ptr<Self>;
  using ConstPointer = std::shared_ptr<const Self>;
  using WeakPointer = std::weak_ptr<Self>;
  using ConstWeakPointer = std::weak_ptr<const Self>;
  static Pointer NullPointer();

  using Container = std::vector<Pointer>;

  static Pointer New();

  /**
   * @brief Returns the name of the class for TexturePreset
   */
  virtual std::string getNameOfClass() const;
  /**
   * @brief Returns the name of the class for TexturePreset
   */
  static std::string ClassName();

  static Pointer New(unsigned int xtal, const std::string& name, double e1, double e2, double e3)
  {
    Pointer p(new TexturePreset);
    p->setCrystalStructure(xtal);
    p->setName(name);
    p->setEuler1(e1);
    p->setEuler2(e2);
    p->setEuler3(e3);
    return p;
  }

  virtual ~TexturePreset();

  /**
   * @brief Setter property for CrystalStructure
   */
  void setCrystalStructure(unsigned int value);
  /**
   * @brief Getter property for CrystalStructure
   * @return Value of CrystalStructure
   */
  unsigned int getCrystalStructure() const;

  /**
   * @brief Setter property for Name
   */
  void setName(const std::string& value);
  /**
   * @brief Getter property for Name
   * @return Value of Name
   */
  std::string getName() const;

  /**
   * @brief Setter property for Euler1
   */
  void setEuler1(double value);
  /**
   * @brief Getter property for Euler1
   * @return Value of Euler1
   */
  double getEuler1() const;

  /**
   * @brief Setter property for Euler2
   */
  void setEuler2(double value);
  /**
   * @brief Getter property for Euler2
   * @return Value of Euler2
   */
  double getEuler2() const;

  /**
   * @brief Setter property for Euler3
   */
  void setEuler3(double value);
  /**
   * @brief Getter property for Euler3
   * @return Value of Euler3
   */
  double getEuler3() const;

protected:
  TexturePreset();

public:
  TexturePreset(const TexturePreset&) = delete;            // Copy Constructor Not Implemented
  TexturePreset(TexturePreset&&) = delete;                 // Move Constructor Not Implemented
  TexturePreset& operator=(const TexturePreset&) = delete; // Copy Assignment Not Implemented
  TexturePreset& operator=(TexturePreset&&) = delete;      // Move Assignment Not Implemented

private:
  unsigned int m_CrystalStructure = {};
  std::string m_Name = {};
  double m_Euler1 = {};
  double m_Euler2 = {};
  double m_Euler3 = {};
};

/**
 * @brief The CubicTexturePresets class
 */
class EbsdLib_EXPORT CubicTexturePresets
{
public:
  virtual ~CubicTexturePresets();
  static TexturePreset::Container getTextures();

protected:
  CubicTexturePresets();

public:
  CubicTexturePresets(const CubicTexturePresets&) = delete;            // Copy Constructor Not Implemented
  CubicTexturePresets(CubicTexturePresets&&) = delete;                 // Move Constructor Not Implemented
  CubicTexturePresets& operator=(const CubicTexturePresets&) = delete; // Copy Assignment Not Implemented
  CubicTexturePresets& operator=(CubicTexturePresets&&) = delete;      // Move Assignment Not Implemented
};

/**
 * @brief The HexTexturePresets class
 */
class EbsdLib_EXPORT HexTexturePresets
{
public:
  virtual ~HexTexturePresets();
  static TexturePreset::Container getTextures();

protected:
  HexTexturePresets();

public:
  HexTexturePresets(const HexTexturePresets&) = delete;            // Copy Constructor Not Implemented
  HexTexturePresets(HexTexturePresets&&) = delete;                 // Move Constructor Not Implemented
  HexTexturePresets& operator=(const HexTexturePresets&) = delete; // Copy Assignment Not Implemented
  HexTexturePresets& operator=(HexTexturePresets&&) = delete;      // Move Assignment Not Implemented
};
