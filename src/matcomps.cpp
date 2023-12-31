
#include "common.hpp"
#include "material.hpp"

inline bool CE2MaterialDB::ComponentInfo::readString()
{
  return BSReflStream::Chunk::readString(stringBuf);
}

inline bool CE2MaterialDB::ComponentInfo::readAndStoreString(
    const std::string*& s, int type)
{
  if ((filePos + 2ULL) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
  unsigned int  len = readUInt16Fast();
  if ((filePos + std::uint64_t(len)) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
  s = cdb.readStringParam(stringBuf, *this, len, type);
  return true;
}

unsigned int CE2MaterialDB::ComponentInfo::readChunk(
    BSReflStream::Chunk& chunkBuf)
{
  unsigned int  chunkType = cdbBuf.readChunk(chunkBuf, 0, true);
#if ENABLE_CDB_DEBUG
  std::fputc('\n', stdout);
#endif
  return chunkType;
}

static inline bool parseLayerNumber(unsigned char& n, const std::string& s)
{
  if (s.length() == 16 && s.starts_with("MATERIAL_LAYER_"))
  {
    unsigned char tmp = (unsigned char) (s[15] - '0');
    if (tmp < CE2Material::maxLayers)
    {
      n = tmp;
      return true;
    }
  }
  return false;
}

static inline bool parseBlenderNumber(unsigned char& n, const std::string& s)
{
  if (s.length() == 13 && s.starts_with("BLEND_LAYER_"))
  {
    unsigned char tmp = (unsigned char) (s[12] - '0');
    if (tmp < CE2Material::maxBlenders)
    {
      n = tmp;
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

void CE2MaterialDB::ComponentInfo::readLayeredEmissivityComponent(
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::LayeredEmissiveSettings  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::LayeredEmissiveSettings * >(
             p.cdb.allocateSpace(sizeof(CE2Material::LayeredEmissiveSettings),
                                 m->layeredEmissiveSettings));
    if (!m->layeredEmissiveSettings)
    {
      sp->isEnabled = false;
      sp->layer1Index = 0;              // "MATERIAL_LAYER_0"
      sp->layer1MaskIndex = 0;          // "None"
      sp->layer2Active = false;
      sp->layer2Index = 1;              // "MATERIAL_LAYER_1"
      sp->layer2MaskIndex = 0;
      sp->blender1Index = 0;            // "BLEND_LAYER_0"
      sp->blender1Mode = 0;             // "Lerp"
      sp->layer3Active = false;
      sp->layer3Index = 2;              // "MATERIAL_LAYER_2"
      sp->layer3MaskIndex = 0;
      sp->blender2Index = 1;            // "BLEND_LAYER_1"
      sp->blender2Mode = 0;
      sp->adaptiveEmittance = false;
      sp->enableAdaptiveLimits = false;
      sp->layer1Tint = 0xFFFFFFFFU;
      sp->layer2Tint = 0xFFFFFFFFU;
      sp->layer3Tint = 0xFFFFFFFFU;
      sp->clipThreshold = 0.0f;
      sp->luminousEmittance = 100.0f;
      sp->exposureOffset = 0.0f;
      sp->maxOffset = 1.0f;
      sp->minOffset = 0.0f;
    }
    m->layeredEmissiveSettings = sp;
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 22U, isDiff); )
  {
    if ((1U << n) & 0x00120411U)        // 0, 4, 10, 17, 20: booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      switch (n)
      {
        case 0U:
          m->setFlags(CE2Material::Flag_LayeredEmissivity, tmp);
          sp->isEnabled = tmp;
          break;
        case 4U:
          sp->layer2Active = tmp;
          break;
        case 10U:
          sp->layer3Active = tmp;
          break;
        case 17U:
          sp->adaptiveEmittance = tmp;
          break;
        case 20U:
          sp->enableAdaptiveLimits = tmp;
          break;
      }
    }
    else if ((1U << n) & 0x006D0000U)   // 16, 18, 19, 21, 22: floats
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (!sp)
        continue;
      switch (n)
      {
        case 16U:
          sp->clipThreshold = tmp;
          break;
        case 18U:
          sp->luminousEmittance = tmp;
          break;
        case 19U:
          sp->exposureOffset = tmp;
          break;
        case 21U:
          sp->maxOffset = tmp;
          break;
        case 22U:
          sp->minOffset = tmp;
          break;
      }
    }
    else if ((1U << n) & 0x00001044U)   // 2, 6, 12: colors
    {
      if (!sp)
      {
        FloatVector4  tmp(0.0f);
        readColorValue(tmp, p, isDiff);
      }
      else
      {
        if (n == 2U)
          readColorValue(sp->layer1Tint, p, isDiff);
        else if (n == 6U)
          readColorValue(sp->layer2Tint, p, isDiff);
        else
          readColorValue(sp->layer3Tint, p, isDiff);
      }
    }
    else if ((1U << n) & 0x00000822U)   // 1, 5, 11: layer numbers
    {
      if (!p.readString())
        break;
      unsigned char tmp = 0;
      if (!(sp && parseLayerNumber(tmp, p.stringBuf)))
        continue;
      if (n == 1U)
        sp->layer1Index = tmp;
      else if (n == 5U)
        sp->layer2Index = tmp;
      else
        sp->layer3Index = tmp;
    }
    else if ((1U << n) & 0x00002088U)   // 3, 7, 13: mask numbers
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp, "\004None\010Blender1\010Blender2\010Blender3"))
        break;
      if (!(sp && tmp != 0xFF))
        continue;
      if (n == 3U)
        sp->layer1MaskIndex = tmp;
      else if (n == 7U)
        sp->layer2MaskIndex = tmp;
      else
        sp->layer3MaskIndex = tmp;
    }
    else if ((1U << n) & 0x00004100U)   // 8, 14: blender numbers
    {
      if (!p.readString())
        break;
      unsigned char tmp = 0;
      if (!(sp && parseBlenderNumber(tmp, p.stringBuf)))
        continue;
      if (n == 8U)
        sp->blender1Index = tmp;
      else
        sp->blender2Index = tmp;
    }
    else                                // 9, 15: blender modes
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp,
                      "\004Lerp\010Additive\013Subtractive\016Multiplicative"))
      {
        break;
      }
      if (!(sp && tmp != 0xFF))
        continue;
      if (n == 9U)
        sp->blender1Mode = tmp;
      else
        sp->blender2Mode = tmp;
    }
  }
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  if (p.o->type == 1) [[likely]]
    m = static_cast< CE2Material * >(p.o);
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 8U, isDiff); )
  {
    if ((1U << n) & 0x00000006U)        // 1, 2: booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      if (n == 1U)
        m->setFlags(CE2Material::Flag_AlphaDetailBlendMask, tmp);
      else
        m->setFlags(CE2Material::Flag_AlphaVertexColor, tmp);
    }
    else if ((1U << n) & 0x000001E0U)   // 5, 6, 7, 8: floats
    {
      float   tmp;
      if (!p.readFloat0To1(tmp))
        break;
      if (!m)
        continue;
      if (n == 5U)
        m->alphaHeightBlendThreshold = tmp;
      else if (n == 6U)
        m->alphaHeightBlendFactor = tmp;
      else if (n == 7U)
        m->alphaPosition = tmp;
      else
        m->alphaContrast = tmp;
    }
    else if (n == 0U)
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp,
                      "\006Linear\010Additive\020PositionContrast\004None"))
      {
        break;
      }
      if (m && tmp != 0xFF)
        m->alphaBlendMode = tmp;
    }
    else if (n == 3U)
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp, "\003Red\005Green\004Blue\005Alpha"))
        break;
      if (m && tmp != 0xFF)
        m->alphaVertexColorChannel = tmp;
    }
    else                                // 4: UV stream ID
    {
      readUVStreamID(p, isDiff);
    }
  }
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
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::EmissiveSettingsComponent
//   Bool  Enabled
//   BSMaterial::EmittanceSettings  Settings

void CE2MaterialDB::ComponentInfo::readEmissiveSettingsComponent(
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::EmissiveSettings *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::EmissiveSettings * >(
             p.cdb.allocateSpace(sizeof(CE2Material::EmissiveSettings),
                                 m->emissiveSettings));
    if (!m->emissiveSettings)
    {
      sp->isEnabled = false;
      sp->sourceLayer = 0;
      sp->maskSourceBlender = 0;        // "None"
      sp->adaptiveEmittance = false;
      sp->enableAdaptiveLimits = false;
      sp->clipThreshold = 0.0f;
      sp->luminousEmittance = 432.0f;
      sp->emissiveTint = FloatVector4(1.0f);
      sp->exposureOffset = 0.0f;
      sp->maxOffset = 9999.0f;
      sp->minOffset = 0.0f;
    }
    m->emissiveSettings = sp;
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 1U, isDiff); )
  {
    if (n == 0U)
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      m->setFlags(CE2Material::Flag_Emissive, tmp);
      sp->isEnabled = tmp;
    }
    else
    {
      readEmittanceSettings(p, isDiff);
    }
  }
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
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::FlipbookComponent
//   Bool  IsAFlipbook
//   UInt32  Columns
//   UInt32  Rows
//   Float  FPS
//   Bool  Loops

void CE2MaterialDB::ComponentInfo::readFlipbookComponent(
    ComponentInfo& p, bool isDiff)
{
  CE2Material::Material *m = nullptr;
  if (p.o->type == 4) [[likely]]
    m = static_cast< CE2Material::Material * >(p.o);
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 4U, isDiff); )
  {
    if (n == 0U || n == 4U)
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      if (n == 0U)
        m->flipbookFlags = (m->flipbookFlags & 2) | (unsigned char) tmp;
      else
        m->flipbookFlags = (m->flipbookFlags & 1) | ((unsigned char) tmp << 1);
    }
    else if (n <= 2U)
    {
      std::uint32_t tmp;
      if (!p.readUInt32(tmp))
        break;
      if (!m)
        continue;
      tmp = std::min< std::uint32_t >(tmp, 255U);
      if (n == 1U)
        m->flipbookColumns = (unsigned char) tmp;
      else
        m->flipbookRows = (unsigned char) tmp;
    }
    else
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (m)
        m->flipbookFPS = tmp;
    }
  }
}

// BSMaterial::PhysicsMaterialType
//   UInt32  Value

void CE2MaterialDB::ComponentInfo::readPhysicsMaterialType(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    std::uint32_t tmp;
    if (!p.readUInt32(tmp))
      break;
    if (p.o->type == 1 &&
        p.componentType
        == BSReflStream::String_BSMaterial_CollisionComponent) [[likely]]
    {
      CE2Material *m = static_cast< CE2Material * >(p.o);
      switch (tmp)
      {
        case 0x064003D4U:               // "metal"
          m->physicsMaterialType = 6;
          break;
        case 0x1DD9C611U:               // "wood"
          m->physicsMaterialType = 7;
          break;
        case 0x4BDC3571U:               // "materialphyicedebrislarge"
          m->physicsMaterialType = 5;
          break;
        case 0x6B81B7B0U:               // "mat"
          m->physicsMaterialType = 2;
          break;
        case 0x7A0EB611U:               // "materialmat"
          m->physicsMaterialType = 4;
          break;
        case 0xAD5ACB92U:               // "materialgroundtilevinyl"
          m->physicsMaterialType = 3;
          break;
        case 0xF0170989U:               // "carpet"
          m->physicsMaterialType = 1;
          break;
        default:
          m->physicsMaterialType = 0;
          break;
      }
    }
  }
}

// BSMaterial::TerrainTintSettingsComponent
//   Bool  Enabled
//   Float  TerrainBlendStrength
//   Float  TerrainBlendGradientFactor

void CE2MaterialDB::ComponentInfo::readTerrainTintSettingsComponent(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::UVStreamID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readUVStreamID(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    const CE2Material::UVStream *uvStream =
        static_cast< const CE2Material::UVStream * >(
            readBSComponentDB2ID(p, isDiff, 6));
    if (p.componentType
        == BSReflStream::String_BSMaterial_UVStreamID) [[likely]]
    {
      if (p.o->type == 2)
        static_cast< CE2Material::Blender * >(p.o)->uvStream = uvStream;
      else if (p.o->type == 3)
        static_cast< CE2Material::Layer * >(p.o)->uvStream = uvStream;
    }
    else if (p.componentType
             == BSReflStream::String_BSMaterial_AlphaSettingsComponent)
    {
      if (p.o->type == 1)
        static_cast< CE2Material * >(p.o)->alphaUVStream = uvStream;
    }
    else if (p.componentType
             == BSReflStream::String_BSMaterial_DetailBlenderSettingsComponent)
    {
      if (p.o->type == 1)
      {
        CE2Material::DetailBlenderSettings  *sp =
            const_cast< CE2Material::DetailBlenderSettings * >(
                static_cast< CE2Material * >(p.o)->detailBlenderSettings);
        sp->uvStream = uvStream;
      }
    }
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::DecalSettings  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::DecalSettings * >(
             p.cdb.allocateSpace(sizeof(CE2Material::DecalSettings),
                                 m->decalSettings));
    if (!m->decalSettings)
    {
      sp->isDecal = false;
      sp->isPlanet = false;
      sp->blendMode = 0;                // "None"
      sp->animatedDecalIgnoresTAA = false;
      sp->decalAlpha = 1.0f;
      sp->writeMask = 0x0737U;
      sp->isProjected = false;
      sp->useParallaxMapping = false;
      sp->parallaxOcclusionShadows = false;
      sp->maxParallaxSteps = 72;
      sp->surfaceHeightMap = p.cdb.stringBuffers.front().data();
      sp->parallaxOcclusionScale = 1.0f;
      sp->renderLayer = 0;              // "Top"
      sp->useGBufferNormals = true;
    }
    m->decalSettings = sp;
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 7U, isDiff); )
  {
    if ((1U << n) & 0x00000099U)        // 0, 3, 4, 7: booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      switch (n)
      {
        case 0U:
          m->setFlags(CE2Material::Flag_IsDecal, tmp);
          if (tmp)
            m->setFlags(CE2Material::Flag_AlphaBlending, true);
          sp->isDecal = tmp;
          break;
        case 3U:
          sp->isPlanet = tmp;
          break;
        case 4U:
          sp->isProjected = tmp;
          break;
        case 7U:
          sp->animatedDecalIgnoresTAA = tmp;
          break;
      }
    }
    else if (n == 1U)
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (sp)
        sp->decalAlpha = tmp;
    }
    else if (n == 2U)
    {
      std::uint32_t tmp;
      if (!p.readUInt32(tmp))
        break;
      if (sp)
        sp->writeMask = tmp;
    }
    else if (n == 5U)
    {
      readProjectedDecalSettings(p, isDiff);
    }
    else
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp, "\004None\010Additive"))
        break;
      if (sp && tmp != 0xFF)
        sp->blendMode = tmp;
    }
  }
}

// BSBind::Directory
//   String  Name
//   Map  Children
//   UInt64  SourceDirectoryHash

void CE2MaterialDB::ComponentInfo::readDirectory(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::WaterSettings  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::WaterSettings * >(
             p.cdb.allocateSpace(sizeof(CE2Material::WaterSettings),
                                 m->waterSettings));
    if (!m->waterSettings)
    {
      m->setFlags(CE2Material::Flag_IsWater | CE2Material::Flag_AlphaBlending,
                  true);
      // FIXME: find correct default water settings
      sp->waterEdgeFalloff = 0.0f;
      sp->waterWetnessMaxDepth = 0.0f;
      sp->waterEdgeNormalFalloff = 0.0f;
      sp->waterDepthBlur = 0.0f;
      sp->reflectance = FloatVector4(0.0f, 0.0f, 0.0f, 0.0f);
      sp->phytoplanktonReflectance = FloatVector4(0.0f, 0.0f, 0.0f, 0.0f);
      sp->sedimentReflectance = FloatVector4(0.0f, 0.0f, 0.0f, 0.0f);
      sp->yellowMatterReflectance = FloatVector4(0.0f, 0.0f, 0.0f, 0.0f);
      sp->lowLOD = false;
      sp->placedWater = false;
    }
    m->waterSettings = sp;
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 21U, isDiff); )
  {
    if (n <= 19U)
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (!sp)
        continue;
      switch (n)
      {
        case 0U:
          sp->waterEdgeFalloff = tmp;
          break;
        case 1U:
          sp->waterWetnessMaxDepth = tmp;
          break;
        case 2U:
          sp->waterEdgeNormalFalloff = tmp;
          break;
        case 3U:
          sp->waterDepthBlur = tmp;
          break;
        case 4U:
          sp->reflectance[3] = tmp;
          break;
        case 5U:
        case 6U:
        case 7U:
          sp->phytoplanktonReflectance[n - 5U] = tmp;
          break;
        case 8U:
        case 9U:
        case 10U:
          sp->sedimentReflectance[n - 8U] = tmp;
          break;
        case 11U:
        case 12U:
        case 13U:
          sp->yellowMatterReflectance[n - 11U] = tmp;
          break;
        case 14U:
          sp->phytoplanktonReflectance[3] = tmp;
          break;
        case 15U:
          sp->sedimentReflectance[3] = tmp;
          break;
        case 16U:
          sp->yellowMatterReflectance[3] = tmp;
          break;
        case 17U:
        case 18U:
        case 19U:
          sp->reflectance[n - 17U] = tmp;
          break;
      }
    }
    else
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!sp)
        continue;
      if (n == 20U)
        sp->lowLOD = tmp;
      else
        sp->placedWater = tmp;
    }
  }
}

// BSFloatCurve::Control
//   Float  Input
//   Float  Value

void CE2MaterialDB::ComponentInfo::readControl(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSBind::ComponentProperty
//   String  Name
//   BSComponentDB2::ID  Object
//   ClassReference  ComponentType
//   UInt16  ComponentIndex
//   String  PathStr

void CE2MaterialDB::ComponentInfo::readComponentProperty(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// XMFLOAT4
//   Float  x
//   Float  y
//   Float  z
//   Float  w

bool CE2MaterialDB::ComponentInfo::readXMFLOAT4(
    FloatVector4& v, ComponentInfo& p, bool isDiff)
{
  bool    r = false;
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 3U, isDiff); )
  {
    if (!p.readFloat(v[n]))
      break;
    r = true;
  }
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
//   UInt16  DepthBiasInUlp

void CE2MaterialDB::ComponentInfo::readEffectSettingsComponent(
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::EffectSettings *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::EffectSettings * >(
             p.cdb.allocateSpace(sizeof(CE2Material::EffectSettings),
                                 m->effectSettings));
    if (!m->effectSettings)
    {
      m->setFlags(CE2Material::Flag_IsEffect | CE2Material::Flag_AlphaBlending,
                  true);
      sp->flags = CE2Material::EffectFlag_ZTest;
      sp->blendMode = 0;                // "AlphaBlend"
      sp->falloffStartAngle = 0.0f;
      sp->falloffStopAngle = 0.0f;
      sp->falloffStartOpacity = 0.0f;
      sp->falloffStopOpacity = 0.0f;
      sp->alphaThreshold = 0.5f;
      sp->softFalloffDepth = 2.0f;
      sp->frostingBgndBlend = 0.98f;
      sp->frostingBlurBias = 0.0f;
      sp->materialAlpha = 1.0f;
      sp->backlightScale = 0.0f;
      sp->backlightSharpness = 8.0f;
      sp->backlightTransparency = 0.0f;
      sp->backlightTintColor = FloatVector4(1.0f);
      sp->depthBias = 0;
    }
    m->effectSettings = sp;
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 32U, isDiff); )
  {
    if (n == 32U) [[unlikely]]
    {
      std::uint16_t tmp;
      if (!p.readUInt16(tmp))
        break;
      if (sp)
        sp->depthBias = tmp;
    }
    else if ((1U << n) & 0xE163F6C3U)   // boolean flags
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (sp)
      {
        if (!tmp)
          sp->flags &= ~(1U << n);
        else
          sp->flags |= (1U << n);
      }
    }
    else if ((1U << n) & 0x0E1C093CU)   // floats
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (!sp)
        continue;
      switch (n)
      {
        case 2U:
          sp->falloffStartAngle = tmp;
          break;
        case 3U:
          sp->falloffStopAngle = tmp;
          break;
        case 4U:
          sp->falloffStartOpacity = tmp;
          break;
        case 5U:
          sp->falloffStopOpacity = tmp;
          break;
        case 8U:
          sp->alphaThreshold = tmp;
          break;
        case 11U:
          sp->softFalloffDepth = tmp;
          break;
        case 18U:
          sp->frostingBgndBlend = tmp;
          break;
        case 19U:
          sp->frostingBlurBias = tmp;
          break;
        case 20U:
          sp->materialAlpha = tmp;
          break;
        case 25U:
          sp->backlightScale = tmp;
          break;
        case 26U:
          sp->backlightSharpness = tmp;
          break;
        case 27U:
          sp->backlightTransparency = tmp;
          break;
      }
    }
    else if (n == 23U)
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp, "\012AlphaBlend\010Additive\022SourceSoftAdditive"
                           "\010Multiply\027DestinationSoftAdditive"
                           "\037DestinationInvertedSoftAdditive\013TakeSmaller"
                           "\004None"))
      {
        break;
      }
      if (sp && tmp != 0xFF)
        sp->blendMode = tmp;
    }
    else
    {
      if (!sp)
      {
        FloatVector4  c(0.0f);
        readColorValue(c, p, isDiff);
      }
      else
      {
        readColorValue(sp->backlightTintColor, p, isDiff);
      }
    }
  }
}

// BSComponentDB::CTName
//   String  Name

void CE2MaterialDB::ComponentInfo::readCTName(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    if (!p.readAndStoreString(p.o->name, 0))
      break;
  }
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::GlobalLayerData  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::GlobalLayerData * >(
             p.cdb.allocateSpace(sizeof(CE2Material::GlobalLayerData),
                                 m->globalLayerData));
    if (!m->globalLayerData)
    {
      sp->texcoordScaleXY = 1.0f;
      sp->texcoordScaleYZ = 1.0f;
      sp->texcoordScaleXZ = 1.0f;
      sp->usesDirectionality = true;
      sp->blendNormalsAdditively = true;
      sp->useNoiseMaskTexture = false;
      sp->noiseMaskTxtReplacementEnabled = false;
      sp->albedoTintColor = FloatVector4(1.0f);
      sp->sourceDirection = FloatVector4(0.0f, 0.0f, 1.0f, 1.0f);
      sp->directionalityScale = 1.0f;
      sp->directionalitySaturation = 1.0f;
      sp->blendPosition = 0.5f;
      sp->blendContrast = 0.5f;
      sp->materialMaskIntensityScale = 1.0f;
      sp->noiseMaskTextureReplacement = 0xFFFFFFFFU;
      sp->noiseMaskTexture = p.cdb.stringBuffers.front().data();
      sp->texcoordScaleAndBias = FloatVector4(1.0f, 1.0f, 0.0f, 0.0f);
      sp->worldspaceScaleFactor = 0.0f;
      sp->hurstExponent = 0.5f;
      sp->baseFrequency = 0.0f;
      sp->frequencyMultiplier = 1.0f;
      sp->maskIntensityMin = 0.0f;
      sp->maskIntensityMax = 1.0f;
    }
    m->globalLayerData = sp;
    m->setFlags(CE2Material::Flag_GlobalLayerData, true);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 12U, isDiff); )
  {
    if ((1U << n) & 0x00000DC7U)        // 0, 1, 2, 6, 7, 8, 10, 11: floats
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (!sp)
        continue;
      switch (n)
      {
        case 0U:
          sp->texcoordScaleXY = tmp;
          break;
        case 1U:
          sp->texcoordScaleYZ = tmp;
          break;
        case 2U:
          sp->texcoordScaleXZ = tmp;
          break;
        case 6U:
          sp->sourceDirection[3] = tmp;
          break;
        case 7U:
          sp->directionalityScale = tmp;
          break;
        case 8U:
          sp->directionalitySaturation = tmp;
          break;
        case 10U:
          sp->blendPosition = tmp;
          break;
        case 11U:
          sp->blendContrast = tmp;
          break;
      }
    }
    else if ((1U << n) & 0x00000210U)   // 4, 9: booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!sp)
        continue;
      if (n == 4U)
        sp->usesDirectionality = tmp;
      else
        sp->blendNormalsAdditively = tmp;
    }
    else if (n == 3U)
    {
      FloatVector4  tmp(0.0f);
      if (sp) [[likely]]
        readColorValue(sp->albedoTintColor, p, isDiff);
      else
        readColorValue(tmp, p, isDiff);
    }
    else if (n == 5U)
    {
      FloatVector4  tmp(0.0f);
      if (sp) [[likely]]
        readXMFLOAT3(sp->sourceDirection, p, isDiff);
      else
        readXMFLOAT3(tmp, p, isDiff);
    }
    else
    {
      readGlobalLayerNoiseSettings(p, isDiff);
    }
  }
}

// BSMaterial::Offset
//   XMFLOAT2  Value

void CE2MaterialDB::ComponentInfo::readOffset(
    ComponentInfo& p, bool isDiff)
{
  FloatVector4  tmp(0.0f);
  FloatVector4  *c = &tmp;
  if (p.o->type == 6 &&
      p.componentType == BSReflStream::String_BSMaterial_Offset) [[likely]]
  {
    c = &(static_cast< CE2Material::UVStream * >(p.o)->scaleAndOffset);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
    readXMFLOAT2H(*c, p, isDiff);
}

// BSMaterial::TextureAddressModeComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readTextureAddressModeComponent(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    unsigned char tmp = 0xFF;
    if (!p.readEnum(tmp, "\004Wrap\005Clamp\006Mirror\006Border"))
      break;
    if (p.o->type == 6 && tmp != 0xFF)
      static_cast< CE2Material::UVStream * >(p.o)->textureAddressMode = tmp;
  }
}

// BSBind::FloatCurveController
//   BSFloatCurve  Curve
//   Bool  Loop

void CE2MaterialDB::ComponentInfo::readFloatCurveController(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterialBinding::MaterialPropertyNode
//   String  Name
//   String  Binding
//   UInt16  LayerIndex

void CE2MaterialDB::ComponentInfo::readMaterialPropertyNode(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material::DecalSettings  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    CE2Material *m = static_cast< CE2Material * >(p.o);
    sp = const_cast< CE2Material::DecalSettings * >(m->decalSettings);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 6U, isDiff); )
  {
    if ((1U << n) & 0x00000049U)        // 0, 3, 6: booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!sp)
        continue;
      if (n == 0U)
        sp->useParallaxMapping = tmp;
      else if (n == 3U)
        sp->parallaxOcclusionShadows = tmp;
      else
        sp->useGBufferNormals = tmp;
    }
    else if (n == 1U)
    {
      readTextureFile(p, isDiff);
    }
    else if (n == 2U)
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (sp)
        sp->parallaxOcclusionScale = tmp;
    }
    else if (n == 4U)
    {
      unsigned char tmp;
      if (!p.readUInt8(tmp))
        break;
      if (sp)
        sp->maxParallaxSteps = tmp;
    }
    else
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp, "\003Top\006Middle"))
        break;
      if (sp && tmp != 0xFF)
        sp->renderLayer = tmp;
    }
  }
}

// BSMaterial::ParamBool
//   Bool  Value

void CE2MaterialDB::ComponentInfo::readParamBool(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    bool    tmp;
    if (!p.readBool(tmp))
      break;
    unsigned int  i = p.componentIndex;
    if (p.o->type == 2 && i < CE2Material::Blender::maxBoolParams)
    {
      static_cast< CE2Material::Blender * >(p.o)->boolParams[i] = tmp;
    }
    else if (p.o->type == 1 && i == 0U)
    {
      static_cast< CE2Material * >(p.o)->setFlags(CE2Material::Flag_TwoSided,
                                                  tmp);
    }
  }
}

// BSBind::Float3DCurveController
//   BSFloat3DCurve  Curve
//   Bool  Loop
//   String  Mask

void CE2MaterialDB::ComponentInfo::readFloat3DCurveController(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

bool CE2MaterialDB::ComponentInfo::readColorValue(
    FloatVector4& c, ComponentInfo& p, bool isDiff)
{
  bool    r = false;
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
    r = r | readXMFLOAT4(c, p, isDiff);
  if (r) [[likely]]
    c.maxValues(FloatVector4(0.0f)).minValues(FloatVector4(1.0f));
  return r;
}

bool CE2MaterialDB::ComponentInfo::readColorValue(
    std::uint32_t& c, ComponentInfo& p, bool isDiff)
{
  FloatVector4  tmp(&c);
  tmp *= (1.0f / 255.0f);
  bool    r = false;
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
    r = r | readXMFLOAT4(tmp, p, isDiff);
  if (r) [[likely]]
    c = std::uint32_t(tmp * 255.0f);
  return r;
}

// BSMaterial::Color
//   XMFLOAT4  Value

void CE2MaterialDB::ComponentInfo::readColor(
    ComponentInfo& p, bool isDiff)
{
  CE2Material::Material *m = nullptr;
  if (p.o->type == 4) [[likely]]
    m = static_cast< CE2Material::Material * >(p.o);
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    if (!m)
    {
      FloatVector4  c(0.0f);
      readXMFLOAT4(c, p, isDiff);
    }
    else if (readXMFLOAT4(m->color, p, isDiff))
    {
      m->color.maxValues(FloatVector4(0.0f)).minValues(FloatVector4(1.0f));
    }
  }
}

// BSMaterial::SourceTextureWithReplacement
//   BSMaterial::MRTextureFile  Texture
//   BSMaterial::TextureReplacement  Replacement

bool CE2MaterialDB::ComponentInfo::readSourceTextureWithReplacement(
    const std::string*& texturePath, std::uint32_t& textureReplacement,
    bool& textureReplacementEnabled, ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 1U, isDiff); )
  {
    if (n == 0U)
    {
      for (unsigned int n2 = 0U - 1U; p.getFieldNumber(n2, 0U, isDiff); )
      {
        if (!p.readAndStoreString(texturePath, 1))
          return false;
      }
    }
    else
    {
      for (unsigned int n2 = 0U - 1U; p.getFieldNumber(n2, 1U, isDiff); )
      {
        if (n2 == 0U)
        {
          if (!p.readBool(textureReplacementEnabled))
            return false;
        }
        else
        {
          readColorValue(textureReplacement, p, isDiff);
        }
      }
    }
  }
  return true;
}

// BSComponentDB2::DBFileIndex::EdgeInfo
//   BSComponentDB2::ID  SourceID
//   BSComponentDB2::ID  TargetID
//   UInt16  Index
//   UInt16  Type

void CE2MaterialDB::ComponentInfo::readEdgeInfo(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::DetailBlenderSettings
//   Bool  IsDetailBlendMaskSupported
//   BSMaterial::SourceTextureWithReplacement  DetailBlendMask
//   BSMaterial::UVStreamID  DetailBlendMaskUVStream

void CE2MaterialDB::ComponentInfo::readDetailBlenderSettings(
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::DetailBlenderSettings  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = const_cast< CE2Material::DetailBlenderSettings * >(
             m->detailBlenderSettings);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 2U, isDiff); )
  {
    if (n == 0U)
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      m->setFlags(CE2Material::Flag_UseDetailBlender, tmp);
      sp->isEnabled = tmp;
    }
    else if (n == 1U)
    {
      CE2Material::DetailBlenderSettings  tmp;
      CE2Material::DetailBlenderSettings  *spTmp = (!sp ? &tmp : sp);
      if (!readSourceTextureWithReplacement(
               spTmp->texturePath, spTmp->textureReplacement,
               spTmp->textureReplacementEnabled, p, isDiff))
      {
        break;
      }
    }
    else
    {
      readUVStreamID(p, isDiff);
    }
  }
}

// BSMaterial::LayerID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readLayerID(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    const CE2MaterialObject *o = readBSComponentDB2ID(p, isDiff, 3);
    if (p.o->type == 1 && p.componentIndex < CE2Material::maxLayers)
    {
      CE2Material *m = static_cast< CE2Material * >(p.o);
      m->layers[p.componentIndex] =
          static_cast< const CE2Material::Layer * >(o);
      if (!o)
        m->layerMask &= ~(1U << p.componentIndex);
      else
        m->layerMask |= (1U << p.componentIndex);
    }
  }
}

// BSBind::Controllers::Mapping
//   BSBind::Address  Address
//   Ref  Controller

void CE2MaterialDB::ComponentInfo::readMapping(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// ClassReference

void CE2MaterialDB::ComponentInfo::readClassReference(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSComponentDB2::DBFileIndex::ComponentInfo
//   BSComponentDB2::ID  ObjectID
//   UInt16  Index
//   UInt16  Type

void CE2MaterialDB::ComponentInfo::readComponentInfo(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::Scale
//   XMFLOAT2  Value

void CE2MaterialDB::ComponentInfo::readScale(
    ComponentInfo& p, bool isDiff)
{
  FloatVector4  tmp(0.0f);
  FloatVector4  *c = &tmp;
  if (p.o->type == 6 &&
      p.componentType == BSReflStream::String_BSMaterial_Scale) [[likely]]
  {
    c = &(static_cast< CE2Material::UVStream * >(p.o)->scaleAndOffset);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
    readXMFLOAT2L(*c, p, isDiff);
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
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::UVStreamParamBool
//   Bool  Value

void CE2MaterialDB::ComponentInfo::readUVStreamParamBool(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSComponentDB2::DBFileIndex::ComponentTypeInfo
//   ClassReference  Class
//   UInt16  Version
//   Bool  IsEmpty

void CE2MaterialDB::ComponentInfo::readComponentTypeInfo(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSBind::Multiplex
//   String  Name
//   Map  Nodes

void CE2MaterialDB::ComponentInfo::readMultiplex(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    m->setFlags(CE2Material::Flag_HasOpacityComponent, true);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 9U, isDiff); )
  {
    if ((1U << n) & 0x00000022U)        // booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      if (n == 1U)
        m->setFlags(CE2Material::Flag_OpacityLayer2Active, tmp);
      else
        m->setFlags(CE2Material::Flag_OpacityLayer3Active, tmp);
    }
    else if ((1U << n) & 0x000000CDU)   // layer and blender index strings
    {
      if (!p.readString())
        break;
      unsigned char tmp = 0xFF;
      if ((1U << n) & 0x00000045U)
        parseLayerNumber(tmp, p.stringBuf);
      else
        parseBlenderNumber(tmp, p.stringBuf);
      if (!(m && tmp != 0xFF))
        continue;
      switch (n)
      {
        case 0U:
          m->opacityLayer1 = tmp;
          break;
        case 2U:
          m->opacityLayer2 = tmp;
          break;
        case 3U:
          m->opacityBlender1 = tmp;
          break;
        case 6U:
          m->opacityLayer3 = tmp;
          break;
        case 7U:
          m->opacityBlender2 = tmp;
          break;
      }
    }
    else if ((1U << n) & 0x00000110U)   // blender modes
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp,
                      "\004Lerp\010Additive\013Subtractive\016Multiplicative"))
      {
        break;
      }
      if (!(m && tmp != 0xFF))
        continue;
      if (n == 4U)
        m->opacityBlender1Mode = tmp;
      else
        m->opacityBlender2Mode = tmp;
    }
    else
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (m)
        m->specularOpacityOverride = tmp;
    }
  }
}

// BSMaterial::BlendParamFloat
//   Float  Value

void CE2MaterialDB::ComponentInfo::readBlendParamFloat(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSComponentDB2::DBFileIndex
//   Map  ComponentTypes
//   List  Objects
//   List  Components
//   List  Edges
//   Bool  Optimized

void CE2MaterialDB::ComponentInfo::readDBFileIndex(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::Internal::CompiledDB::FilePair
//   BSResource::ID  First
//   BSResource::ID  Second

void CE2MaterialDB::ComponentInfo::readFilePair(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSBind::Float2DLerpController
//   Float  Duration
//   Bool  Loop
//   XMFLOAT2  Start
//   XMFLOAT2  End
//   String  Easing
//   String  Mask

void CE2MaterialDB::ComponentInfo::readFloat2DLerpController(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSComponentDB2::ID
//   UInt32  Value

const CE2MaterialObject * CE2MaterialDB::ComponentInfo::readBSComponentDB2ID(
    ComponentInfo& p, bool isDiff, unsigned char typeRequired)
{
  const CE2MaterialObject *o = nullptr;
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    std::uint32_t objectID;
    if (!p.readUInt32(objectID))
      break;
    if (!(objectID && objectID < p.objectTable.size() &&
          p.objectTable[objectID]))
    {
#if ENABLE_CDB_DEBUG
      if (objectID)
      {
        std::printf("Warning: invalid object ID 0x%08X in material database\n",
                    (unsigned int) objectID);
      }
#endif
    }
    else
    {
      o = p.objectTable[objectID];
      if (typeRequired) [[likely]]
      {
        if ((o->type ^ typeRequired) & 0xFF) [[unlikely]]
        {
#if ENABLE_CDB_DEBUG
          std::printf("Warning: linked object 0x%08X is of type %d, "
                      "expected %d\n",
                      (unsigned int) objectID,
                      int(o->type & 0xFF), int(typeRequired));
#endif
          o = nullptr;
        }
      }
    }
  }
  return o;
}

// BSMaterial::TextureReplacement
//   Bool  Enabled
//   BSMaterial::Color  Color

void CE2MaterialDB::ComponentInfo::readTextureReplacement(
    ComponentInfo& p, bool isDiff)
{
  CE2Material::Blender  *blender = nullptr;
  CE2Material::TextureSet *txtSet = nullptr;
  if (p.o->type == 5 &&
      p.componentIndex < CE2Material::TextureSet::maxTexturePaths) [[likely]]
  {
    txtSet = static_cast< CE2Material::TextureSet * >(p.o);
  }
  else if (p.o->type == 2)
  {
    blender = static_cast< CE2Material::Blender * >(p.o);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 1U, isDiff); )
  {
    if (n == 0U)
    {
      bool    isEnabled;
      if (!p.readBool(isEnabled))
        break;
      if (txtSet) [[likely]]
      {
        if (!isEnabled)
          txtSet->textureReplacementMask &= ~(1U << p.componentIndex);
        else
          txtSet->textureReplacementMask |= (1U << p.componentIndex);
      }
      else if (blender)
      {
        blender->textureReplacementEnabled = isEnabled;
      }
      continue;
    }
    FloatVector4  c(0.0f);
    if (txtSet)
      readColorValue(txtSet->textureReplacements[p.componentIndex], p, isDiff);
    else if (blender)
      readColorValue(blender->textureReplacement, p, isDiff);
    else
      readColorValue(c, p, isDiff);
  }
}

// BSMaterial::BlendModeComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readBlendModeComponent(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    unsigned char tmp = 0xFF;
    if (!p.readEnum(tmp, "\006Linear\010Additive\020PositionContrast\004None"
                         "\020CharacterCombine\004Skin"))
    {
      break;
    }
    if (p.o->type == 2 && tmp != 0xFF)
      static_cast< CE2Material::Blender * >(p.o)->blendMode = tmp;
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::LayeredEdgeFalloff *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::LayeredEdgeFalloff * >(
             p.cdb.allocateSpace(sizeof(CE2Material::LayeredEdgeFalloff),
                                 m->layeredEdgeFalloff));
    if (!m->layeredEdgeFalloff)
    {
      sp->falloffStartAngles[0] = 0.0f;
      sp->falloffStartAngles[1] = 0.0f;
      sp->falloffStartAngles[2] = 0.0f;
      sp->falloffStopAngles[0] = 0.0f;
      sp->falloffStopAngles[1] = 0.0f;
      sp->falloffStopAngles[2] = 0.0f;
      sp->falloffStartOpacities[0] = 0.0f;
      sp->falloffStartOpacities[1] = 0.0f;
      sp->falloffStartOpacities[2] = 0.0f;
      sp->falloffStopOpacities[0] = 0.0f;
      sp->falloffStopOpacities[1] = 0.0f;
      sp->falloffStopOpacities[2] = 0.0f;
      sp->activeLayersMask = 0;
      sp->useRGBFalloff = false;
    }
    m->layeredEdgeFalloff = sp;
    m->setFlags(CE2Material::Flag_LayeredEdgeFalloff, true);
  }
  BSReflStream::Chunk listBuf;
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 5U, isDiff); )
  {
    if (n <= 3U)
    {
      if (p.readChunk(listBuf) != BSReflStream::ChunkType_LIST)
      {
        errorMessage("unexpected chunk type for "
                     "BSMaterial::LayeredEdgeFalloffComponent");
      }
      if (listBuf.size() < 8 || !sp ||
          p.cdbBuf.findString(listBuf.readUInt32Fast())
          != BSReflStream::String_Float)
      {
        continue;
      }
      unsigned int  listSize = listBuf.readUInt32Fast();
      for (unsigned int i = 0U; i < listSize && i < 3U; i++)
      {
        float   tmp;
        if (!listBuf.readFloat(tmp))
          break;
        if (n == 0U)
          sp->falloffStartAngles[i] = tmp;
        else if (n == 1U)
          sp->falloffStopAngles[i] = tmp;
        else if (n == 2U)
          sp->falloffStartOpacities[i] = tmp;
        else
          sp->falloffStopOpacities[i] = tmp;
      }
    }
    else if (n == 4U)
    {
      unsigned char tmp;
      if (!p.readUInt8(tmp))
        break;
      if (sp)
        sp->activeLayersMask = tmp & 7;
    }
    else
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (sp)
        sp->useRGBFalloff = tmp;
    }
  }
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::VegetationSettings *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::VegetationSettings * >(
             p.cdb.allocateSpace(sizeof(CE2Material::VegetationSettings),
                                 m->vegetationSettings));
    if (!m->vegetationSettings)
    {
      sp->isEnabled = false;
      sp->leafFrequency = 0.0f;
      sp->leafAmplitude = 0.0f;
      sp->branchFlexibility = 0.0f;
      sp->trunkFlexibility = 0.0f;
      sp->terrainBlendStrength = 0.0f;
      sp->terrainBlendGradientFactor = 0.0f;
    }
    m->vegetationSettings = sp;
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 6U, isDiff); )
  {
    if (n == 0U)
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      m->setFlags(CE2Material::Flag_IsVegetation, tmp);
      sp->isEnabled = tmp;
    }
    else
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (!sp)
        continue;
      switch (n)
      {
        case 1U:
          sp->leafFrequency = tmp;
          break;
        case 2U:
          sp->leafAmplitude = tmp;
          break;
        case 3U:
          sp->branchFlexibility = tmp;
          break;
        case 4U:
          sp->trunkFlexibility = tmp;
          break;
        case 5U:
          sp->terrainBlendStrength = tmp;
          break;
        case 6U:
          sp->terrainBlendGradientFactor = tmp;
          break;
      }
    }
  }
}

// BSMaterial::TextureResolutionSetting
//   String  ResolutionHint

void CE2MaterialDB::ComponentInfo::readTextureResolutionSetting(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    unsigned char tmp = 0xFF;
    if (!p.readEnum(tmp, "\006Tiling\011UniqueMap"
                         "\017DetailMapTiling\020HighResUniqueMap"))
    {
      break;
    }
    if (p.o->type == 5 && tmp != 0xFF)
      static_cast< CE2Material::TextureSet * >(p.o)->resolutionHint = tmp;
  }
}

// BSBind::Address
//   List  Path

void CE2MaterialDB::ComponentInfo::readAddress(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSBind::DirectoryComponent
//   Ref  upDir

void CE2MaterialDB::ComponentInfo::readDirectoryComponent(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterialBinding::MaterialUVStreamPropertyNode
//   String  Name
//   BSMaterial::UVStreamID  StreamID
//   String  BindingType

void CE2MaterialDB::ComponentInfo::readMaterialUVStreamPropertyNode(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::ShaderRouteComponent
//   String  Route

void CE2MaterialDB::ComponentInfo::readShaderRouteComponent(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    unsigned char tmp = 7;
    if (!p.readEnum(tmp, "\010Deferred\006Effect\015PlanetaryRing"
                         "\025PrecomputedScattering\005Water"))
    {
      break;
    }
    if (p.o->type == 1)
      static_cast< CE2Material * >(p.o)->shaderRoute = tmp;
  }
}

// BSBind::FloatLerpController
//   Float  Duration
//   Bool  Loop
//   Float  Start
//   Float  End
//   String  Easing

void CE2MaterialDB::ComponentInfo::readFloatLerpController(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::ColorChannelTypeComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readColorChannelTypeComponent(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    unsigned char tmp = 0xFF;
    if (!p.readEnum(tmp, "\003Red\005Green\004Blue\005Alpha"))
      break;
    if (p.o->type == 2 && tmp != 0xFF)
      static_cast< CE2Material::Blender * >(p.o)->colorChannel = tmp;
  }
}

// BSMaterial::AlphaSettingsComponent
//   Bool  HasOpacity
//   Float  AlphaTestThreshold
//   String  OpacitySourceLayer
//   BSMaterial::AlphaBlenderSettings  Blender
//   Bool  UseDitheredTransparency

void CE2MaterialDB::ComponentInfo::readAlphaSettingsComponent(
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  if (p.o->type == 1) [[likely]]
    m = static_cast< CE2Material * >(p.o);
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 4U, isDiff); )
  {
    if ((1U << n) & 0x00000011U)        // 0, 4: booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!m)
        continue;
      if (!n)
        m->setFlags(CE2Material::Flag_HasOpacity, tmp);
      else
        m->setFlags(CE2Material::Flag_DitheredTransparency, tmp);
    }
    else if (n == 1U)
    {
      float   tmp;
      if (!p.readFloat0To1(tmp))
        break;
      if (m)
        m->alphaThreshold = tmp;
    }
    else if (n == 2U)
    {
      if (!p.readString())
        break;
      if (m)
        parseLayerNumber(m->alphaSourceLayer, p.stringBuf);
    }
    else
    {
      readAlphaBlenderSettings(p, isDiff);
    }
  }
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
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::TextureSetID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readTextureSetID(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    const CE2MaterialObject *o = readBSComponentDB2ID(p, isDiff, 5);
    if (p.o->type == 4)
    {
      static_cast< CE2Material::Material * >(p.o)->textureSet =
          static_cast< const CE2Material::TextureSet * >(o);
    }
  }
}

// BSBind::Float2DCurveController
//   BSFloat2DCurve  Curve
//   Bool  Loop
//   String  Mask

void CE2MaterialDB::ComponentInfo::readFloat2DCurveController(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::TextureFile
//   String  FileName

void CE2MaterialDB::ComponentInfo::readTextureFile(
    ComponentInfo& p, bool isDiff)
{
  if (p.componentType == BSReflStream::String_BSMaterial_TextureFile) [[likely]]
  {
    readMRTextureFile(p, isDiff);
    return;
  }
  if (p.componentType != BSReflStream::String_BSMaterial_DecalSettingsComponent)
    return;
  CE2Material::DecalSettings  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    CE2Material *m = static_cast< CE2Material * >(p.o);
    sp = const_cast< CE2Material::DecalSettings * >(m->decalSettings);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    const std::string *tmp;
    if (!p.readAndStoreString(tmp, 1))
      break;
    if (sp)
      sp->surfaceHeightMap = tmp;
  }
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material::TranslucencySettings *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    CE2Material *m = static_cast< CE2Material * >(p.o);
    sp = const_cast< CE2Material::TranslucencySettings * >(
             m->translucencySettings);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 9U, isDiff); )
  {
    if (n <= 2U)                        // 0, 1, 2: booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!sp)
        continue;
      if (n == 0U)
        sp->isThin = tmp;
      else if (n == 1U)
        sp->flipBackFaceNormalsInVS = tmp;
      else
        sp->useSSS = tmp;
    }
    else if (n <= 8U)                   // 3, 4, 5, 6, 7, 8: floats
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (!sp)
        continue;
      switch (n)
      {
        case 3U:
          sp->sssWidth = tmp;
          break;
        case 4U:
          sp->sssStrength = tmp;
          break;
        case 5U:
          sp->transmissiveScale = tmp;
          break;
        case 6U:
          sp->transmittanceWidth = tmp;
          break;
        case 7U:
          sp->specLobe0RoughnessScale = tmp;
          break;
        case 8U:
          sp->specLobe1RoughnessScale = tmp;
          break;
      }
    }
    else
    {
      if (!p.readString())
        break;
      if (sp)
        parseLayerNumber(sp->sourceLayer, p.stringBuf);
    }
  }
}

// BSMaterial::MouthSettingsComponent
//   Bool  Enabled
//   Bool  IsTeeth
//   String  AOVertexColorChannel

void CE2MaterialDB::ComponentInfo::readMouthSettingsComponent(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::DetailBlenderSettingsComponent
//   BSMaterial::DetailBlenderSettings  DetailBlenderSettings

void CE2MaterialDB::ComponentInfo::readDetailBlenderSettingsComponent(
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::DetailBlenderSettings  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::DetailBlenderSettings * >(
             p.cdb.allocateSpace(sizeof(CE2Material::DetailBlenderSettings),
                                 m->detailBlenderSettings));
    if (!m->detailBlenderSettings)
    {
      sp->isEnabled = false;
      sp->textureReplacementEnabled = false;
      sp->textureReplacement = 0xFFFFFFFFU;
      sp->texturePath = p.cdb.stringBuffers.front().data();
      sp->uvStream = nullptr;
    }
    m->detailBlenderSettings = sp;
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    readDetailBlenderSettings(p, isDiff);
  }
}

// BSComponentDB2::DBFileIndex::ObjectInfo
//   BSResource::ID  PersistentID
//   BSComponentDB2::ID  DBID
//   BSComponentDB2::ID  Parent
//   Bool  HasData

void CE2MaterialDB::ComponentInfo::readObjectInfo(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::StarmapBodyEffectComponent
//   Bool  Enabled
//   String  Type

void CE2MaterialDB::ComponentInfo::readStarmapBodyEffectComponent(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::MaterialParamFloat
//   Float  Value

void CE2MaterialDB::ComponentInfo::readMaterialParamFloat(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    float   tmp;
    if (!p.readFloat0To1(tmp))
      break;
    unsigned int  i = p.componentIndex;
    if (p.o->type == 2 && i < CE2Material::Blender::maxFloatParams)
      static_cast< CE2Material::Blender * >(p.o)->floatParams[i] = tmp;
    else if (p.o->type == 5)
      static_cast< CE2Material::TextureSet * >(p.o)->floatParam = tmp;
  }
}

// BSFloat2DCurve
//   BSFloatCurve  XCurve
//   BSFloatCurve  YCurve

void CE2MaterialDB::ComponentInfo::readBSFloat2DCurve(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSFloat3DCurve
//   BSFloatCurve  XCurve
//   BSFloatCurve  YCurve
//   BSFloatCurve  ZCurve

void CE2MaterialDB::ComponentInfo::readBSFloat3DCurve(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::TranslucencySettingsComponent
//   Bool  Enabled
//   BSMaterial::TranslucencySettings  Settings

void CE2MaterialDB::ComponentInfo::readTranslucencySettingsComponent(
    ComponentInfo& p, bool isDiff)
{
  CE2Material *m = nullptr;
  CE2Material::TranslucencySettings *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    sp = reinterpret_cast< CE2Material::TranslucencySettings * >(
             p.cdb.allocateSpace(sizeof(CE2Material::TranslucencySettings),
                                 m->translucencySettings));
    if (!m->translucencySettings)
    {
      sp->isEnabled = false;
      sp->isThin = false;
      sp->flipBackFaceNormalsInVS = false;
      sp->useSSS = false;
      sp->sssWidth = 0.2f;
      sp->sssStrength = 0.2f;
      sp->transmissiveScale = 1.0f;
      sp->transmittanceWidth = 0.03f;
      sp->specLobe0RoughnessScale = 0.55f;
      sp->specLobe1RoughnessScale = 1.2f;
      sp->sourceLayer = 0;              // "MATERIAL_LAYER_0"
    }
    m->translucencySettings = sp;
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 1U, isDiff); )
  {
    if (n == 0U)
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (m)
      {
        m->setFlags(CE2Material::Flag_Translucency, tmp);
        sp->isEnabled = tmp;
      }
    }
    else
    {
      readTranslucencySettings(p, isDiff);
    }
  }
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material::GlobalLayerData  *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    CE2Material *m = static_cast< CE2Material * >(p.o);
    sp = const_cast< CE2Material::GlobalLayerData * >(m->globalLayerData);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 9U, isDiff); )
  {
    if ((1U << n) & 0x000003F1U)        // 0, 4, 5, 6, 7, 8, 9: floats
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (!sp)
        continue;
      switch (n)
      {
        case 0U:
          sp->materialMaskIntensityScale = tmp;
          break;
        case 4U:
          sp->worldspaceScaleFactor = tmp;
          break;
        case 5U:
          sp->hurstExponent = tmp;
          break;
        case 6U:
          sp->baseFrequency = tmp;
          break;
        case 7U:
          sp->frequencyMultiplier = tmp;
          break;
        case 8U:
          sp->maskIntensityMin = tmp;
          break;
        case 9U:
          sp->maskIntensityMax = tmp;
          break;
      }
    }
    else if (n == 1U)
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (sp)
        sp->useNoiseMaskTexture = tmp;
    }
    else if (n == 2U)
    {
      if (sp) [[likely]]
      {
        if (!readSourceTextureWithReplacement(
                 sp->noiseMaskTexture, sp->noiseMaskTextureReplacement,
                 sp->noiseMaskTxtReplacementEnabled, p, isDiff))
        {
          break;
        }
      }
      else
      {
        const std::string *tmp1 = nullptr;
        std::uint32_t tmp2 = 0U;
        bool    tmp3 = false;
        if (!readSourceTextureWithReplacement(tmp1, tmp2, tmp3, p, isDiff))
          break;
      }
    }
    else
    {
      FloatVector4  tmp(0.0f);
      if (sp) [[likely]]
        readXMFLOAT4(sp->texcoordScaleAndBias, p, isDiff);
      else
        readXMFLOAT4(tmp, p, isDiff);
    }
  }
}

// BSMaterial::MaterialID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readMaterialID(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    const CE2MaterialObject *o = readBSComponentDB2ID(p, isDiff, 4);
    if (p.o->type == 3)
    {
      static_cast< CE2Material::Layer * >(p.o)->material =
          static_cast< const CE2Material::Material * >(o);
    }
  }
}

// XMFLOAT2
//   Float  x
//   Float  y

bool CE2MaterialDB::ComponentInfo::readXMFLOAT2L(
    FloatVector4& v, ComponentInfo& p, bool isDiff)
{
  bool    r = false;
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 1U, isDiff); )
  {
    if (!p.readFloat(v[n]))
      break;
    r = true;
  }
  return r;
}

bool CE2MaterialDB::ComponentInfo::readXMFLOAT2H(
    FloatVector4& v, ComponentInfo& p, bool isDiff)
{
  bool    r = false;
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 1U, isDiff); )
  {
    if (!p.readFloat(v[n + 2U]))
      break;
    r = true;
  }
  return r;
}

// BSBind::TimerController

void CE2MaterialDB::ComponentInfo::readTimerController(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSBind::Controllers
//   List  MappingsA
//   Bool  UseRandomOffset

void CE2MaterialDB::ComponentInfo::readControllers(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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
    ComponentInfo& p, bool isDiff)
{
  CE2Material::EmissiveSettings *sp = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    CE2Material *m = static_cast< CE2Material * >(p.o);
    sp = const_cast< CE2Material::EmissiveSettings * >(m->emissiveSettings);
  }
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 9U, isDiff); )
  {
    if ((1U << n) & 0x00000368U)        // 3, 5, 6, 8, 9: floats
    {
      float   tmp;
      if (!p.readFloat(tmp))
        break;
      if (!sp)
        continue;
      if (n == 3U)
        sp->clipThreshold = tmp;
      else if (n == 5U)
        sp->luminousEmittance = tmp;
      else if (n == 6U)
        sp->exposureOffset = tmp;
      else if (n == 8U)
        sp->maxOffset = tmp;
      else
        sp->minOffset = tmp;
    }
    else if ((1U << n) & 0x00000090U)   // 4, 7: booleans
    {
      bool    tmp;
      if (!p.readBool(tmp))
        break;
      if (!sp)
        continue;
      if (n == 4U)
        sp->adaptiveEmittance = tmp;
      else
        sp->enableAdaptiveLimits = tmp;
    }
    else if (n == 0U)
    {
      if (!p.readString())
        break;
      if (sp)
        parseLayerNumber(sp->sourceLayer, p.stringBuf);
    }
    else if (n == 1U)
    {
      if (!sp)
      {
        FloatVector4  c(0.0f);
        readColorValue(c, p, isDiff);
      }
      else
      {
        readColorValue(sp->emissiveTint, p, isDiff);
      }
    }
    else
    {
      unsigned char tmp = 0xFF;
      if (!p.readEnum(tmp, "\004None\010Blender1\010Blender2\010Blender3"))
        break;
      if (sp && tmp != 0xFF)
        sp->maskSourceBlender = tmp;
    }
  }
}

// BSMaterial::ShaderModelComponent
//   String  FileName

void CE2MaterialDB::ComponentInfo::readShaderModelComponent(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    if (!p.readString())
      break;
    if (p.o->type != 1)
      continue;
    CE2Material *m = static_cast< CE2Material * >(p.o);
    size_t  n0 = 0;
    size_t  n2 = sizeof(CE2Material::shaderModelNames) / sizeof(char *);
    while ((n2 - n0) >= 2)
    {
      size_t  n1 = (n0 + n2) >> 1;
      if (p.stringBuf.compare(CE2Material::shaderModelNames[n1]) < 0)
        n2 = n1;
      else
        n0 = n1;
    }
    unsigned char tmp = 55;             // "Unknown"
    if (p.stringBuf == CE2Material::shaderModelNames[n0])
      tmp = (unsigned char) n0;
    m->shaderModel = tmp;
    m->setFlags(CE2Material::Flag_TwoSided,
                bool((1ULL << tmp) & 0xF060000000000000ULL));
  }
}

// BSResource::ID
//   UInt32  Dir
//   UInt32  File
//   UInt32  Ext

void CE2MaterialDB::ComponentInfo::readBSResourceID(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// XMFLOAT3
//   Float  x
//   Float  y
//   Float  z

bool CE2MaterialDB::ComponentInfo::readXMFLOAT3(
    FloatVector4& v, ComponentInfo& p, bool isDiff)
{
  bool    r = false;
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 2U, isDiff); )
  {
    if (!p.readFloat(v[n]))
      break;
    r = true;
  }
  return r;
}

// BSMaterial::MaterialOverrideColorTypeComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readMaterialOverrideColorTypeComponent(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    unsigned char tmp = 0xFF;
    if (!p.readEnum(tmp, "\010Multiply\004Lerp"))
      break;
    if (p.o->type == 4 && tmp != 0xFF)
      static_cast< CE2Material::Material * >(p.o)->colorMode = tmp;
  }
}

// BSMaterial::Channel
//   String  Value

void CE2MaterialDB::ComponentInfo::readChannel(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    unsigned char tmp = 0xFF;
    if (!p.readEnum(tmp, "\004Zero\003One\003Two\005Three"))
      break;
    if (p.o->type == 6 &&
        p.componentType == BSReflStream::String_BSMaterial_Channel &&
        tmp != 0xFF)
    {
      static_cast< CE2Material::UVStream * >(p.o)->channel = tmp;
    }
  }
}

// BSBind::ColorCurveController
//   BSColorCurve  Curve
//   Bool  Loop
//   String  Mask

void CE2MaterialDB::ComponentInfo::readColorCurveController(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::Internal::CompiledDB
//   String  BuildVersion
//   Map  HashMap
//   List  Collisions
//   List  Circular

void CE2MaterialDB::ComponentInfo::readCompiledDB(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
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

void CE2MaterialDB::ComponentInfo::readHairSettingsComponent(
    ComponentInfo& p, bool isDiff)
{
  (void) isDiff;
  CE2Material *m = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    m->setFlags(CE2Material::Flag_IsHair, true);
  }
}

// BSColorCurve
//   BSFloatCurve  RedChannel
//   BSFloatCurve  GreenChannel
//   BSFloatCurve  BlueChannel
//   BSFloatCurve  AlphaChannel

void CE2MaterialDB::ComponentInfo::readBSColorCurve(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSBind::ControllerComponent
//   Ref  upControllers

void CE2MaterialDB::ComponentInfo::readControllerComponent(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::MRTextureFile
//   String  FileName

void CE2MaterialDB::ComponentInfo::readMRTextureFile(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    const std::string *txtPath;
    if (!p.readAndStoreString(txtPath, 1))
      break;
    unsigned int  i = p.componentIndex;
    if (p.o->type == 5 &&
        i < CE2Material::TextureSet::maxTexturePaths) [[likely]]
    {
      CE2Material::TextureSet *txtSet =
          static_cast< CE2Material::TextureSet * >(p.o);
      txtSet->texturePaths[i] = txtPath;
      if (txtPath->empty())
        txtSet->texturePathMask &= ~(1U << i);
      else
        txtSet->texturePathMask |= (1U << i);
    }
    else if (p.o->type == 2)
    {
      static_cast< CE2Material::Blender * >(p.o)->texturePath = txtPath;
    }
  }
}

// BSMaterial::TextureSetKindComponent
//   String  Value

void CE2MaterialDB::ComponentInfo::readTextureSetKindComponent(
    ComponentInfo& p, bool isDiff)
{
  (void) p;
  (void) isDiff;
}

// BSMaterial::BlenderID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readBlenderID(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    const CE2MaterialObject *o = readBSComponentDB2ID(p, isDiff, 2);
    if (p.o->type == 1 && p.componentIndex < CE2Material::maxBlenders)
    {
      static_cast< CE2Material * >(p.o)->blenders[p.componentIndex] =
          static_cast< const CE2Material::Blender * >(o);
    }
  }
}

// BSMaterial::CollisionComponent
//   BSMaterial::PhysicsMaterialType  MaterialTypeOverride

void CE2MaterialDB::ComponentInfo::readCollisionComponent(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
    readPhysicsMaterialType(p, isDiff);
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
    ComponentInfo& p, bool isDiff)
{
  (void) isDiff;
  CE2Material *m = nullptr;
  if (p.o->type == 1) [[likely]]
  {
    m = static_cast< CE2Material * >(p.o);
    m->setFlags(CE2Material::Flag_IsTerrain, true);
  }
}

// BSMaterial::LODMaterialID
//   BSComponentDB2::ID  ID

void CE2MaterialDB::ComponentInfo::readLODMaterialID(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    const CE2MaterialObject *o = readBSComponentDB2ID(p, isDiff, 1);
    if (p.o->type == 1 && p.componentIndex < CE2Material::maxLODMaterials)
    {
      static_cast< CE2Material * >(p.o)->lodMaterials[p.componentIndex] =
          static_cast< const CE2Material * >(o);
    }
  }
}

// BSMaterial::MipBiasSetting
//   Bool  DisableMipBiasHint

void CE2MaterialDB::ComponentInfo::readMipBiasSetting(
    ComponentInfo& p, bool isDiff)
{
  for (unsigned int n = 0U - 1U; p.getFieldNumber(n, 0U, isDiff); )
  {
    bool    tmp;
    if (!p.readBool(tmp))
      break;
    if (p.o->type == 5)
      static_cast< CE2Material::TextureSet * >(p.o)->disableMipBiasHint = tmp;
  }
}

const CE2MaterialDB::ComponentInfo::ReadFunctionType
    CE2MaterialDB::ComponentInfo::readFunctionTable[128] =
{
  // indexed by string ID
  // (returned by BSReflStream::getClassName(strtOffs)) - 128
  (ReadFunctionType) 0,         // "BSBind::ColorLerpController"
  (ReadFunctionType) 0,         // "BSBind::ComponentProperty"
  &readControllerComponent,     // "BSBind::ControllerComponent"
  (ReadFunctionType) 0,         // "BSBind::Controllers"
  (ReadFunctionType) 0,         // "BSBind::Controllers::Mapping"
  (ReadFunctionType) 0,         // "BSBind::Directory"
  &readDirectoryComponent,      // "BSBind::DirectoryComponent"
  (ReadFunctionType) 0,         // "BSBind::Float2DCurveController"
  (ReadFunctionType) 0,         // "BSBind::Float2DLerpController"
  (ReadFunctionType) 0,         // "BSBind::Float3DCurveController"
  (ReadFunctionType) 0,         // "BSBind::Float3DLerpController"
  (ReadFunctionType) 0,         // "BSBind::FloatCurveController"
  (ReadFunctionType) 0,         // "BSBind::FloatLerpController"
  (ReadFunctionType) 0,         // "BSBind::Multiplex"
  (ReadFunctionType) 0,         // "BSBind::Snapshot"
  (ReadFunctionType) 0,         // "BSBind::Snapshot::Entry"
  (ReadFunctionType) 0,         // "BSBind::TimerController"
  (ReadFunctionType) 0,         // "BSBlendable::ColorValue"
  (ReadFunctionType) 0,         // "BSBlendable::FloatValue"
  (ReadFunctionType) 0,         // "BSColorCurve"
  (ReadFunctionType) 0,         // "BSComponentDB2::DBFileIndex"
  (ReadFunctionType) 0,         // "BSComponentDB2::DBFileIndex::ComponentInfo"
  (ReadFunctionType) 0,     // "BSComponentDB2::DBFileIndex::ComponentTypeInfo"
  (ReadFunctionType) 0,         // "BSComponentDB2::DBFileIndex::EdgeInfo"
  (ReadFunctionType) 0,         // "BSComponentDB2::DBFileIndex::ObjectInfo"
  (ReadFunctionType) 0,         // "BSComponentDB2::ID"
  &readCTName,                  // "BSComponentDB::CTName"
  (ReadFunctionType) 0,         // "BSFloat2DCurve"
  (ReadFunctionType) 0,         // "BSFloat3DCurve"
  (ReadFunctionType) 0,         // "BSFloatCurve"
  (ReadFunctionType) 0,         // "BSFloatCurve::Control"
  (ReadFunctionType) 0,         // "BSGalaxy::BGSSunPresetForm"
  (ReadFunctionType) 0,     // "BSGalaxy::BGSSunPresetForm::DawnDuskSettings"
  (ReadFunctionType) 0,         // "BSGalaxy::BGSSunPresetForm::NightSettings"
  (ReadFunctionType) 0,         // "BSHoudini::HoudiniAssetData"
  (ReadFunctionType) 0,         // "BSHoudini::HoudiniAssetData::Parameter"
  (ReadFunctionType) 0,         // "BSMaterial::AlphaBlenderSettings"
  &readAlphaSettingsComponent,  // "BSMaterial::AlphaSettingsComponent"
  &readBlendModeComponent,      // "BSMaterial::BlendModeComponent"
  &readBlendParamFloat,         // "BSMaterial::BlendParamFloat"
  &readBlenderID,               // "BSMaterial::BlenderID"
  &readChannel,                 // "BSMaterial::Channel"
  &readCollisionComponent,      // "BSMaterial::CollisionComponent"
  &readColor,                   // "BSMaterial::Color"
  &readColorChannelTypeComponent,   // "BSMaterial::ColorChannelTypeComponent"
  &readColorRemapSettingsComponent, // "BSMaterial::ColorRemapSettingsComponent"
  &readDecalSettingsComponent,  // "BSMaterial::DecalSettingsComponent"
  (ReadFunctionType) 0,         // "BSMaterial::DetailBlenderSettings"
  // "BSMaterial::DetailBlenderSettingsComponent"
  &readDetailBlenderSettingsComponent,
  &readDistortionComponent,     // "BSMaterial::DistortionComponent"
  &readEffectSettingsComponent, // "BSMaterial::EffectSettingsComponent"
  &readEmissiveSettingsComponent,   // "BSMaterial::EmissiveSettingsComponent"
  (ReadFunctionType) 0,         // "BSMaterial::EmittanceSettings"
  &readEyeSettingsComponent,    // "BSMaterial::EyeSettingsComponent"
  &readFlipbookComponent,       // "BSMaterial::FlipbookComponent"
  &readFlowSettingsComponent,   // "BSMaterial::FlowSettingsComponent"
  &readGlobalLayerDataComponent,    // "BSMaterial::GlobalLayerDataComponent"
  (ReadFunctionType) 0,         // "BSMaterial::GlobalLayerNoiseSettings"
  &readHairSettingsComponent,   // "BSMaterial::HairSettingsComponent"
  (ReadFunctionType) 0,         // "BSMaterial::Internal::CompiledDB"
  (ReadFunctionType) 0,         // "BSMaterial::Internal::CompiledDB::FilePair"
  &readLODMaterialID,           // "BSMaterial::LODMaterialID"
  &readLayerID,                 // "BSMaterial::LayerID"
  &readLayeredEdgeFalloffComponent, // "BSMaterial::LayeredEdgeFalloffComponent"
  &readLayeredEmissivityComponent,  // "BSMaterial::LayeredEmissivityComponent"
  &readLevelOfDetailSettings,   // "BSMaterial::LevelOfDetailSettings"
  &readMRTextureFile,           // "BSMaterial::MRTextureFile"
  &readMaterialID,              // "BSMaterial::MaterialID"
  // "BSMaterial::MaterialOverrideColorTypeComponent"
  &readMaterialOverrideColorTypeComponent,
  &readMaterialParamFloat,      // "BSMaterial::MaterialParamFloat"
  &readMipBiasSetting,          // "BSMaterial::MipBiasSetting"
  &readMouthSettingsComponent,  // "BSMaterial::MouthSettingsComponent"
  &readOffset,                  // "BSMaterial::Offset"
  &readOpacityComponent,        // "BSMaterial::OpacityComponent"
  &readParamBool,               // "BSMaterial::ParamBool"
  (ReadFunctionType) 0,         // "BSMaterial::PhysicsMaterialType"
  (ReadFunctionType) 0,         // "BSMaterial::ProjectedDecalSettings"
  &readScale,                   // "BSMaterial::Scale"
  &readShaderModelComponent,    // "BSMaterial::ShaderModelComponent"
  &readShaderRouteComponent,    // "BSMaterial::ShaderRouteComponent"
  (ReadFunctionType) 0,         // "BSMaterial::SourceTextureWithReplacement"
  &readStarmapBodyEffectComponent,  // "BSMaterial::StarmapBodyEffectComponent"
  &readTerrainSettingsComponent,    // "BSMaterial::TerrainSettingsComponent"
  // "BSMaterial::TerrainTintSettingsComponent"
  &readTerrainTintSettingsComponent,
  &readTextureAddressModeComponent, // "BSMaterial::TextureAddressModeComponent"
  &readTextureFile,             // "BSMaterial::TextureFile"
  &readTextureReplacement,      // "BSMaterial::TextureReplacement"
  &readTextureResolutionSetting,    // "BSMaterial::TextureResolutionSetting"
  &readTextureSetID,            // "BSMaterial::TextureSetID"
  &readTextureSetKindComponent, // "BSMaterial::TextureSetKindComponent"
  (ReadFunctionType) 0,         // "BSMaterial::TranslucencySettings"
  // "BSMaterial::TranslucencySettingsComponent"
  &readTranslucencySettingsComponent,
  &readUVStreamID,              // "BSMaterial::UVStreamID"
  &readUVStreamParamBool,       // "BSMaterial::UVStreamParamBool"
  &readVegetationSettingsComponent, // "BSMaterial::VegetationSettingsComponent"
  &readWaterFoamSettingsComponent,  // "BSMaterial::WaterFoamSettingsComponent"
  &readWaterGrimeSettingsComponent, // "BSMaterial::WaterGrimeSettingsComponent"
  &readWaterSettingsComponent,  // "BSMaterial::WaterSettingsComponent"
  (ReadFunctionType) 0,         // "BSMaterialBinding::MaterialPropertyNode"
  (ReadFunctionType) 0,     // "BSMaterialBinding::MaterialUVStreamPropertyNode"
  (ReadFunctionType) 0,         // "BSResource::ID"
  (ReadFunctionType) 0,         // "BSSequence::AnimationEvent"
  (ReadFunctionType) 0,         // "BSSequence::AnimationTrack"
  (ReadFunctionType) 0,         // "BSSequence::CameraShakeEvent"
  (ReadFunctionType) 0,         // "BSSequence::CameraShakeStrengthTrack"
  (ReadFunctionType) 0,         // "BSSequence::CameraShakeTrack"
  (ReadFunctionType) 0,         // "BSSequence::ColorCurveEvent"
  (ReadFunctionType) 0,         // "BSSequence::ColorLerpEvent"
  (ReadFunctionType) 0,         // "BSSequence::ColorTriggerEvent"
  (ReadFunctionType) 0,         // "BSSequence::CullEvent"
  (ReadFunctionType) 0,         // "BSSequence::DissolveEvent"
  (ReadFunctionType) 0,         // "BSSequence::DissolveFrequencyScaleTrack"
  (ReadFunctionType) 0,         // "BSSequence::DissolveOffsetTrack"
  (ReadFunctionType) 0,         // "BSSequence::DissolveTrack"
  (ReadFunctionType) 0,         // "BSSequence::ExplosionObjectSpawn"
  (ReadFunctionType) 0,         // "BSSequence::Float2LerpEvent"
  (ReadFunctionType) 0,         // "BSSequence::Float2TriggerEvent"
  (ReadFunctionType) 0,         // "BSSequence::FloatCurveEvent"
  (ReadFunctionType) 0,         // "BSSequence::FloatLerpEvent"
  (ReadFunctionType) 0,         // "BSSequence::FloatNoiseEvent"
  (ReadFunctionType) 0,         // "BSSequence::FloatTriggerEvent"
  (ReadFunctionType) 0,         // "BSSequence::ImageSpaceLifetimeEvent"
  (ReadFunctionType) 0,         // "BSSequence::ImageSpaceStrengthTrack"
  (ReadFunctionType) 0,         // "BSSequence::ImageSpaceTrack"
  (ReadFunctionType) 0,         // "BSSequence::ImpactEffectEvent"
  (ReadFunctionType) 0,         // "BSSequence::ImpactEffectTrack"
  (ReadFunctionType) 0,         // "BSSequence::LightColorTrack"
  (ReadFunctionType) 0          // "BSSequence::LightEffectReferenceTrack"
};

