
#ifndef SFCUBE_HPP_INCLUDED
#define SFCUBE_HPP_INCLUDED

#include "common.hpp"
#include "fp32vec4.hpp"

class SFCubeMapFilter
{
 protected:
  enum
  {
    width = 256,
    height = 256,
    dxgiFormat = 0x0A           // DXGI_FORMAT_R16G16B16A16_FLOAT
  };
  std::vector< FloatVector4 > inBuf;
  std::vector< FloatVector4 > cubeCoordTable;
  size_t  faceDataSize;
  void (*pixelStoreFunction)(unsigned char *p, FloatVector4 c);
  static FloatVector4 convertCoord(int x, int y, int w, int n);
  void processImage_Copy(unsigned char *outBufP, int w, int h, int y0, int y1);
  void processImage_Diffuse(unsigned char *outBufP,
                            int w, int h, int y0, int y1);
  void processImage_Specular(unsigned char *outBufP,
                             int w, int h, int y0, int y1, float roughness);
  static void pixelStore_R8G8B8A8(unsigned char *p, FloatVector4 c);
  static void pixelStore_R9G9B9E5(unsigned char *p, FloatVector4 c);
  static void threadFunction(SFCubeMapFilter *p, unsigned char *outBufP,
                             int w, int h, int m, int maxMip, int y0, int y1);
  static void transpose4x8(std::vector< FloatVector4 >& v);
  static void transpose8x4(std::vector< FloatVector4 >& v);
 public:
  SFCubeMapFilter();
  ~SFCubeMapFilter();
  // Returns the new buffer size. If outFmtFloat is true, the output format is
  // DXGI_FORMAT_R9G9B9E5_SHAREDEXP instead of DXGI_FORMAT_R8G8B8A8_UNORM_SRGB.
  size_t convertImage(unsigned char *buf, size_t bufSize,
                      bool outFmtFloat = false);
};

class SFCubeMapCache
{
 protected:
  std::map< std::uint64_t, std::vector< unsigned char > > cachedTextures;
 public:
  SFCubeMapCache();
  ~SFCubeMapCache();
  size_t convertImage(unsigned char *buf, size_t bufSize,
                      bool outFmtFloat = false);
};

#endif

