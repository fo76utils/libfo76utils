
#ifndef MATERIAL_HPP_INCLUDED
#define MATERIAL_HPP_INCLUDED

#include "common.hpp"
#include "fp32vec4.hpp"
#include "bsmatcdb.hpp"

#include <mutex>

// CE2Material (.mat file), object type 1
//   |
//   +--(BSMaterial::BlenderID)-- CE2Material::Blender, object type 2
//   |      |
//   |      +--(BSMaterial::UVStreamID)-- CE2Material::UVStream, object type 6
//   +--(BSMaterial::LayerID)-- CE2Material::Layer, object type 3
//   |      |
//   |      +--(BSMaterial::UVStreamID)-- CE2Material::UVStream, object type 6
//   |      |
//   |      +--(BSMaterial::MaterialID)-- CE2Material::Material, object type 4
//   |             |
//   |       (BSMaterial::TextureSetID)-- CE2Material::TextureSet, object type 5
//   +--(BSMaterial::LODMaterialID)-- LOD material, object type 1

struct CE2MaterialObject
{
  // 0: CE2MaterialObject
  // 1: CE2Material
  // 2: CE2Material::Blender
  // 3: CE2Material::Layer
  // 4: CE2Material::Material
  // 5: CE2Material::TextureSet
  // 6: CE2Material::UVStream
  unsigned char type;
  const BSMaterialsCDB::MaterialObject  *cdbObject;     // valid if type > 0
  const char  *name;
  const CE2MaterialObject *parent;
  CE2MaterialObject(unsigned char t = 0)
    : type(t),
      cdbObject(nullptr),
      name(""),
      parent(nullptr)
  {
  }
  void printObjectInfo(std::string& buf, size_t indentCnt) const;
};

struct CE2Material : public CE2MaterialObject   // object type 1
{
  struct UVStream : public CE2MaterialObject    // object type 6
  {
    // U scale, V scale, U offset, V offset
    FloatVector4  scaleAndOffset;
    // 0 = "Wrap", 1 = "Clamp", 2 = "Mirror", 3 = "Border"
    unsigned char textureAddressMode;
    // 1 = "One" (default), 2 = "Two"
    unsigned char channel;
    UVStream();
    void printObjectInfo(std::string& buf, size_t indentCnt) const;
  };
  struct TextureSet : public CE2MaterialObject  // object type 5
  {
    enum
    {
      maxTexturePaths = 21
    };
    std::uint32_t texturePathMask;
    float   floatParam;                 // normal map intensity
    // texturePaths[0] =  albedo (_color.dds)
    // texturePaths[1] =  normal map (_normal.dds)
    // texturePaths[2] =  alpha (_opacity.dds)
    // texturePaths[3] =  roughness (_rough.dds)
    // texturePaths[4] =  metalness (_metal.dds)
    // texturePaths[5] =  ambient occlusion (_ao.dds)
    // texturePaths[6] =  height map (_height.dds)
    // texturePaths[7] =  glow map (_emissive.dds)
    // texturePaths[8] =  translucency (_transmissive.dds)
    // texturePaths[9] =  _curvature.dds
    // texturePaths[10] = _mask.dds
    // texturePaths[12] = _zoffset.dds
    // texturePaths[14] = overlay color
    // texturePaths[15] = overlay roughness
    // texturePaths[16] = overlay metalness
    // texturePaths[20] = _id.dds
    // NOTE: string view pointers are always valid and the strings are
    // null-terminated
    const std::string_view  *texturePaths[maxTexturePaths];
    // texture replacements are colors in R8G8B8A8 format
    std::uint32_t textureReplacementMask;
    std::uint32_t textureReplacements[maxTexturePaths];
    // 0 = "Tiling" (default), 1 = "UniqueMap", 2 = "DetailMapTiling",
    // 3 = "HighResUniqueMap"
    unsigned char resolutionHint;
    bool    disableMipBiasHint;
    TextureSet();
    void printObjectInfo(std::string& buf, size_t indentCnt) const;
  };
  struct Material : public CE2MaterialObject    // object type 4
  {
    FloatVector4  color;
    // bit 0 = MaterialOverrideColorTypeComponent, 0: "Multiply", 1: "Lerp"
    // bit 1 = ParamBool, 1: use vertex color as tint
    unsigned char colorModeFlags;
    // bit 0 = is flipbook, bit 1 = loops
    unsigned char flipbookFlags;
    unsigned char flipbookColumns;
    unsigned char flipbookRows;
    float   flipbookFPS;
    const TextureSet  *textureSet;
    Material();
    void printObjectInfo(std::string& buf, size_t indentCnt) const;
  };
  struct Layer : public CE2MaterialObject       // object type 3
  {
    const Material  *material;
    const UVStream  *uvStream;
    Layer();
    void printObjectInfo(std::string& buf, size_t indentCnt) const;
  };
  struct Blender : public CE2MaterialObject     // object type 2
  {
    enum
    {
      maxFloatParams = 5,
      maxBoolParams = 8
    };
    const UVStream  *uvStream;
    const std::string_view  *texturePath;
    std::uint32_t textureReplacement;
    bool    textureReplacementEnabled;
    // 0 = "Linear" (default), 1 = "Additive", 2 = "PositionContrast",
    // 3 = "None", 4 = "CharacterCombine", 5 = "Skin"
    unsigned char blendMode;
    // 0 = "Red" (default), 1 = "Green", 2 = "Blue", 3 = "Alpha"
    unsigned char colorChannel;
    // values set via BSMaterial::MaterialParamFloat and BSMaterial::ParamBool
    // 0: height blend threshold
    // 1: height blend factor
    // 2: position
    // 3: 1.0 - contrast
    // 4: mask intensity
    float   floatParams[maxFloatParams];
    // 0: blend albedo texture
    // 1: blend metalness texture
    // 2: blend roughness texture
    // 3: blend normal map texture
    // 4: blend normals additively
    // 5: vertex alpha
    // 6: blend ambient occlusion texture
    // 7: use dual blend mask
    bool    boolParams[maxBoolParams];
    Blender();
    void printObjectInfo(std::string& buf, size_t indentCnt) const;
  };
  enum
  {
    textureNumColor = 0,
    textureNumNormal = 1,
    textureNumOpacity = 2,
    textureNumRough = 3,
    textureNumMetal = 4,
    textureNumAO = 5,
    textureNumHeight = 6,
    textureNumEmissive = 7,
    textureNumTransmissive = 8,
    textureNumCurvature = 9,
    textureNumMask = 10
  };
  enum
  {
    // flag values can be combined (bitwise OR) with NIFFile::NIFTriShape::flags
    Flag_HasOpacity = 0x00000001,
    Flag_AlphaVertexColor = 0x00000002,
    Flag_IsEffect = 0x00000004,
    Flag_IsDecal = 0x00000008,
    Flag_TwoSided = 0x00000010,
    Flag_IsVegetation = 0x00000020,
    Flag_LayeredEmissivity = 0x00000040,
    Flag_Glow = 0x00000080,
    Flag_Emissive = 0x00000080,
    Flag_Translucency = 0x00000100,
    Flag_AlphaDetailBlendMask = 0x00000200,
    Flag_DitheredTransparency = 0x00000400,
    // Flag_TSOrdered = 0x00000800,
    Flag_AlphaBlending = 0x00001000,
    // Flag_TSVertexColors = 0x00002000,
    Flag_IsWater = 0x00004000,
    // Flag_TSHidden = 0x00008000,
    Flag_HasOpacityComponent = 0x00010000,
    Flag_OpacityLayer2Active = 0x00020000,
    Flag_OpacityLayer3Active = 0x00040000,
    Flag_IsTerrain = 0x00080000,
    Flag_IsHair = 0x00100000,
    Flag_UseDetailBlender = 0x00200000,
    Flag_LayeredEdgeFalloff = 0x00400000,
    Flag_GlobalLayerData = 0x00800000
    // Flag_TSMarker = 0x01000000
  };
  enum
  {
    EffectFlag_UseFalloff = 0x00000001,
    EffectFlag_UseRGBFalloff = 0x00000002,
    EffectFlag_VertexColorBlend = 0x00000040,
    EffectFlag_IsAlphaTested = 0x00000080,
    EffectFlag_NoHalfResOpt = 0x00000200,
    EffectFlag_SoftEffect = 0x00000400,
    EffectFlag_EmissiveOnly = 0x00001000,
    EffectFlag_EmissiveOnlyAuto = 0x00002000,
    EffectFlag_DirShadows = 0x00004000,
    EffectFlag_NonDirShadows = 0x00008000,
    EffectFlag_IsGlass = 0x00010000,
    EffectFlag_Frosting = 0x00020000,
    EffectFlag_ZTest = 0x00200000,
    EffectFlag_ZWrite = 0x00400000,
    EffectFlag_BacklightEnable = 0x01000000,
    EffectFlag_RenderBeforeClouds = 0x10000000,
    EffectFlag_MVFixup = 0x20000000,
    EffectFlag_MVFixupEdgesOnly = 0x40000000,
    EffectFlag_RenderBeforeOIT = 0x80000000U
  };
  enum
  {
    maxLayers = 6,
    maxBlenders = 5,
    maxLODMaterials = 3
  };
  struct EffectSettings
  {
    std::uint32_t flags;
    // 0 = "AlphaBlend", 1 = "Additive", 2 = "SourceSoftAdditive",
    // 3 = "Multiply", 4 = "DestinationSoftAdditive",
    // 5 = "DestinationInvertedSoftAdditive", 6 = "TakeSmaller", 7 = "None"
    unsigned char blendMode;
    float   falloffStartAngle;
    float   falloffStopAngle;
    float   falloffStartOpacity;
    float   falloffStopOpacity;
    float   alphaThreshold;
    float   softFalloffDepth;
    float   frostingBgndBlend;
    float   frostingBlurBias;
    float   materialAlpha;
    float   backlightScale;
    float   backlightSharpness;
    float   backlightTransparency;
    FloatVector4  backlightTintColor;
    int     depthBias;
    EffectSettings();
    inline void setFlags(std::uint32_t m, bool n)
    {
      flags = (flags & ~m) | ((0U - std::uint32_t(n)) & m);
    }
  };
  struct EmissiveSettings
  {
    bool    isEnabled;
    unsigned char sourceLayer;
    unsigned char maskSourceBlender;    // 0: "None", 1: "Blender1"
    bool    adaptiveEmittance;
    bool    enableAdaptiveLimits;
    float   clipThreshold;
    float   luminousEmittance;
    FloatVector4  emissiveTint;         // R, G, B, overall scale
    float   exposureOffset;
    float   maxOffset;
    float   minOffset;
    EmissiveSettings();
  };
  struct LayeredEmissiveSettings
  {
    bool    isEnabled;
    unsigned char layer1Index;          // "MATERIAL_LAYER_n"
    unsigned char layer1MaskIndex;      // 0: "None", 1: "Blender1"
    bool    layer2Active;
    unsigned char layer2Index;
    unsigned char layer2MaskIndex;
    unsigned char blender1Index;        // "BLEND_LAYER_n"
    // 0 = "Lerp", 1 = "Additive", 2 = "Subtractive", 3 = "Multiplicative"
    unsigned char blender1Mode;
    bool    layer3Active;
    unsigned char layer3Index;
    unsigned char layer3MaskIndex;
    unsigned char blender2Index;
    unsigned char blender2Mode;
    bool    adaptiveEmittance;
    bool    enableAdaptiveLimits;
    bool    ignoresFog;
    std::uint32_t layer1Tint;           // R, G, B, overall scale
    std::uint32_t layer2Tint;
    std::uint32_t layer3Tint;
    float   clipThreshold;
    float   luminousEmittance;
    float   exposureOffset;
    float   maxOffset;
    float   minOffset;
    LayeredEmissiveSettings();
  };
  struct TranslucencySettings
  {
    bool    isEnabled;
    bool    isThin;
    bool    flipBackFaceNormalsInVS;
    bool    useSSS;
    float   sssWidth;
    float   sssStrength;
    float   transmissiveScale;
    float   transmittanceWidth;
    float   specLobe0RoughnessScale;
    float   specLobe1RoughnessScale;
    unsigned char sourceLayer;          // default: 0 ("MATERIAL_LAYER_0")
    TranslucencySettings();
  };
  struct DecalSettings
  {
    bool    isDecal;
    bool    isPlanet;
    // 0 = "None" (default), 1 = "Additive"
    unsigned char blendMode;
    bool    animatedDecalIgnoresTAA;
    float   decalAlpha;
    // bits 0-2: output albedo R, G, B
    // bits 4-5: output normal X, Y
    // bit 8:    output ambient occlusion
    // bit 9:    output roughness
    // bit 10:   output metalness
    // bit 20:   output emissive R
    // bit 21:   output emissive G
    // bit 22:   output emissive B
    // bit 23:   output emissive A
    // defaults to 0x0737 (all channels enabled)
    std::uint32_t writeMask;
    bool    isProjected;
    // projected decal settings
    bool    useParallaxMapping;
    bool    parallaxOcclusionShadows;
    unsigned char maxParallaxSteps;
    const std::string_view  *surfaceHeightMap;
    float   parallaxOcclusionScale;
    // 0 = "Top", 1 = "Middle", 2 = "Bottom" (default)
    unsigned char renderLayer;
    bool    useGBufferNormals;
    DecalSettings();
  };
  struct VegetationSettings
  {
    bool    isEnabled;
    float   leafFrequency;
    float   leafAmplitude;
    float   branchFlexibility;
    float   trunkFlexibility;
    float   terrainBlendStrength;       // the last two variables are deprecated
    float   terrainBlendGradientFactor;
    VegetationSettings();
  };
  struct DetailBlenderSettings
  {
    bool    isEnabled;
    bool    textureReplacementEnabled;
    std::uint32_t textureReplacement;
    const std::string_view  *texturePath;
    const UVStream  *uvStream;
    DetailBlenderSettings();
  };
  struct LayeredEdgeFalloff
  {
    float   falloffStartAngles[3];
    float   falloffStopAngles[3];
    float   falloffStartOpacities[3];
    float   falloffStopOpacities[3];
    unsigned char activeLayersMask;
    bool    useRGBFalloff;
    LayeredEdgeFalloff();
  };
  struct WaterSettings
  {
    float   waterEdgeFalloff;
    float   waterWetnessMaxDepth;
    float   waterEdgeNormalFalloff;
    float   waterDepthBlur;
    // color R, color G, color B, refraction magnitude
    FloatVector4  reflectance;
    // color R, color G, color B, max. concentration
    FloatVector4  phytoplanktonReflectance;
    FloatVector4  sedimentReflectance;
    FloatVector4  yellowMatterReflectance;
    bool    lowLOD;
    bool    placedWater;
    WaterSettings();
  };
  struct GlobalLayerData
  {
    float   texcoordScaleXY;
    float   texcoordScaleYZ;
    float   texcoordScaleXZ;
    bool    usesDirectionality;
    bool    blendNormalsAdditively;
    bool    useNoiseMaskTexture;
    bool    noiseMaskTxtReplacementEnabled;
    FloatVector4  albedoTintColor;
    // sourceDirection[3] = directionalityIntensity
    FloatVector4  sourceDirection;
    float   directionalityScale;
    float   directionalitySaturation;
    float   blendPosition;      // used if blendNormalsAdditively is false
    float   blendContrast;
    // GlobalLayerNoiseData
    float   materialMaskIntensityScale;
    std::uint32_t noiseMaskTextureReplacement;
    const std::string_view  *noiseMaskTexture;
    FloatVector4  texcoordScaleAndBias;
    float   worldspaceScaleFactor;
    float   hurstExponent;
    float   baseFrequency;
    float   frequencyMultiplier;
    float   maskIntensityMin;
    float   maskIntensityMax;
    GlobalLayerData();
  };
  struct HairSettings
  {
    bool    isEnabled;
    bool    isSpikyHair;
    unsigned char depthOffsetMaskVertexColorChannel;
    unsigned char aoVertexColorChannel;
    float   specScale;
    float   specularTransmissionScale;
    float   directTransmissionScale;
    float   diffuseTransmissionScale;
    float   roughness;
    float   contactShadowSoftening;
    float   backscatterStrength;
    float   backscatterWrap;
    float   variationStrength;
    float   indirectSpecularScale;
    float   indirectSpecularTransmissionScale;
    float   indirectSpecRoughness;
    float   edgeMaskContrast;
    float   edgeMaskMin;
    float   edgeMaskDistanceMin;
    float   edgeMaskDistanceMax;
    float   ditherScale;
    float   ditherDistanceMin;
    float   ditherDistanceMax;
    // tangent[3] = TangentBend
    FloatVector4  tangent;
    float   maxDepthOffset;
    HairSettings();
  };
  // defined in mat_dump.cpp
  static const char *objectTypeStrings[7];
  static const char *materialFlagNames[32];
  static const char *shaderModelNames[64];
  static const char *shaderRouteNames[8];
  static const char *colorChannelNames[4];
  static const char *alphaBlendModeNames[8];
  static const char *blenderModeNames[4];
  static const char *textureAddressModeNames[4];
  static const char *colorModeNames[2];
  static const char *effectBlendModeNames[8];
  static const char *effectFlagNames[32];
  static const char *decalBlendModeNames[2];
  static const char *decalRenderLayerNames[3];
  static const char *maskSourceBlenderNames[4];
  static const char *channelNames[4];
  static const char *resolutionSettingNames[4];
  static const char *physicsMaterialNames[8];
  static const std::string_view emptyStringView;
  static const CE2Material  defaultLayeredMaterial;
  static const CE2Material::Blender defaultBlender;
  static const CE2Material::Layer   defaultLayer;
  static const CE2Material::Material    defaultMaterial;
  static const CE2Material::TextureSet  defaultTextureSet;
  static const CE2Material::UVStream    defaultUVStream;
  std::uint32_t flags;
  std::uint32_t layerMask;
  const Layer   *layers[maxLayers];
  float   alphaThreshold;
  // index to shaderModelNames, default: 31 ("BaseMaterial")
  unsigned char shaderModel;
  unsigned char alphaSourceLayer;
  // 0 = "Linear" (default), 1 = "Additive", 2 = "PositionContrast", 3 = "None"
  unsigned char alphaBlendMode;
  unsigned char alphaVertexColorChannel;
  float   alphaHeightBlendThreshold;
  float   alphaHeightBlendFactor;
  float   alphaPosition;
  float   alphaContrast;
  const UVStream  *alphaUVStream;               // can be NULL
  // 0 = "Deferred" (default), 1 = "Effect", 2 = "PlanetaryRing",
  // 3 = "PrecomputedScattering", 4 = "Water"
  unsigned char shaderRoute;
  unsigned char opacityLayer1;
  unsigned char opacityLayer2;
  unsigned char opacityBlender1;
  // 0 = "Lerp", 1 = "Additive", 2 = "Subtractive", 3 = "Multiplicative"
  unsigned char opacityBlender1Mode;
  unsigned char opacityLayer3;
  unsigned char opacityBlender2;
  unsigned char opacityBlender2Mode;
  float   specularOpacityOverride;
  // 0 = "None" (default), 1 = "Carpet", 2 = "Mat",
  // 3 = "MaterialGroundTileVinyl", 4 = "MaterialMat",
  // 5 = "MaterialPHYIceDebrisLarge", 6 = "Metal", 7 = "Wood"
  unsigned char physicsMaterialType;
  const Blender *blenders[maxBlenders];
  const CE2Material *lodMaterials[maxLODMaterials];
  // the following pointers are valid if the corresponding bit in flags is set
  const EffectSettings  *effectSettings;
  const EmissiveSettings  *emissiveSettings;
  const LayeredEmissiveSettings *layeredEmissiveSettings;
  const TranslucencySettings  *translucencySettings;
  const DecalSettings   *decalSettings;
  const VegetationSettings  *vegetationSettings;
  const DetailBlenderSettings *detailBlenderSettings;
  const LayeredEdgeFalloff  *layeredEdgeFalloff;
  const WaterSettings   *waterSettings;
  const GlobalLayerData *globalLayerData;
  const HairSettings  *hairSettings;
  CE2Material();
  inline void setFlags(std::uint32_t m, bool n)
  {
    flags = (flags & ~m) | ((0U - std::uint32_t(n)) & m);
  }
  void printObjectInfo(std::string& buf, size_t indentCnt,
                       bool isLODMaterial = false) const;
};

class CE2MaterialDB : public BSMaterialsCDB
{
 protected:
  struct ComponentInfo
  {
    CE2MaterialDB&  cdb;
    CE2MaterialObject *o;
    const BSMaterialsCDB::MaterialComponent *componentData;
    inline bool readBool(bool& n,
                         const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    inline bool readUInt8(unsigned char& n,
                          const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    inline bool readUInt16(std::uint16_t& n,
                           const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    inline bool readUInt32(std::uint32_t& n,
                           const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    inline bool readFloat(float& n,
                          const BSMaterialsCDB::CDBObject *p, size_t fieldNum,
                          bool clampTo0To1 = false);
    inline bool readString(const char*& s,
                           const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    bool readPath(const std::string_view*& s,
                  const BSMaterialsCDB::CDBObject *p, size_t fieldNum,
                  const char *prefix = nullptr, const char *suffix = nullptr);
    // t = sequence of strings with length prefix (e.g. "\005False\004True")
    bool readEnum(unsigned char& n,
                  const BSMaterialsCDB::CDBObject *p, size_t fieldNum,
                  const char *t);
    bool readLayerNumber(unsigned char& n,
                         const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    bool readBlenderNumber(unsigned char& n,
                           const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    void readLayeredEmissivityComponent(const BSMaterialsCDB::CDBObject *p);
    void readAlphaBlenderSettings(const BSMaterialsCDB::CDBObject *p);
    void readBSFloatCurve(const BSMaterialsCDB::CDBObject *p);
    void readEmissiveSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readWaterFoamSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readFlipbookComponent(const BSMaterialsCDB::CDBObject *p);
    void readPhysicsMaterialType(const BSMaterialsCDB::CDBObject *p);
    void readTerrainTintSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readUVStreamID(const BSMaterialsCDB::CDBObject *p);
    void readDecalSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readDirectory(const BSMaterialsCDB::CDBObject *p);
    void readWaterSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readControl(const BSMaterialsCDB::CDBObject *p);
    void readComponentProperty(const BSMaterialsCDB::CDBObject *p);
    bool readXMFLOAT4(FloatVector4& v,
                      const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    void readEffectSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readCTName(const BSMaterialsCDB::CDBObject *p);
    void readGlobalLayerDataComponent(const BSMaterialsCDB::CDBObject *p);
    void readOffset(const BSMaterialsCDB::CDBObject *p);
    void readTextureAddressModeComponent(const BSMaterialsCDB::CDBObject *p);
    void readFloatCurveController(const BSMaterialsCDB::CDBObject *p);
    void readMaterialPropertyNode(const BSMaterialsCDB::CDBObject *p);
    void readProjectedDecalSettings(const BSMaterialsCDB::CDBObject *p);
    void readParamBool(const BSMaterialsCDB::CDBObject *p);
    void readFloat3DCurveController(const BSMaterialsCDB::CDBObject *p);
    bool readColorValue(FloatVector4& c,
                        const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    bool readColorValue(std::uint32_t& c,
                        const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    void readColor(const BSMaterialsCDB::CDBObject *p);
    bool readSourceTextureWithReplacement(
        const std::string_view*& texturePath, std::uint32_t& textureReplacement,
        bool& textureReplacementEnabled,
        const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    void readFlowSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readDetailBlenderSettings(const BSMaterialsCDB::CDBObject *p);
    void readLayerID(const BSMaterialsCDB::CDBObject *p);
    void readMapping(const BSMaterialsCDB::CDBObject *p);
    void readClassReference(const BSMaterialsCDB::CDBObject *p);
    void readScale(const BSMaterialsCDB::CDBObject *p);
    void readWaterGrimeSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readUVStreamParamBool(const BSMaterialsCDB::CDBObject *p);
    void readMultiplex(const BSMaterialsCDB::CDBObject *p);
    void readOpacityComponent(const BSMaterialsCDB::CDBObject *p);
    void readBlendParamFloat(const BSMaterialsCDB::CDBObject *p);
    void readColorRemapSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readEyeSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readFloat2DLerpController(const BSMaterialsCDB::CDBObject *p);
    bool readBSComponentDB2ID(const CE2MaterialObject*& linkedObject,
                              const BSMaterialsCDB::CDBObject *p,
                              unsigned char typeRequired = 0);
    void readTextureReplacement(const BSMaterialsCDB::CDBObject *p);
    void readBlendModeComponent(const BSMaterialsCDB::CDBObject *p);
    void readLayeredEdgeFalloffComponent(const BSMaterialsCDB::CDBObject *p);
    void readVegetationSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readTextureResolutionSetting(const BSMaterialsCDB::CDBObject *p);
    void readAddress(const BSMaterialsCDB::CDBObject *p);
    void readDirectoryComponent(const BSMaterialsCDB::CDBObject *p);
    void readMaterialUVStreamPropertyNode(const BSMaterialsCDB::CDBObject *p);
    void readShaderRouteComponent(const BSMaterialsCDB::CDBObject *p);
    void readFloatLerpController(const BSMaterialsCDB::CDBObject *p);
    void readColorChannelTypeComponent(const BSMaterialsCDB::CDBObject *p);
    void readAlphaSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readLevelOfDetailSettings(const BSMaterialsCDB::CDBObject *p);
    void readTextureSetID(const BSMaterialsCDB::CDBObject *p);
    void readFloat2DCurveController(const BSMaterialsCDB::CDBObject *p);
    void readTextureFile(const BSMaterialsCDB::CDBObject *p);
    void readTranslucencySettings(const BSMaterialsCDB::CDBObject *p);
    void readMouthSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readDistortionComponent(const BSMaterialsCDB::CDBObject *p);
    void readDetailBlenderSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readStarmapBodyEffectComponent(const BSMaterialsCDB::CDBObject *p);
    void readMaterialParamFloat(const BSMaterialsCDB::CDBObject *p);
    void readBSFloat2DCurve(const BSMaterialsCDB::CDBObject *p);
    void readBSFloat3DCurve(const BSMaterialsCDB::CDBObject *p);
    void readTranslucencySettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readGlobalLayerNoiseSettings(const BSMaterialsCDB::CDBObject *p);
    void readMaterialID(const BSMaterialsCDB::CDBObject *p);
    bool readXMFLOAT2L(FloatVector4& v,
                       const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    bool readXMFLOAT2H(FloatVector4& v,
                       const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    void readTimerController(const BSMaterialsCDB::CDBObject *p);
    void readControllers(const BSMaterialsCDB::CDBObject *p);
    void readEmittanceSettings(const BSMaterialsCDB::CDBObject *p);
    void readShaderModelComponent(const BSMaterialsCDB::CDBObject *p);
    void readBSResourceID(const BSMaterialsCDB::CDBObject *p);
    bool readXMFLOAT3(FloatVector4& v,
                      const BSMaterialsCDB::CDBObject *p, size_t fieldNum);
    void readMaterialOverrideColorTypeComponent(
        const BSMaterialsCDB::CDBObject *p);
    void readChannel(const BSMaterialsCDB::CDBObject *p);
    void readColorCurveController(const BSMaterialsCDB::CDBObject *p);
    void readHairSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readBSColorCurve(const BSMaterialsCDB::CDBObject *p);
    void readControllerComponent(const BSMaterialsCDB::CDBObject *p);
    void readMRTextureFile(const BSMaterialsCDB::CDBObject *p);
    void readTextureSetKindComponent(const BSMaterialsCDB::CDBObject *p);
    void readBlenderID(const BSMaterialsCDB::CDBObject *p);
    void readCollisionComponent(const BSMaterialsCDB::CDBObject *p);
    void readTerrainSettingsComponent(const BSMaterialsCDB::CDBObject *p);
    void readLODMaterialID(const BSMaterialsCDB::CDBObject *p);
    void readMipBiasSetting(const BSMaterialsCDB::CDBObject *p);
    void loadComponent(const BSMaterialsCDB::MaterialComponent *p);
    ComponentInfo(CE2MaterialDB& p, CE2MaterialObject *q)
      : cdb(p),
        o(q)
    {
    }
  };
  struct StoredStdStringHashMap
  {
    const std::string_view  **buf;      // buf[0] is always an empty string
    size_t  hashMask;
    size_t  size;
    StoredStdStringHashMap();
    ~StoredStdStringHashMap();
    void clear();
    const std::string_view *insert(BSMaterialsCDB& cdb, const std::string& s);
    void expandBuffer();
  };
  struct CE2MatObjectHashMap
  {
    const CE2MaterialObject **buf;
    size_t  hashMask;
    size_t  size;
    CE2MatObjectHashMap();
    ~CE2MatObjectHashMap();
    void clear();
    void storeObject(const CE2MaterialObject *o);
    inline const CE2MaterialObject *findObject(BSResourceID objectID) const;
    void expandBuffer();
  };
  std::mutex  materialDBMutex;
  CE2MatObjectHashMap materialObjectMap;
  StoredStdStringHashMap  storedStdStrings;
  std::string stringBuf;
  inline const std::string_view *storeStdString(const std::string& s)
  {
    return storedStdStrings.insert(*this, s);
  }
  CE2MaterialObject *findMaterialObject(
      const BSMaterialsCDB::MaterialObject *p);
 public:
  CE2MaterialDB();
  ~CE2MaterialDB();
  void loadArchives(const BA2File& archive);
  const CE2Material *loadMaterial(const std::string_view& materialPath);
  void clear();
  // returns a set of null-terminated material paths, using 'buf' for storage
  void getMaterialList(
      std::set< std::string_view >& materialPaths, AllocBuffers& buf,
      bool excludeJSONMaterials = false,
      bool (*fileFilterFunc)(void *p, const std::string_view& s) = nullptr,
      void *fileFilterFuncData = nullptr) const;
};

#endif

