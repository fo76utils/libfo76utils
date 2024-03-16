
#include "common.hpp"
#include "filebuf.hpp"
#include "pbr_lut.hpp"

#include <thread>

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

// the code for generating the BRDF LUT is based on
// "Real Shading in Unreal Engine 4" (s2013_pbs_epic_notes_v2.pdf)

FloatVector4 SF_PBR_Tables::importanceSampleGGX(
    FloatVector4 xi, FloatVector4 normal, float a2)
{
  double  cosTheta = 1.0 - double(xi[1]);
  cosTheta = std::sqrt(cosTheta / (double(a2) * double(xi[1]) + cosTheta));
  double  sinTheta = std::sqrt(1.0 - (cosTheta * cosTheta));
  double  phi = double(xi[0]) * 6.28318530717959;
  float   h_x = float(std::cos(phi) * sinTheta);
  float   h_y = float(std::sin(phi) * sinTheta);
  float   h_z = float(cosTheta);
  FloatVector4  t_z(1.0f, 0.0f, 0.0f, 0.0f);
  if (float(std::fabs(normal[2])) < 0.999f)
    t_z = FloatVector4(0.0f, 0.0f, 1.0f, 0.0f);
  FloatVector4  t_x(t_z.crossProduct3(normal));
  t_x /= float(std::sqrt(t_x.dotProduct3(t_x)));
  FloatVector4  t_y(normal.crossProduct3(t_x));
  FloatVector4  h((t_x * h_x) + (t_y * h_y) + (normal * h_z));
  h /= float(std::sqrt(h.dotProduct3(h)));
  return h;
}

inline FloatVector8 SF_PBR_Tables::fresnel_n(FloatVector8 x)
{
  FloatVector8  y(((((x * -1.03202882f + 3.64690610f) * x - 5.46708684f) * x
                  + 4.84513712f) * x - 2.99292756f) * x + 1.0f);
  return y * y;
}

// ported from NifSkope fragment shader code, with modifications

#if 0
float SF_PBR_Tables::OrenNayarFull(
    float nDotL, float angleLN, float angleVN, float gamma, float roughness)
{
  float   alpha = std::max(angleVN, angleLN);
  float   beta = std::min(angleVN, angleLN);

  float   roughnessSquared = roughness * roughness;
  float   roughnessSquared9 = (roughnessSquared / (roughnessSquared + 0.09f));

  // C1, C2, and C3
  float   c1 = 1.0f - 0.5f * (roughnessSquared / (roughnessSquared + 0.33f));
  float   c2 = 0.45f * roughnessSquared9;

  if (gamma >= 0.0f)
    c2 *= float(std::sin(alpha));
  else
    c2 *= (float(std::sin(alpha)) - (beta * beta * beta * 0.258012275f));

  float   powValue = alpha * beta * 0.405284735f;
  float   c3 = 0.125f * roughnessSquared9 * powValue * powValue;

  // Avoid asymptote at pi/2
  float   asym = 1.57079633f;
  float   lim1 = asym + 0.005f;
  float   lim2 = asym - 0.005f;

  float   ab2 = (alpha + beta) * 0.5f;

  beta = (beta < asym ? std::min(beta, lim2) : std::max(beta, lim1));
  ab2 = (ab2 < asym ? std::min(ab2, lim2) : std::max(ab2, lim1));

  // Reflection
  float   a = gamma * c2 * float(std::tan(beta));
  float   b = (1.0f - float(std::fabs(gamma))) * c3 * float(std::tan(ab2));

  float   l1 = nDotL * (c1 + a + b);

  // Interreflection
  float   twoBetaPi = beta * 0.636619772f;
  float   l2 = 0.17f * nDotL * (roughnessSquared / (roughnessSquared + 0.13f))
               * (1.0f - (gamma * twoBetaPi * twoBetaPi));

  return l1 + l2;
}
#endif

FloatVector4 SF_PBR_Tables::OrenNayar_1(
    float nDotL, float angleLN, float angleVN, float gamma)
{
  float   alpha = std::max(angleVN, angleLN);
  float   beta = std::min(angleVN, angleLN);

  FloatVector4  g(nDotL);
  if (gamma >= 0.0f)
    g[1] = float(std::sin(alpha));
  else
    g[1] = float(std::sin(alpha)) - (beta * beta * beta * 0.258012275f);

  float   powValue = alpha * beta * 0.405284735f;
  g[2] = powValue * powValue;

  // Avoid asymptote at pi/2
  float   asym = 1.57079633f;
  float   lim1 = asym + 0.005f;
  float   lim2 = asym - 0.005f;

  float   ab2 = (alpha + beta) * 0.5f;

  beta = (beta < asym ? std::min(beta, lim2) : std::max(beta, lim1));
  ab2 = (ab2 < asym ? std::min(ab2, lim2) : std::max(ab2, lim1));

  // Reflection
  g[1] = g[1] * nDotL * gamma * float(std::tan(beta));
  g[2] = g[2] * nDotL * (1.0f - float(std::fabs(gamma))) * float(std::tan(ab2));

  // Interreflection
  float   twoBetaPi = beta * 0.636619772f;
  g[3] = nDotL * (1.0f - (gamma * twoBetaPi * twoBetaPi));

  return g;
}

float SF_PBR_Tables::OrenNayar_2(FloatVector4 g, float roughness)
{
  float   roughnessSquared = roughness * roughness;
  float   roughnessSquared9 = (roughnessSquared / (roughnessSquared + 0.09f));

  // C1, C2, and C3
  float   c1 = 1.0f - 0.5f * (roughnessSquared / (roughnessSquared + 0.33f));
  float   c2 = 0.45f * roughnessSquared9;
  float   c3 = 0.125f * roughnessSquared9;

  // Reflection
  float   l1 = (g[0] * c1) + (g[1] * c2) + (g[2] * c3);

  // Interreflection
  float   l2 = g[3] * 0.17f * (roughnessSquared / (roughnessSquared + 0.13f));

  return l1 + l2;
}

void SF_PBR_Tables::threadFunction(
    unsigned char *outBuf, int width, int nSpec, int y0, int y1,
    const FloatVector4 *coordTable_D)
{
  nSpec = (nSpec + 7) & ~7;
  std::vector< FloatVector8 > coordTable_S(size_t(nSpec) >> 2,
                                           FloatVector8(0.0f));
  outBuf = outBuf + (size_t(y0) * size_t(width) * sizeof(std::uint64_t));
  for (int y = y0; y < y1; y++)
  {
    FloatVector4  normal(0.0f, 0.0f, 1.0f, 0.0f);
    float   roughness = (float(y) + 0.5f) / float(width);
    float   a = roughness * roughness;
    float   a2 = a * a;
    float   k = a * 0.5f;
    for (int i = 0; i < nSpec; i++)
    {
      FloatVector4  xi(Hammersley(i, nSpec));
      FloatVector4  tmp(importanceSampleGGX(xi, normal, a2));
      coordTable_S[(i & ~7) >> 2][i & 7] = tmp[0];
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
      s[2] = OrenNayar_2(coordTable_D[x], roughness);
      s[3] = fresnel_n(FloatVector8(nDotV))[0];
      FileBuffer::writeUInt64Fast(outBuf, s.convertToFloat16());
    }
  }
}

SF_PBR_Tables::SF_PBR_Tables(int width, int nSpec, int nDiff)
  : imageData(size_t(width) * size_t(width) * sizeof(std::uint64_t) + 148, 0)
{
  unsigned char *p = imageData.data();
  // DXGI_FORMAT_R16G16B16A16_FLOAT
  FileBuffer::writeDDSHeader(p, 0x0A, width, width);
  p = p + 148;

  std::vector< FloatVector4 > coordTable_D(size_t(width), FloatVector4(0.0f));
  for (int x = 0; x < width; x++)
  {
    float   nDotV = (float(x) + 0.5f) / float(width);
    FloatVector4  v(float(std::sqrt(1.0f - (nDotV * nDotV))), 0.0f, nDotV,
                    float(std::acos(nDotV)));
    float   weight = 0.0f;
    for (int i = 0; i < nDiff; i++)
    {
      FloatVector4  xi(Hammersley(i, nDiff));
      double  phi = double(xi[0]) * 6.28318530717959;
      double  cosTheta = xi[1];
      double  sinTheta = std::sqrt(1.0 - (cosTheta * cosTheta));
      FloatVector4  l;
      l[0] = float(std::cos(phi) * sinTheta);
      l[1] = float(std::sin(phi) * sinTheta);
      l[2] = xi[1];
      l[3] = float(std::acos(xi[1]));
      float   gamma = l.dotProduct2(l);
      if (gamma > 0.000001f)            // gamma = cos(phi_L - phi_V), phi_V = 0
        gamma = l[0] / float(std::sqrt(gamma));
      coordTable_D[x] += OrenNayar_1(l[2], l[3], v[3], gamma);
      weight += l[2];
    }
    coordTable_D[x] /= weight;
  }

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
                                   (i + 1) * width / threadCnt,
                                   coordTable_D.data());
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

