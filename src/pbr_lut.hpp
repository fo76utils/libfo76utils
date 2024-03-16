
#ifndef PBR_LUT_HPP_INCLUDED
#define PBR_LUT_HPP_INCLUDED

#include "common.hpp"
#include "fp32vec4.hpp"
#include "fp32vec8.hpp"

class SF_PBR_Tables
{
 protected:
  std::vector< unsigned char >  imageData;
  static inline FloatVector4 Hammersley(int i, int n);
  static FloatVector4 importanceSampleGGX(FloatVector4 xi, FloatVector4 normal,
                                          float a2);    // pow(roughness, 4)
  // approximates Fresnel function for n = 1.5, normalized to 0.0 to 1.0
  static inline FloatVector8 fresnel_n(FloatVector8 x);
  // gamma = cos(phi_L - phi_V)
  static FloatVector4 OrenNayar_1(float nDotL, float angleLN, float angleVN,
                                  float gamma);
  // g = return value of OrenNayar_1()
  static float OrenNayar_2(FloatVector4 g, float roughness);
  static void threadFunction(unsigned char *outBuf, int width, int nSpec,
                             int y0, int y1, const FloatVector4 *coordTable_D);
 public:
  SF_PBR_Tables(int width = 512, int nSpec = 4096, int nDiff = 1024);
  // texture U = N·V
  // texture V = roughness
  // R = indirect specular F
  // G = indirect specular G
  // B = indirect diffuse scale
  // A = normalized Fresnel function without roughness
  inline const std::vector< unsigned char >& getImageData() const
  {
    return imageData;
  }
};

#endif

