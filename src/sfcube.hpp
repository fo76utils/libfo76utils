
#ifndef SFCUBE_HPP_INCLUDED
#define SFCUBE_HPP_INCLUDED

#include "common.hpp"
#include "fp32vec4.hpp"

class SFCubeMapFilter
{
 protected:
  static const int  filterMipRange = 6; // roughness = 1.0 at this mip level
  std::vector< FloatVector4 > inBuf;
  std::vector< FloatVector4 > cubeCoordTable;
  std::uint32_t faceDataSize;
  std::uint32_t width;
  void (*pixelStoreFunction)(unsigned char *p, FloatVector4 c);
 public:
  static FloatVector4 convertCoord(int x, int y, int w, int n);
 protected:
  void processImage_Copy(unsigned char *outBufP, int w, int h, int y0, int y1);
  void processImage_Diffuse(unsigned char *outBufP,
                            int w, int h, int y0, int y1);
  void processImage_Specular(unsigned char *outBufP,
                             int w, int h, int y0, int y1, float roughness);
  static void pixelStore_R8G8B8A8(unsigned char *p, FloatVector4 c);
  static void pixelStore_R9G9B9E5(unsigned char *p, FloatVector4 c);
  static void threadFunction(SFCubeMapFilter *p, unsigned char *outBufP,
                             int w, int h, int m, int y0, int y1);
  static void transpose4x8(std::vector< FloatVector4 >& v);
  static void transpose8x4(std::vector< FloatVector4 >& v);
  // returns the number of mip levels, or 0 on error
  int readImageData(const unsigned char *buf, size_t bufSize);
 public:
  SFCubeMapFilter(size_t outputWidth = 256);
  ~SFCubeMapFilter();
  // Returns the new buffer size. If outFmtFloat is true, the output format is
  // DXGI_FORMAT_R9G9B9E5_SHAREDEXP instead of DXGI_FORMAT_R8G8B8A8_UNORM_SRGB.
  // The buffer must have sufficient capacity for width * height * 8 * 4 + 148
  // bytes, which may be greater than bufSize if the input format is not
  // R16G16B16A16_FLOAT.
  size_t convertImage(unsigned char *buf, size_t bufSize,
                      bool outFmtFloat = false, size_t bufCapacity = 0);
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

