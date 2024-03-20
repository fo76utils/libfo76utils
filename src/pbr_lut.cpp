
#include "common.hpp"
#include "filebuf.hpp"
#include "pbr_lut.hpp"

#include <thread>

// the code for generating the BRDF LUT is based on
// "Real Shading in Unreal Engine 4" (s2013_pbs_epic_notes_v2.pdf)

FloatVector4 SF_PBR_Tables::importanceSampleGGX(FloatVector4 xi, float a2)
{
  double  cosTheta = 1.0 - double(xi[1]);
  cosTheta = std::sqrt(cosTheta / (double(a2) * double(xi[1]) + cosTheta));
  double  sinTheta = std::sqrt(1.0 - (cosTheta * cosTheta));
  double  phi = double(xi[0]) * 6.28318530717959;
  float   h_x = float(std::cos(phi) * sinTheta);
  float   h_y = float(std::sin(phi) * sinTheta);
  float   h_z = float(cosTheta);
  return FloatVector4(h_x, h_y, h_z, 0.0f);
}

inline FloatVector8 SF_PBR_Tables::fresnel_n(FloatVector8 x)
{
  FloatVector8  y(((((x * -1.03202882f + 3.64690610f) * x - 5.46708684f) * x
                  + 4.84513712f) * x - 2.99292756f) * x + 1.0f);
  return y * y;
}

void SF_PBR_Tables::threadFunction(
    unsigned char *outBuf, int width, int nSpec, int y0, int y1)
{
  nSpec = (nSpec + 7) & ~7;
  std::vector< FloatVector8 > coordTable_S(size_t(nSpec) >> 2,
                                           FloatVector8(0.0f));
  outBuf = outBuf + (size_t(y0) * size_t(width) * sizeof(std::uint64_t));
  for (int y = y0; y < y1; y++)
  {
    float   roughness = (float(y) + 0.5f) / float(width);
    float   a = roughness * roughness;
    float   a2 = a * a;
    float   k = a * 0.5f;
    for (int i = 0; i < nSpec; i++)
    {
      FloatVector4  xi(Hammersley(i, nSpec));
      FloatVector4  tmp(importanceSampleGGX(xi, a2));
      coordTable_S[(i & ~7) >> 2][i & 7] = tmp[1];      // normal = (0, 0, 1)
      coordTable_S[((i & ~7) >> 2) + 1][i & 7] = tmp[2];
    }
    for (int x = 0; x < width; x++, outBuf = outBuf + sizeof(std::uint64_t))
    {
      float   nDotV = (float(x) + 0.5f) / float(width);
      FloatVector4  v(float(std::sqrt(1.0f - (nDotV * nDotV))), 0.0f, nDotV,
                      float(std::acos(nDotV)));
      FloatVector8  s1(0.0f);
      FloatVector8  s2(0.0f);
      FloatVector8  v_x(v[0]);
      FloatVector8  v_z(v[2]);
      for (int i = 0; (i + 8) <= nSpec; i = i + 8)
      {
        FloatVector8  h_x(coordTable_S[i >> 2]);
        FloatVector8  nDotH(coordTable_S[(i >> 2) + 1]);        // h_z
        FloatVector8  vDotH((h_x * v_x) + (nDotH * v_z));
        FloatVector8  nDotL(nDotH * (vDotH + vDotH) - v_z);
        nDotH.maxValues(FloatVector8(0.0f));
        vDotH.maxValues(FloatVector8(0.0f));
        nDotL.maxValues(FloatVector8(0.0f));
        FloatVector8  g(nDotL * vDotH
                        / ((nDotL * (FloatVector8(1.0f) - k) + k) * nDotH));
        FloatVector8  f(fresnel_n(vDotH));
        s1 += (f * g);
        s2 += g;
      }
      FloatVector4  s(s1.dotProduct(FloatVector8(1.0f)),
                      s2.dotProduct(FloatVector8(1.0f)), 0.0f, 0.0f);
      s /= (float(nSpec) * (nDotV * (1.0f - k) + k));
      s[0] = s[0] / s[1];
      FloatVector8  tmp(nDotV, roughness, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
      tmp = fresnel_n(tmp);
      s[2] = tmp[0];
      s[3] = tmp[1];
      FileBuffer::writeUInt64Fast(outBuf, s.convertToFloat16());
    }
  }
}

SF_PBR_Tables::SF_PBR_Tables(int width, int nSpec)
  : imageData(size_t(width) * size_t(width) * sizeof(std::uint64_t) + 148, 0)
{
  unsigned char *p = imageData.data();
  // DXGI_FORMAT_R16G16B16A16_FLOAT
  FileBuffer::writeDDSHeader(p, 0x0A, width, width);
  p = p + 148;

  int     threadCnt = int(std::thread::hardware_concurrency());
  threadCnt = std::min< int >(std::max< int >(threadCnt, 1), 16);
  threadCnt = std::min(threadCnt, width);
  std::thread *threads[16];
  for (int i = 0; i < 16; i++)
    threads[i] = nullptr;
  try
  {
    for (int i = 0; i < threadCnt; i++)
    {
      threads[i] = new std::thread(threadFunction, p, width, nSpec,
                                   i * width / threadCnt,
                                   (i + 1) * width / threadCnt);
    }
    for (int i = 0; i < threadCnt; i++)
    {
      threads[i]->join();
      delete threads[i];
      threads[i] = nullptr;
    }
  }
  catch (...)
  {
    for (int i = 0; i < 16; i++)
    {
      if (threads[i])
      {
        threads[i]->join();
        delete threads[i];
      }
    }
    throw;
  }
}

