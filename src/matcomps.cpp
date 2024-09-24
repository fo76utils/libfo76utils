
#include "common.hpp"
#include "material.hpp"

CE2Material::EffectSettings::EffectSettings()
  : flags(CE2Material::EffectFlag_ZTest),
    blendMode(0),               // "AlphaBlend"
    falloffStartAngle(0.0f),
    falloffStopAngle(0.0f),
    falloffStartOpacity(0.0f),
    falloffStopOpacity(0.0f),
    alphaThreshold(0.5f),
    softFalloffDepth(2.0f),
    frostingBgndBlend(0.98f),
    frostingBlurBias(0.0f),
    materialAlpha(1.0f),
    backlightScale(0.0f),
    backlightSharpness(8.0f),
    backlightTransparency(0.0f),
    backlightTintColor(FloatVector4(1.0f)),
    depthBias(0)
{
}

CE2Material::EmissiveSettings::EmissiveSettings()
  : isEnabled(false),
    sourceLayer(0),             // "MATERIAL_LAYER_0"
    maskSourceBlender(0),       // "None"
    adaptiveEmittance(false),
    enableAdaptiveLimits(false),
    clipThreshold(0.0f),
    luminousEmittance(100.0f),
    emissiveTint(FloatVector4(1.0f)),
    exposureOffset(0.0f),
    maxOffset(9999.0f),
    minOffset(0.0f)
{
}

CE2Material::LayeredEmissiveSettings::LayeredEmissiveSettings()
  : isEnabled(false),
    layer1Index(0),             // "MATERIAL_LAYER_0"
    layer1MaskIndex(0),         // "None"
    layer2Active(false),
    layer2Index(1),             // "MATERIAL_LAYER_1"
    layer2MaskIndex(0),         // "None"
    blender1Index(0),           // "BLEND_LAYER_0"
    blender1Mode(0),            // "Lerp"
    layer3Active(false),
    layer3Index(2),             // "MATERIAL_LAYER_2"
    layer3MaskIndex(0),         // "None"
    blender2Index(1),           // "BLEND_LAYER_1"
    blender2Mode(0),            // "Lerp"
    adaptiveEmittance(false),
    enableAdaptiveLimits(false),
    ignoresFog(false),
    layer1Tint(0xFFFFFFFFU),
    layer2Tint(0xFFFFFFFFU),
    layer3Tint(0xFFFFFFFFU),
    clipThreshold(0.0f),
    luminousEmittance(100.0f),
    exposureOffset(0.0f),
    maxOffset(1.0f),
    minOffset(0.0f)
{
}

CE2Material::TranslucencySettings::TranslucencySettings()
  : isEnabled(false),
    isThin(false),
    flipBackFaceNormalsInVS(false),
    useSSS(false),
    sssWidth(0.025f),
    sssStrength(0.5f),
    transmissiveScale(0.05f),
    transmittanceWidth(0.03f),
    specLobe0RoughnessScale(0.55f),
    specLobe1RoughnessScale(1.2f),
    sourceLayer(0)              // "MATERIAL_LAYER_0"
{
}

CE2Material::DecalSettings::DecalSettings()
  : isDecal(false),
    isPlanet(false),
    blendMode(0),               // "None"
    animatedDecalIgnoresTAA(false),
    decalAlpha(1.0f),
    writeMask(0x0737U),
    isProjected(false),
    useParallaxMapping(false),
    parallaxOcclusionShadows(false),
    maxParallaxSteps(72),
    surfaceHeightMap(&emptyStringView),
    parallaxOcclusionScale(1.0f),
    renderLayer(2),             // "Bottom"
    useGBufferNormals(true)
{
}

CE2Material::VegetationSettings::VegetationSettings()
  : isEnabled(false),
    leafFrequency(3.69f),
    leafAmplitude(0.068f),
    branchFlexibility(0.03f),
    trunkFlexibility(4.0f),
    terrainBlendStrength(0.0f),
    terrainBlendGradientFactor(0.0f)
{
}

CE2Material::DetailBlenderSettings::DetailBlenderSettings()
  : isEnabled(false),
    textureReplacementEnabled(false),
    textureReplacement(0xFFFFFFFFU),
    texturePath(&emptyStringView),
    uvStream(nullptr)
{
}

CE2Material::LayeredEdgeFalloff::LayeredEdgeFalloff()
  : activeLayersMask(0),
    useRGBFalloff(false)
{
  falloffStartAngles[0] = 0.0f;
  falloffStartAngles[1] = 0.0f;
  falloffStartAngles[2] = 0.0f;
  falloffStopAngles[0] = 0.0f;
  falloffStopAngles[1] = 0.0f;
  falloffStopAngles[2] = 0.0f;
  falloffStartOpacities[0] = 0.0f;
  falloffStartOpacities[1] = 0.0f;
  falloffStartOpacities[2] = 0.0f;
  falloffStopOpacities[0] = 0.0f;
  falloffStopOpacities[1] = 0.0f;
  falloffStopOpacities[2] = 0.0f;
}

CE2Material::WaterSettings::WaterSettings()
  : waterEdgeFalloff(0.1f),
    waterWetnessMaxDepth(0.05f),
    waterEdgeNormalFalloff(3.0f),
    waterDepthBlur(0.0f),
    reflectance(FloatVector4(0.0f, 0.003f, 0.004f, 0.1199f)),
    phytoplanktonReflectance(FloatVector4(-0.0001f, 0.002f, 0.0003f, 0.04f)),
    sedimentReflectance(FloatVector4(0.0001f, 0.0001f, 0.0f, 0.14f)),
    yellowMatterReflectance(FloatVector4(0.0007f, 0.0f, -0.0001f, 0.04f)),
    lowLOD(false),
    placedWater(false)
{
}

CE2Material::GlobalLayerData::GlobalLayerData()
  : texcoordScaleXY(0.25f),
    texcoordScaleYZ(0.25f),
    texcoordScaleXZ(0.25f),
    usesDirectionality(true),
    blendNormalsAdditively(true),
    useNoiseMaskTexture(false),
    noiseMaskTxtReplacementEnabled(false),
    albedoTintColor(FloatVector4(1.0f)),
    sourceDirection(FloatVector4(0.0f, 0.0f, 1.0f, 0.85f)),
    directionalityScale(2.0f),
    directionalitySaturation(1.3f),
    blendPosition(0.5f),
    blendContrast(0.5f),
    materialMaskIntensityScale(1.0f),
    noiseMaskTextureReplacement(0xFFFFFFFFU),
    noiseMaskTexture(&emptyStringView),
    texcoordScaleAndBias(FloatVector4(1.0f, 1.0f, 0.0f, 0.0f)),
    worldspaceScaleFactor(0.25f),
    hurstExponent(1.0f),
    baseFrequency(32.0f),
    frequencyMultiplier(2.0f),
    maskIntensityMin(0.0f),
    maskIntensityMax(1.0f)
{
}

CE2Material::HairSettings::HairSettings()
  : isEnabled(false),
    isSpikyHair(false),
    depthOffsetMaskVertexColorChannel(0),       // "Red"
    aoVertexColorChannel(1),                    // "Green"
    specScale(1.0f),
    specularTransmissionScale(0.5f),
    directTransmissionScale(0.33f),
    diffuseTransmissionScale(0.2f),
    roughness(0.38f),
    contactShadowSoftening(0.25f),
    backscatterStrength(0.5f),
    backscatterWrap(0.5f),
    variationStrength(0.2f),
    indirectSpecularScale(1.0f),
    indirectSpecularTransmissionScale(0.5f),
    indirectSpecRoughness(0.2f),
    edgeMaskContrast(0.7f),
    edgeMaskMin(0.75f),
    edgeMaskDistanceMin(0.5f),
    edgeMaskDistanceMax(5.0f),
    ditherScale(0.5f),
    ditherDistanceMin(0.5f),
    ditherDistanceMax(1.5f),
    tangent(0.0f, -1.0f, 0.0f, 0.3f),
    maxDepthOffset(0.075f)
{
}

inline bool CE2MaterialDB::ComponentInfo::readBool(
    bool& n, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_Bool)
  {
    n = p->children()[fieldNum]->boolValue();
    return true;
  }
  return false;
}

inline bool CE2MaterialDB::ComponentInfo::readUInt8(
    unsigned char& n, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_UInt8)
  {
    n = (unsigned char) p->children()[fieldNum]->uintValue();
    return true;
  }
  return false;
}

inline bool CE2MaterialDB::ComponentInfo::readUInt16(
    std::uint16_t& n, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_UInt16)
  {
    n = std::uint16_t(p->children()[fieldNum]->uintValue());
    return true;
  }
  return false;
}

inline bool CE2MaterialDB::ComponentInfo::readUInt32(
    std::uint32_t& n, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_UInt32)
  {
    n = std::uint32_t(p->children()[fieldNum]->uintValue());
    return true;
  }
  return false;
}

inline bool CE2MaterialDB::ComponentInfo::readFloat(
    float& n, const BSMaterialsCDB::CDBObject *p, size_t fieldNum,
    bool clampTo0To1)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_Float)
  {
    n = p->children()[fieldNum]->floatValue();
    if (clampTo0To1)
      n = std::min((n > 0.0f ? n : 0.0f), 1.0f);
    return true;
  }
  return false;
}

inline bool CE2MaterialDB::ComponentInfo::readString(
    const char*& s, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_String)
  {
    s = p->children()[fieldNum]->stringValue();
    return true;
  }
  return false;
}

bool CE2MaterialDB::ComponentInfo::readPath(
    const std::string_view*& s, const BSMaterialsCDB::CDBObject *p,
    size_t fieldNum, const char *prefix, const char *suffix)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_String)
  {
    const char  *t = p->children()[fieldNum]->stringValue();
    FileBuffer  tmpBuf(reinterpret_cast< const unsigned char * >(t),
                       std::strlen(t) + 1, 0);
    tmpBuf.readPath(cdb.stringBuf, std::string::npos, prefix, suffix);
    s = cdb.storeStdString(cdb.stringBuf);
    return true;
  }
  return false;
}

bool CE2MaterialDB::ComponentInfo::readEnum(
    unsigned char& n, const BSMaterialsCDB::CDBObject *p, size_t fieldNum,
    const char *t)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_String)
  {
    const char  *s = p->children()[fieldNum]->stringValue();
    size_t  len = std::strlen(s);
    for (size_t i = 0; *t; i++)
    {
      size_t  len2 = (unsigned char) *t;
      const char  *s2 = t + 1;
      t = s2 + len2;
      if (len2 != len)
        continue;
      for (size_t j = 0; len2 && s2[j] == s[j]; j++, len2--)
        ;
      if (!len2)
      {
        n = (unsigned char) i;
        return true;
      }
    }
  }
  return false;
}

bool CE2MaterialDB::ComponentInfo::readLayerNumber(
    unsigned char& n, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_String)
  {
    const char  *s = p->children()[fieldNum]->stringValue();
    if (std::strncmp(s, "MATERIAL_LAYER_", 15) == 0 &&
        s[15] >= '0' && s[15] < char(CE2Material::maxLayers + '0') &&
        s[16] == '\0')
    {
      n = (unsigned char) (s[15] - '0');
      return true;
    }
  }
  return false;
}

bool CE2MaterialDB::ComponentInfo::readBlenderNumber(
    unsigned char& n, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_String)
  {
    const char  *s = p->children()[fieldNum]->stringValue();
    if (std::strncmp(s, "BLEND_LAYER_", 12) == 0 &&
        s[12] >= '0' && s[12] < char(CE2Material::maxBlenders + '0') &&
        s[13] == '\0')
    {
      n = (unsigned char) (s[12] - '0');
      return true;
    }
  }
  return false;
}

// BSMaterial::LayeredEmissivityComponent
//   Bool  Enabled
//   String  FirstLayerIndex
//   BSMaterial::Color  FirstLayerTint
//   String  FirstLayerMaskIndex
//   Bool  SecondLayerActive
//   String  SecondLayerIndex
//   BSMaterial::Color  SecondLayerTint
//   String  SecondLayerMaskIndex
//   String  FirstBlenderIndex
//   String  FirstBlenderMode
//   Bool  ThirdLayerActive
//   String  ThirdLayerIndex
//   BSMaterial::Color  ThirdLayerTint
//   String  ThirdLayerMaskIndex
//   String  SecondBlenderIndex
//   String  SecondBlenderMode
//   Float  EmissiveClipThreshold
//   Bool  AdaptiveEmittance
//   Float  LuminousEmittance
//   Float  ExposureOffset
//   Bool  EnableAdaptiveLimits
//   Float  MaxOffsetEmittance
//   Float  MinOffsetEmittance
//   Bool  IgnoresFog

void CE2MaterialDB::ComponentInfo::readLayeredEmissivityComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::LayeredEmissiveSettings  *sp =
      cdb.constructObject< CE2Material::LayeredEmissiveSettings >();
  m->layeredEmissiveSettings = sp;
  bool    tmp;
  if (readBool(tmp, p, 0))
  {
    m->setFlags(CE2Material::Flag_LayeredEmissivity, tmp);
    sp->isEnabled = tmp;
  }
  readLayerNumber(sp->layer1Index, p, 1);
  readColorValue(sp->layer1Tint, p, 2);
  readEnum(sp->layer1MaskIndex, p, 3,
           "\004None\010Blender1\010Blender2\010Blender3");
  readBool(sp->layer2Active, p, 4);
  readLayerNumber(sp->layer2Index, p, 5);
  readColorValue(sp->layer2Tint, p, 6);
  readEnum(sp->layer2MaskIndex, p, 7,
           "\004None\010Blender1\010Blender2\010Blender3");
  readBlenderNumber(sp->blender1Index, p, 8);
  readEnum(sp->blender1Mode, p, 9,
           "\004Lerp\010Additive\013Subtractive\016Multiplicative");
  readBool(sp->layer3Active, p, 10);
  readLayerNumber(sp->layer3Index, p, 11);
  readColorValue(sp->layer3Tint, p, 12);
  readEnum(sp->layer3MaskIndex, p, 13,
           "\004None\010Blender1\010Blender2\010Blender3");
  readBlenderNumber(sp->blender2Index, p, 14);
  readEnum(sp->blender2Mode, p, 15,
           "\004Lerp\010Additive\013Subtractive\016Multiplicative");
  readFloat(sp->clipThreshold, p, 16);
  readBool(sp->adaptiveEmittance, p, 17);
  readFloat(sp->luminousEmittance, p, 18);
  readFloat(sp->exposureOffset, p, 19);
  readBool(sp->enableAdaptiveLimits, p, 20);
  readFloat(sp->maxOffset, p, 21);
  readFloat(sp->minOffset, p, 22);
  readBool(sp->ignoresFog, p, 23);
}

// BSMaterial::AlphaBlenderSettings
//   String  Mode
//   Bool  UseDetailBlendMask
//   Bool  UseVertexColor
//   String  VertexColorChannel
//   BSMaterial::UVStreamID  OpacityUVStream
//   Float  HeightBlendThreshold
//   Float  HeightBlendFactor
//   Float  Position
//   Float  Contrast

void CE2MaterialDB::ComponentInfo::readAlphaBlenderSettings(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  if (!(p && p->type == BSReflStream::String_BSMaterial_AlphaBlenderSettings))
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  readEnum(m->alphaBlendMode, p, 0,
           "\006Linear\010Additive\020PositionContrast\004None");
  bool    tmp;
  if (readBool(tmp, p, 1))
    m->setFlags(CE2Material::Flag_AlphaDetailBlendMask, tmp);
  if (readBool(tmp, p, 2))
    m->setFlags(CE2Material::Flag_AlphaVertexColor, tmp);
  readEnum(m->alphaVertexColorChannel, p, 3,
           "\003Red\005Green\004Blue\005Alpha");
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 5)
    readUVStreamID(p->children()[4]);
  readFloat(m->alphaHeightBlendThreshold, p, 5, true);
  readFloat(m->alphaHeightBlendFactor, p, 6, true);
  readFloat(m->alphaPosition, p, 7, true);
  readFloat(m->alphaContrast, p, 8, true);
}

// BSFloatCurve
//   List  Controls
//   Float  MaxInput
//   Float  MinInput
//   Float  InputDistance
//   Float  MaxValue
//   Float  MinValue
//   Float  DefaultValue
//   String  Type
//   String  Edge
//   Bool  IsSampleInterpolating

void CE2MaterialDB::ComponentInfo::readBSFloatCurve(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::EmissiveSettingsComponent
//   Bool  Enabled
//   BSMaterial::EmittanceSettings  Settings

void CE2MaterialDB::ComponentInfo::readEmissiveSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::EmissiveSettings *sp =
      cdb.constructObject< CE2Material::EmissiveSettings >();
  m->emissiveSettings = sp;
  bool    tmp;
  if (readBool(tmp, p, 0))
  {
    m->setFlags(CE2Material::Flag_Emissive, tmp);
    sp->isEnabled = tmp;
  }
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 2)
    readEmittanceSettings(p->children()[1]);
}

// BSMaterial::WaterFoamSettingsComponent
//   String  Mode
//   Float  MaskDistanceFromShoreStart
//   Float  MaskDistanceFromShoreEnd
//   Float  MaskDistanceRampWidth
//   Float  WaveShoreFadeInnerDistance
//   Float  WaveShoreFadeOuterDistance
//   Float  WaveSpawnFadeInDistance
//   XMFLOAT4  MaskNoiseAmp
//   XMFLOAT4  MaskNoiseFreq
//   XMFLOAT4  MaskNoiseBias
//   XMFLOAT4  MaskNoiseAnimSpeed
//   Float  MaskNoiseGlobalScale
//   Float  MaskWaveParallax
//   Float  BlendMaskPosition
//   Float  BlendMaskContrast
//   Float  FoamTextureScrollSpeed
//   Float  FoamTextureDistortion
//   Float  WaveSpeed
//   Float  WaveAmplitude
//   Float  WaveScale
//   Float  WaveDistortionAmount
//   Float  WaveParallaxFalloffBias
//   Float  WaveParallaxFalloffScale
//   Float  WaveParallaxInnerStrength
//   Float  WaveParallaxOuterStrength
//   Bool  WaveFlipWaveDirection

void CE2MaterialDB::ComponentInfo::readWaterFoamSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::FlipbookComponent
//   Bool  IsAFlipbook
//   UInt32  Columns
//   UInt32  Rows
//   Float  FPS
//   Bool  Loops

void CE2MaterialDB::ComponentInfo::readFlipbookComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 4) [[unlikely]]
    return;
  CE2Material::Material *m = static_cast< CE2Material::Material * >(o);
  bool    tmp;
  if (readBool(tmp, p, 0))
    m->flipbookFlags = (m->flipbookFlags & 2) | (unsigned char) tmp;
  std::uint32_t tmp2;
  if (readUInt32(tmp2, p, 1))
    m->flipbookColumns = (unsigned char) std::min< std::uint32_t >(tmp2, 255U);
  if (readUInt32(tmp2, p, 2))
    m->flipbookRows = (unsigned char) std::min< std::uint32_t >(tmp2, 255U);
  readFloat(m->flipbookFPS, p, 3);
  if (readBool(tmp, p, 4))
    m->flipbookFlags = (m->flipbookFlags & 1) | ((unsigned char) tmp << 1);
}

// BSMaterial::PhysicsMaterialType
//   UInt32  Value

void CE2MaterialDB::ComponentInfo::readPhysicsMaterialType(
    const BSMaterialsCDB::CDBObject *p)
{
  if (!(p && p->type == BSReflStream::String_BSMaterial_PhysicsMaterialType))
    return;
  if (o->type != 1) [[unlikely]]
    return;
  std::uint32_t tmp;
  if (!readUInt32(tmp, p, 0))
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  switch (tmp)
  {
    case 0x064003D4U:           // "metal"
      m->physicsMaterialType = 6;
      break;
    case 0x1DD9C611U:           // "wood"
      m->physicsMaterialType = 7;
      break;
    case 0x4BDC3571U:           // "materialphyicedebrislarge"
      m->physicsMaterialType = 5;
      break;
    case 0x6B81B7B0U:           // "mat"
      m->physicsMaterialType = 2;
      break;
    case 0x7A0EB611U:           // "materialmat"
      m->physicsMaterialType = 4;
      break;
    case 0xAD5ACB92U:           // "materialgroundtilevinyl"
      m->physicsMaterialType = 3;
      break;
    case 0xF0170989U:           // "carpet"
      m->physicsMaterialType = 1;
      break;
    default:
      m->physicsMaterialType = 0;
      break;
  }
}

// BSMaterial::TerrainTintSettingsComponent
//   Bool  Enabled
//   Float  TerrainBlendStrength
//   Float  TerrainBlendGradientFactor

void CE2MaterialDB::ComponentInfo::readTerrainTintSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::UVStreamID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readUVStreamID(
    const BSMaterialsCDB::CDBObject *p)
{
  const CE2MaterialObject *tmp;
  if (!readBSComponentDB2ID(tmp, p, 6))
    return;
  const CE2Material::UVStream *uvStream =
      static_cast< const CE2Material::UVStream * >(tmp);
  switch (componentData->className)
  {
    case BSReflStream::String_BSMaterial_UVStreamID:
      if (o->type == 2)
        static_cast< CE2Material::Blender * >(o)->uvStream = uvStream;
      else if (o->type == 3)
        static_cast< CE2Material::Layer * >(o)->uvStream = uvStream;
      break;
    case BSReflStream::String_BSMaterial_AlphaSettingsComponent:
      if (o->type == 1)
        static_cast< CE2Material * >(o)->alphaUVStream = uvStream;
      break;
    case BSReflStream::String_BSMaterial_DetailBlenderSettingsComponent:
      if (o->type == 1)
      {
        const_cast< CE2Material::DetailBlenderSettings * >(
            static_cast< CE2Material * >(o)->detailBlenderSettings)->uvStream =
                uvStream;
      }
      break;
  }
}

// BSMaterial::DecalSettingsComponent
//   Bool  IsDecal
//   Float  MaterialOverallAlpha
//   UInt32  WriteMask
//   Bool  IsPlanet
//   Bool  IsProjected
//   BSMaterial::ProjectedDecalSettings  ProjectedDecalSetting
//   String  BlendMode
//   Bool  AnimatedDecalIgnoresTAA

void CE2MaterialDB::ComponentInfo::readDecalSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::DecalSettings  *sp =
      cdb.constructObject< CE2Material::DecalSettings >();
  m->decalSettings = sp;
  bool    tmp;
  if (readBool(tmp, p, 0))
  {
    m->setFlags(CE2Material::Flag_IsDecal, tmp);
    if (tmp)
      m->setFlags(CE2Material::Flag_AlphaBlending, true);
    sp->isDecal = tmp;
  }
  readFloat(sp->decalAlpha, p, 1);
  readUInt32(sp->writeMask, p, 2);
  readBool(sp->isPlanet, p, 3);
  readBool(sp->isProjected, p, 4);
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 6)
    readProjectedDecalSettings(p->children()[5]);
  readEnum(sp->blendMode, p, 6, "\004None\010Additive");
  readBool(sp->animatedDecalIgnoresTAA, p, 7);
}

// BSBind::Directory
//   String  Name
//   Map  Children
//   UInt64  SourceDirectoryHash

void CE2MaterialDB::ComponentInfo::readDirectory(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::WaterSettingsComponent
//   Float  WaterEdgeFalloff
//   Float  WaterWetnessMaxDepth
//   Float  WaterEdgeNormalFalloff
//   Float  WaterDepthBlur
//   Float  WaterRefractionMagnitude
//   Float  PhytoplanktonReflectanceColorR
//   Float  PhytoplanktonReflectanceColorG
//   Float  PhytoplanktonReflectanceColorB
//   Float  SedimentReflectanceColorR
//   Float  SedimentReflectanceColorG
//   Float  SedimentReflectanceColorB
//   Float  YellowMatterReflectanceColorR
//   Float  YellowMatterReflectanceColorG
//   Float  YellowMatterReflectanceColorB
//   Float  MaxConcentrationPlankton
//   Float  MaxConcentrationSediment
//   Float  MaxConcentrationYellowMatter
//   Float  ReflectanceR
//   Float  ReflectanceG
//   Float  ReflectanceB
//   Bool  LowLOD
//   Bool  PlacedWater

void CE2MaterialDB::ComponentInfo::readWaterSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::WaterSettings  *sp =
      cdb.constructObject< CE2Material::WaterSettings >();
  m->setFlags(CE2Material::Flag_IsWater | CE2Material::Flag_AlphaBlending,
              true);
  m->waterSettings = sp;
  readFloat(sp->waterEdgeFalloff, p, 0);
  readFloat(sp->waterWetnessMaxDepth, p, 1);
  readFloat(sp->waterEdgeNormalFalloff, p, 2);
  readFloat(sp->waterDepthBlur, p, 3);
  readFloat(sp->reflectance[3], p, 4);
  readFloat(sp->phytoplanktonReflectance[0], p, 5);
  readFloat(sp->phytoplanktonReflectance[1], p, 6);
  readFloat(sp->phytoplanktonReflectance[2], p, 7);
  readFloat(sp->sedimentReflectance[0], p, 8);
  readFloat(sp->sedimentReflectance[1], p, 9);
  readFloat(sp->sedimentReflectance[2], p, 10);
  readFloat(sp->yellowMatterReflectance[0], p, 11);
  readFloat(sp->yellowMatterReflectance[1], p, 12);
  readFloat(sp->yellowMatterReflectance[2], p, 13);
  readFloat(sp->phytoplanktonReflectance[3], p, 14);
  readFloat(sp->sedimentReflectance[3], p, 15);
  readFloat(sp->yellowMatterReflectance[3], p, 16);
  readFloat(sp->reflectance[0], p, 17);
  readFloat(sp->reflectance[1], p, 18);
  readFloat(sp->reflectance[2], p, 19);
  readBool(sp->lowLOD, p, 20);
  readBool(sp->placedWater, p, 21);
}

// BSFloatCurve::Control
//   Float  Input
//   Float  Value

void CE2MaterialDB::ComponentInfo::readControl(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSBind::ComponentProperty
//   String  Name
//   BSComponentDB2::ID  Object
//   ClassReference  ComponentType
//   UInt16  ComponentIndex
//   String  PathStr

void CE2MaterialDB::ComponentInfo::readComponentProperty(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// XMFLOAT4
//   Float  x
//   Float  y
//   Float  z
//   Float  w

bool CE2MaterialDB::ComponentInfo::readXMFLOAT4(
    FloatVector4& v, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (!(p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt))
    return false;
  const BSMaterialsCDB::CDBObject *q = p->children()[fieldNum];
  if (!(q && q->type == BSReflStream::String_XMFLOAT4 && q->childCnt == 4))
    return false;
  bool    r = readFloat(v[0], q, 0);
  r = r | readFloat(v[1], q, 1);
  r = r | readFloat(v[2], q, 2);
  r = r | readFloat(v[3], q, 3);
  return r;
}

// BSMaterial::EffectSettingsComponent
//   Bool  UseFallOff
//   Bool  UseRGBFallOff
//   Float  FalloffStartAngle
//   Float  FalloffStopAngle
//   Float  FalloffStartOpacity
//   Float  FalloffStopOpacity
//   Bool  VertexColorBlend
//   Bool  IsAlphaTested
//   Float  AlphaTestThreshold
//   Bool  NoHalfResOptimization
//   Bool  SoftEffect
//   Float  SoftFalloffDepth
//   Bool  EmissiveOnlyEffect
//   Bool  EmissiveOnlyAutomaticallyApplied
//   Bool  ReceiveDirectionalShadows
//   Bool  ReceiveNonDirectionalShadows
//   Bool  IsGlass
//   Bool  Frosting
//   Float  FrostingUnblurredBackgroundAlphaBlend
//   Float  FrostingBlurBias
//   Float  MaterialOverallAlpha
//   Bool  ZTest
//   Bool  ZWrite
//   String  BlendingMode
//   Bool  BackLightingEnable
//   Float  BacklightingScale
//   Float  BacklightingSharpness
//   Float  BacklightingTransparencyFactor
//   BSMaterial::Color  BackLightingTintColor
//   Bool  DepthMVFixup
//   Bool  DepthMVFixupEdgesOnly
//   Bool  ForceRenderBeforeOIT
//   Bool  ForceRenderBeforeClouds (new in version 1.14.68.0)
//   UInt16  DepthBiasInUlp

void CE2MaterialDB::ComponentInfo::readEffectSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::EffectSettings *sp =
      cdb.constructObject< CE2Material::EffectSettings >();
  m->setFlags(CE2Material::Flag_IsEffect | CE2Material::Flag_AlphaBlending,
              true);
  m->effectSettings = sp;
  bool    tmp;
  if (readBool(tmp, p, 0))
    sp->setFlags(CE2Material::EffectFlag_UseFalloff, tmp);
  if (readBool(tmp, p, 1))
    sp->setFlags(CE2Material::EffectFlag_UseRGBFalloff, tmp);
  readFloat(sp->falloffStartAngle, p, 2);
  readFloat(sp->falloffStopAngle, p, 3);
  readFloat(sp->falloffStartOpacity, p, 4);
  readFloat(sp->falloffStopOpacity, p, 5);
  if (readBool(tmp, p, 6))
    sp->setFlags(CE2Material::EffectFlag_VertexColorBlend, tmp);
  if (readBool(tmp, p, 7))
    sp->setFlags(CE2Material::EffectFlag_IsAlphaTested, tmp);
  readFloat(sp->alphaThreshold, p, 8);
  if (readBool(tmp, p, 9))
    sp->setFlags(CE2Material::EffectFlag_NoHalfResOpt, tmp);
  if (readBool(tmp, p, 10))
    sp->setFlags(CE2Material::EffectFlag_SoftEffect, tmp);
  readFloat(sp->softFalloffDepth, p, 11);
  if (readBool(tmp, p, 12))
    sp->setFlags(CE2Material::EffectFlag_EmissiveOnly, tmp);
  if (readBool(tmp, p, 13))
    sp->setFlags(CE2Material::EffectFlag_EmissiveOnlyAuto, tmp);
  if (readBool(tmp, p, 14))
    sp->setFlags(CE2Material::EffectFlag_DirShadows, tmp);
  if (readBool(tmp, p, 15))
    sp->setFlags(CE2Material::EffectFlag_NonDirShadows, tmp);
  if (readBool(tmp, p, 16))
    sp->setFlags(CE2Material::EffectFlag_IsGlass, tmp);
  if (readBool(tmp, p, 17))
    sp->setFlags(CE2Material::EffectFlag_Frosting, tmp);
  readFloat(sp->frostingBgndBlend, p, 18);
  readFloat(sp->frostingBlurBias, p, 19);
  readFloat(sp->materialAlpha, p, 20);
  if (readBool(tmp, p, 21))
    sp->setFlags(CE2Material::EffectFlag_ZTest, tmp);
  if (readBool(tmp, p, 22))
    sp->setFlags(CE2Material::EffectFlag_ZWrite, tmp);
  readEnum(sp->blendMode, p, 23,
           "\012AlphaBlend\010Additive\022SourceSoftAdditive"
           "\010Multiply\027DestinationSoftAdditive"
           "\037DestinationInvertedSoftAdditive\013TakeSmaller\004None");
  if (readBool(tmp, p, 24))
    sp->setFlags(CE2Material::EffectFlag_BacklightEnable, tmp);
  readFloat(sp->backlightScale, p, 25);
  readFloat(sp->backlightSharpness, p, 26);
  readFloat(sp->backlightTransparency, p, 27);
  readColorValue(sp->backlightTintColor, p, 28);
  if (readBool(tmp, p, 29))
    sp->setFlags(CE2Material::EffectFlag_MVFixup, tmp);
  if (readBool(tmp, p, 30))
    sp->setFlags(CE2Material::EffectFlag_MVFixupEdgesOnly, tmp);
  if (readBool(tmp, p, 31))
    sp->setFlags(CE2Material::EffectFlag_RenderBeforeOIT, tmp);
  bool    isNewVersion = (p->childCnt >= 34);
  if (isNewVersion && readBool(tmp, p, 32))
    sp->setFlags(CE2Material::EffectFlag_RenderBeforeClouds, tmp);
  std::uint16_t tmp2;
  if (readUInt16(tmp2, p, size_t(isNewVersion) + 32))
    sp->depthBias = tmp2;
}

// BSComponentDB::CTName
//   String  Name

void CE2MaterialDB::ComponentInfo::readCTName(
    const BSMaterialsCDB::CDBObject *p)
{
  readString(o->name, p, 0);
}

// BSMaterial::GlobalLayerDataComponent
//   Float  TexcoordScale_XY
//   Float  TexcoordScale_YZ
//   Float  TexcoordScale_XZ
//   BSMaterial::Color  AlbedoTintColor
//   Bool  UsesDirectionality
//   XMFLOAT3  SourceDirection
//   Float  DirectionalityIntensity
//   Float  DirectionalityScale
//   Float  DirectionalitySaturation
//   Bool  BlendNormalsAdditively
//   Float  BlendPosition
//   Float  BlendContrast
//   BSMaterial::GlobalLayerNoiseSettings  GlobalLayerNoiseData

void CE2MaterialDB::ComponentInfo::readGlobalLayerDataComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::GlobalLayerData  *sp =
      cdb.constructObject< CE2Material::GlobalLayerData >();
  m->globalLayerData = sp;
  m->setFlags(CE2Material::Flag_GlobalLayerData, true);
  readFloat(sp->texcoordScaleXY, p, 0);
  readFloat(sp->texcoordScaleYZ, p, 1);
  readFloat(sp->texcoordScaleXZ, p, 2);
  readColorValue(sp->albedoTintColor, p, 3);
  readBool(sp->usesDirectionality, p, 4);
  readXMFLOAT3(sp->sourceDirection, p, 5);
  readFloat(sp->sourceDirection[3], p, 6);
  readFloat(sp->directionalityScale, p, 7);
  readFloat(sp->directionalitySaturation, p, 8);
  readBool(sp->blendNormalsAdditively, p, 9);
  readFloat(sp->blendPosition, p, 10);
  readFloat(sp->blendContrast, p, 11);
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 13)
    readGlobalLayerNoiseSettings(p->children()[12]);
}

// BSMaterial::Offset
//   XMFLOAT2  Value

void CE2MaterialDB::ComponentInfo::readOffset(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type == 6 &&
      componentData->className
      == BSReflStream::String_BSMaterial_Offset) [[likely]]
  {
    readXMFLOAT2H(static_cast< CE2Material::UVStream * >(o)->scaleAndOffset,
                  p, 0);
  }
}

// BSMaterial::TextureAddressModeComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readTextureAddressModeComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type == 6) [[likely]]
  {
    readEnum(static_cast< CE2Material::UVStream * >(o)->textureAddressMode,
             p, 0, "\004Wrap\005Clamp\006Mirror\006Border");
  }
}

// BSBind::FloatCurveController
//   BSFloatCurve  Curve
//   Bool  Loop

void CE2MaterialDB::ComponentInfo::readFloatCurveController(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterialBinding::MaterialPropertyNode
//   String  Name
//   String  Binding
//   UInt16  LayerIndex

void CE2MaterialDB::ComponentInfo::readMaterialPropertyNode(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::ProjectedDecalSettings
//   Bool  UseParallaxOcclusionMapping
//   BSMaterial::TextureFile  SurfaceHeightMap
//   Float  ParallaxOcclusionScale
//   Bool  ParallaxOcclusionShadows
//   UInt8  MaxParralaxOcclusionSteps
//   String  RenderLayer
//   Bool  UseGBufferNormals

void CE2MaterialDB::ComponentInfo::readProjectedDecalSettings(
    const BSMaterialsCDB::CDBObject *p)
{
  if (!(p && p->type == BSReflStream::String_BSMaterial_ProjectedDecalSettings))
    return;
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::DecalSettings  *sp =
      const_cast< CE2Material::DecalSettings * >(m->decalSettings);
  readBool(sp->useParallaxMapping, p, 0);
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 2)
    readTextureFile(p->children()[1]);
  readFloat(sp->parallaxOcclusionScale, p, 2);
  readBool(sp->parallaxOcclusionShadows, p, 3);
  readUInt8(sp->maxParallaxSteps, p, 4);
  readEnum(sp->renderLayer, p, 5, "\003Top\006Middle\006Bottom");
  readBool(sp->useGBufferNormals, p, 6);
}

// BSMaterial::ParamBool
//   Bool  Value

void CE2MaterialDB::ComponentInfo::readParamBool(
    const BSMaterialsCDB::CDBObject *p)
{
  bool    tmp;
  if (!readBool(tmp, p, 0))
    return;
  std::uint32_t i = componentData->key & 0xFFFFU;
  if (o->type == 2)
  {
    if (i < CE2Material::Blender::maxBoolParams)
      static_cast< CE2Material::Blender * >(o)->boolParams[i] = tmp;
    return;
  }
  if (i != 0U)
    return;
  if (o->type == 1)
  {
    static_cast< CE2Material * >(o)->setFlags(CE2Material::Flag_TwoSided, tmp);
  }
  else if (o->type == 4)
  {
    static_cast< CE2Material::Material * >(o)->colorModeFlags =
        (static_cast< CE2Material::Material * >(o)->colorModeFlags & 1)
        | ((unsigned char) tmp << 1);
  }
}

// BSBind::Float3DCurveController
//   BSFloat3DCurve  Curve
//   Bool  Loop
//   String  Mask

void CE2MaterialDB::ComponentInfo::readFloat3DCurveController(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

bool CE2MaterialDB::ComponentInfo::readColorValue(
    FloatVector4& c, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_BSMaterial_Color)
  {
    if (readXMFLOAT4(c, p->children()[fieldNum], 0))
    {
      c.maxValues(FloatVector4(0.0f)).minValues(FloatVector4(1.0f));
      return true;
    }
  }
  return false;
}

bool CE2MaterialDB::ComponentInfo::readColorValue(
    std::uint32_t& c, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
      p->children()[fieldNum] &&
      p->children()[fieldNum]->type == BSReflStream::String_BSMaterial_Color)
  {
    FloatVector4  tmp(&c);
    tmp *= (1.0f / 255.0f);
    if (readXMFLOAT4(tmp, p->children()[fieldNum], 0))
    {
      c = std::uint32_t(tmp * 255.0f);
      return true;
    }
  }
  return false;
}

// BSMaterial::Color
//   XMFLOAT4  Value

void CE2MaterialDB::ComponentInfo::readColor(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 4) [[unlikely]]
    return;
  CE2Material::Material *m = static_cast< CE2Material::Material * >(o);
  if (readXMFLOAT4(m->color, p, 0))
    m->color.maxValues(FloatVector4(0.0f)).minValues(FloatVector4(1.0f));
}

// BSMaterial::SourceTextureWithReplacement
//   BSMaterial::MRTextureFile  Texture
//   BSMaterial::TextureReplacement  Replacement

bool CE2MaterialDB::ComponentInfo::readSourceTextureWithReplacement(
    const std::string_view*& texturePath, std::uint32_t& textureReplacement,
    bool& textureReplacementEnabled,
    const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (!(p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt &&
        p->children()[fieldNum] &&
        p->children()[fieldNum]->type
        == BSReflStream::String_BSMaterial_SourceTextureWithReplacement))
  {
    return false;
  }
  const BSMaterialsCDB::CDBObject *q = p->children()[fieldNum];
  bool    r = false;
  if (q->childCnt >= 1 && q->children()[0] &&
      q->children()[0]->type == BSReflStream::String_BSMaterial_MRTextureFile)
  {
    r = readPath(texturePath, q->children()[0], 0, "textures/", ".dds");
  }
  if (q->childCnt >= 2 && q->children()[1] &&
      q->children()[1]->type
      == BSReflStream::String_BSMaterial_TextureReplacement)
  {
    q = q->children()[1];
    r = r | readBool(textureReplacementEnabled, q, 0);
    r = r | readColorValue(textureReplacement, q, 1);
  }
  return r;
}

// BSMaterial::FlowSettingsComponent
//   BSMaterial::Offset  FlowUVOffset
//   BSMaterial::Scale  FlowUVScale
//   Float  FlowExtent
//   BSMaterial::Channel  FlowSourceUVChannel
//   Bool  FlowIsAnimated
//   Float  FlowSpeed
//   Bool  FlowMapAndTexturesAreFlipbooks
//   BSMaterial::UVStreamID  TargetUVStream
//   String  UVStreamTargetLayer
//   String  UVStreamTargetBlender
//   BSMaterial::TextureFile  FlowMap
//   Bool  ApplyFlowOnANMR
//   Bool  ApplyFlowOnOpacity
//   Bool  ApplyFlowOnEmissivity

void CE2MaterialDB::ComponentInfo::readFlowSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::DetailBlenderSettings
//   Bool  IsDetailBlendMaskSupported
//   BSMaterial::SourceTextureWithReplacement  DetailBlendMask
//   BSMaterial::UVStreamID  DetailBlendMaskUVStream

void CE2MaterialDB::ComponentInfo::readDetailBlenderSettings(
    const BSMaterialsCDB::CDBObject *p)
{
  if (!(p && p->type == BSReflStream::String_BSMaterial_DetailBlenderSettings))
    return;
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::DetailBlenderSettings  *sp =
      const_cast< CE2Material::DetailBlenderSettings * >(
          m->detailBlenderSettings);
  bool    tmp;
  if (readBool(tmp, p, 0))
  {
    m->setFlags(CE2Material::Flag_UseDetailBlender, tmp);
    sp->isEnabled = tmp;
  }
  readSourceTextureWithReplacement(sp->texturePath, sp->textureReplacement,
                                   sp->textureReplacementEnabled, p, 1);
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 3)
    readUVStreamID(p->children()[2]);
}

// BSMaterial::LayerID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readLayerID(
    const BSMaterialsCDB::CDBObject *p)
{
  std::uint32_t i = componentData->key & 0xFFFFU;
  if (!(o->type == 1 && i < CE2Material::maxLayers)) [[unlikely]]
    return;
  const CE2MaterialObject *tmp;
  if (readBSComponentDB2ID(tmp, p, 3))
  {
    CE2Material *m = static_cast< CE2Material * >(o);
    m->layers[i] = static_cast< const CE2Material::Layer * >(tmp);
    if (!tmp)
      m->layerMask &= ~(1U << i);
    else
      m->layerMask |= (1U << i);
  }
}

// BSBind::Controllers::Mapping
//   BSBind::Address  Address
//   Ref  Controller

void CE2MaterialDB::ComponentInfo::readMapping(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// ClassReference

void CE2MaterialDB::ComponentInfo::readClassReference(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::Scale
//   XMFLOAT2  Value

void CE2MaterialDB::ComponentInfo::readScale(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type == 6 &&
      componentData->className
      == BSReflStream::String_BSMaterial_Scale) [[likely]]
  {
    readXMFLOAT2L(static_cast< CE2Material::UVStream * >(o)->scaleAndOffset,
                  p, 0);
  }
}

// BSMaterial::WaterGrimeSettingsComponent
//   String  Mode
//   Float  MaskDistanceFromShoreStart
//   Float  MaskDistanceFromShoreEnd
//   Float  MaskDistanceRampWidth
//   XMFLOAT4  MaskNoiseAmp
//   XMFLOAT4  MaskNoiseFreq
//   XMFLOAT4  MaskNoiseBias
//   XMFLOAT4  MaskNoiseAnimSpeed
//   Float  MaskNoiseGlobalScale
//   Float  MaskWaveParallax
//   Float  BlendMaskPosition
//   Float  BlendMaskContrast
//   Float  NormalOverride

void CE2MaterialDB::ComponentInfo::readWaterGrimeSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::UVStreamParamBool
//   Bool  Value

void CE2MaterialDB::ComponentInfo::readUVStreamParamBool(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSBind::Multiplex
//   String  Name
//   Map  Nodes

void CE2MaterialDB::ComponentInfo::readMultiplex(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::OpacityComponent
//   String  FirstLayerIndex
//   Bool  SecondLayerActive
//   String  SecondLayerIndex
//   String  FirstBlenderIndex
//   String  FirstBlenderMode
//   Bool  ThirdLayerActive
//   String  ThirdLayerIndex
//   String  SecondBlenderIndex
//   String  SecondBlenderMode
//   Float  SpecularOpacityOverride

void CE2MaterialDB::ComponentInfo::readOpacityComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  m->setFlags(CE2Material::Flag_HasOpacityComponent, true);
  readLayerNumber(m->opacityLayer1, p, 0);
  bool    tmp;
  if (readBool(tmp, p, 1))
    m->setFlags(CE2Material::Flag_OpacityLayer2Active, tmp);
  readLayerNumber(m->opacityLayer2, p, 2);
  readBlenderNumber(m->opacityBlender1, p, 3);
  readEnum(m->opacityBlender1Mode, p, 4,
           "\004Lerp\010Additive\013Subtractive\016Multiplicative");
  if (readBool(tmp, p, 5))
    m->setFlags(CE2Material::Flag_OpacityLayer3Active, tmp);
  readLayerNumber(m->opacityLayer3, p, 6);
  readBlenderNumber(m->opacityBlender2, p, 7);
  readEnum(m->opacityBlender2Mode, p, 8,
           "\004Lerp\010Additive\013Subtractive\016Multiplicative");
  readFloat(m->specularOpacityOverride, p, 9);
}

// BSMaterial::BlendParamFloat
//   Float  Value

void CE2MaterialDB::ComponentInfo::readBlendParamFloat(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::ColorRemapSettingsComponent
//   Bool  RemapAlbedo
//   Bool  RemapOpacity
//   Bool  RemapEmissive
//   BSMaterial::SourceTextureWithReplacement  AlbedoPaletteTex
//   BSMaterial::Color  AlbedoTint
//   BSMaterial::SourceTextureWithReplacement  AlphaPaletteTex
//   Float  AlphaTint
//   BSMaterial::SourceTextureWithReplacement  EmissivePaletteTex
//   BSMaterial::Color  EmissiveTint

void CE2MaterialDB::ComponentInfo::readColorRemapSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::EyeSettingsComponent
//   Bool  Enabled
//   Float  ScleraEyeRoughness
//   Float  CorneaEyeRoughness
//   Float  ScleraSpecularity
//   Float  IrisSpecularity
//   Float  CorneaSpecularity
//   Float  IrisDepthPosition
//   Float  IrisTotalDepth
//   Float  DepthScale
//   Float  IrisDepthTransitionRatio
//   Float  IrisUVSize
//   Float  LightingWrap
//   Float  LightingPower

void CE2MaterialDB::ComponentInfo::readEyeSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSBind::Float2DLerpController
//   Float  Duration
//   Bool  Loop
//   XMFLOAT2  Start
//   XMFLOAT2  End
//   String  Easing
//   String  Mask

void CE2MaterialDB::ComponentInfo::readFloat2DLerpController(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSComponentDB2::ID
//   UInt32  Value

bool CE2MaterialDB::ComponentInfo::readBSComponentDB2ID(
    const CE2MaterialObject*& linkedObject,
    const BSMaterialsCDB::CDBObject *p, unsigned char typeRequired)
{
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 1 &&
      p->children()[0] &&
      p->children()[0]->type == BSReflStream::String_BSComponentDB2_ID)
  {
    const CE2MaterialObject *tmp =
        cdb.findMaterialObject(p->children()[0]->linkedObject());
    if (typeRequired && tmp && tmp->type != typeRequired)
      tmp = nullptr;
    linkedObject = tmp;
    return true;
  }
  return false;
}

// BSMaterial::TextureReplacement
//   Bool  Enabled
//   BSMaterial::Color  Color

void CE2MaterialDB::ComponentInfo::readTextureReplacement(
    const BSMaterialsCDB::CDBObject *p)
{
  std::uint32_t i = componentData->key & 0xFFFFU;
  if (o->type == 5 && i < CE2Material::TextureSet::maxTexturePaths) [[likely]]
  {
    CE2Material::TextureSet *txtSet =
        static_cast< CE2Material::TextureSet * >(o);
    bool    isEnabled;
    if (readBool(isEnabled, p, 0))
    {
      if (!isEnabled)
        txtSet->textureReplacementMask &= ~(1U << i);
      else
        txtSet->textureReplacementMask |= (1U << i);
    }
    readColorValue(txtSet->textureReplacements[i], p, 1);
  }
  else if (o->type == 2)
  {
    CE2Material::Blender  *blender = static_cast< CE2Material::Blender * >(o);
    readBool(blender->textureReplacementEnabled, p, 0);
    readColorValue(blender->textureReplacement, p, 1);
  }
}

// BSMaterial::BlendModeComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readBlendModeComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type == 2)
  {
    readEnum(static_cast< CE2Material::Blender * >(o)->blendMode, p, 0,
             "\006Linear\010Additive\020PositionContrast\004None"
             "\020CharacterCombine\004Skin");
  }
}

// BSMaterial::LayeredEdgeFalloffComponent
//   List  FalloffStartAngles
//   List  FalloffStopAngles
//   List  FalloffStartOpacities
//   List  FalloffStopOpacities
//   UInt8  ActiveLayersMask
//   Bool  UseRGBFallOff

void CE2MaterialDB::ComponentInfo::readLayeredEdgeFalloffComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::LayeredEdgeFalloff *sp =
      cdb.constructObject< CE2Material::LayeredEdgeFalloff >();
  m->layeredEdgeFalloff = sp;
  for (size_t i = 0; i < 4; i++)
  {
    if (!(p && p->type > BSReflStream::String_Unknown && i < p->childCnt &&
          p->children()[i] &&
          p->children()[i]->type == BSReflStream::String_List))
    {
      continue;
    }
    const BSMaterialsCDB::CDBObject *q = p->children()[i];
    float   *tmp = &(sp->falloffStartAngles[0]);
    if (i == 1)
      tmp = &(sp->falloffStopAngles[0]);
    else if (i == 2)
      tmp = &(sp->falloffStartOpacities[0]);
    else if (i == 3)
      tmp = &(sp->falloffStopOpacities[0]);
    for (size_t j = 0; j < 3; j++)
    {
      if (j < q->childCnt && q->children()[j] &&
          q->children()[j]->type == BSReflStream::String_Float)
      {
        tmp[j] = q->children()[j]->floatValue();
      }
    }
  }
  unsigned char tmp;
  if (readUInt8(tmp, p, 4))
  {
    tmp = tmp & 7;
    sp->activeLayersMask = tmp;
    m->setFlags(CE2Material::Flag_LayeredEdgeFalloff, bool(tmp));
  }
  readBool(sp->useRGBFalloff, p, 5);
}

// BSMaterial::VegetationSettingsComponent
//   Bool  Enabled
//   Float  LeafFrequency
//   Float  LeafAmplitude
//   Float  BranchFlexibility
//   Float  TrunkFlexibility
//   Float  DEPRECATEDTerrainBlendStrength
//   Float  DEPRECATEDTerrainBlendGradientFactor

void CE2MaterialDB::ComponentInfo::readVegetationSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::VegetationSettings *sp =
      cdb.constructObject< CE2Material::VegetationSettings >();
  m->vegetationSettings = sp;
  bool    tmp;
  if (readBool(tmp, p, 0))
  {
    m->setFlags(CE2Material::Flag_IsVegetation, tmp);
    sp->isEnabled = tmp;
  }
  readFloat(sp->leafFrequency, p, 1);
  readFloat(sp->leafAmplitude, p, 2);
  readFloat(sp->branchFlexibility, p, 3);
  readFloat(sp->trunkFlexibility, p, 4);
  readFloat(sp->terrainBlendStrength, p, 5);
  readFloat(sp->terrainBlendGradientFactor, p, 6);
}

// BSMaterial::TextureResolutionSetting
//   String  ResolutionHint

void CE2MaterialDB::ComponentInfo::readTextureResolutionSetting(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type == 5) [[likely]]
  {
    readEnum(static_cast< CE2Material::TextureSet * >(o)->resolutionHint, p, 0,
             "\006Tiling\011UniqueMap\017DetailMapTiling\020HighResUniqueMap");
  }
}

// BSBind::Address
//   List  Path

void CE2MaterialDB::ComponentInfo::readAddress(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSBind::DirectoryComponent
//   Ref  upDir

void CE2MaterialDB::ComponentInfo::readDirectoryComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterialBinding::MaterialUVStreamPropertyNode
//   String  Name
//   BSMaterial::UVStreamID  StreamID
//   String  BindingType

void CE2MaterialDB::ComponentInfo::readMaterialUVStreamPropertyNode(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::ShaderRouteComponent
//   String  Route

void CE2MaterialDB::ComponentInfo::readShaderRouteComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type == 1) [[likely]]
  {
    readEnum(static_cast< CE2Material * >(o)->shaderRoute, p, 0,
             "\010Deferred\006Effect\015PlanetaryRing"
             "\025PrecomputedScattering\005Water");
  }
}

// BSBind::FloatLerpController
//   Float  Duration
//   Bool  Loop
//   Float  Start
//   Float  End
//   String  Easing

void CE2MaterialDB::ComponentInfo::readFloatLerpController(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::ColorChannelTypeComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readColorChannelTypeComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type == 2)
  {
    readEnum(static_cast< CE2Material::Blender * >(o)->colorChannel, p, 0,
             "\003Red\005Green\004Blue\005Alpha");
  }
}

// BSMaterial::AlphaSettingsComponent
//   Bool  HasOpacity
//   Float  AlphaTestThreshold
//   String  OpacitySourceLayer
//   BSMaterial::AlphaBlenderSettings  Blender
//   Bool  UseDitheredTransparency

void CE2MaterialDB::ComponentInfo::readAlphaSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  bool    tmp;
  if (readBool(tmp, p, 0))
    m->setFlags(CE2Material::Flag_HasOpacity, tmp);
  readFloat(m->alphaThreshold, p, 1, true);
  readLayerNumber(m->alphaSourceLayer, p, 2);
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 4)
    readAlphaBlenderSettings(p->children()[3]);
  if (readBool(tmp, p, 4))
    m->setFlags(CE2Material::Flag_DitheredTransparency, tmp);
}

// BSMaterial::LevelOfDetailSettings
//   UInt8  NumLODMaterials
//   String  MostSignificantLayer
//   String  SecondMostSignificantLayer
//   String  MediumLODRootMaterial
//   String  LowLODRootMaterial
//   String  VeryLowLODRootMaterial
//   Float  Bias

void CE2MaterialDB::ComponentInfo::readLevelOfDetailSettings(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::TextureSetID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readTextureSetID(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 4) [[unlikely]]
    return;
  const CE2MaterialObject *tmp;
  if (readBSComponentDB2ID(tmp, p, 5))
  {
    static_cast< CE2Material::Material * >(o)->textureSet =
        static_cast< const CE2Material::TextureSet * >(tmp);
  }
}

// BSBind::Float2DCurveController
//   BSFloat2DCurve  Curve
//   Bool  Loop
//   String  Mask

void CE2MaterialDB::ComponentInfo::readFloat2DCurveController(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::TextureFile
//   String  FileName

void CE2MaterialDB::ComponentInfo::readTextureFile(
    const BSMaterialsCDB::CDBObject *p)
{
  if (componentData->className
      == BSReflStream::String_BSMaterial_TextureFile) [[likely]]
  {
    readMRTextureFile(p);
    return;
  }
  if (componentData->className
      != BSReflStream::String_BSMaterial_DecalSettingsComponent)
  {
    return;
  }
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::DecalSettings  *sp =
      const_cast< CE2Material::DecalSettings * >(m->decalSettings);
  readPath(sp->surfaceHeightMap, p, 0, "textures/", ".dds");
}

// BSMaterial::TranslucencySettings
//   Bool  Thin
//   Bool  FlipBackFaceNormalsInViewSpace
//   Bool  UseSSS
//   Float  SSSWidth
//   Float  SSSStrength
//   Float  TransmissiveScale
//   Float  TransmittanceWidth
//   Float  SpecLobe0RoughnessScale
//   Float  SpecLobe1RoughnessScale
//   String  TransmittanceSourceLayer

void CE2MaterialDB::ComponentInfo::readTranslucencySettings(
    const BSMaterialsCDB::CDBObject *p)
{
  if (!(p && p->type == BSReflStream::String_BSMaterial_TranslucencySettings))
    return;
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::TranslucencySettings *sp =
      const_cast< CE2Material::TranslucencySettings * >(
          m->translucencySettings);
  readBool(sp->isThin, p, 0);
  readBool(sp->flipBackFaceNormalsInVS, p, 1);
  readBool(sp->useSSS, p, 2);
  readFloat(sp->sssWidth, p, 3);
  readFloat(sp->sssStrength, p, 4);
  readFloat(sp->transmissiveScale, p, 5);
  readFloat(sp->transmittanceWidth, p, 6);
  readFloat(sp->specLobe0RoughnessScale, p, 7);
  readFloat(sp->specLobe1RoughnessScale, p, 8);
  readLayerNumber(sp->sourceLayer, p, 9);
}

// BSMaterial::MouthSettingsComponent
//   Bool  Enabled
//   Bool  IsTeeth
//   String  AOVertexColorChannel

void CE2MaterialDB::ComponentInfo::readMouthSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::DistortionComponent
//   Bool  Enabled
//   Float  Strength
//   Bool  UseVertexAlpha
//   Bool  NormalAffectsStrength
//   Bool  CameraDistanceFade
//   Float  NearFadeValue
//   Float  FarFadeValue
//   String  OpacitySourceLayer
//   Float  BlurStrength

void CE2MaterialDB::ComponentInfo::readDistortionComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::DetailBlenderSettingsComponent
//   BSMaterial::DetailBlenderSettings  DetailBlenderSettings

void CE2MaterialDB::ComponentInfo::readDetailBlenderSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::DetailBlenderSettings  *sp =
      cdb.constructObject< CE2Material::DetailBlenderSettings >();
  m->detailBlenderSettings = sp;
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 1)
    readDetailBlenderSettings(p->children()[0]);
}

// BSMaterial::StarmapBodyEffectComponent
//   Bool  Enabled
//   String  Type

void CE2MaterialDB::ComponentInfo::readStarmapBodyEffectComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::MaterialParamFloat
//   Float  Value

void CE2MaterialDB::ComponentInfo::readMaterialParamFloat(
    const BSMaterialsCDB::CDBObject *p)
{
  std::uint32_t i = componentData->key & 0xFFFFU;
  if (o->type == 2 && i < CE2Material::Blender::maxFloatParams)
  {
    readFloat(static_cast< CE2Material::Blender * >(o)->floatParams[i],
              p, 0, true);
  }
  else if (o->type == 5)
  {
    readFloat(static_cast< CE2Material::TextureSet * >(o)->floatParam,
              p, 0, true);
  }
}

// BSFloat2DCurve
//   BSFloatCurve  XCurve
//   BSFloatCurve  YCurve

void CE2MaterialDB::ComponentInfo::readBSFloat2DCurve(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSFloat3DCurve
//   BSFloatCurve  XCurve
//   BSFloatCurve  YCurve
//   BSFloatCurve  ZCurve

void CE2MaterialDB::ComponentInfo::readBSFloat3DCurve(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::TranslucencySettingsComponent
//   Bool  Enabled
//   BSMaterial::TranslucencySettings  Settings

void CE2MaterialDB::ComponentInfo::readTranslucencySettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::TranslucencySettings *sp =
      cdb.constructObject< CE2Material::TranslucencySettings >();
  m->translucencySettings = sp;
  bool    tmp;
  if (readBool(tmp, p, 0))
  {
    m->setFlags(CE2Material::Flag_Translucency, tmp);
    sp->isEnabled = tmp;
  }
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 2)
    readTranslucencySettings(p->children()[1]);
}

// BSMaterial::GlobalLayerNoiseSettings
//   Float  MaterialMaskIntensityScale
//   Bool  UseNoiseMaskTexture
//   BSMaterial::SourceTextureWithReplacement  NoiseMaskTexture
//   XMFLOAT4  TexcoordScaleAndBias
//   Float  WorldspaceScaleFactor
//   Float  HurstExponent
//   Float  BaseFrequency
//   Float  FrequencyMultiplier
//   Float  MaskIntensityMin
//   Float  MaskIntensityMax

void CE2MaterialDB::ComponentInfo::readGlobalLayerNoiseSettings(
    const BSMaterialsCDB::CDBObject *p)
{
  if (!p || p->type != BSReflStream::String_BSMaterial_GlobalLayerNoiseSettings)
    return;
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::GlobalLayerData  *sp =
      const_cast< CE2Material::GlobalLayerData * >(m->globalLayerData);
  readFloat(sp->materialMaskIntensityScale, p, 0);
  readBool(sp->useNoiseMaskTexture, p, 1);
  readSourceTextureWithReplacement(sp->noiseMaskTexture,
                                   sp->noiseMaskTextureReplacement,
                                   sp->noiseMaskTxtReplacementEnabled, p, 2);
  readXMFLOAT4(sp->texcoordScaleAndBias, p, 3);
  readFloat(sp->worldspaceScaleFactor, p, 4);
  readFloat(sp->hurstExponent, p, 5);
  readFloat(sp->baseFrequency, p, 6);
  readFloat(sp->frequencyMultiplier, p, 7);
  readFloat(sp->maskIntensityMin, p, 8);
  readFloat(sp->maskIntensityMax, p, 9);
}

// BSMaterial::MaterialID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readMaterialID(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 3) [[unlikely]]
    return;
  const CE2MaterialObject *tmp;
  if (readBSComponentDB2ID(tmp, p, 4))
  {
    static_cast< CE2Material::Layer * >(o)->material =
        static_cast< const CE2Material::Material * >(tmp);
  }
}

// XMFLOAT2
//   Float  x
//   Float  y

bool CE2MaterialDB::ComponentInfo::readXMFLOAT2L(
    FloatVector4& v, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (!(p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt))
    return false;
  const BSMaterialsCDB::CDBObject *q = p->children()[fieldNum];
  if (!(q && q->type == BSReflStream::String_XMFLOAT2 && q->childCnt == 2))
    return false;
  bool    r = readFloat(v[0], q, 0);
  r = r | readFloat(v[1], q, 1);
  return r;
}

bool CE2MaterialDB::ComponentInfo::readXMFLOAT2H(
    FloatVector4& v, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (!(p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt))
    return false;
  const BSMaterialsCDB::CDBObject *q = p->children()[fieldNum];
  if (!(q && q->type == BSReflStream::String_XMFLOAT2 && q->childCnt == 2))
    return false;
  bool    r = readFloat(v[2], q, 0);
  r = r | readFloat(v[3], q, 1);
  return r;
}

// BSBind::TimerController

void CE2MaterialDB::ComponentInfo::readTimerController(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSBind::Controllers
//   List  MappingsA
//   Bool  UseRandomOffset

void CE2MaterialDB::ComponentInfo::readControllers(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::EmittanceSettings
//   String  EmissiveSourceLayer
//   BSMaterial::Color  EmissiveTint
//   String  EmissiveMaskSourceBlender
//   Float  EmissiveClipThreshold
//   Bool  AdaptiveEmittance
//   Float  LuminousEmittance
//   Float  ExposureOffset
//   Bool  EnableAdaptiveLimits
//   Float  MaxOffsetEmittance
//   Float  MinOffsetEmittance

void CE2MaterialDB::ComponentInfo::readEmittanceSettings(
    const BSMaterialsCDB::CDBObject *p)
{
  if (!(p && p->type == BSReflStream::String_BSMaterial_EmittanceSettings))
    return;
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::EmissiveSettings *sp =
      const_cast< CE2Material::EmissiveSettings * >(m->emissiveSettings);
  readLayerNumber(sp->sourceLayer, p, 0);
  readColorValue(sp->emissiveTint, p, 1);
  readEnum(sp->maskSourceBlender, p, 2,
           "\004None\010Blender1\010Blender2\010Blender3");
  readFloat(sp->clipThreshold, p, 3);
  readBool(sp->adaptiveEmittance, p, 4);
  readFloat(sp->luminousEmittance, p, 5);
  readFloat(sp->exposureOffset, p, 6);
  readBool(sp->enableAdaptiveLimits, p, 7);
  readFloat(sp->maxOffset, p, 8);
  readFloat(sp->minOffset, p, 9);
}

// BSMaterial::ShaderModelComponent
//   String  FileName

void CE2MaterialDB::ComponentInfo::readShaderModelComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 1) [[unlikely]]
    return;
  const char  *s;
  if (!readString(s, p, 0))
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  size_t  n0 = 0;
  size_t  n2 = sizeof(CE2Material::shaderModelNames) / sizeof(char *);
  while ((n2 - n0) >= 2)
  {
    size_t  n1 = (n0 + n2) >> 1;
    if (std::strcmp(s, CE2Material::shaderModelNames[n1]) < 0)
      n2 = n1;
    else
      n0 = n1;
  }
  unsigned char tmp = 55;       // "Unknown"
  if (std::strcmp(s, CE2Material::shaderModelNames[n0]) == 0)
    tmp = (unsigned char) n0;
  m->shaderModel = tmp;
  m->setFlags(CE2Material::Flag_TwoSided,
              bool((1ULL << tmp) & 0xF060000000000000ULL));
}

// BSResource::ID
//   UInt32  Dir
//   UInt32  File
//   UInt32  Ext

void CE2MaterialDB::ComponentInfo::readBSResourceID(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// XMFLOAT3
//   Float  x
//   Float  y
//   Float  z

bool CE2MaterialDB::ComponentInfo::readXMFLOAT3(
    FloatVector4& v, const BSMaterialsCDB::CDBObject *p, size_t fieldNum)
{
  if (!(p && p->type > BSReflStream::String_Unknown && fieldNum < p->childCnt))
    return false;
  const BSMaterialsCDB::CDBObject *q = p->children()[fieldNum];
  if (!(q && q->type == BSReflStream::String_XMFLOAT3 && q->childCnt == 3))
    return false;
  bool    r = readFloat(v[0], q, 0);
  r = r | readFloat(v[1], q, 1);
  r = r | readFloat(v[2], q, 2);
  return r;
}

// BSMaterial::MaterialOverrideColorTypeComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readMaterialOverrideColorTypeComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type != 4) [[unlikely]]
    return;
  unsigned char tmp;
  if (readEnum(tmp, p, 0, "\010Multiply\004Lerp"))
  {
    static_cast< CE2Material::Material * >(o)->colorModeFlags =
        (static_cast< CE2Material::Material * >(o)->colorModeFlags & 2) | tmp;
  }
}

// BSMaterial::Channel
//   String  Value

void CE2MaterialDB::ComponentInfo::readChannel(
    const BSMaterialsCDB::CDBObject *p)
{
  if (!(o->type == 6 &&
        componentData->className
        == BSReflStream::String_BSMaterial_Channel)) [[unlikely]]
  {
    return;
  }
  readEnum(static_cast< CE2Material::UVStream * >(o)->channel, p, 0,
           "\004Zero\003One\003Two\005Three");
}

// BSBind::ColorCurveController
//   BSColorCurve  Curve
//   Bool  Loop
//   String  Mask

void CE2MaterialDB::ComponentInfo::readColorCurveController(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::HairSettingsComponent
//   Bool  Enabled
//   Bool  IsSpikyHair
//   Float  SpecScale
//   Float  SpecularTransmissionScale
//   Float  DirectTransmissionScale
//   Float  DiffuseTransmissionScale
//   Float  Roughness
//   Float  ContactShadowSoftening
//   Float  BackscatterStrength
//   Float  BackscatterWrap
//   Float  VariationStrength
//   Float  IndirectSpecularScale
//   Float  IndirectSpecularTransmissionScale
//   Float  IndirectSpecRoughness
//   Float  AlphaDistance
//   Float  MipBase
//   Float  AlphaBias
//   Float  EdgeMaskContrast
//   Float  EdgeMaskMin
//   Float  EdgeMaskDistanceMin
//   Float  EdgeMaskDistanceMax
//   Float  MaxDepthOffset
//   Float  DitherScale
//   Float  DitherDistanceMin
//   Float  DitherDistanceMax
//   XMFLOAT3  Tangent
//   Float  TangentBend
//   String  DepthOffsetMaskVertexColorChannel
//   String  AOVertexColorChannel
// NOTE: AlphaDistance, MipBase and AlphaBias have been removed
// as of version 1.10.30.0

void CE2MaterialDB::ComponentInfo::readHairSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  CE2Material::HairSettings *sp =
      cdb.constructObject< CE2Material::HairSettings >();
  m->hairSettings = sp;
  bool    tmp;
  if (readBool(tmp, p, 0))
  {
    sp->isEnabled = tmp;
    m->setFlags(CE2Material::Flag_IsHair, tmp);
  }
  if (readBool(tmp, p, 1))
    sp->isSpikyHair = tmp;
  readFloat(sp->specScale, p, 2);
  readFloat(sp->specularTransmissionScale, p, 3);
  readFloat(sp->directTransmissionScale, p, 4);
  readFloat(sp->diffuseTransmissionScale, p, 5);
  readFloat(sp->roughness, p, 6);
  readFloat(sp->contactShadowSoftening, p, 7);
  readFloat(sp->backscatterStrength, p, 8);
  readFloat(sp->backscatterWrap, p, 9);
  readFloat(sp->variationStrength, p, 10);
  readFloat(sp->indirectSpecularScale, p, 11);
  readFloat(sp->indirectSpecularTransmissionScale, p, 12);
  readFloat(sp->indirectSpecRoughness, p, 13);
  size_t  n = 0;
  if (p->childCnt >= 29)
    n = 3;              // skip deprecated fields
  readFloat(sp->edgeMaskContrast, p, n + 14);
  readFloat(sp->edgeMaskMin, p, n + 15);
  readFloat(sp->edgeMaskDistanceMin, p, n + 16);
  readFloat(sp->edgeMaskDistanceMax, p, n + 17);
  readFloat(sp->maxDepthOffset, p, n + 18);
  readFloat(sp->ditherScale, p, n + 19);
  readFloat(sp->ditherDistanceMin, p, n + 20);
  readFloat(sp->ditherDistanceMax, p, n + 21);
  readXMFLOAT3(sp->tangent, p, n + 22);
  readFloat(sp->tangent[3], p, n + 23);
  readEnum(sp->depthOffsetMaskVertexColorChannel, p, n + 24,
           "\003Red\005Green\004Blue\005Alpha");
  readEnum(sp->aoVertexColorChannel, p, n + 25,
           "\003Red\005Green\004Blue\005Alpha");
}

// BSColorCurve
//   BSFloatCurve  RedChannel
//   BSFloatCurve  GreenChannel
//   BSFloatCurve  BlueChannel
//   BSFloatCurve  AlphaChannel

void CE2MaterialDB::ComponentInfo::readBSColorCurve(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSBind::ControllerComponent
//   Ref  upControllers

void CE2MaterialDB::ComponentInfo::readControllerComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::MRTextureFile
//   String  FileName

void CE2MaterialDB::ComponentInfo::readMRTextureFile(
    const BSMaterialsCDB::CDBObject *p)
{
  const std::string_view  **txtPath;
  std::uint32_t *txtMask;
  std::uint32_t i = componentData->key & 0xFFFFU;
  if (o->type == 5 && i < CE2Material::TextureSet::maxTexturePaths) [[likely]]
  {
    txtPath = &(static_cast< CE2Material::TextureSet * >(o)->texturePaths[i]);
    txtMask = &(static_cast< CE2Material::TextureSet * >(o)->texturePathMask);
  }
  else if (o->type == 2)
  {
    txtPath = &(static_cast< CE2Material::Blender * >(o)->texturePath);
    txtMask = &i;
  }
  else
  {
    return;
  }
  if (!(*txtPath)->empty()) [[unlikely]]
  {
    if (componentData->className
        != BSReflStream::String_BSMaterial_MRTextureFile)
    {
      return;
    }
  }
  if (readPath(*txtPath, p, 0, "textures/", ".dds"))
  {
    if ((*txtPath)->empty())
      *txtMask &= ~(1U << i);
    else
      *txtMask |= (1U << i);
  }
}

// BSMaterial::TextureSetKindComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readTextureSetKindComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
}

// BSMaterial::BlenderID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readBlenderID(
    const BSMaterialsCDB::CDBObject *p)
{
  std::uint32_t i = componentData->key & 0xFFFFU;
  if (!(o->type == 1 && i < CE2Material::maxBlenders)) [[unlikely]]
    return;
  const CE2MaterialObject *tmp;
  if (readBSComponentDB2ID(tmp, p, 2))
  {
    static_cast< CE2Material * >(o)->blenders[i] =
        static_cast< const CE2Material::Blender * >(tmp);
  }
}

// BSMaterial::CollisionComponent
//   BSMaterial::PhysicsMaterialType  MaterialTypeOverride

void CE2MaterialDB::ComponentInfo::readCollisionComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  if (p && p->type > BSReflStream::String_Unknown && p->childCnt >= 1)
    readPhysicsMaterialType(p->children()[0]);
}

// BSMaterial::TerrainSettingsComponent
//   Bool  Enabled
//   String  TextureMappingType
//   Float  RotationAngle
//   Float  BlendSoftness
//   Float  TilingDistance
//   Float  MaxDisplacement
//   Float  DisplacementMidpoint

void CE2MaterialDB::ComponentInfo::readTerrainSettingsComponent(
    const BSMaterialsCDB::CDBObject *p)
{
  (void) p;
  if (o->type != 1) [[unlikely]]
    return;
  CE2Material *m = static_cast< CE2Material * >(o);
  m->setFlags(CE2Material::Flag_IsTerrain, true);
}

// BSMaterial::LODMaterialID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readLODMaterialID(
    const BSMaterialsCDB::CDBObject *p)
{
  std::uint32_t i = componentData->key & 0xFFFFU;
  if (!(o->type == 1 && i < CE2Material::maxLODMaterials)) [[unlikely]]
    return;
  const CE2MaterialObject *tmp;
  if (readBSComponentDB2ID(tmp, p, 1))
  {
    static_cast< CE2Material * >(o)->lodMaterials[i] =
        static_cast< const CE2Material * >(tmp);
  }
}

// BSMaterial::MipBiasSetting
//   Bool  DisableMipBiasHint

void CE2MaterialDB::ComponentInfo::readMipBiasSetting(
    const BSMaterialsCDB::CDBObject *p)
{
  if (o->type == 5) [[likely]]
  {
    readBool(static_cast< CE2Material::TextureSet * >(o)->disableMipBiasHint,
             p, 0);
  }
}

void CE2MaterialDB::ComponentInfo::loadComponent(
    const BSMaterialsCDB::MaterialComponent *p)
{
  componentData = p;
  const BSMaterialsCDB::CDBObject *q = p->o;
  switch (p->className)
  {
    case BSReflStream::String_BSBind_ControllerComponent:
      readControllerComponent(q);
      break;
    case BSReflStream::String_BSBind_DirectoryComponent:
      readDirectoryComponent(q);
      break;
    case BSReflStream::String_BSComponentDB_CTName:
      readCTName(q);
      break;
    case BSReflStream::String_BSMaterial_AlphaSettingsComponent:
      readAlphaSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_BlendModeComponent:
      readBlendModeComponent(q);
      break;
    case BSReflStream::String_BSMaterial_BlendParamFloat:
      readBlendParamFloat(q);
      break;
    case BSReflStream::String_BSMaterial_BlenderID:
      readBlenderID(q);
      break;
    case BSReflStream::String_BSMaterial_Channel:
      readChannel(q);
      break;
    case BSReflStream::String_BSMaterial_CollisionComponent:
      readCollisionComponent(q);
      break;
    case BSReflStream::String_BSMaterial_Color:
      readColor(q);
      break;
    case BSReflStream::String_BSMaterial_ColorChannelTypeComponent:
      readColorChannelTypeComponent(q);
      break;
    case BSReflStream::String_BSMaterial_ColorRemapSettingsComponent:
      readColorRemapSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_DecalSettingsComponent:
      readDecalSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_DetailBlenderSettingsComponent:
      readDetailBlenderSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_DistortionComponent:
      readDistortionComponent(q);
      break;
    case BSReflStream::String_BSMaterial_EffectSettingsComponent:
      readEffectSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_EmissiveSettingsComponent:
      readEmissiveSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_EyeSettingsComponent:
      readEyeSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_FlipbookComponent:
      readFlipbookComponent(q);
      break;
    case BSReflStream::String_BSMaterial_FlowSettingsComponent:
      readFlowSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_GlobalLayerDataComponent:
      readGlobalLayerDataComponent(q);
      break;
    case BSReflStream::String_BSMaterial_HairSettingsComponent:
      readHairSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_LODMaterialID:
      readLODMaterialID(q);
      break;
    case BSReflStream::String_BSMaterial_LayerID:
      readLayerID(q);
      break;
    case BSReflStream::String_BSMaterial_LayeredEdgeFalloffComponent:
      readLayeredEdgeFalloffComponent(q);
      break;
    case BSReflStream::String_BSMaterial_LayeredEmissivityComponent:
      readLayeredEmissivityComponent(q);
      break;
    case BSReflStream::String_BSMaterial_LevelOfDetailSettings:
      readLevelOfDetailSettings(q);
      break;
    case BSReflStream::String_BSMaterial_MRTextureFile:
      readMRTextureFile(q);
      break;
    case BSReflStream::String_BSMaterial_MaterialID:
      readMaterialID(q);
      break;
    case BSReflStream::String_BSMaterial_MaterialOverrideColorTypeComponent:
      readMaterialOverrideColorTypeComponent(q);
      break;
    case BSReflStream::String_BSMaterial_MaterialParamFloat:
      readMaterialParamFloat(q);
      break;
    case BSReflStream::String_BSMaterial_MipBiasSetting:
      readMipBiasSetting(q);
      break;
    case BSReflStream::String_BSMaterial_MouthSettingsComponent:
      readMouthSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_Offset:
      readOffset(q);
      break;
    case BSReflStream::String_BSMaterial_OpacityComponent:
      readOpacityComponent(q);
      break;
    case BSReflStream::String_BSMaterial_ParamBool:
      readParamBool(q);
      break;
    case BSReflStream::String_BSMaterial_Scale:
      readScale(q);
      break;
    case BSReflStream::String_BSMaterial_ShaderModelComponent:
      readShaderModelComponent(q);
      break;
    case BSReflStream::String_BSMaterial_ShaderRouteComponent:
      readShaderRouteComponent(q);
      break;
    case BSReflStream::String_BSMaterial_StarmapBodyEffectComponent:
      readStarmapBodyEffectComponent(q);
      break;
    case BSReflStream::String_BSMaterial_TerrainSettingsComponent:
      readTerrainSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_TerrainTintSettingsComponent:
      readTerrainTintSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_TextureAddressModeComponent:
      readTextureAddressModeComponent(q);
      break;
    case BSReflStream::String_BSMaterial_TextureFile:
      readTextureFile(q);
      break;
    case BSReflStream::String_BSMaterial_TextureReplacement:
      readTextureReplacement(q);
      break;
    case BSReflStream::String_BSMaterial_TextureResolutionSetting:
      readTextureResolutionSetting(q);
      break;
    case BSReflStream::String_BSMaterial_TextureSetID:
      readTextureSetID(q);
      break;
    case BSReflStream::String_BSMaterial_TextureSetKindComponent:
      readTextureSetKindComponent(q);
      break;
    case BSReflStream::String_BSMaterial_TranslucencySettingsComponent:
      readTranslucencySettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_UVStreamID:
      readUVStreamID(q);
      break;
    case BSReflStream::String_BSMaterial_UVStreamParamBool:
      readUVStreamParamBool(q);
      break;
    case BSReflStream::String_BSMaterial_VegetationSettingsComponent:
      readVegetationSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_WaterFoamSettingsComponent:
      readWaterFoamSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_WaterGrimeSettingsComponent:
      readWaterGrimeSettingsComponent(q);
      break;
    case BSReflStream::String_BSMaterial_WaterSettingsComponent:
      readWaterSettingsComponent(q);
      break;
  }
}

