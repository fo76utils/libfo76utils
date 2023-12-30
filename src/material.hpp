
#ifndef MATERIAL_HPP_INCLUDED
#define MATERIAL_HPP_INCLUDED

#include "common.hpp"
#include "fp32vec4.hpp"
#include "filebuf.hpp"
#include "ba2file.hpp"
#include "ddstxt.hpp"
#include "bsrefl.hpp"

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
  // For uninitialized objects, ~(base object ID) * 256 is added to type.
  std::int32_t  type;
  // extension (0x0074616D for "mat\0")
  std::uint32_t e;
  // b0 to b31 = base name hash (lower case, no extension)
  // b32 to b63 = directory name hash (lower case with '\\', no trailing '\\')
  std::uint64_t h;
  const std::string *name;
  const CE2MaterialObject *parent;
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
    void printObjectInfo(std::string& buf, size_t indentCnt) const;
  };
  struct TextureSet : public CE2MaterialObject  // object type 5
  {
    enum
    {
      maxTexturePaths = 21
    };
    std::uint32_t texturePathMask;
    float   floatParam;                 // possibly mip bias
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
    // texturePaths[20] = _id.dds
    const std::string *texturePaths[maxTexturePaths];
    // texture replacements are colors in R8G8B8A8 format
    std::uint32_t textureReplacementMask;
    std::uint32_t textureReplacements[maxTexturePaths];
    // 0 = "Tiling" (default), 1 = "UniqueMap", 2 = "DetailMapTiling",
    // 3 = "HighResUniqueMap"
    unsigned char resolutionHint;
    bool    disableMipBiasHint;
    void printObjectInfo(std::string& buf, size_t indentCnt) const;
  };
  struct Material : public CE2MaterialObject    // object type 4
  {
    FloatVector4  color;
    // MaterialOverrideColorTypeComponent, 0 = "Multiply", 1 = "Lerp"
    unsigned char colorMode;
    // bit 0 = is flipbook, bit 1 = loops
    unsigned char flipbookFlags;
    unsigned char flipbookColumns;
    unsigned char flipbookRows;
    float   flipbookFPS;
    const TextureSet  *textureSet;
    void printObjectInfo(std::string& buf, size_t indentCnt) const;
  };
  struct Layer : public CE2MaterialObject       // object type 3
  {
    const Material  *material;
    const UVStream  *uvStream;
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
    const std::string *texturePath;
    std::uint32_t textureReplacement;
    bool    textureReplacementEnabled;
    // 0 = "Linear" (default), 1 = "Additive", 2 = "PositionContrast",
    // 3 = "None", 4 = "CharacterCombine", 5 = "Skin"
    unsigned char blendMode;
    // 0 = "Red" (default), 1 = "Green", 2 = "Blue", 3 = "Alpha"
    unsigned char colorChannel;
    // values set via BSMaterial::MaterialParamFloat and BSMaterial::ParamBool
    float   floatParams[maxFloatParams];
    bool    boolParams[maxBoolParams];
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
    std::uint32_t layer1Tint;           // R, G, B, overall scale
    std::uint32_t layer2Tint;
    std::uint32_t layer3Tint;
    float   clipThreshold;
    float   luminousEmittance;
    float   exposureOffset;
    float   maxOffset;
    float   minOffset;
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
  };
  struct DecalSettings
  {
    bool    isDecal;
    bool    isPlanet;
    // 0 = "None" (default), 1 = "Additive"
    unsigned char blendMode;
    bool    animatedDecalIgnoresTAA;
    float   decalAlpha;
    std::uint32_t writeMask;
    bool    isProjected;
    // projected decal settings
    bool    useParallaxMapping;
    bool    parallaxOcclusionShadows;
    unsigned char maxParallaxSteps;
    const std::string *surfaceHeightMap;
    float   parallaxOcclusionScale;
    // 0 = "Top" (default), 1 = "Middle"
    unsigned char renderLayer;
    bool    useGBufferNormals;
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
  };
  struct DetailBlenderSettings
  {
    bool    isEnabled;
    bool    textureReplacementEnabled;
    std::uint32_t textureReplacement;
    const std::string *texturePath;
    const UVStream  *uvStream;
  };
  struct LayeredEdgeFalloff
  {
    float   falloffStartAngles[3];
    float   falloffStopAngles[3];
    float   falloffStartOpacities[3];
    float   falloffStopOpacities[3];
    unsigned char activeLayersMask;
    bool    useRGBFalloff;
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
    const std::string *noiseMaskTexture;
    FloatVector4  texcoordScaleAndBias;
    float   worldspaceScaleFactor;
    float   hurstExponent;
    float   baseFrequency;
    float   frequencyMultiplier;
    float   maskIntensityMin;
    float   maskIntensityMax;
  };
  static const char *materialFlagNames[32];
  static const char *shaderModelNames[64];
  std::uint32_t flags;
  std::uint32_t layerMask;
  const Layer   *layers[maxLayers];
  float   alphaThreshold;
  // index to shaderModelNames defined in mat_dump.cpp, default: "BaseMaterial"
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
  inline void setFlags(std::uint32_t m, bool n)
  {
    flags = (flags & ~m) | ((0U - std::uint32_t(n)) & m);
  }
  void printObjectInfo(std::string& buf, size_t indentCnt,
                       bool isLODMaterial = false) const;
};

class CE2MaterialDB
{
 protected:
  class ComponentInfo : public BSReflStream::Chunk
  {
   public:
    typedef void (*ReadFunctionType)(ComponentInfo&, bool);
    static const ReadFunctionType readFunctionTable[128];
    CE2MaterialDB&  cdb;
    CE2MaterialObject *o;
    unsigned int  componentIndex;
    unsigned int  componentType;        // as BSReflStream::stringTable[] index
    std::string   stringBuf;
    std::vector< CE2MaterialObject * >  objectTable;
    BSReflStream& cdbBuf;
    static void readLayeredEmissivityComponent(ComponentInfo& p, bool isDiff);
    static void readAlphaBlenderSettings(ComponentInfo& p, bool isDiff);
    static void readBSFloatCurve(ComponentInfo& p, bool isDiff);
    static void readEmissiveSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readWaterFoamSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readFlipbookComponent(ComponentInfo& p, bool isDiff);
    static void readPhysicsMaterialType(ComponentInfo& p, bool isDiff);
    static void readTerrainTintSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readUVStreamID(ComponentInfo& p, bool isDiff);
    static void readDecalSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readDirectory(ComponentInfo& p, bool isDiff);
    static void readWaterSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readControl(ComponentInfo& p, bool isDiff);
    static void readComponentProperty(ComponentInfo& p, bool isDiff);
    static bool readXMFLOAT4(FloatVector4& v, ComponentInfo& p, bool isDiff);
    static void readEffectSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readCTName(ComponentInfo& p, bool isDiff);
    static void readGlobalLayerDataComponent(ComponentInfo& p, bool isDiff);
    static void readOffset(ComponentInfo& p, bool isDiff);
    static void readTextureAddressModeComponent(ComponentInfo& p, bool isDiff);
    static void readFloatCurveController(ComponentInfo& p, bool isDiff);
    static void readMaterialPropertyNode(ComponentInfo& p, bool isDiff);
    static void readProjectedDecalSettings(ComponentInfo& p, bool isDiff);
    static void readParamBool(ComponentInfo& p, bool isDiff);
    static void readFloat3DCurveController(ComponentInfo& p, bool isDiff);
    static bool readColorValue(FloatVector4& c, ComponentInfo& p, bool isDiff);
    static bool readColorValue(std::uint32_t& c, ComponentInfo& p, bool isDiff);
    static void readColor(ComponentInfo& p, bool isDiff);
    static bool readSourceTextureWithReplacement(
        const std::string*& texturePath, std::uint32_t& textureReplacement,
        bool& textureReplacementEnabled, ComponentInfo& p, bool isDiff);
    static void readEdgeInfo(ComponentInfo& p, bool isDiff);
    static void readFlowSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readDetailBlenderSettings(ComponentInfo& p, bool isDiff);
    static void readLayerID(ComponentInfo& p, bool isDiff);
    static void readMapping(ComponentInfo& p, bool isDiff);
    static void readClassReference(ComponentInfo& p, bool isDiff);
    static void readComponentInfo(ComponentInfo& p, bool isDiff);
    static void readScale(ComponentInfo& p, bool isDiff);
    static void readWaterGrimeSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readUVStreamParamBool(ComponentInfo& p, bool isDiff);
    static void readComponentTypeInfo(ComponentInfo& p, bool isDiff);
    static void readMultiplex(ComponentInfo& p, bool isDiff);
    static void readOpacityComponent(ComponentInfo& p, bool isDiff);
    static void readBlendParamFloat(ComponentInfo& p, bool isDiff);
    static void readDBFileIndex(ComponentInfo& p, bool isDiff);
    static void readColorRemapSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readEyeSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readFilePair(ComponentInfo& p, bool isDiff);
    static void readFloat2DLerpController(ComponentInfo& p, bool isDiff);
    static const CE2MaterialObject *readBSComponentDB2ID(
        ComponentInfo& p, bool isDiff, unsigned char typeRequired = 0);
    static void readTextureReplacement(ComponentInfo& p, bool isDiff);
    static void readBlendModeComponent(ComponentInfo& p, bool isDiff);
    static void readLayeredEdgeFalloffComponent(ComponentInfo& p, bool isDiff);
    static void readVegetationSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readTextureResolutionSetting(ComponentInfo& p, bool isDiff);
    static void readAddress(ComponentInfo& p, bool isDiff);
    static void readDirectoryComponent(ComponentInfo& p, bool isDiff);
    static void readMaterialUVStreamPropertyNode(ComponentInfo& p, bool isDiff);
    static void readShaderRouteComponent(ComponentInfo& p, bool isDiff);
    static void readFloatLerpController(ComponentInfo& p, bool isDiff);
    static void readColorChannelTypeComponent(ComponentInfo& p, bool isDiff);
    static void readAlphaSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readLevelOfDetailSettings(ComponentInfo& p, bool isDiff);
    static void readTextureSetID(ComponentInfo& p, bool isDiff);
    static void readFloat2DCurveController(ComponentInfo& p, bool isDiff);
    static void readTextureFile(ComponentInfo& p, bool isDiff);
    static void readTranslucencySettings(ComponentInfo& p, bool isDiff);
    static void readMouthSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readDistortionComponent(ComponentInfo& p, bool isDiff);
    static void readDetailBlenderSettingsComponent(ComponentInfo& p,
                                                   bool isDiff);
    static void readObjectInfo(ComponentInfo& p, bool isDiff);
    static void readStarmapBodyEffectComponent(ComponentInfo& p, bool isDiff);
    static void readMaterialParamFloat(ComponentInfo& p, bool isDiff);
    static void readBSFloat2DCurve(ComponentInfo& p, bool isDiff);
    static void readBSFloat3DCurve(ComponentInfo& p, bool isDiff);
    static void readTranslucencySettingsComponent(ComponentInfo& p,
                                                  bool isDiff);
    static void readGlobalLayerNoiseSettings(ComponentInfo& p, bool isDiff);
    static void readMaterialID(ComponentInfo& p, bool isDiff);
    static bool readXMFLOAT2L(FloatVector4& v, ComponentInfo& p, bool isDiff);
    static bool readXMFLOAT2H(FloatVector4& v, ComponentInfo& p, bool isDiff);
    static void readTimerController(ComponentInfo& p, bool isDiff);
    static void readControllers(ComponentInfo& p, bool isDiff);
    static void readEmittanceSettings(ComponentInfo& p, bool isDiff);
    static void readShaderModelComponent(ComponentInfo& p, bool isDiff);
    static void readBSResourceID(ComponentInfo& p, bool isDiff);
    static bool readXMFLOAT3(FloatVector4& v, ComponentInfo& p, bool isDiff);
    static void readMaterialOverrideColorTypeComponent(ComponentInfo& p,
                                                       bool isDiff);
    static void readChannel(ComponentInfo& p, bool isDiff);
    static void readColorCurveController(ComponentInfo& p, bool isDiff);
    static void readCompiledDB(ComponentInfo& p, bool isDiff);
    static void readHairSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readBSColorCurve(ComponentInfo& p, bool isDiff);
    static void readControllerComponent(ComponentInfo& p, bool isDiff);
    static void readMRTextureFile(ComponentInfo& p, bool isDiff);
    static void readTextureSetKindComponent(ComponentInfo& p, bool isDiff);
    static void readBlenderID(ComponentInfo& p, bool isDiff);
    static void readCollisionComponent(ComponentInfo& p, bool isDiff);
    static void readTerrainSettingsComponent(ComponentInfo& p, bool isDiff);
    static void readLODMaterialID(ComponentInfo& p, bool isDiff);
    static void readMipBiasSetting(ComponentInfo& p, bool isDiff);
    ComponentInfo(CE2MaterialDB& p, BSReflStream& cdbFileBuf)
      : cdb(p),
        cdbBuf(cdbFileBuf)
    {
    }
    inline bool readString();
    inline bool readAndStoreString(const std::string*& s, int type);
    // returns chunk type (e.g. 0x5453494C for "LIST")
    unsigned int readChunk(BSReflStream::Chunk& chunkBuf);
  };
  enum
  {
    objectNameHashMask = 0x0007FFFF,
    stringBufShift = 16,
    stringBufMask = 0xFFFF,
    stringHashMask = 0x001FFFFF
  };
  // objectNameHashMask + 1 elements
  std::vector< CE2MaterialObject * >  objectNameMap;
  std::vector< std::vector< unsigned char > > objectBuffers;
  // stringHashMask + 1 elements
  std::vector< std::uint32_t >  storedStringParams;
  // stringBuffers[0][0] = ""
  std::vector< std::vector< std::string > > stringBuffers;
  inline const CE2MaterialObject *findObject(
      const std::vector< CE2MaterialObject * >& t, unsigned int objectID) const;
  void initializeObject(CE2MaterialObject *o,
                        const std::vector< CE2MaterialObject * >& objectTable);
  void *allocateSpace(size_t nBytes, const void *copySrc = nullptr,
                      size_t alignBytes = 16);
  CE2MaterialObject *allocateObject(
      std::vector< CE2MaterialObject * >& objectTable,
      std::uint32_t objectID, std::uint32_t baseObjID,
      std::uint64_t h, std::uint32_t e);
  // type = 0: general string (stored without conversion)
  // type = 1: DDS file name (prefix = "textures/", suffix = ".dds")
  const std::string *readStringParam(std::string& stringBuf, FileBuffer& buf,
                                     size_t len, int type);
 public:
  CE2MaterialDB();
  // fileName defaults to "materials/materialsbeta.cdb", it can be a comma
  // separated list of multiple CDB file names
  CE2MaterialDB(const BA2File& ba2File, const char *fileName = nullptr);
  virtual ~CE2MaterialDB();
  void loadCDBFile(BSReflStream& buf);
  void loadCDBFile(const unsigned char *buf, size_t bufSize);
  void loadCDBFile(const BA2File& ba2File, const char *fileName = nullptr);
  const CE2Material *findMaterial(const std::string& fileName) const;
};

#endif

