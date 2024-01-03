
#include "sfcube.hpp"
#include "filebuf.hpp"
#include "fp32vec8.hpp"
#include "ddstxt.hpp"

#include <thread>

// face 0: E,      -X = up,   +X = down, -Y = N,    +Y = S
// face 1: W,      -X = down, +X = up,   -Y = N,    +Y = S
// face 2: N,      -X = W,    +X = E,    -Y = down, +Y = up
// face 3: S,      -X = W,    +X = E,    -Y = up,   +Y = down
// face 4: top,    -X = W,    +X = E,    -Y = N,    +Y = S
// face 5: bottom, -X = E,    +X = W,    -Y = N,    +Y = S

FloatVector4 SFCubeMapFilter::convertCoord(int x, int y, int w, int n)
{
  FloatVector4  v(0.0f);
  switch (n)
  {
    case 0:
      v[0] = float(w);
      v[1] = float(w - (y << 1));
      v[2] = float(w - (x << 1));
      v += FloatVector4(0.0f, -1.0f, -1.0f, 0.0f);
      break;
    case 1:
      v[0] = float(-w);
      v[1] = float(w - (y << 1));
      v[2] = float((x << 1) - w);
      v += FloatVector4(0.0f, -1.0f, 1.0f, 0.0f);
      break;
    case 2:
      v[0] = float((x << 1) - w);
      v[1] = float(w);
      v[2] = float((y << 1) - w);
      v += FloatVector4(1.0f, 0.0f, 1.0f, 0.0f);
      break;
    case 3:
      v[0] = float((x << 1) - w);
      v[1] = float(-w);
      v[2] = float(w - (y << 1));
      v += FloatVector4(1.0f, 0.0f, -1.0f, 0.0f);
      break;
    case 4:
      v[0] = float((x << 1) - w);
      v[1] = float(w - (y << 1));
      v[2] = float(w);
      v += FloatVector4(1.0f, -1.0f, 0.0f, 0.0f);
      break;
    case 5:
      v[0] = float(w - (x << 1));
      v[1] = float(w - (y << 1));
      v[2] = float(-w);
      v += FloatVector4(-1.0f, -1.0f, 0.0f, 0.0f);
      break;
  }
  // normalize vector
  float   scale = 1.0f / float(std::sqrt(v.dotProduct3(v)));
  // v[3] = scale factor to account for variable angular resolution
  v[3] = float(w);
  v *= scale;
  return v;
}

void SFCubeMapFilter::processImage_Copy(
    unsigned char *outBufP, int w, int h, int y0, int y1)
{
  for (int n = 0; n < 6; n++)
  {
    for (int y = y0; y < y1; y++)
    {
      unsigned char *p = outBufP + (size_t(y * w) * sizeof(std::uint32_t));
      for (int x = 0; x < w; x++, p = p + 4)
      {
        FloatVector4  c(inBuf[(n * h + y) * w + x]);
        pixelStoreFunction(p, c);
      }
    }
    outBufP = outBufP + faceDataSize;
  }
}

void SFCubeMapFilter::processImage_Diffuse(
    unsigned char *outBufP, int w, int h, int y0, int y1)
{
  for (int n = 0; n < 6; n++)
  {
    for (int y = y0; y < y1; y++)
    {
      unsigned char *p = outBufP + (size_t(y * w) * sizeof(std::uint32_t));
      for (int x = 0; x < w; x++, p = p + 4)
      {
        // v1 = normal vector
        FloatVector4  v1(cubeCoordTable[(n * h + y) * w + x]);
        FloatVector4  c(0.0f);
        float   totalWeight = 0.0f;
        for (std::vector< FloatVector4 >::const_iterator
                 i = cubeCoordTable.begin(); i != cubeCoordTable.end(); i++)
        {
          // v2 = light vector
          FloatVector4  v2(*i);
          float   weight = v2.dotProduct3(v1);
          if (weight > 0.0f)
          {
            weight *= v2[3];
            c += (inBuf[i - cubeCoordTable.begin()] * weight);
            totalWeight += weight;
          }
        }
        pixelStoreFunction(p, c / totalWeight);
      }
    }
    outBufP = outBufP + faceDataSize;
  }
}

void SFCubeMapFilter::processImage_Specular(
    unsigned char *outBufP, int w, int h, int y0, int y1, float roughness)
{
  std::vector< FloatVector4 > tmpBuf(size_t(w * (y1 - y0)) * 6,
                                     FloatVector4(0.0f));
  float   a = roughness * roughness;
  float   a2 = a * a;
  for (size_t j0 = 0; (j0 + 8) <= cubeCoordTable.size(); )
  {
    // accumulate convolution output in tmpBuf using smaller partitions of the
    // impulse response so that data used from cubeCoordTable and inBuf fits in
    // L1 cache
    size_t  j1 = j0 + 512;
    if (j1 > cubeCoordTable.size())
      j1 = cubeCoordTable.size();
    FloatVector4  *tmpBufPtr = tmpBuf.data();
    for (int n = 0; n < 6; n++)
    {
      for (int y = y0; y < y1; y++)
      {
        // w must be even
        for (int x = 0; x < w; x++, tmpBufPtr++)
        {
          int     i = (n * h + y) * w + x;
          std::vector< FloatVector4 >::const_iterator j =
              cubeCoordTable.begin() + ((i & ~7) | ((i & 4) >> 2));
          // v1 = reflected view vector (R), assume V = N = R
          FloatVector8  v1x(j[0][i & 3]);
          FloatVector8  v1y(j[2][i & 3]);
          FloatVector8  v1z(j[4][i & 3]);
          FloatVector8  c_r(0.0f);
          FloatVector8  c_g(0.0f);
          FloatVector8  c_b(0.0f);
          FloatVector8  totalWeight(0.0f);
          std::vector< FloatVector4 >::const_iterator k = inBuf.begin() + j0;
          for (j = cubeCoordTable.begin() + j0;
               (j + 8) <= (cubeCoordTable.begin() + j1); j = j + 8, k = k + 8)
          {
            // v2 = light vector
            FloatVector8  v2x(&(j[0]));
            FloatVector8  v2y(&(j[2]));
            FloatVector8  v2z(&(j[4]));
            // d = N·L = R·L = 2.0 * N·H * N·H - 1.0
            FloatVector8  d = (v1x * v2x) + (v1y * v2y) + (v1z * v2z);
            if (d.getSignMask() == 255U)
              continue;
            FloatVector8  v2w(&(j[6]));
            d.maxValues(FloatVector8(0.0f));
            FloatVector8  g1 = d;
            // g2 = geometry function denominator * 2.0 (a = k * 2.0)
            FloatVector8  g2 = d * (2.0f - a) + a;
            // D denominator = (N·H * N·H * (a2 - 1.0) + 1.0)² * 4.0
            d = (d + 1.0f) * (a2 - 1.0f) + 2.0f;
            FloatVector8  weight = g1 * v2w / (g2 * d * d);
            c_r += (FloatVector8(&(k[0])) * weight);
            c_g += (FloatVector8(&(k[2])) * weight);
            c_b += (FloatVector8(&(k[4])) * weight);
            totalWeight += weight;
          }
          FloatVector4  c(c_r.dotProduct(FloatVector8(1.0f)),
                          c_g.dotProduct(FloatVector8(1.0f)),
                          c_b.dotProduct(FloatVector8(1.0f)),
                          totalWeight.dotProduct(FloatVector8(1.0f)));
          *tmpBufPtr = *tmpBufPtr + c;
        }
      }
    }
    j0 = j1;
  }
  const FloatVector4  *tmpBufPtr = tmpBuf.data();
  for (int n = 0; n < 6; n++)
  {
    for (int y = y0; y < y1; y++)
    {
      unsigned char *p = outBufP + (size_t(y * w) * sizeof(std::uint32_t));
      for (int x = 0; x < w; x++, p = p + 4, tmpBufPtr++)
      {
        FloatVector4  c(*tmpBufPtr);
        pixelStoreFunction(p, c / c[3]);
      }
    }
    outBufP = outBufP + faceDataSize;
  }
}

void SFCubeMapFilter::pixelStore_R8G8B8A8(unsigned char *p, FloatVector4 c)
{
  std::uint32_t tmp = std::uint32_t(c.srgbCompress());
  p[0] = (unsigned char) (tmp & 0xFF);
  p[1] = (unsigned char) ((tmp >> 8) & 0xFF);
  p[2] = (unsigned char) ((tmp >> 16) & 0xFF);
  p[3] = 0xFF;
}

void SFCubeMapFilter::pixelStore_R9G9B9E5(unsigned char *p, FloatVector4 c)
{
  std::uint32_t tmp = c.convertToR9G9B9E5();
  p[0] = (unsigned char) (tmp & 0xFF);
  p[1] = (unsigned char) ((tmp >> 8) & 0xFF);
  p[2] = (unsigned char) ((tmp >> 16) & 0xFF);
  p[3] = (unsigned char) ((tmp >> 24) & 0xFF);
}

void SFCubeMapFilter::threadFunction(
    SFCubeMapFilter *p, unsigned char *outBufP,
    int w, int h, int m, int maxMip, int y0, int y1)
{
  if (m == 0)
  {
    p->processImage_Copy(outBufP, w, h, y0, y1);
  }
  else if (m < (maxMip - 1))
  {
    float   roughness = 1.0f;   // at 4x4 resolution
    if (m < (maxMip - 3))
    {
      float   tmp = float(m) / float(maxMip - 2);
      // 8x8 resolution is also used to approximate the diffuse filter,
      // with roughness = 6/7
      tmp = tmp * float((maxMip - 2) * 48) / float((maxMip - 3) * 49);
      roughness = 1.0f - float(std::sqrt(1.0 - tmp));
    }
    p->processImage_Specular(outBufP, w, h, y0, y1, roughness);
  }
  else
  {
    p->processImage_Diffuse(outBufP, w, h, y0, y1);
  }
}

void SFCubeMapFilter::transpose4x8(std::vector< FloatVector4 >& v)
{
  std::vector< FloatVector4 >::iterator i;
  for (i = v.begin(); (i + 8) <= v.end(); i = i + 8)
  {
    FloatVector4  tmp0(i[0]);
    FloatVector4  tmp1(i[1]);
    FloatVector4  tmp2(i[2]);
    FloatVector4  tmp3(i[3]);
    FloatVector4  tmp4(i[4]);
    FloatVector4  tmp5(i[5]);
    FloatVector4  tmp6(i[6]);
    FloatVector4  tmp7(i[7]);
    i[0] = FloatVector4(tmp0[0], tmp1[0], tmp2[0], tmp3[0]);
    i[1] = FloatVector4(tmp4[0], tmp5[0], tmp6[0], tmp7[0]);
    i[2] = FloatVector4(tmp0[1], tmp1[1], tmp2[1], tmp3[1]);
    i[3] = FloatVector4(tmp4[1], tmp5[1], tmp6[1], tmp7[1]);
    i[4] = FloatVector4(tmp0[2], tmp1[2], tmp2[2], tmp3[2]);
    i[5] = FloatVector4(tmp4[2], tmp5[2], tmp6[2], tmp7[2]);
    i[6] = FloatVector4(tmp0[3], tmp1[3], tmp2[3], tmp3[3]);
    i[7] = FloatVector4(tmp4[3], tmp5[3], tmp6[3], tmp7[3]);
  }
}

void SFCubeMapFilter::transpose8x4(std::vector< FloatVector4 >& v)
{
  std::vector< FloatVector4 >::iterator i;
  for (i = v.begin(); (i + 8) <= v.end(); i = i + 8)
  {
    FloatVector4  tmp0(i[0]);
    FloatVector4  tmp1(i[1]);
    FloatVector4  tmp2(i[2]);
    FloatVector4  tmp3(i[3]);
    FloatVector4  tmp4(i[4]);
    FloatVector4  tmp5(i[5]);
    FloatVector4  tmp6(i[6]);
    FloatVector4  tmp7(i[7]);
    i[0] = FloatVector4(tmp0[0], tmp2[0], tmp4[0], tmp6[0]);
    i[1] = FloatVector4(tmp0[1], tmp2[1], tmp4[1], tmp6[1]);
    i[2] = FloatVector4(tmp0[2], tmp2[2], tmp4[2], tmp6[2]);
    i[3] = FloatVector4(tmp0[3], tmp2[3], tmp4[3], tmp6[3]);
    i[4] = FloatVector4(tmp1[0], tmp3[0], tmp5[0], tmp7[0]);
    i[5] = FloatVector4(tmp1[1], tmp3[1], tmp5[1], tmp7[1]);
    i[6] = FloatVector4(tmp1[2], tmp3[2], tmp5[2], tmp7[2]);
    i[7] = FloatVector4(tmp1[3], tmp3[3], tmp5[3], tmp7[3]);
  }
}

int SFCubeMapFilter::readImageData(const unsigned char *buf, size_t bufSize)
{
  size_t  w0 = FileBuffer::readUInt32Fast(buf + 16);
  size_t  h0 = FileBuffer::readUInt32Fast(buf + 12);
  size_t  sizeRequired = w0 * h0 * 6;
  faceDataSize = 0;
  for (size_t w2 = width * width; w2; w2 = w2 >> 2)
    faceDataSize += w2;
  faceDataSize = faceDataSize * sizeof(std::uint32_t);
  FloatVector4  scale(0.0f);
  if (buf[128] == 0x0A)
  {
    // DXGI_FORMAT_R16G16B16A16_FLOAT
    sizeRequired = sizeRequired * sizeof(std::uint64_t) + 148;
    if (bufSize < sizeRequired || ((bufSize - 148) % 48) != 0)
      return 0;
    inBuf.resize(w0 * h0 * 6, FloatVector4(0.0f));
    for (size_t n = 0; n < 6; n++)
    {
      for (size_t y = 0; y < h0; y++)
      {
        for (size_t x = 0; x < w0; x++)
        {
          size_t  i = y * w0 + x;
          size_t  j = n * w0 * h0 + i;
          i = i * sizeof(std::uint64_t);
          i = i + (n * ((bufSize - 148) / 6)) + 148;
          std::uint64_t tmp = FileBuffer::readUInt64Fast(buf + i);
          FloatVector4  c(FloatVector4::convertFloat16(tmp));
          c.maxValues(FloatVector4(0.0f));
          c.minValues(FloatVector4(65536.0f));
          inBuf[j] = c;
          scale += c;
        }
      }
    }
  }
  else if (buf[128] == 0x5F || buf[128] == 0x60)
  {
    // DXGI_FORMAT_BC6H_UF16 or DXGI_FORMAT_BC6H_SF16
    bool    (*decompressFunc)(const std::uint8_t *,
                              std::uint32_t, std::uint32_t, std::uint8_t *);
    if (buf[128] != 0x60)
      decompressFunc = &detexDecompressBlockBPTC_FLOAT;
    else
      decompressFunc = &detexDecompressBlockBPTC_SIGNED_FLOAT;
    sizeRequired = sizeRequired + 148;
    if (bufSize < sizeRequired || ((bufSize - 148) % 96) != 0)
      return 0;
    inBuf.resize(w0 * h0 * 6, FloatVector4(0.0f));
    std::uint8_t  tmpBuf[128];
    for (size_t n = 0; n < 6; n++)
    {
      for (size_t y = 0; y < h0; y = y + 4)
      {
        for (size_t x = 0; x < w0; x = x + 4)
        {
          size_t  i = (y >> 2) * (w0 >> 2) + (x >> 2);
          i = (i << 4) + (n * ((bufSize - 148) / 6)) + 148;
          size_t  j = n * w0 * h0 + (y * w0 + x);
          (void) decompressFunc(reinterpret_cast< const std::uint8_t * >(
                                    buf + i), 0xFFFFFFFFU, 0U, tmpBuf);
          for (i = 0; i < 16; i++)
          {
            std::uint64_t tmp = FileBuffer::readUInt64Fast(&(tmpBuf[i << 3]));
            FloatVector4  c(FloatVector4::convertFloat16(tmp));
            c.maxValues(FloatVector4(0.0f));
            c.minValues(FloatVector4(65536.0f));
            inBuf[j + ((i >> 2) * w0) + (i & 3)] = c;
            scale += c;
          }
        }
      }
    }
  }
  else
  {
    if (buf[128] == 0x43)       // DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
      return 0;                 // assume the texture is already filtered
    try
    {
      DDSTexture  t(buf, bufSize, 0);
      if (t.getWidth() != int(w0) || t.getHeight() != int(h0) ||
          t.getTextureCount() != 6)
      {
        return 0;
      }
      inBuf.resize(w0 * h0 * 6, FloatVector4(0.0f));
      size_t  j = 0;
      for (int n = 0; n < 6; n++)
      {
        for (int y = 0; y < int(h0); y++)
        {
          for (int x = 0; x < int(w0); x++, j++)
          {
            FloatVector4  c(t.getPixelN(x, y, 0, n));
            if (t.isSRGBTexture())
              inBuf[j] = c.srgbExpand();
            else
              inBuf[j] = c * (1.0f / 255.0f);
          }
        }
      }
    }
    catch (FO76UtilsError&)
    {
      return 0;
    }
  }
  if (buf[128] == 0x0A || buf[128] == 0x5F || buf[128] == 0x60)
  {
    // normalize float formats
    scale[0] = scale[0] + scale[1] + scale[2];
    scale[0] = (15.0f / 3.0f) * scale[0] / float(int(inBuf.size()));
    scale = FloatVector4(1.0f / std::min(std::max(scale[0], 1.0f), 65536.0f));
    for (size_t i = 0; i < inBuf.size(); i++)
      inBuf[i] = inBuf[i] * scale;
  }
  return int(std::bit_width((unsigned long) w0));
}

SFCubeMapFilter::SFCubeMapFilter()
{
}

SFCubeMapFilter::~SFCubeMapFilter()
{
}

size_t SFCubeMapFilter::convertImage(
    unsigned char *buf, size_t bufSize, bool outFmtFloat, size_t bufCapacity)
{
  if (bufSize < 148)
    return bufSize;
  size_t  w0 = FileBuffer::readUInt32Fast(buf + 16);
  size_t  h0 = FileBuffer::readUInt32Fast(buf + 12);
  if (FileBuffer::readUInt32Fast(buf) != 0x20534444 ||          // "DDS "
      FileBuffer::readUInt32Fast(buf + 84) != 0x30315844 ||     // "DX10"
      w0 != h0 || w0 < width || w0 > 32768 || (w0 & (w0 - 1)))
  {
    return bufSize;
  }
  int     mipCnt = readImageData(buf, bufSize);
  size_t  newSize = faceDataSize * 6 + 148;
  if (mipCnt < 1 || std::max(bufSize, bufCapacity) < newSize)
    return bufSize;

  if (!outFmtFloat)
  {
    pixelStoreFunction = &pixelStore_R8G8B8A8;
    buf[128] = 0x1D;                    // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
  }
  else
  {
    pixelStoreFunction = &pixelStore_R9G9B9E5;
    buf[128] = 0x43;                    // DXGI_FORMAT_R9G9B9E5_SHAREDEXP
  }

  unsigned char *outBufP = buf + 148;
  int     threadCnt = int(std::thread::hardware_concurrency());
  threadCnt = std::min< int >(std::max< int >(threadCnt, 1), 16);
  std::thread *threads[16];
  for (int i = 0; i < 16; i++)
    threads[i] = nullptr;
  int     w = int(w0);
  int     h = int(h0);
  int     maxMip = int(std::bit_width((unsigned int) width)) - 1;
  for (int m = 0; m < mipCnt; m++)
  {
    if (w <= width && h <= height)
    {
      cubeCoordTable.resize(size_t(w * h) * 6, FloatVector4(0.0f));
      for (int n = 0; n < 6; n++)
      {
        for (int y = 0; y < h; y++)
        {
          for (int x = 0; x < w; x++)
            cubeCoordTable[(n * h + y) * w + x] = convertCoord(x, y, w, n);
        }
      }
      if (m > (mipCnt - 9) && m < (mipCnt - 2))
      {
        // specular: reorder data for more efficient use of SIMD
        transpose4x8(inBuf);
        transpose4x8(cubeCoordTable);
      }
      try
      {
        if (h < 16)
          threadCnt = 1;
        else if ((h >> 3) < threadCnt)
          threadCnt = h >> 3;
        for (int i = 0; i < threadCnt; i++)
        {
          threads[i] = new std::thread(threadFunction, this, outBufP,
                                       w, h, m + maxMip - (mipCnt - 1), maxMip,
                                       i * h / threadCnt,
                                       (i + 1) * h / threadCnt);
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
      if (m > (mipCnt - 9) && m < (mipCnt - 2))
        transpose8x4(inBuf);
      outBufP = outBufP + (size_t(w * h) * sizeof(std::uint32_t));
    }
    // calculate mipmaps
    int     w2 = (w + 1) >> 1;
    int     h2 = (h + 1) >> 1;
    for (int n = 0; n < 6; n++)
    {
      for (int y = 0; y < h2; y++)
      {
        for (int x = 0; x < w2; x++)
        {
          int     x0 = x << 1;
          int     x1 = x0 + int(w > 1);
          int     y0 = y << 1;
          int     y1 = y0 + int(h > 1);
          FloatVector4  c0 = inBuf[(n * w * h) + (y0 * w + x0)];
          FloatVector4  c1 = inBuf[(n * w * h) + (y0 * w + x1)];
          FloatVector4  c2 = inBuf[(n * w * h) + (y1 * w + x0)];
          FloatVector4  c3 = inBuf[(n * w * h) + (y1 * w + x1)];
          c0 = (c0 + c1 + c2 + c3) * 0.25f;
          inBuf[(n * w2 * h2) + (y * w2 + x)] = c0;
        }
      }
    }
    w = w2;
    h = h2;
    inBuf.resize(size_t(w * h) * 6);
  }

  buf[8] = 0x0F;        // DDSD_CAPS, DDSD_HEIGHT, DDSD_WIDTH, DDSD_PITCH
  buf[9] = 0x10;        // DDSD_PIXELFORMAT
  buf[10] = 0x02;       // DDSD_MIPMAPCOUNT
  buf[11] = 0x00;
  buf[12] = (unsigned char) (height & 0xFF);
  buf[13] = (unsigned char) (height >> 8);
  buf[16] = (unsigned char) (width & 0xFF);
  buf[17] = (unsigned char) (width >> 8);
  buf[20] = (unsigned char) ((sizeof(std::uint32_t) * width) & 0xFF);
  buf[21] = (unsigned char) ((sizeof(std::uint32_t) * width) >> 8);
  buf[22] = 0x00;
  buf[23] = 0x00;
  buf[28] = (unsigned char) (maxMip + 1);
  buf[108] = buf[108] | 0x08;           // DDSCAPS_COMPLEX
  buf[113] = buf[113] | 0xFE;           // DDSCAPS2_CUBEMAP*
  return newSize;
}

SFCubeMapCache::SFCubeMapCache()
{
}

SFCubeMapCache::~SFCubeMapCache()
{
}

size_t SFCubeMapCache::convertImage(
    unsigned char *buf, size_t bufSize, bool outFmtFloat, size_t bufCapacity)
{
  std::uint32_t h = 0xFFFFFFFFU;
  size_t  i = 0;
  for ( ; (i + 8) <= bufSize; i = i + 8)
  {
    std::uint64_t tmp = FileBuffer::readUInt64Fast(buf + i);
    hashFunctionCRC32C< std::uint64_t >(h, tmp);
  }
  for ( ; i < bufSize; i++)
    hashFunctionCRC32C< unsigned char >(h, buf[i]);
  std::uint64_t k = (std::uint64_t(bufSize) << 32) | h;
  std::vector< unsigned char >& v = cachedTextures[k];
  if (v.size() > 0)
  {
    std::memcpy(buf, v.data(), v.size());
    return v.size();
  }
  SFCubeMapFilter cubeMapFilter;
  size_t  newSize =
      cubeMapFilter.convertImage(buf, bufSize, outFmtFloat, bufCapacity);
  if (newSize && newSize < bufSize)
  {
    v.resize(newSize);
    std::memcpy(v.data(), buf, newSize);
  }
  return newSize;
}

