
#ifndef PBR_LUT_HPP_INCLUDED
#define PBR_LUT_HPP_INCLUDED

#include "common.hpp"
#include "fp32vec4.hpp"

class SF_PBR_Tables
{
 protected:
  std::vector< unsigned char >  imageData;
  static inline FloatVector4 Hammersley(int i, int n);
  static FloatVector4 importanceSampleGGX(FloatVector4 xi, FloatVector4 normal,
                                          float a2);    // pow(roughness, 4)
  // approximates Fresnel function for n = 1.5, normalized to 0.0 to 1.0
  static inline float fresnel_n(float x);
  // gamma = cos(phi_L - phi_V)
  static float OrenNayarFull(float nDotL, float angleLN, float angleVN,
                             float gamma, float roughness);
  static void threadFunction(unsigned char *outBuf, int width, int y0, int y1);
 public:
  SF_PBR_Tables(int width = 512);
  // texture U = N·V
  // texture V = roughness
  // R = indirect specular F
  // G = indirect specular G
  // B = indirect diffuse scale * 2.0 - 1.25
  inline const std::vector< unsigned char >& getImageData() const
  {
    return imageData;
  }
};

#endif

