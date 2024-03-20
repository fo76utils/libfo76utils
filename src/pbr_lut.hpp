
#ifndef PBR_LUT_HPP_INCLUDED
#define PBR_LUT_HPP_INCLUDED

#include "common.hpp"
#include "fp32vec4.hpp"
#include "fp32vec8.hpp"

class SF_PBR_Tables
{
 public:
  static inline FloatVector4 Hammersley(int i, int n);
  // Returns H vector, assuming normal = (0, 0, 1). a2 = pow(roughness, 4.0)
  static FloatVector4 importanceSampleGGX(FloatVector4 xi, float a2);
 protected:
  std::vector< unsigned char >  imageData;
  // approximates Fresnel function for n = 1.5, normalized to 0.0 to 1.0
  static inline FloatVector8 fresnel_n(FloatVector8 x);
  static void threadFunction(unsigned char *outBuf, int width, int nSpec,
                             int y0, int y1);
 public:
  SF_PBR_Tables(int width = 512, int nSpec = 4096);
  // R = indirect specular F (U = N·V, V = roughness)
  // G = indirect specular G
  // B = fresnel_n(U)
  // A = fresnel_n(V)
  inline const std::vector< unsigned char >& getImageData() const
  {
    return imageData;
  }
};

inline FloatVector4 SF_PBR_Tables::Hammersley(int i, int n)
{
  FloatVector4  xi(0.0f);
  xi[0] = float(i) / float(n);
  std::uint32_t tmp = std::uint32_t(i);
  tmp = ((tmp & 0x55555555U) << 1) | ((tmp >> 1) & 0x55555555U);
  tmp = ((tmp & 0x33333333U) << 2) | ((tmp >> 2) & 0x33333333U);
  tmp = ((tmp & 0x0F0F0F0FU) << 4) | ((tmp >> 4) & 0x0F0F0F0FU);
  tmp = FileBuffer::swapUInt32(tmp);
  xi[1] = float(int(tmp >> 1)) * float(0.5 / double(0x40000000));
  return xi;
}

#endif

