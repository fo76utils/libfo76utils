
#include "sfcube.hpp"
#include "filebuf.hpp"
#include "fp32vec8.hpp"
#include "ddstxt.hpp"

#include <thread>

const float SFCubeMapFilter::defaultRoughnessTable[7] =
{
  0.00000000f, 0.10435608f, 0.21922359f, 0.34861218f, 0.50000000f, 0.69098301f,
  1.00000000f
};

// face 0: E,      -X = up,   +X = down, -Y = N,    +Y = S
// face 1: W,      -X = down, +X = up,   -Y = N,    +Y = S
// face 2: N,      -X = W,    +X = E,    -Y = down, +Y = up
// face 3: S,      -X = W,    +X = E,    -Y = up,   +Y = down
// face 4: top,    -X = W,    +X = E,    -Y = N,    +Y = S
// face 5: bottom, -X = E,    +X = W,    -Y = N,    +Y = S

FloatVector4 SFCubeMapFilter::convertCoord(int x, int y, int w, int n)
{
  FloatVector4  v((float) w);
  switch (n)
  {
    case 0:
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
  v *= (1.0f / float(std::sqrt(v.dotProduct3(v))));
  // v[3] = scale factor to account for variable angular resolution
  v[3] = v[3] * v[3] * v[3];
  return v;
}

void SFCubeMapFilter::processImage_Copy(
    unsigned char *outBufP, int w, int y0, int y1)
{
  for (int n = 0; n < 6; n++)
  {
    for (int y = y0; y < y1; y++)
    {
      unsigned char *p = outBufP + (size_t(y * w) * sizeof(std::uint32_t));
      for (int x = 0; x < w; x++, p = p + 4)
      {
        FloatVector4  c(imageBuf[(n * w + y) * w + x]);
        pixelStoreFunction(p, c);
      }
    }
    outBufP = outBufP + faceDataSize;
  }
}

void SFCubeMapFilter::processImage_Diffuse(
    unsigned char *outBufP, int w, int y0, int y1)
{
  for (int n = 0; n < 6; n++)
  {
    for (int y = y0; y < y1; y++)
    {
      unsigned char *p = outBufP + (size_t(y * w) * sizeof(std::uint32_t));
      for (int x = 0; x < w; x++, p = p + 4)
      {
        // v1 = normal vector
        FloatVector4  v1(cubeCoordTable[(n * w + y) * w + x]);
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
            c += (imageBuf[i - cubeCoordTable.begin()] * weight);
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
    unsigned char *outBufP, int w, int y0, int y1, float roughness)
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
          int     i = (n * w + y) * w + x;
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
          const FloatVector4  *k = imageBuf + j0;
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
            FloatVector8  nDotL = d;
            // D denominator = (N·H * N·H * (a2 - 1.0) + 1.0)² * 4.0
            //               = ((R·L + 1.0) * (a2 - 1.0) + 2.0)²
            d = d * (a2 - 1.0f) + (a2 + 1.0f);
            FloatVector8  weight = nDotL * v2w / (d * d);
            c_r += (FloatVector8(k) * weight);
            c_g += (FloatVector8(k + 2) * weight);
            c_b += (FloatVector8(k + 4) * weight);
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
  std::uint32_t tmp = std::uint32_t(c.srgbCompress()) | 0xFF000000U;
  FileBuffer::writeUInt32Fast(p, tmp);
}

void SFCubeMapFilter::pixelStore_R9G9B9E5(unsigned char *p, FloatVector4 c)
{
  FileBuffer::writeUInt32Fast(p, c.convertToR9G9B9E5());
}

void SFCubeMapFilter::threadFunction(
    SFCubeMapFilter *p, unsigned char *outBufP,
    int w, int y0, int y1, float roughness, int filterMode)
{
  if (filterMode < 1)
    p->processImage_Copy(outBufP, w, y0, y1);
  else if (filterMode == 1)
    p->processImage_Specular(outBufP, w, y0, y1, roughness);
  else
    p->processImage_Diffuse(outBufP, w, y0, y1);
}

void SFCubeMapFilter::transpose4x8(FloatVector4 *v, size_t n)
{
  FloatVector4  *endPtr = v + n;
  for ( ; (v + 8) <= endPtr; v = v + 8)
  {
    FloatVector4  tmp0(v[0]);
    FloatVector4  tmp1(v[1]);
    FloatVector4  tmp2(v[2]);
    FloatVector4  tmp3(v[3]);
    FloatVector4  tmp4(v[4]);
    FloatVector4  tmp5(v[5]);
    FloatVector4  tmp6(v[6]);
    FloatVector4  tmp7(v[7]);
    v[0] = FloatVector4(tmp0[0], tmp1[0], tmp2[0], tmp3[0]);
    v[1] = FloatVector4(tmp4[0], tmp5[0], tmp6[0], tmp7[0]);
    v[2] = FloatVector4(tmp0[1], tmp1[1], tmp2[1], tmp3[1]);
    v[3] = FloatVector4(tmp4[1], tmp5[1], tmp6[1], tmp7[1]);
    v[4] = FloatVector4(tmp0[2], tmp1[2], tmp2[2], tmp3[2]);
    v[5] = FloatVector4(tmp4[2], tmp5[2], tmp6[2], tmp7[2]);
    v[6] = FloatVector4(tmp0[3], tmp1[3], tmp2[3], tmp3[3]);
    v[7] = FloatVector4(tmp4[3], tmp5[3], tmp6[3], tmp7[3]);
  }
}

void SFCubeMapFilter::transpose8x4(FloatVector4 *v, size_t n)
{
  FloatVector4  *endPtr = v + n;
  for ( ; (v + 8) <= endPtr; v = v + 8)
  {
    FloatVector4  tmp0(v[0]);
    FloatVector4  tmp1(v[1]);
    FloatVector4  tmp2(v[2]);
    FloatVector4  tmp3(v[3]);
    FloatVector4  tmp4(v[4]);
    FloatVector4  tmp5(v[5]);
    FloatVector4  tmp6(v[6]);
    FloatVector4  tmp7(v[7]);
    v[0] = FloatVector4(tmp0[0], tmp2[0], tmp4[0], tmp6[0]);
    v[1] = FloatVector4(tmp0[1], tmp2[1], tmp4[1], tmp6[1]);
    v[2] = FloatVector4(tmp0[2], tmp2[2], tmp4[2], tmp6[2]);
    v[3] = FloatVector4(tmp0[3], tmp2[3], tmp4[3], tmp6[3]);
    v[4] = FloatVector4(tmp1[0], tmp3[0], tmp5[0], tmp7[0]);
    v[5] = FloatVector4(tmp1[1], tmp3[1], tmp5[1], tmp7[1]);
    v[6] = FloatVector4(tmp1[2], tmp3[2], tmp5[2], tmp7[2]);
    v[7] = FloatVector4(tmp1[3], tmp3[3], tmp5[3], tmp7[3]);
  }
}

int SFCubeMapFilter::readImageData(
    std::vector< FloatVector4 >& imageData,
    const unsigned char *buf, size_t bufSize)
{
  size_t  w0 = FileBuffer::readUInt32Fast(buf + 16);
  size_t  h0 = FileBuffer::readUInt32Fast(buf + 12);
  size_t  sizeRequired = w0 * h0 * 6;
  faceDataSize = 0;
  for (std::uint32_t w2 = width * width; w2; w2 = w2 >> 2)
    faceDataSize += w2;
  faceDataSize = faceDataSize * sizeof(std::uint32_t);
  FloatVector4  scale(0.0f);
  if (buf[128] == 0x0A)
  {
    // DXGI_FORMAT_R16G16B16A16_FLOAT
    sizeRequired = sizeRequired * sizeof(std::uint64_t) + 148;
    if (bufSize < sizeRequired || ((bufSize - 148) % 48) != 0)
      return 0;
    imageData.resize(w0 * h0 * 6, FloatVector4(0.0f));
    for (size_t n = 0; n < 6; n++)
    {
      const unsigned char *p1 = buf + ((n * ((bufSize - 148) / 6)) + 148);
      FloatVector4  *p2 = imageData.data() + (n * w0 * h0);
      for (size_t y = 0; y < h0; y++)
      {
        FloatVector4  tmpScale(0.0f);
        for (size_t x = 0; x < w0; x++, p1 = p1 + sizeof(std::uint64_t), p2++)
        {
          std::uint64_t tmp = FileBuffer::readUInt64Fast(p1);
          FloatVector4  c(FloatVector4::convertFloat16(tmp));
          c.maxValues(FloatVector4(0.0f)).minValues(FloatVector4(65536.0f));
          *p2 = c;
          tmpScale += c;
        }
        scale += tmpScale;
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
    imageData.resize(w0 * h0 * 6, FloatVector4(0.0f));
    std::uint8_t  tmpBuf[128];
    for (size_t n = 0; n < 6; n++)
    {
      const unsigned char *p1 = buf + ((n * ((bufSize - 148) / 6)) + 148);
      FloatVector4  *p2 = imageData.data() + (n * w0 * h0);
      for (size_t y = 0; y < h0; y = y + 4, p2 = p2 + (w0 * 3))
      {
        FloatVector4  tmpScale(0.0f);
        for (size_t x = 0; x < w0; x = x + 4, p1 = p1 + 16, p2 = p2 + 4)
        {
          (void) decompressFunc(reinterpret_cast< const std::uint8_t * >(p1),
                                0xFFFFFFFFU, 0U, tmpBuf);
          for (size_t i = 0; i < 16; i++)
          {
            std::uint64_t tmp = FileBuffer::readUInt64Fast(&(tmpBuf[i << 3]));
            FloatVector4  c(FloatVector4::convertFloat16(tmp));
            c.maxValues(FloatVector4(0.0f)).minValues(FloatVector4(65536.0f));
            p2[((i >> 2) * w0) + (i & 3)] = c;
            tmpScale += c;
          }
        }
        scale += tmpScale;
      }
    }
  }
  else if (buf[128] == 0x43)
  {
    // DXGI_FORMAT_R9G9B9E5_SHAREDEXP: assume the texture is already filtered,
    // no normalization
    sizeRequired = sizeRequired * sizeof(std::uint32_t) + 148;
    if (bufSize < sizeRequired || ((bufSize - 148) % 24) != 0)
      return 0;
    imageData.resize(w0 * h0 * 6, FloatVector4(0.0f));
    for (size_t n = 0; n < 6; n++)
    {
      const unsigned char *p1 = buf + ((n * ((bufSize - 148) / 6)) + 148);
      FloatVector4  *p2 = imageData.data() + (n * w0 * h0);
      for (size_t y = 0; y < h0; y++)
      {
        for (size_t x = 0; x < w0; x++, p1 = p1 + sizeof(std::uint32_t), p2++)
          *p2 = FloatVector4::convertR9G9B9E5(FileBuffer::readUInt32Fast(p1));
      }
    }
  }
  else
  {
    try
    {
      DDSTexture  t(buf, bufSize, 0);
      if (t.getWidth() != int(w0) || t.getHeight() != int(h0) ||
          !t.getIsCubeMap())
      {
        return 0;
      }
      imageData.resize(w0 * h0 * 6, FloatVector4(0.0f));
      size_t  j = 0;
      for (int n = 0; n < 6; n++)
      {
        for (int y = 0; y < int(h0); y++)
        {
          for (int x = 0; x < int(w0); x++, j++)
          {
            FloatVector4  c(t.getPixelN(x, y, 0, n));
            if (t.isSRGBTexture())
              imageData[j] = c.srgbExpand();
            else
              imageData[j] = c * (1.0f / 255.0f);
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
    scale[0] = normalizeLevel * scale[0] / float(int(imageData.size()));
    if (scale[0] > 1.0f)
    {
      scale = FloatVector4(1.0f / std::min(scale[0], 65536.0f));
      for (size_t i = 0; i < imageData.size(); i++)
        imageData[i] = imageData[i] * scale;
    }
  }
  return int(std::bit_width((unsigned long) w0));
}

static inline void getCubeMapPixel(
    FloatVector4& c, float& scale, float weight,
    const FloatVector4 *inBuf, int x, int y, int n, int w)
{
  if (!DDSTexture::wrapCubeMapCoord(x, y, n, w - 1)) [[unlikely]]
  {
    scale = 1.0f / (1.0f - weight);
    return;
  }
  c += (inBuf[(size_t(n) * size_t(w) + size_t(y)) * size_t(w) + size_t(x)]
        * weight);
}

void SFCubeMapFilter::upsampleImage(
    std::vector< FloatVector4 >& outBuf, int outWidth,
    const std::vector< FloatVector4 >& inBuf, int inWidth)
{
  outBuf.resize(size_t(outWidth) * size_t(outWidth) * 6, FloatVector4(0.0f));
  const FloatVector4  *inPtr = inBuf.data();
  FloatVector4  *outPtr = outBuf.data();
  float   xyScale = float(inWidth) / float(outWidth);
  float   offset = xyScale * 0.5f - 0.5f;
  for (int n = 0; n < 6; n++)
  {
    for (int y = 0; y < outWidth; y++)
    {
      float   yf = float(y) * xyScale + offset;
      float   yi = float(std::floor(yf));
      int     y0 = int(yi);
      yf -= yi;
      int     y1 = y0 + 1;
      for (int x = 0; x < outWidth; x++, outPtr++)
      {
        float   xf = float(x) * xyScale + offset;
        float   xi = float(std::floor(xf));
        int     x0 = int(xi);
        xf -= xi;
        int     x1 = x0 + 1;
        FloatVector4  c(0.0f);
        float   weight3 = xf * yf;
        float   weight2 = yf - weight3;
        float   weight1 = xf - weight3;
        float   weight0 = (1.0f - xf) - weight2;
        if (!((x0 | y0 | x1 | y1) & ~(inWidth - 1))) [[likely]]
        {
          c = inPtr[(size_t(n) * size_t(inWidth) + size_t(y0))
                    * size_t(inWidth) + size_t(x0)] * weight0;
          c += (inPtr[(size_t(n) * size_t(inWidth) + size_t(y0))
                      * size_t(inWidth) + size_t(x1)] * weight1);
          c += (inPtr[(size_t(n) * size_t(inWidth) + size_t(y1))
                      * size_t(inWidth) + size_t(x0)] * weight2);
          c += (inPtr[(size_t(n) * size_t(inWidth) + size_t(y1))
                      * size_t(inWidth) + size_t(x1)] * weight3);
          *outPtr = c;
        }
        else
        {
          float   scale = 1.0f;
          getCubeMapPixel(c, scale, weight0, inPtr, x0, y0, n, inWidth);
          getCubeMapPixel(c, scale, weight1, inPtr, x1, y0, n, inWidth);
          getCubeMapPixel(c, scale, weight2, inPtr, x0, y1, n, inWidth);
          getCubeMapPixel(c, scale, weight3, inPtr, x1, y1, n, inWidth);
          *outPtr = c * scale;
        }
      }
    }
  }
}

SFCubeMapFilter::SFCubeMapFilter(size_t outputWidth)
{
  if (outputWidth < 16 || outputWidth > 2048 ||
      (outputWidth & (outputWidth - 1)))
  {
    errorMessage("SFCubeMapFilter: invalid output dimensions");
  }
  width = std::uint32_t(outputWidth);
  roughnessTable = &(defaultRoughnessTable[0]);
  roughnessTableSize = int(sizeof(defaultRoughnessTable) / sizeof(float));
  normalizeLevel = float(12.5 / 3.0);
}

SFCubeMapFilter::~SFCubeMapFilter()
{
}

size_t SFCubeMapFilter::convertImage(
    unsigned char *buf, size_t bufSize, bool outFmtFloat, size_t bufCapacity)
{
  if (bufSize < 148)
    return 0;
  size_t  w0 = FileBuffer::readUInt32Fast(buf + 16);
  size_t  h0 = FileBuffer::readUInt32Fast(buf + 12);
  if (FileBuffer::readUInt32Fast(buf) != 0x20534444 ||          // "DDS "
      FileBuffer::readUInt32Fast(buf + 84) != 0x30315844 ||     // "DX10"
      w0 != h0 || w0 < 16 || w0 > 32768 || (w0 & (w0 - 1)))
  {
    return 0;
  }
  std::vector< FloatVector4 > inBuf1;
  int     mipCnt = readImageData(inBuf1, buf, bufSize);
  size_t  newSize = faceDataSize * 6 + 148;
  if (mipCnt < 1 || std::max(bufSize, bufCapacity) < newSize)
    return 0;
  imageBuf = inBuf1.data();
  std::vector< FloatVector4 > inBuf2;

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
  int     w = int(width);
  for (int m = 0; w > 0; m++, w = w >> 1)
  {
    if (int(w0) < w)
    {
      upsampleImage(inBuf2, w, inBuf1, int(w0));
      imageBuf = inBuf2.data();
    }
    else
    {
      imageBuf = inBuf1.data();
      while (int(w0) > w)
      {
        // calculate mipmaps
        size_t  w2 = w0 >> 1;
        for (size_t y = 0; y < (w2 * 6); y++)
        {
          for (size_t x = 0; x < w2; x++)
          {
            size_t  x0 = x << 1;
            size_t  x1 = x0 + 1;
            size_t  y0 = y << 1;
            size_t  y1 = y0 + 1;
            FloatVector4  c0(imageBuf[y0 * w0 + x0]);
            FloatVector4  c1(imageBuf[y0 * w0 + x1]);
            FloatVector4  c2(imageBuf[y1 * w0 + x0]);
            FloatVector4  c3(imageBuf[y1 * w0 + x1]);
            imageBuf[y * w2 + x] = (c0 + c1 + c2 + c3) * 0.25f;
          }
        }
        w0 = w2;
      }
    }
    float   roughness = 1.0f;
    if (m < roughnessTableSize)
      roughness = roughnessTable[m];
    int     filterMode = int(roughness >= 0.015625f);
    if (filterMode)
    {
      // specular filter requires cubeCoordTable.size() to be divisible by 8
      filterMode += (w & 1);
      cubeCoordTable.resize(size_t(w * w) * 6, FloatVector4(0.0f));
      for (int n = 0; n < 6; n++)
      {
        for (int y = 0; y < w; y++)
        {
          for (int x = 0; x < w; x++)
            cubeCoordTable[(n * w + y) * w + x] = convertCoord(x, y, w, n);
        }
      }
      if (filterMode == 1)
      {
        // specular: reorder data for more efficient use of SIMD
        transpose4x8(imageBuf, size_t(w * w) * 6);
        transpose4x8(cubeCoordTable.data(), cubeCoordTable.size());
      }
    }
    try
    {
      threadCnt = std::min< int >(threadCnt, std::max< int >(w >> 3, 1));
      for (int i = 0; i < threadCnt; i++)
      {
        threads[i] = new std::thread(threadFunction, this, outBufP, w,
                                     i * w / threadCnt, (i + 1) * w / threadCnt,
                                     roughness, filterMode);
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
    if (filterMode == 1)
      transpose8x4(imageBuf, size_t(w * w) * 6);
    outBufP = outBufP + (size_t(w * w) * sizeof(std::uint32_t));
  }

  // DDSD_CAPS, DDSD_HEIGHT, DDSD_WIDTH, DDSD_PITCH,
  // DDSD_PIXELFORMAT, DDSD_MIPMAPCOUNT
  FileBuffer::writeUInt32Fast(buf + 8, 0x0002100FU);
  FileBuffer::writeUInt32Fast(buf + 12, width); // height
  FileBuffer::writeUInt32Fast(buf + 16, width); // width
  FileBuffer::writeUInt32Fast(buf + 20,         // pitch
                              std::uint32_t(width * sizeof(std::uint32_t)));
  buf[28] = (unsigned char) std::bit_width(width);
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
    unsigned char *buf, size_t bufSize, bool outFmtFloat, size_t bufCapacity,
    size_t outputWidth)
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
  SFCubeMapFilter cubeMapFilter(outputWidth);
  size_t  newSize =
      cubeMapFilter.convertImage(buf, bufSize, outFmtFloat, bufCapacity);
  if (newSize)
  {
    v.resize(newSize);
    std::memcpy(v.data(), buf, newSize);
    return newSize;
  }
  return bufSize;
}

void SFCubeMapFilter::setRoughnessTable(const float *p, size_t n)
{
  if (!p)
  {
    p = &(defaultRoughnessTable[0]);
    n = std::min< size_t >(n, sizeof(defaultRoughnessTable) / sizeof(float));
  }
  roughnessTable = p;
  roughnessTableSize = int(n);
}

