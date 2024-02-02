
#ifndef FP32VEC8_HPP_INCLUDED
#define FP32VEC8_HPP_INCLUDED

#include "fp32vec4.hpp"

struct FloatVector8
{
#if ENABLE_X86_64_AVX
 private:
  static constexpr float      floatMinVal = 5.16987883e-26f;
  static constexpr YMM_Float  floatMinValV =
  {
    floatMinVal, floatMinVal, floatMinVal, floatMinVal,
    floatMinVal, floatMinVal, floatMinVal, floatMinVal
  };
 public:
  YMM_Float v;
  inline FloatVector8(const YMM_Float& r)
    : v(r)
  {
  }
#else
  float   v[8];
#endif
  inline FloatVector8()
  {
  }
  inline FloatVector8(const float& x);
  // construct from 8 packed 8-bit integers
  inline FloatVector8(const std::uint64_t *p);
  // construct from a pair of FloatVector4
  inline FloatVector8(const FloatVector4 *p);
  inline FloatVector8(FloatVector4 v0, FloatVector4 v1);
  inline FloatVector8(float v0, float v1, float v2, float v3,
                      float v4, float v5, float v6, float v7);
  inline FloatVector8(const float *p);
  inline FloatVector8(const std::int16_t *p);
  inline FloatVector8(const std::int32_t *p);
  // construct from 8 half precision floats
  // if noInfNaN is true, Inf and NaN values are never created
  inline FloatVector8(const std::uint16_t *p, bool noInfNaN);
  // store as 8 floats (does not require p to be aligned to 32 bytes)
  inline void convertToFloats(float *p) const;
  inline void convertToFloatVector4(FloatVector4 *p) const;
  inline void convertToInt16(std::int16_t *p) const;
  inline void convertToInt32(std::int32_t *p) const;
  inline void convertToFloat16(std::uint16_t *p) const;
  inline float& operator[](size_t n)
  {
    return v[n];
  }
  inline const float& operator[](size_t n) const
  {
    return v[n];
  }
  inline FloatVector8& operator+=(const FloatVector8& r);
  inline FloatVector8& operator-=(const FloatVector8& r);
  inline FloatVector8& operator*=(const FloatVector8& r);
  inline FloatVector8& operator/=(const FloatVector8& r);
  inline FloatVector8 operator+(const FloatVector8& r) const;
  inline FloatVector8 operator-(const FloatVector8& r) const;
  inline FloatVector8 operator*(const FloatVector8& r) const;
  inline FloatVector8 operator/(const FloatVector8& r) const;
  inline FloatVector8& operator+=(float r);
  inline FloatVector8& operator-=(float r);
  inline FloatVector8& operator*=(float r);
  inline FloatVector8& operator/=(float r);
  inline FloatVector8 operator+(const float& r) const;
  inline FloatVector8 operator-(const float& r) const;
  inline FloatVector8 operator*(const float& r) const;
  inline FloatVector8 operator/(const float& r) const;
  // v[n] = v[((mask >> ((n & 3) * 2)) & 3) | (n & 4)]
  inline FloatVector8& shuffleValues(unsigned char mask);
  // v[n] = mask & (1 << n) ? r.v[n] : v[n]
  inline FloatVector8& blendValues(const FloatVector8& r, unsigned char mask);
  inline FloatVector8& minValues(const FloatVector8& r);
  inline FloatVector8& maxValues(const FloatVector8& r);
  inline FloatVector8& floorValues();
  inline FloatVector8& ceilValues();
  inline FloatVector8& roundValues();
  inline float dotProduct(const FloatVector8& r) const;
  inline FloatVector8& squareRoot();
  inline FloatVector8& squareRootFast();
  inline FloatVector8& rsqrtFast();     // elements must be positive
  inline FloatVector8& rcpFast();       // approximates 1.0 / v
  inline FloatVector8& rcpSqr();        // 1.0 / v²
  inline FloatVector8& log2V();
  inline FloatVector8& exp2V();
  // convert to 8 packed 8-bit integers
  inline operator std::uint64_t() const;
  inline std::uint32_t getSignMask() const;
};

#if ENABLE_X86_64_AVX

inline FloatVector8::FloatVector8(const float& x)
#if ENABLE_X86_64_AVX2
{
  __asm__ ("vbroadcastss %1, %t0" : "=x" (v) : "xm" (x));
}
#else
  : v { x, x, x, x, x, x, x, x }
{
}
#endif

inline FloatVector8::FloatVector8(const std::uint64_t *p)
{
#if ENABLE_X86_64_AVX2
  __asm__ ("vpmovzxbd %1, %t0" : "=x" (v) : "m" (*p));
  __asm__ ("vcvtdq2ps %t0, %t0" : "+x" (v));
#else
  const unsigned char *b = reinterpret_cast< const unsigned char * >(p);
  XMM_UInt32  tmp;
  __asm__ ("vpmovzxbd %1, %x0" : "=x" (v) : "m" (*p));
  __asm__ ("vpmovzxbd %1, %0" : "=x" (tmp) : "m" (*(b + 4)));
  __asm__ ("vcvtdq2ps %x0, %x0" : "+x" (v));
  __asm__ ("vcvtdq2ps %0, %0" : "+x" (tmp));
  __asm__ ("vinsertf128 $0x01, %1, %t0, %t0" : "+x" (v) : "x" (tmp));
#endif
}

inline FloatVector8::FloatVector8(const FloatVector4 *p)
{
  __asm__ ("vmovups %1, %t0" : "=x" (v) : "m" (*p));
}

inline FloatVector8::FloatVector8(FloatVector4 v0, FloatVector4 v1)
{
  __asm__ ("vinsertf128 $0x01, %x2, %t1, %t0"
           : "=x" (v) : "x" (v0.v), "xm" (v1.v));
}

inline FloatVector8::FloatVector8(float v0, float v1, float v2, float v3,
                                  float v4, float v5, float v6, float v7)
  : v { v0, v1, v2, v3, v4, v5, v6, v7 }
{
}

inline FloatVector8::FloatVector8(const float *p)
{
  __asm__ ("vmovups %1, %t0" : "=x" (v) : "m" (*p));
}

inline FloatVector8::FloatVector8(const std::int16_t *p)
{
#if ENABLE_X86_64_AVX2
  __asm__ ("vpmovsxwd %1, %t0" : "=x" (v) : "m" (*p));
  __asm__ ("vcvtdq2ps %t0, %t0" : "+x" (v));
#else
  XMM_UInt32  tmp;
  __asm__ ("vpmovsxwd %1, %x0" : "=x" (v) : "m" (*p));
  __asm__ ("vpmovsxwd %1, %0" : "=x" (tmp) : "m" (*(p + 4)));
  __asm__ ("vcvtdq2ps %x0, %x0" : "+x" (v));
  __asm__ ("vcvtdq2ps %0, %0" : "+x" (tmp));
  __asm__ ("vinsertf128 $0x01, %1, %t0, %t0" : "+x" (v) : "x" (tmp));
#endif
}

inline FloatVector8::FloatVector8(const std::int32_t *p)
{
  __asm__ ("vmovdqu %1, %t0" : "=x" (v) : "m" (*p));
  __asm__ ("vcvtdq2ps %t0, %t0" : "+x" (v));
}

inline FloatVector8::FloatVector8(const std::uint16_t *p, bool noInfNaN)
{
#if ENABLE_X86_64_AVX2 || defined(__F16C__)
  if (!noInfNaN)
  {
    __asm__ ("vcvtph2ps %1, %t0" : "=x" (v) : "m" (*p));
  }
  else
  {
    __asm__ ("vmovdqu %1, %x0" : "=x" (v) : "m" (*p));
    const XMM_UInt16  expMaskTbl =
    {
      0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00
    };
    XMM_UInt16  tmp;
    __asm__ ("vpand %2, %x1, %0" : "=x" (tmp) : "x" (v), "x" (expMaskTbl));
    __asm__ ("vpcmpeqw %1, %0, %0" : "+x" (tmp) : "x" (expMaskTbl));
    __asm__ ("vpandn %x0, %1, %x0" : "+x" (v) : "x" (tmp));
    __asm__ ("vcvtph2ps %x0, %t0" : "+x" (v));
  }
#else
  std::uint64_t v0, v1;
  __asm__ ("mov %1, %0" : "=r" (v0) : "m" (*p));
  __asm__ ("mov %1, %0" : "=r" (v1) : "m" (*(p + 4)));
  v = FloatVector8(FloatVector4::convertFloat16(v0, noInfNaN),
                   FloatVector4::convertFloat16(v1, noInfNaN)).v;
#endif
}

inline void FloatVector8::convertToFloats(float *p) const
{
  __asm__ ("vmovups %t1, %0" : "=m" (*p) : "x" (v));
}

inline void FloatVector8::convertToFloatVector4(FloatVector4 *p) const
{
  __asm__ ("vmovups %t1, %0" : "=m" (*p) : "x" (v));
}

inline void FloatVector8::convertToInt16(std::int16_t *p) const
{
#if ENABLE_X86_64_AVX2
  YMM_UInt32  tmp;
  __asm__ ("vcvtps2dq %t1, %t0" : "=x" (tmp) : "xm" (v));
  __asm__ ("vpackssdw %t0, %t0, %t0" : "+x" (tmp));
  __asm__ ("vmovdqu %x1, %0" : "=m" (*p) : "x" (tmp));
#else
  XMM_UInt32  tmp1, tmp2;
  __asm__ ("vextractf128 $0x01, %t1, %0" : "=x" (tmp2) : "x" (v));
  __asm__ ("vcvtps2dq %x1, %0" : "=x" (tmp1) : "x" (v));
  __asm__ ("vcvtps2dq %0, %0" : "+x" (tmp2));
  __asm__ ("vpackssdw %1, %0, %0" : "+x" (tmp1) : "x" (tmp2));
  __asm__ ("vmovdqu %1, %0" : "=m" (*p) : "x" (tmp1));
#endif
}

inline void FloatVector8::convertToInt32(std::int32_t *p) const
{
  YMM_UInt32  tmp;
  __asm__ ("vcvtps2dq %t1, %t0" : "=x" (tmp) : "xm" (v));
  __asm__ ("vmovdqu %t1, %0" : "=m" (*p) : "x" (tmp));
}

inline void FloatVector8::convertToFloat16(std::uint16_t *p) const
{
#if ENABLE_X86_64_AVX2 || defined(__F16C__)
  __asm__ ("vcvtps2ph $0x00, %t1, %0" : "=m" (*p) : "x" (v));
#else
  p[0] = ::convertToFloat16(v[0]);
  p[1] = ::convertToFloat16(v[1]);
  p[2] = ::convertToFloat16(v[2]);
  p[3] = ::convertToFloat16(v[3]);
  p[4] = ::convertToFloat16(v[4]);
  p[5] = ::convertToFloat16(v[5]);
  p[6] = ::convertToFloat16(v[6]);
  p[7] = ::convertToFloat16(v[7]);
#endif
}

inline FloatVector8& FloatVector8::operator+=(const FloatVector8& r)
{
  v += r.v;
  return (*this);
}

inline FloatVector8& FloatVector8::operator-=(const FloatVector8& r)
{
  v -= r.v;
  return (*this);
}

inline FloatVector8& FloatVector8::operator*=(const FloatVector8& r)
{
  v *= r.v;
  return (*this);
}

inline FloatVector8& FloatVector8::operator/=(const FloatVector8& r)
{
  v /= r.v;
  return (*this);
}

inline FloatVector8 FloatVector8::operator+(const FloatVector8& r) const
{
  FloatVector8  tmp(*this);
  return (tmp += r);
}

inline FloatVector8 FloatVector8::operator-(const FloatVector8& r) const
{
  FloatVector8  tmp(*this);
  return (tmp -= r);
}

inline FloatVector8 FloatVector8::operator*(const FloatVector8& r) const
{
  FloatVector8  tmp(*this);
  return (tmp *= r);
}

inline FloatVector8 FloatVector8::operator/(const FloatVector8& r) const
{
  FloatVector8  tmp(*this);
  return (tmp /= r);
}

inline FloatVector8& FloatVector8::operator+=(float r)
{
  v += r;
  return (*this);
}

inline FloatVector8& FloatVector8::operator-=(float r)
{
  v -= r;
  return (*this);
}

inline FloatVector8& FloatVector8::operator*=(float r)
{
  v *= r;
  return (*this);
}

inline FloatVector8& FloatVector8::operator/=(float r)
{
  v /= r;
  return (*this);
}

inline FloatVector8 FloatVector8::operator+(const float& r) const
{
  FloatVector8  tmp(*this);
  return (tmp += r);
}

inline FloatVector8 FloatVector8::operator-(const float& r) const
{
  FloatVector8  tmp(*this);
  return (tmp -= r);
}

inline FloatVector8 FloatVector8::operator*(const float& r) const
{
  FloatVector8  tmp(*this);
  return (tmp *= r);
}

inline FloatVector8 FloatVector8::operator/(const float& r) const
{
  FloatVector8  tmp(*this);
  return (tmp /= r);
}

inline FloatVector8& FloatVector8::shuffleValues(unsigned char mask)
{
  v = __builtin_ia32_shufps256(v, v, mask);
  return (*this);
}

inline FloatVector8& FloatVector8::blendValues(
    const FloatVector8& r, unsigned char mask)
{
  v = __builtin_ia32_blendps256(v, r.v, mask);
  return (*this);
}

inline FloatVector8& FloatVector8::minValues(const FloatVector8& r)
{
  __asm__ ("vminps %t1, %t0, %t0" : "+x" (v) : "xm" (r.v));
  return (*this);
}

inline FloatVector8& FloatVector8::maxValues(const FloatVector8& r)
{
  __asm__ ("vmaxps %t1, %t0, %t0" : "+x" (v) : "xm" (r.v));
  return (*this);
}

inline FloatVector8& FloatVector8::floorValues()
{
  __asm__ ("vroundps $0x09, %t0, %t0" : "+x" (v));
  return (*this);
}

inline FloatVector8& FloatVector8::ceilValues()
{
  __asm__ ("vroundps $0x0a, %t0, %t0" : "+x" (v));
  return (*this);
}

inline FloatVector8& FloatVector8::roundValues()
{
  __asm__ ("vroundps $0x08, %t0, %t0" : "+x" (v));
  return (*this);
}

inline float FloatVector8::dotProduct(const FloatVector8& r) const
{
  YMM_Float tmp1;
  __asm__ ("vmulps %t2, %t1, %t0" : "=x" (tmp1) : "x" (v), "xm" (r.v));
  XMM_Float tmp2;
  __asm__ ("vextractf128 $0x01, %t1, %0" : "=x" (tmp2) : "x" (tmp1));
  __asm__ ("vaddps %1, %x0, %x0" : "+x" (tmp1) : "x" (tmp2));
  __asm__ ("vmovshdup %x1, %0" : "=x" (tmp2) : "x" (tmp1));
  __asm__ ("vaddps %1, %x0, %x0" : "+x" (tmp1) : "x" (tmp2));
  __asm__ ("vshufps $0x4e, %x1, %x1, %0" : "=x" (tmp2) : "x" (tmp1));
  __asm__ ("vaddps %1, %x0, %x0" : "+x" (tmp1) : "x" (tmp2));
  return tmp1[0];
}

inline FloatVector8& FloatVector8::squareRoot()
{
  YMM_Float tmp = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  __asm__ ("vmaxps %t1, %t0, %t0" : "+x" (tmp) : "x" (v));
  __asm__ ("vsqrtps %t1, %t0" : "=x" (v) : "x" (tmp));
  return (*this);
}

inline FloatVector8& FloatVector8::squareRootFast()
{
  YMM_Float tmp1 = v;
  __asm__ ("vmaxps %t1, %t0, %t0" : "+x" (tmp1) : "xm" (floatMinValV));
  __asm__ ("vrsqrtps %t1, %t0" : "=x" (v) : "x" (tmp1));
  v *= tmp1;
  return (*this);
}

inline FloatVector8& FloatVector8::rsqrtFast()
{
  __asm__ ("vrsqrtps %t0, %t0" : "+x" (v));
  return (*this);
}

inline FloatVector8& FloatVector8::rcpFast()
{
  __asm__ ("vrcpps %t0, %t0" : "+x" (v));
  return (*this);
}

inline FloatVector8& FloatVector8::rcpSqr()
{
  YMM_Float tmp1(v * v);
  YMM_Float tmp2;
  __asm__ ("vrcpps %t1, %t0" : "=x" (tmp2) : "x" (tmp1));
  v = (2.0f - tmp1 * tmp2) * tmp2;
  return (*this);
}

inline FloatVector8& FloatVector8::log2V()
{
#if ENABLE_X86_64_AVX2
  YMM_Float e, m, tmp;
  __asm__ ("vpsrld $0x17, %t1, %t0" : "=x" (e) : "x" (v));
  __asm__ ("vmovd %1, %x0" : "=x" (tmp) : "r" (0x3F800000U));
  __asm__ ("vpslld $0x17, %t1, %t0" : "=x" (m) : "x" (e));
  __asm__ ("vcvtdq2ps %t0, %t0" : "+x" (e));
  __asm__ ("vpbroadcastd %x0, %t0" : "+x" (tmp));
  __asm__ ("vpxor %t1, %t0, %t0" : "+x" (m) : "x" (v));
  __asm__ ("vpor %t1, %t0, %t0" : "+x" (m) : "x" (tmp));
  YMM_Float m2 = m * m;
  tmp = m * 4.05608897f - (127.0f + 2.50847106f);
  tmp = tmp + (((m2 * -0.08021013f) + (m * 0.63728021f) - 2.10465275f) * m2);
  v = tmp + e;
#else
  FloatVector4  tmp1(v[0], v[1], v[2], v[3]);
  FloatVector4  tmp2(v[4], v[5], v[6], v[7]);
  v = FloatVector8(tmp1.log2V(), tmp2.log2V()).v;
#endif
  return (*this);
}

inline FloatVector8& FloatVector8::exp2V()
{
#if ENABLE_X86_64_AVX2
  YMM_Float e;
  __asm__ ("vroundps $0x09, %t1, %t0" : "=x" (e) : "x" (v));
  YMM_Float m = v - e;          // e = floor(v)
  __asm__ ("vcvtps2dq %t0, %t0" : "+x" (e));
  __asm__ ("vpslld $0x17, %t0, %t0" : "+x" (e));
  m = (m * 0.00825060f + 0.05924474f) * (m * m) + (m * 0.34671664f + 1.0f);
  m = m * m;
  __asm__ ("vpaddd %t2, %t1, %t0" : "=x" (v) : "x" (m), "x" (e));
#else
  FloatVector4  tmp1(v[0], v[1], v[2], v[3]);
  FloatVector4  tmp2(v[4], v[5], v[6], v[7]);
  v = FloatVector8(tmp1.exp2V(), tmp2.exp2V()).v;
#endif
  return (*this);
}

inline FloatVector8::operator std::uint64_t() const
{
  std::uint64_t c;
#if ENABLE_X86_64_AVX2
  YMM_UInt32  tmp;
  __asm__ ("vcvtps2dq %t1, %t0" : "=x" (tmp) : "xm" (v));
  __asm__ ("vpackssdw %t0, %t0, %t0" : "+x" (tmp));
  __asm__ ("vpackuswb %x0, %x0, %x0" : "+x" (tmp));
  __asm__ ("vmovq %x1, %0" : "=r" (c) : "x" (tmp));
#else
  XMM_Float tmp1, tmp2;
  __asm__ ("vextractf128 $0x01, %t1, %0" : "=x" (tmp2) : "x" (v));
  __asm__ ("vcvtps2dq %x1, %0" : "=x" (tmp1) : "x" (v));
  __asm__ ("vcvtps2dq %0, %0" : "+x" (tmp2));
  __asm__ ("vpackssdw %1, %0, %0" : "+x" (tmp1) : "x" (tmp2));
  __asm__ ("vpackuswb %0, %0, %0" : "+x" (tmp1));
  __asm__ ("vmovq %1, %0" : "=r" (c) : "x" (tmp1));
#endif
  return c;
}

#else                                   // !ENABLE_X86_64_AVX

inline FloatVector8::FloatVector8(const float& x)
{
  v[0] = x;
  v[1] = x;
  v[2] = x;
  v[3] = x;
  v[4] = x;
  v[5] = x;
  v[6] = x;
  v[7] = x;
}

inline FloatVector8::FloatVector8(const std::uint64_t *p)
{
  std::uint64_t tmp = *p;
  v[0] = float(int(tmp & 0xFF));
  v[1] = float(int((tmp >> 8) & 0xFF));
  v[2] = float(int((tmp >> 16) & 0xFF));
  v[3] = float(int((tmp >> 24) & 0xFF));
  v[4] = float(int((tmp >> 32) & 0xFF));
  v[5] = float(int((tmp >> 40) & 0xFF));
  v[6] = float(int((tmp >> 48) & 0xFF));
  v[7] = float(int((tmp >> 56) & 0xFF));
}

inline FloatVector8::FloatVector8(const FloatVector4 *p)
{
  v[0] = p[0][0];
  v[1] = p[0][1];
  v[2] = p[0][2];
  v[3] = p[0][3];
  v[4] = p[1][0];
  v[5] = p[1][1];
  v[6] = p[1][2];
  v[7] = p[1][3];
}

inline FloatVector8::FloatVector8(FloatVector4 v0, FloatVector4 v1)
{
  v[0] = v0[0];
  v[1] = v0[1];
  v[2] = v0[2];
  v[3] = v0[3];
  v[4] = v1[0];
  v[5] = v1[1];
  v[6] = v1[2];
  v[7] = v1[3];
}

inline FloatVector8::FloatVector8(float v0, float v1, float v2, float v3,
                                  float v4, float v5, float v6, float v7)
{
  v[0] = v0;
  v[1] = v1;
  v[2] = v2;
  v[3] = v3;
  v[4] = v4;
  v[5] = v5;
  v[6] = v6;
  v[7] = v7;
}

inline FloatVector8::FloatVector8(const float *p)
{
  v[0] = p[0];
  v[1] = p[1];
  v[2] = p[2];
  v[3] = p[3];
  v[4] = p[4];
  v[5] = p[5];
  v[6] = p[6];
  v[7] = p[7];
}

inline FloatVector8::FloatVector8(const std::int16_t *p)
{
  v[0] = float(p[0]);
  v[1] = float(p[1]);
  v[2] = float(p[2]);
  v[3] = float(p[3]);
  v[4] = float(p[4]);
  v[5] = float(p[5]);
  v[6] = float(p[6]);
  v[7] = float(p[7]);
}

inline FloatVector8::FloatVector8(const std::int32_t *p)
{
  v[0] = float(p[0]);
  v[1] = float(p[1]);
  v[2] = float(p[2]);
  v[3] = float(p[3]);
  v[4] = float(p[4]);
  v[5] = float(p[5]);
  v[6] = float(p[6]);
  v[7] = float(p[7]);
}

inline FloatVector8::FloatVector8(const std::uint16_t *p, bool noInfNaN)
{
  (void) noInfNaN;
  v[0] = ::convertFloat16(p[0]);
  v[1] = ::convertFloat16(p[1]);
  v[2] = ::convertFloat16(p[2]);
  v[3] = ::convertFloat16(p[3]);
  v[4] = ::convertFloat16(p[4]);
  v[5] = ::convertFloat16(p[5]);
  v[6] = ::convertFloat16(p[6]);
  v[7] = ::convertFloat16(p[7]);
}

inline void FloatVector8::convertToFloats(float *p) const
{
  p[0] = v[0];
  p[1] = v[1];
  p[2] = v[2];
  p[3] = v[3];
  p[4] = v[4];
  p[5] = v[5];
  p[6] = v[6];
  p[7] = v[7];
}

inline void FloatVector8::convertToFloatVector4(FloatVector4 *p) const
{
  p[0][0] = v[0];
  p[0][1] = v[1];
  p[0][2] = v[2];
  p[0][3] = v[3];
  p[1][0] = v[4];
  p[1][1] = v[5];
  p[1][2] = v[6];
  p[1][3] = v[7];
}

inline void FloatVector8::convertToInt16(std::int16_t *p) const
{
  p[0] = std::int16_t(std::min< int >(std::max< int >(roundFloat(v[0]), -32768),
                                      32767));
  p[1] = std::int16_t(std::min< int >(std::max< int >(roundFloat(v[1]), -32768),
                                      32767));
  p[2] = std::int16_t(std::min< int >(std::max< int >(roundFloat(v[2]), -32768),
                                      32767));
  p[3] = std::int16_t(std::min< int >(std::max< int >(roundFloat(v[3]), -32768),
                                      32767));
  p[4] = std::int16_t(std::min< int >(std::max< int >(roundFloat(v[4]), -32768),
                                      32767));
  p[5] = std::int16_t(std::min< int >(std::max< int >(roundFloat(v[5]), -32768),
                                      32767));
  p[6] = std::int16_t(std::min< int >(std::max< int >(roundFloat(v[6]), -32768),
                                      32767));
  p[7] = std::int16_t(std::min< int >(std::max< int >(roundFloat(v[7]), -32768),
                                      32767));
}

inline void FloatVector8::convertToInt32(std::int32_t *p) const
{
  p[0] = std::int32_t(roundFloat(v[0]));
  p[1] = std::int32_t(roundFloat(v[1]));
  p[2] = std::int32_t(roundFloat(v[2]));
  p[3] = std::int32_t(roundFloat(v[3]));
  p[4] = std::int32_t(roundFloat(v[4]));
  p[5] = std::int32_t(roundFloat(v[5]));
  p[6] = std::int32_t(roundFloat(v[6]));
  p[7] = std::int32_t(roundFloat(v[7]));
}

inline void FloatVector8::convertToFloat16(std::uint16_t *p) const
{
  p[0] = ::convertToFloat16(v[0]);
  p[1] = ::convertToFloat16(v[1]);
  p[2] = ::convertToFloat16(v[2]);
  p[3] = ::convertToFloat16(v[3]);
  p[4] = ::convertToFloat16(v[4]);
  p[5] = ::convertToFloat16(v[5]);
  p[6] = ::convertToFloat16(v[6]);
  p[7] = ::convertToFloat16(v[7]);
}

inline FloatVector8& FloatVector8::operator+=(const FloatVector8& r)
{
  v[0] += r.v[0];
  v[1] += r.v[1];
  v[2] += r.v[2];
  v[3] += r.v[3];
  v[4] += r.v[4];
  v[5] += r.v[5];
  v[6] += r.v[6];
  v[7] += r.v[7];
  return (*this);
}

inline FloatVector8& FloatVector8::operator-=(const FloatVector8& r)
{
  v[0] -= r.v[0];
  v[1] -= r.v[1];
  v[2] -= r.v[2];
  v[3] -= r.v[3];
  v[4] -= r.v[4];
  v[5] -= r.v[5];
  v[6] -= r.v[6];
  v[7] -= r.v[7];
  return (*this);
}

inline FloatVector8& FloatVector8::operator*=(const FloatVector8& r)
{
  v[0] *= r.v[0];
  v[1] *= r.v[1];
  v[2] *= r.v[2];
  v[3] *= r.v[3];
  v[4] *= r.v[4];
  v[5] *= r.v[5];
  v[6] *= r.v[6];
  v[7] *= r.v[7];
  return (*this);
}

inline FloatVector8& FloatVector8::operator/=(const FloatVector8& r)
{
  v[0] /= r.v[0];
  v[1] /= r.v[1];
  v[2] /= r.v[2];
  v[3] /= r.v[3];
  v[4] /= r.v[4];
  v[5] /= r.v[5];
  v[6] /= r.v[6];
  v[7] /= r.v[7];
  return (*this);
}

inline FloatVector8 FloatVector8::operator+(const FloatVector8& r) const
{
  return FloatVector8(v[0] + r.v[0], v[1] + r.v[1],
                      v[2] + r.v[2], v[3] + r.v[3],
                      v[4] + r.v[4], v[5] + r.v[5],
                      v[6] + r.v[6], v[7] + r.v[7]);
}

inline FloatVector8 FloatVector8::operator-(const FloatVector8& r) const
{
  return FloatVector8(v[0] - r.v[0], v[1] - r.v[1],
                      v[2] - r.v[2], v[3] - r.v[3],
                      v[4] - r.v[4], v[5] - r.v[5],
                      v[6] - r.v[6], v[7] - r.v[7]);
}

inline FloatVector8 FloatVector8::operator*(const FloatVector8& r) const
{
  return FloatVector8(v[0] * r.v[0], v[1] * r.v[1],
                      v[2] * r.v[2], v[3] * r.v[3],
                      v[4] * r.v[4], v[5] * r.v[5],
                      v[6] * r.v[6], v[7] * r.v[7]);
}

inline FloatVector8 FloatVector8::operator/(const FloatVector8& r) const
{
  return FloatVector8(v[0] / r.v[0], v[1] / r.v[1],
                      v[2] / r.v[2], v[3] / r.v[3],
                      v[4] / r.v[4], v[5] / r.v[5],
                      v[6] / r.v[6], v[7] / r.v[7]);
}

inline FloatVector8& FloatVector8::operator+=(float r)
{
  v[0] += r;
  v[1] += r;
  v[2] += r;
  v[3] += r;
  v[4] += r;
  v[5] += r;
  v[6] += r;
  v[7] += r;
  return (*this);
}

inline FloatVector8& FloatVector8::operator-=(float r)
{
  v[0] -= r;
  v[1] -= r;
  v[2] -= r;
  v[3] -= r;
  v[4] -= r;
  v[5] -= r;
  v[6] -= r;
  v[7] -= r;
  return (*this);
}

inline FloatVector8& FloatVector8::operator*=(float r)
{
  v[0] *= r;
  v[1] *= r;
  v[2] *= r;
  v[3] *= r;
  v[4] *= r;
  v[5] *= r;
  v[6] *= r;
  v[7] *= r;
  return (*this);
}

inline FloatVector8& FloatVector8::operator/=(float r)
{
  v[0] /= r;
  v[1] /= r;
  v[2] /= r;
  v[3] /= r;
  v[4] /= r;
  v[5] /= r;
  v[6] /= r;
  v[7] /= r;
  return (*this);
}

inline FloatVector8 FloatVector8::operator+(const float& r) const
{
  return FloatVector8(v[0] + r, v[1] + r, v[2] + r, v[3] + r,
                      v[4] + r, v[5] + r, v[6] + r, v[7] + r);
}

inline FloatVector8 FloatVector8::operator-(const float& r) const
{
  return FloatVector8(v[0] - r, v[1] - r, v[2] - r, v[3] - r,
                      v[4] - r, v[5] - r, v[6] - r, v[7] - r);
}

inline FloatVector8 FloatVector8::operator*(const float& r) const
{
  return FloatVector8(v[0] * r, v[1] * r, v[2] * r, v[3] * r,
                      v[4] * r, v[5] * r, v[6] * r, v[7] * r);
}

inline FloatVector8 FloatVector8::operator/(const float& r) const
{
  return FloatVector8(v[0] / r, v[1] / r, v[2] / r, v[3] / r,
                      v[4] / r, v[5] / r, v[6] / r, v[7] / r);
}

inline FloatVector8& FloatVector8::shuffleValues(unsigned char mask)
{
  FloatVector8  tmp(*this);
  v[0] = tmp.v[mask & 3];
  v[1] = tmp.v[(mask >> 2) & 3];
  v[2] = tmp.v[(mask >> 4) & 3];
  v[3] = tmp.v[(mask >> 6) & 3];
  v[4] = tmp.v[(mask & 3) | 4];
  v[5] = tmp.v[((mask >> 2) & 3) | 4];
  v[6] = tmp.v[((mask >> 4) & 3) | 4];
  v[7] = tmp.v[((mask >> 6) & 3) | 4];
  return (*this);
}

inline FloatVector8& FloatVector8::blendValues(
    const FloatVector8& r, unsigned char mask)
{
  v[0] = (!(mask & 0x01) ? v[0] : r.v[0]);
  v[1] = (!(mask & 0x02) ? v[1] : r.v[1]);
  v[2] = (!(mask & 0x04) ? v[2] : r.v[2]);
  v[3] = (!(mask & 0x08) ? v[3] : r.v[3]);
  v[4] = (!(mask & 0x10) ? v[4] : r.v[4]);
  v[5] = (!(mask & 0x20) ? v[5] : r.v[5]);
  v[6] = (!(mask & 0x40) ? v[6] : r.v[6]);
  v[7] = (!(mask & 0x80) ? v[7] : r.v[7]);
  return (*this);
}

inline FloatVector8& FloatVector8::minValues(const FloatVector8& r)
{
  v[0] = (r.v[0] < v[0] ? r.v[0] : v[0]);
  v[1] = (r.v[1] < v[1] ? r.v[1] : v[1]);
  v[2] = (r.v[2] < v[2] ? r.v[2] : v[2]);
  v[3] = (r.v[3] < v[3] ? r.v[3] : v[3]);
  v[4] = (r.v[4] < v[4] ? r.v[4] : v[4]);
  v[5] = (r.v[5] < v[5] ? r.v[5] : v[5]);
  v[6] = (r.v[6] < v[6] ? r.v[6] : v[6]);
  v[7] = (r.v[7] < v[7] ? r.v[7] : v[7]);
  return (*this);
}

inline FloatVector8& FloatVector8::maxValues(const FloatVector8& r)
{
  v[0] = (r.v[0] > v[0] ? r.v[0] : v[0]);
  v[1] = (r.v[1] > v[1] ? r.v[1] : v[1]);
  v[2] = (r.v[2] > v[2] ? r.v[2] : v[2]);
  v[3] = (r.v[3] > v[3] ? r.v[3] : v[3]);
  v[4] = (r.v[4] > v[4] ? r.v[4] : v[4]);
  v[5] = (r.v[5] > v[5] ? r.v[5] : v[5]);
  v[6] = (r.v[6] > v[6] ? r.v[6] : v[6]);
  v[7] = (r.v[7] > v[7] ? r.v[7] : v[7]);
  return (*this);
}

inline FloatVector8& FloatVector8::floorValues()
{
  v[0] = float(std::floor(v[0]));
  v[1] = float(std::floor(v[1]));
  v[2] = float(std::floor(v[2]));
  v[3] = float(std::floor(v[3]));
  v[4] = float(std::floor(v[4]));
  v[5] = float(std::floor(v[5]));
  v[6] = float(std::floor(v[6]));
  v[7] = float(std::floor(v[7]));
  return (*this);
}

inline FloatVector8& FloatVector8::ceilValues()
{
  v[0] = float(std::ceil(v[0]));
  v[1] = float(std::ceil(v[1]));
  v[2] = float(std::ceil(v[2]));
  v[3] = float(std::ceil(v[3]));
  v[4] = float(std::ceil(v[4]));
  v[5] = float(std::ceil(v[5]));
  v[6] = float(std::ceil(v[6]));
  v[7] = float(std::ceil(v[7]));
  return (*this);
}

inline FloatVector8& FloatVector8::roundValues()
{
  v[0] = float(std::round(v[0]));
  v[1] = float(std::round(v[1]));
  v[2] = float(std::round(v[2]));
  v[3] = float(std::round(v[3]));
  v[4] = float(std::round(v[4]));
  v[5] = float(std::round(v[5]));
  v[6] = float(std::round(v[6]));
  v[7] = float(std::round(v[7]));
  return (*this);
}

inline float FloatVector8::dotProduct(const FloatVector8& r) const
{
  FloatVector8  tmp(*this);
  tmp *= r;
  return (tmp[0] + tmp[1] + tmp[2] + tmp[3]
          + tmp[4] + tmp[5] + tmp[6] + tmp[7]);
}

inline FloatVector8& FloatVector8::squareRoot()
{
  maxValues(FloatVector8(0.0f));
  v[0] = float(std::sqrt(v[0]));
  v[1] = float(std::sqrt(v[1]));
  v[2] = float(std::sqrt(v[2]));
  v[3] = float(std::sqrt(v[3]));
  v[4] = float(std::sqrt(v[4]));
  v[5] = float(std::sqrt(v[5]));
  v[6] = float(std::sqrt(v[6]));
  v[7] = float(std::sqrt(v[7]));
  return (*this);
}

inline FloatVector8& FloatVector8::squareRootFast()
{
  maxValues(FloatVector8(0.0f));
  v[0] = float(std::sqrt(v[0]));
  v[1] = float(std::sqrt(v[1]));
  v[2] = float(std::sqrt(v[2]));
  v[3] = float(std::sqrt(v[3]));
  v[4] = float(std::sqrt(v[4]));
  v[5] = float(std::sqrt(v[5]));
  v[6] = float(std::sqrt(v[6]));
  v[7] = float(std::sqrt(v[7]));
  return (*this);
}

inline FloatVector8& FloatVector8::rsqrtFast()
{
  v[0] = 1.0f / float(std::sqrt(v[0]));
  v[1] = 1.0f / float(std::sqrt(v[1]));
  v[2] = 1.0f / float(std::sqrt(v[2]));
  v[3] = 1.0f / float(std::sqrt(v[3]));
  v[4] = 1.0f / float(std::sqrt(v[4]));
  v[5] = 1.0f / float(std::sqrt(v[5]));
  v[6] = 1.0f / float(std::sqrt(v[6]));
  v[7] = 1.0f / float(std::sqrt(v[7]));
  return (*this);
}

inline FloatVector8& FloatVector8::rcpFast()
{
  v[0] = 1.0f / v[0];
  v[1] = 1.0f / v[1];
  v[2] = 1.0f / v[2];
  v[3] = 1.0f / v[3];
  v[4] = 1.0f / v[4];
  v[5] = 1.0f / v[5];
  v[6] = 1.0f / v[6];
  v[7] = 1.0f / v[7];
  return (*this);
}

inline FloatVector8& FloatVector8::rcpSqr()
{
  v[0] = 1.0f / (v[0] * v[0]);
  v[1] = 1.0f / (v[1] * v[1]);
  v[2] = 1.0f / (v[2] * v[2]);
  v[3] = 1.0f / (v[3] * v[3]);
  v[4] = 1.0f / (v[4] * v[4]);
  v[5] = 1.0f / (v[5] * v[5]);
  v[6] = 1.0f / (v[6] * v[6]);
  v[7] = 1.0f / (v[7] * v[7]);
  return (*this);
}

inline FloatVector8& FloatVector8::log2V()
{
  v[0] = float(std::log2(v[0]));
  v[1] = float(std::log2(v[1]));
  v[2] = float(std::log2(v[2]));
  v[3] = float(std::log2(v[3]));
  v[4] = float(std::log2(v[4]));
  v[5] = float(std::log2(v[5]));
  v[6] = float(std::log2(v[6]));
  v[7] = float(std::log2(v[7]));
  return (*this);
}

inline FloatVector8& FloatVector8::exp2V()
{
  v[0] = float(std::exp2(v[0]));
  v[1] = float(std::exp2(v[1]));
  v[2] = float(std::exp2(v[2]));
  v[3] = float(std::exp2(v[3]));
  v[4] = float(std::exp2(v[4]));
  v[5] = float(std::exp2(v[5]));
  v[6] = float(std::exp2(v[6]));
  v[7] = float(std::exp2(v[7]));
  return (*this);
}

inline FloatVector8::operator std::uint64_t() const
{
  std::uint64_t c0 = floatToUInt8Clamped(v[0], 1.0f);
  std::uint64_t c1 = floatToUInt8Clamped(v[1], 1.0f);
  std::uint64_t c2 = floatToUInt8Clamped(v[2], 1.0f);
  std::uint64_t c3 = floatToUInt8Clamped(v[3], 1.0f);
  std::uint64_t c4 = floatToUInt8Clamped(v[4], 1.0f);
  std::uint64_t c5 = floatToUInt8Clamped(v[5], 1.0f);
  std::uint64_t c6 = floatToUInt8Clamped(v[6], 1.0f);
  std::uint64_t c7 = floatToUInt8Clamped(v[7], 1.0f);
  return (c0 | (c1 << 8) | (c2 << 16) | (c3 << 24)
          | (c4 << 32) | (c5 << 40) | (c6 << 48) | (c7 << 56));
}

#endif                                  // ENABLE_X86_64_AVX

inline std::uint32_t FloatVector8::getSignMask() const
{
  std::uint32_t tmp;
#if ENABLE_X86_64_AVX
  __asm__ ("vmovmskps %t1, %0" : "=r" (tmp) : "x" (v));
#else
  tmp = std::uint32_t(v[0] < 0.0f) | (std::uint32_t(v[1] < 0.0f) << 1)
        | (std::uint32_t(v[2] < 0.0f) << 2) | (std::uint32_t(v[3] < 0.0f) << 3)
        | (std::uint32_t(v[4] < 0.0f) << 4) | (std::uint32_t(v[5] < 0.0f) << 5)
        | (std::uint32_t(v[6] < 0.0f) << 6) | (std::uint32_t(v[7] < 0.0f) << 7);
#endif
  return tmp;
}

#endif

