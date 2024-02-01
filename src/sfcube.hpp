
#ifndef SFCUBE_HPP_INCLUDED
#define SFCUBE_HPP_INCLUDED

#include "common.hpp"
#include "fp32vec4.hpp"

class SFCubeMapFilter
{
 protected:
  enum
  {
    minWidth = 16,
    filterMinWidth = 16
  };
  // roughness = (5.0 - sqrt(25.0 - mipLevel * 4.0)) / 4.0
  static const float  defaultRoughnessTable[7];
  FloatVector4  *imageBuf;
  std::uint32_t faceDataSize;
  std::uint32_t width;
  // width * width * 3 * 10 floats (X, Y, Z, W only for the +X, +Y, +Z faces):
  //   vec8 X, vec8 Y, vec8 Z, vec8 W, vec8 R0, vec8 G0, vec8 B0,
  //   vec8 R1, vec8 G1, vec8 B1
  float   *cubeFilterTable;
  const float   *roughnessTable;
  int     roughnessTableSize;
  float   normalizeLevel;
  void (*pixelStoreFunction)(unsigned char *p, FloatVector4 c);
 public:
  static FloatVector4 convertCoord(int x, int y, int w, int n);
 protected:
  void processImage_Specular(unsigned char *outBufP, int w,
                             size_t startPos, size_t endPos, float roughness);
  void processImage_Copy(unsigned char *outBufP, int w, int w2,
                         size_t startPos, size_t endPos,
                         const FloatVector4 *inBufP);
  static void pixelStore_R8G8B8A8(unsigned char *p, FloatVector4 c);
  static void pixelStore_R9G9B9E5(unsigned char *p, FloatVector4 c);
  // endPos - startPos must be even if the filter is enabled
  // roughness = 1.0 simulates diffuse filter
  static void threadFunction(SFCubeMapFilter *p, unsigned char *outBufP,
                             int w, size_t startPos, size_t endPos,
                             float roughness, bool enableFilter);
  void createFilterTable(int w);
  // returns the number of mip levels, or 0 on error
  int readImageData(std::vector< FloatVector4 >& imageData,
                    const unsigned char *buf, size_t bufSize);
  static void upsampleImage(
      std::vector< FloatVector4 >& outBuf, int outWidth,
      const std::vector< FloatVector4 >& inBuf, int inWidth);
 public:
  SFCubeMapFilter(size_t outputWidth = 256);
  ~SFCubeMapFilter();
  // Returns the new buffer size. If outFmtFloat is true, the output format is
  // DXGI_FORMAT_R9G9B9E5_SHAREDEXP instead of DXGI_FORMAT_R8G8B8A8_UNORM_SRGB.
  // The buffer must have sufficient capacity for width * width * 8 * 4 + 148
  // bytes.
  // On error, 0 is returned and no changes are made to the contents of buf.
  size_t convertImage(unsigned char *buf, size_t bufSize,
                      bool outFmtFloat = false, size_t bufCapacity = 0);
  void setRoughnessTable(const float *p, size_t n);
  // input data in FP16 formats is normalized to have an average level of
  // at most 'n' (default: 1.0 / 12.5)
  inline void setNormalizeLevel(float n)
  {
    normalizeLevel = 1.0f / ((n > 0.0f ? n : 65536.0f) * 3.0f);
  }
};

class SFCubeMapCache
{
 protected:
  std::map< std::uint64_t, std::vector< unsigned char > > cachedTextures;
 public:
  SFCubeMapCache();
  ~SFCubeMapCache();
  size_t convertImage(unsigned char *buf, size_t bufSize,
                      bool outFmtFloat = false, size_t bufCapacity = 0,
                      size_t outputWidth = 256);
};

#endif

