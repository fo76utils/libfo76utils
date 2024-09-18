
#ifndef BSREFL_HPP_INCLUDED
#define BSREFL_HPP_INCLUDED

#include "common.hpp"
#include "filebuf.hpp"

class BSReflStream : public FileBuffer
{
 public:
  enum
  {
    ChunkType_NONE = 0,
    ChunkType_BETH = 0x48544542,
    ChunkType_STRT = 0x54525453,
    ChunkType_TYPE = 0x45505954,
    ChunkType_CLAS = 0x53414C43,
    ChunkType_LIST = 0x5453494C,
    ChunkType_MAPC = 0x4350414D,
    ChunkType_OBJT = 0x544A424F,
    ChunkType_DIFF = 0x46464944,
    ChunkType_USER = 0x52455355,
    ChunkType_USRD = 0x44525355
  };
  enum
  {
    String_None = 0,
    String_String = 1,
    String_List = 2,
    String_Map = 3,
    String_Ref = 4,
    String_Int8 = 7,
    String_UInt8 = 8,
    String_Int16 = 9,
    String_UInt16 = 10,
    String_Int32 = 11,
    String_UInt32 = 12,
    String_Int64 = 13,
    String_UInt64 = 14,
    String_Bool = 15,
    String_Float = 16,
    String_Double = 17,
    String_Unknown = 18,
    String_BSBind_ControllerComponent = 130,
    String_BSBind_DirectoryComponent = 134,
    String_BSComponentDB2_DBFileIndex = 148,
    String_BSComponentDB2_DBFileIndex_ComponentInfo = 149,
    String_BSComponentDB2_DBFileIndex_ComponentTypeInfo = 150,
    String_BSComponentDB2_DBFileIndex_EdgeInfo = 151,
    String_BSComponentDB2_DBFileIndex_ObjectInfo = 152,
    String_BSComponentDB2_ID = 153,
    String_BSComponentDB_CTName = 154,
    String_BSMaterial_AlphaBlenderSettings = 164,
    String_BSMaterial_AlphaSettingsComponent = 165,
    String_BSMaterial_BlendModeComponent = 166,
    String_BSMaterial_BlendParamFloat = 167,
    String_BSMaterial_BlenderID = 168,
    String_BSMaterial_Channel = 169,
    String_BSMaterial_CollisionComponent = 170,
    String_BSMaterial_Color = 171,
    String_BSMaterial_ColorChannelTypeComponent = 172,
    String_BSMaterial_ColorRemapSettingsComponent = 173,
    String_BSMaterial_DecalSettingsComponent = 174,
    String_BSMaterial_DetailBlenderSettings = 175,
    String_BSMaterial_DetailBlenderSettingsComponent = 176,
    String_BSMaterial_DistortionComponent = 177,
    String_BSMaterial_EffectSettingsComponent = 178,
    String_BSMaterial_EmissiveSettingsComponent = 179,
    String_BSMaterial_EmittanceSettings = 180,
    String_BSMaterial_EyeSettingsComponent = 181,
    String_BSMaterial_FlipbookComponent = 182,
    String_BSMaterial_FlowSettingsComponent = 183,
    String_BSMaterial_GlobalLayerDataComponent = 184,
    String_BSMaterial_GlobalLayerNoiseSettings = 185,
    String_BSMaterial_HairSettingsComponent = 186,
    String_BSMaterial_Internal_CompiledDB = 187,
    String_BSMaterial_Internal_CompiledDB_FilePair = 188,
    String_BSMaterial_LODMaterialID = 189,
    String_BSMaterial_LayerID = 190,
    String_BSMaterial_LayeredEdgeFalloffComponent = 191,
    String_BSMaterial_LayeredEmissivityComponent = 192,
    String_BSMaterial_LevelOfDetailSettings = 193,
    String_BSMaterial_MRTextureFile = 194,
    String_BSMaterial_MaterialID = 195,
    String_BSMaterial_MaterialOverrideColorTypeComponent = 196,
    String_BSMaterial_MaterialParamFloat = 197,
    String_BSMaterial_MipBiasSetting = 198,
    String_BSMaterial_MouthSettingsComponent = 199,
    String_BSMaterial_Offset = 200,
    String_BSMaterial_OpacityComponent = 201,
    String_BSMaterial_ParamBool = 202,
    String_BSMaterial_PhysicsMaterialType = 203,
    String_BSMaterial_ProjectedDecalSettings = 204,
    String_BSMaterial_Scale = 205,
    String_BSMaterial_ShaderModelComponent = 206,
    String_BSMaterial_ShaderRouteComponent = 207,
    String_BSMaterial_SourceTextureWithReplacement = 208,
    String_BSMaterial_StarmapBodyEffectComponent = 209,
    String_BSMaterial_TerrainSettingsComponent = 210,
    String_BSMaterial_TerrainTintSettingsComponent = 211,
    String_BSMaterial_TextureAddressModeComponent = 212,
    String_BSMaterial_TextureFile = 213,
    String_BSMaterial_TextureReplacement = 214,
    String_BSMaterial_TextureResolutionSetting = 215,
    String_BSMaterial_TextureSetID = 216,
    String_BSMaterial_TextureSetKindComponent = 217,
    String_BSMaterial_TranslucencySettings = 218,
    String_BSMaterial_TranslucencySettingsComponent = 219,
    String_BSMaterial_UVStreamID = 220,
    String_BSMaterial_UVStreamParamBool = 221,
    String_BSMaterial_VegetationSettingsComponent = 222,
    String_BSMaterial_WaterFoamSettingsComponent = 223,
    String_BSMaterial_WaterGrimeSettingsComponent = 224,
    String_BSMaterial_WaterSettingsComponent = 225,
    String_BSResource_ID = 228,
    String_XMFLOAT2 = 1096,
    String_XMFLOAT3 = 1097,
    String_XMFLOAT4 = 1098
  };
  class Chunk : public FileBuffer
  {
   public:
    inline Chunk()
      : FileBuffer()
    {
    }
    inline void setData(const unsigned char *fileData, size_t fileSize,
                        size_t filePosition = 0)
    {
      fileBuf = fileData;
      fileBufSize = fileSize;
      filePos = filePosition;
      fileStream = std::uintptr_t(-1);
    }
    inline std::uint16_t readUInt16()
    {
      return FileBuffer::readUInt16();
    }
    inline std::uint32_t readUInt32()
    {
      return FileBuffer::readUInt32();
    }
    // the following functions return false on end of chunk
    inline bool getFieldNumber(unsigned int& n, unsigned int nMax, bool isDiff);
    inline bool readBool(bool& n);
    inline bool readUInt8(unsigned char& n);
    inline bool readUInt16(std::uint16_t& n);
    inline bool readUInt32(std::uint32_t& n);
    inline bool readFloat(float& n);
    inline bool readFloat0To1(float& n);
    inline bool readString(std::string& stringBuf);
    // t = sequence of strings with length prefix (e.g. "\005False\004True")
    bool readEnum(unsigned char& n, const char *t);
  };
  static const char *stringTable[1156];
  static int findString(const char *s);
  unsigned int  chunksRemaining;
 protected:
  std::vector< std::int16_t > stringMap;
  void readStringTable();
 public:
  BSReflStream(const unsigned char *fileData, size_t fileSize)
    : FileBuffer(fileData, fileSize)
  {
    readStringTable();
  }
  BSReflStream(const char *fileName)
    : FileBuffer(fileName)
  {
    readStringTable();
  }
  std::uint32_t findString(unsigned int strtOffs) const;
  inline const char *getStringPtr(unsigned int strtOffs) const
  {
    return stringTable[findString(strtOffs)];
  }
  inline int getClassName(unsigned int strtOffs) const
  {
    if (strtOffs >= stringMap.size()) [[unlikely]]
      return -1;
    return stringMap[strtOffs];
  }
  // returns chunk type (ChunkType_BETH, etc.), or 0 on end of file
  inline unsigned int readChunk(Chunk& chunkBuf, size_t startPos = 0);
  static inline const char *getDefaultMaterialDBPath()
  {
    return "materials/materialsbeta.cdb";
  }
};

inline bool BSReflStream::Chunk::getFieldNumber(
    unsigned int& n, unsigned int nMax, bool isDiff)
{
  if (!isDiff) [[unlikely]]
  {
    n++;
    return (int(n) <= int(nMax));
  }
  if ((filePos + 2ULL) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
  n = readUInt16Fast();
  if (std::int16_t(n) <= std::int16_t(nMax)) [[likely]]
    return (std::int16_t(n) >= 0);
  filePos = fileBufSize;
  return false;
}

inline bool BSReflStream::Chunk::readBool(bool& n)
{
  if (filePos < fileBufSize) [[likely]]
  {
    n = bool(readUInt8Fast());
    return true;
  }
  return false;
}

inline bool BSReflStream::Chunk::readUInt8(unsigned char& n)
{
  if (filePos < fileBufSize) [[likely]]
  {
    n = readUInt8Fast();
    return true;
  }
  return false;
}

inline bool BSReflStream::Chunk::readUInt16(std::uint16_t& n)
{
  if ((filePos + 2ULL) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
  n = readUInt16Fast();
  return true;
}

inline bool BSReflStream::Chunk::readUInt32(std::uint32_t& n)
{
  if ((filePos + 4ULL) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
  n = readUInt32Fast();
  return true;
}

inline bool BSReflStream::Chunk::readFloat(float& n)
{
  if ((filePos + 4ULL) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
#if defined(__i386__) || defined(__x86_64__) || defined(__x86_64)
  std::uint32_t tmp = readUInt32Fast();
  if (!((tmp + 0x00800000U) & 0x7F000000U))
    tmp = 0U;
  n = std::bit_cast< float, std::uint32_t >(tmp);
#else
  n = FileBuffer::readFloat();
#endif
  return true;
}

inline bool BSReflStream::Chunk::readFloat0To1(float& n)
{
  if ((filePos + 4ULL) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
#if defined(__i386__) || defined(__x86_64__) || defined(__x86_64)
  std::uint32_t tmp = readUInt32Fast();
  if (!((tmp + 0x00800000U) & 0x7F000000U))
    tmp = 0U;
  n = std::bit_cast< float, std::uint32_t >(tmp);
#else
  n = FileBuffer::readFloat();
#endif
  n = std::min(std::max(n, 0.0f), 1.0f);
  return true;
}

inline bool BSReflStream::Chunk::readString(std::string& stringBuf)
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
  FileBuffer::readString(stringBuf, len);
  return true;
}

inline unsigned int BSReflStream::readChunk(Chunk& chunkBuf, size_t startPos)
{
  if (!chunksRemaining) [[unlikely]]
    return 0U;
  chunksRemaining--;
  if ((filePos + 8ULL) > fileBufSize)
    errorMessage("unexpected end of reflection stream");
  unsigned int  chunkType = readUInt32Fast();
  unsigned int  chunkSize = readUInt32Fast();
  if ((filePos + std::uint64_t(chunkSize)) > fileBufSize)
    errorMessage("unexpected end of reflection stream");
  chunkBuf.setData(fileBuf + filePos, chunkSize, startPos);
  filePos = filePos + chunkSize;
  return chunkType;
}

struct BSReflDump : public BSReflStream
{
 protected:
  struct CDBClassDef
  {
    std::vector< std::pair< std::uint32_t, std::uint32_t > >  fields;
    bool    isUser = false;
  };
  std::map< std::uint32_t, CDBClassDef >  classes;
  void dumpItem(std::string& s, Chunk& chunkBuf, bool isDiff,
                std::uint32_t itemType, int indentCnt);
 public:
  BSReflDump(const unsigned char *fileData, size_t fileSize)
    : BSReflStream(fileData, fileSize)
  {
  }
  BSReflDump(const char *fileName)
    : BSReflStream(fileName)
  {
  }
  // if verboseMode is true, class definitions are also printed
  void readAllChunks(std::string& s, int indentCnt, bool verboseMode = false);
};

#endif

