
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

inline float SF_PBR_Tables::fresnel_n(float x)
{
  float   y = ((((-1.03202882f * x + 3.64690610f) * x - 5.46708684f) * x
                + 4.84513712f) * x - 2.99292756f) * x + 1.0f;
  return y * y;
}

// ported from NifSkope fragment shader code, with modifications

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

void SF_PBR_Tables::threadFunction(
    unsigned char *outBuf, int width, int y0, int y1)
{
  std::vector< FloatVector4 > coordTable_D(256, FloatVector4(0.0f));
  std::vector< FloatVector4 > coordTable_S(4096, FloatVector4(0.0f));
  for (int i = 0; i < int(coordTable_D.size()); i++)
  {
    FloatVector4  xi(SF_PBR_Tables::Hammersley(i, int(coordTable_D.size())));
    double  phi = double(xi[0]) * 6.28318530717959;
    double  cosTheta = xi[1];
    double  sinTheta = std::sqrt(1.0 - (cosTheta * cosTheta));
    coordTable_D[i][0] = float(std::cos(phi) * sinTheta);
    coordTable_D[i][1] = float(std::sin(phi) * sinTheta);
    coordTable_D[i][2] = xi[1];
    coordTable_D[i][3] = float(std::acos(xi[1]));
  }
  outBuf = outBuf + (size_t(y0) * size_t(width) * sizeof(std::uint32_t));
  for (int y = y0; y < y1; y++)
  {
    FloatVector4  normal(0.0f, 0.0f, 1.0f, 0.0f);
    float   roughness = (float(y) + 0.5f) / float(width);
    float   a = roughness * roughness;
    float   a2 = a * a;
    float   k = a * 0.5f;
    int     n = int(coordTable_S.size());
    for (int i = 0; i < n; i++)
    {
      FloatVector4  xi(SF_PBR_Tables::Hammersley(i, n));
      coordTable_S[i] = SF_PBR_Tables::importanceSampleGGX(xi, normal, a2);
      coordTable_S[i][3] = std::min(std::max(coordTable_S[i][2], 0.0f), 1.0f);
    }
    for (int x = 0; x < width; x++, outBuf = outBuf + sizeof(std::uint32_t))
    {
      float   nDotV = (float(x) + 0.5f) / float(width);
      float   g_v = 1.0f / (nDotV * (1.0f - k) + k);
      FloatVector4  v(float(std::sqrt(1.0f - (nDotV * nDotV))), 0.0f, nDotV,
                      float(std::acos(nDotV)));
      FloatVector4  s(0.0f);
      for (int i = 0; i < n; i++)
      {
        FloatVector4  h(coordTable_S[i]);
        FloatVector4  tmp(h * v);
        float   vDotH = tmp[0] + tmp[2];
        float   nDotL = h[2] * (2.0f * vDotH) - v[2];
        if (!(nDotL > 0.0f))
          continue;
        vDotH = std::min(std::max(vDotH, 0.0f), 1.0f);
        float   nDotH = h[3];
        float   g = g_v * nDotL * vDotH / ((nDotL * (1.0f - k) + k) * nDotH);
        float   f = fresnel_n(vDotH);
        s += (FloatVector4(0.0f, 1.0f - f, f, 0.0f) * g);
      }
      s /= float(n);
      s[1] = s[1] + s[2];
      s[2] = s[2] / s[1];
      for (int i = 0; i < int(coordTable_D.size()); i++)
      {
        FloatVector4  l(coordTable_D[i]);
        float   gamma = l.dotProduct2(l);
        if (gamma > 0.000001f)          // gamma = cos(phi_L - phi_V), phi_V = 0
          gamma = l[0] / float(std::sqrt(gamma));
        s[0] += SF_PBR_Tables::OrenNayarFull(l[2], l[3], v[3], gamma,
                                             roughness);
        s[3] += l[2];
      }
      s[0] = (s[0] / s[3]) * 2.0f - 1.25f;
      s[3] = 1.0f;
      s *= 255.0f;
      FileBuffer::writeUInt32Fast(outBuf, s.convertToA2R10G10B10());
    }
  }
}

SF_PBR_Tables::SF_PBR_Tables(int width)
  : imageData(size_t(width) * size_t(width) * sizeof(std::uint32_t) + 148, 0)
{
  unsigned char *p = imageData.data();
  FileBuffer::writeUInt32Fast(p, 0x20534444U);          // "DDS "
  p[4] = 124;                           // size of header
  FileBuffer::writeUInt32Fast(p + 8, 0x0002100FU);      // flags
  // height, width, pitch
  FileBuffer::writeUInt32Fast(p + 12, std::uint32_t(width));
  FileBuffer::writeUInt32Fast(p + 16, std::uint32_t(width));
  FileBuffer::writeUInt32Fast(p + 20,
                              std::uint32_t(width * sizeof(std::uint32_t)));
  p[28] = 1;                            // number of mipmaps
  p[76] = 32;                           // size of pixel format
  p[80] = 0x04;                         // DDPF_FOURCC
  FileBuffer::writeUInt32Fast(p + 84, 0x30315844U);     // "DX10"
  FileBuffer::writeUInt32Fast(p + 108, 0x00401008U);    // dwCaps
  p[128] = 0x18;                        // DXGI_FORMAT_R10G10B10A2_UNORM
  p[132] = 3;                           // DDS_DIMENSION_TEXTURE2D
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
      threads[i] = new std::thread(threadFunction, p, width,
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

