
#include "common.hpp"
#include "material.hpp"

#include <new>

#if ENABLE_CDB_DEBUG > 1
static void printHexData(const unsigned char *p, size_t n)
{
  if (n < 1)
    return;
  std::string s;
  FileBuffer::printHexData(s, p, n);
  std::fwrite(s.c_str(), sizeof(char), s.length(), stdout);
}
#endif

std::uint32_t CE2MaterialDB::calculateHash(
    std::uint64_t& h, const std::string& fileName)
{
  size_t  baseNamePos = fileName.rfind('/');
  size_t  baseNamePos2 = fileName.rfind('\\');
  size_t  extPos = fileName.rfind('.');
  if (baseNamePos == std::string::npos ||
      (baseNamePos2 != std::string::npos && baseNamePos2 > baseNamePos))
  {
    baseNamePos = baseNamePos2;
  }
  if (extPos == std::string::npos ||
      (baseNamePos != std::string::npos && extPos < baseNamePos))
  {
    extPos = fileName.length();
  }
  size_t  i = 0;
  std::uint32_t crcValue = 0U;
  if (baseNamePos != std::string::npos)
  {
    for ( ; i < baseNamePos; i++)
    {
      unsigned char c = (unsigned char) fileName.c_str()[i];
      if (c >= 0x41 && c <= 0x5A)       // 'A' - 'Z'
        c = c | 0x20;                   // convert to lower case
      else if (c == 0x2F)               // '/'
        c = 0x5C;                       // convert to '\\'
      hashFunctionCRC32(crcValue, c);
    }
    i++;
  }
  h = std::uint64_t(crcValue) << 32;
  crcValue = 0U;
  for ( ; i < extPos; i++)
  {
    unsigned char c = (unsigned char) fileName.c_str()[i];
    if (c >= 0x41 && c <= 0x5A)         // 'A' - 'Z'
      c = c | 0x20;                     // convert to lower case
    else if (c == 0x2F)                 // '/'
      c = 0x5C;                         // convert to '\\'
    hashFunctionCRC32(crcValue, c);
  }
  h = h | crcValue;
  i++;
  if ((i + 3) <= fileName.length())
    return FileBuffer::readUInt32Fast(fileName.c_str() + i);
  if ((i + 2) <= fileName.length())
    return FileBuffer::readUInt16Fast(fileName.c_str() + i);
  if (i < fileName.length())
    return (unsigned char) fileName.c_str()[i];
  return 0U;
}

inline const CE2MaterialObject * CE2MaterialDB::findObject(
    const std::vector< CE2MaterialObject * >& t, unsigned int objectID) const
{
  if (!objectID || objectID >= t.size())
    return nullptr;
  return t[objectID];
}

static const std::uint32_t
    defaultTextureRepl[CE2Material::TextureSet::maxTexturePaths] =
{
  // color, normal, opacity, rough
  0xFF000000U, 0xFFFF8080U, 0xFFFFFFFFU, 0xFF000000U,
  // metal, ao, height, emissive
  0xFF000000U, 0xFFFFFFFFU, 0xFF000000U, 0xFF000000U,
  // transmissive, curvature, mask
  0xFF000000U, 0xFF808080U, 0xFF000000U, 0xFF808080U,
  0xFF000000U, 0xFF000000U, 0xFF808080U, 0xFF808080U,
  0xFF808080U, 0xFF000000U, 0xFF000000U, 0xFFFFFFFFU,
  0xFF808080U
};

void CE2MaterialDB::initializeObject(
    CE2MaterialObject *o, const std::vector< CE2MaterialObject * >& objectTable)
{
  std::uint32_t baseObjID = ~(std::uint32_t(o->type)) >> 8;
  o->type = o->type & 0xFF;
  if (baseObjID) [[likely]]
  {
    // initialize from base object
    const CE2MaterialObject *t = findObject(objectTable, baseObjID);
    if (!t)
      errorMessage("invalid base object in material database");
    if (t->type < 0) [[unlikely]]
      initializeObject(const_cast< CE2MaterialObject * >(t), objectTable);
    if (o->type != t->type)
      errorMessage("invalid base object in material database");
    o->name = t->name;
    size_t  objectSize = sizeof(CE2MaterialObject);
    switch (o->type)
    {
      case 1:
        objectSize = sizeof(CE2Material);
        break;
      case 2:
        objectSize = sizeof(CE2Material::Blender);
        break;
      case 3:
        objectSize = sizeof(CE2Material::Layer);
        break;
      case 4:
        objectSize = sizeof(CE2Material::Material);
        break;
      case 5:
        objectSize = sizeof(CE2Material::TextureSet);
        break;
      case 6:
        objectSize = sizeof(CE2Material::UVStream);
        break;
      default:
        return;
    }
    unsigned char *dstPtr = reinterpret_cast< unsigned char * >(o);
    const unsigned char *srcPtr = reinterpret_cast< const unsigned char * >(t);
    dstPtr = dstPtr + sizeof(CE2MaterialObject);
    srcPtr = srcPtr + sizeof(CE2MaterialObject);
    std::memcpy(dstPtr, srcPtr, objectSize - sizeof(CE2MaterialObject));
    return;
  }
  // initialize with defaults
  const std::string *emptyString = stringBuffers.front().data();
  switch (o->type)
  {
    case 1:
      {
        CE2Material *p = static_cast< CE2Material * >(o);
        p->flags = 0U;
        p->layerMask = 0U;
        for (size_t i = 0; i < CE2Material::maxLayers; i++)
          p->layers[i] = nullptr;
        p->alphaThreshold = 1.0f / 3.0f;
        p->shaderModel = 31;            // "BaseMaterial"
        p->alphaSourceLayer = 0;        // "MATERIAL_LAYER_0"
        p->alphaBlendMode = 0;          // "Linear"
        p->alphaVertexColorChannel = 0; // "Red"
        p->alphaHeightBlendThreshold = 0.0f;
        p->alphaHeightBlendFactor = 0.05f;
        p->alphaPosition = 0.5f;
        p->alphaContrast = 0.0f;
        p->alphaUVStream = nullptr;
        p->shaderRoute = 0;             // "Deferred"
        p->opacityLayer1 = 0;           // "MATERIAL_LAYER_0"
        p->opacityLayer2 = 1;           // "MATERIAL_LAYER_1"
        p->opacityBlender1 = 0;         // "BLEND_LAYER_0"
        p->opacityBlender1Mode = 0;     // "Lerp"
        p->opacityLayer3 = 2;           // "MATERIAL_LAYER_2"
        p->opacityBlender2 = 1;         // "BLEND_LAYER_1"
        p->opacityBlender2Mode = 0;     // "Lerp"
        p->specularOpacityOverride = 0.0f;
        p->physicsMaterialType = 0;
        for (size_t i = 0; i < CE2Material::maxBlenders; i++)
          p->blenders[i] = nullptr;
        for (size_t i = 0; i < CE2Material::maxLODMaterials; i++)
          p->lodMaterials[i] = nullptr;
        p->effectSettings = nullptr;
        p->emissiveSettings = nullptr;
        p->layeredEmissiveSettings = nullptr;
        p->translucencySettings = nullptr;
        p->decalSettings = nullptr;
        p->vegetationSettings = nullptr;
        p->detailBlenderSettings = nullptr;
        p->layeredEdgeFalloff = nullptr;
        p->waterSettings = nullptr;
      }
      break;
    case 2:
      {
        CE2Material::Blender  *p = static_cast< CE2Material::Blender * >(o);
        p->uvStream = nullptr;
        p->texturePath = emptyString;
        p->textureReplacement = 0xFFFFFFFFU;
        p->textureReplacementEnabled = false;
        p->blendMode = 0;               // "Linear"
        p->colorChannel = 0;            // "Red"
        for (size_t i = 0; i < CE2Material::Blender::maxFloatParams; i++)
          p->floatParams[i] = 0.5f;
        for (size_t i = 0; i < CE2Material::Blender::maxBoolParams; i++)
          p->boolParams[i] = false;
      }
      break;
    case 3:
      {
        CE2Material::Layer  *p = static_cast< CE2Material::Layer * >(o);
        p->material = nullptr;
        p->uvStream = nullptr;
      }
      break;
    case 4:
      {
        CE2Material::Material *p = static_cast< CE2Material::Material * >(o);
        p->color = FloatVector4(1.0f);
        p->colorMode = 0;               // "Multiply"
        p->flipbookFlags = 0;
        p->flipbookColumns = 1;
        p->flipbookRows = 1;
        p->flipbookFPS = 30.0f;
        p->textureSet = nullptr;
      }
      break;
    case 5:
      {
        CE2Material::TextureSet *p =
            static_cast< CE2Material::TextureSet * >(o);
        p->texturePathMask = 0U;
        p->floatParam = 0.5f;
        for (size_t i = 0; i < CE2Material::TextureSet::maxTexturePaths; i++)
          p->texturePaths[i] = emptyString;
        p->textureReplacementMask = 0U;
        for (size_t i = 0; i < CE2Material::TextureSet::maxTexturePaths; i++)
          p->textureReplacements[i] = defaultTextureRepl[i];
        p->resolutionHint = 0;          // "Tiling"
      }
      break;
    case 6:
      {
        CE2Material::UVStream *p = static_cast< CE2Material::UVStream * >(o);
        p->scaleAndOffset = FloatVector4(1.0f, 1.0f, 0.0f, 0.0f);
        p->textureAddressMode = 0;      // "Wrap"
        p->channel = 1;                 // "One"
      }
      break;
  }
}

void * CE2MaterialDB::allocateSpace(
    size_t nBytes, const void *copySrc, size_t alignBytes)
{
  std::uintptr_t  addr0 =
      reinterpret_cast< std::uintptr_t >(objectBuffers.back().data());
  std::uintptr_t  endAddr = addr0 + objectBuffers.back().capacity();
  std::uintptr_t  addr =
      reinterpret_cast< std::uintptr_t >(&(*(objectBuffers.back().end())));
  std::uintptr_t  alignMask = std::uintptr_t(alignBytes - 1);
  addr = (addr + alignMask) & ~alignMask;
  std::uintptr_t  bytesRequired = (nBytes + alignMask) & ~alignMask;
  if ((endAddr - addr) < bytesRequired) [[unlikely]]
  {
    std::uintptr_t  bufBytes = 65536U;
    if (bytesRequired > bufBytes)
      throw std::bad_alloc();
    objectBuffers.emplace_back();
    objectBuffers.back().reserve(size_t(bufBytes));
    addr0 = reinterpret_cast< std::uintptr_t >(objectBuffers.back().data());
    addr = (addr0 + alignMask) & ~alignMask;
  }
  objectBuffers.back().resize(size_t((addr + bytesRequired) - addr0));
  unsigned char *p = reinterpret_cast< unsigned char * >(addr);
  if (copySrc) [[unlikely]]
    std::memcpy(p, copySrc, nBytes);
  return p;
}

CE2MaterialObject * CE2MaterialDB::allocateObject(
    std::vector< CE2MaterialObject * >& objectTable,
    std::uint32_t objectID, std::uint32_t baseObjID,
    std::uint64_t h, std::uint32_t e)
{
  if (objectID >= objectTable.size() || baseObjID >= objectTable.size())
    errorMessage("CE2MaterialDB: internal error: object ID is out of range");
  CE2MaterialObject *o = objectTable[objectID];
  if (o) [[unlikely]]
  {
    throw FO76UtilsError("object 0x%08X is defined more than once "
                         "in material database", (unsigned int) objectID);
  }
  std::int32_t  type;
  if (!baseObjID) [[unlikely]]
  {
    switch (h & 0xFFFFFFFFU)
    {
      case 0x7EA3660CU:                 // CRC32 of "layeredmaterials"
        type = 1;
        break;
      case 0x8EBE84FFU:                 // "blenders"
        type = 2;
        break;
      case 0x574A4CF3U:                 // "layers"
        type = 3;
        break;
      case 0x7D1E021BU:                 // "materials"
        type = 4;
        break;
      case 0x06F52154U:                 // "texturesets"
        type = 5;
        break;
      case 0x4298BB09U:                 // "uvstreams"
        type = 6;
        break;
      default:
        errorMessage("invalid root object ID in material database");
        break;
    }
  }
  else
  {
    if (objectTable[baseObjID])
      type = objectTable[baseObjID]->type & 0xFF;
    else
      errorMessage("invalid base object ID in material database");
  }
  static const size_t objectSizeTable[8] =
  {
    sizeof(CE2MaterialObject), sizeof(CE2Material),
    sizeof(CE2Material::Blender), sizeof(CE2Material::Layer),
    sizeof(CE2Material::Material), sizeof(CE2Material::TextureSet),
    sizeof(CE2Material::UVStream), sizeof(CE2MaterialObject)
  };
  o = reinterpret_cast< CE2MaterialObject * >(
          allocateSpace(objectSizeTable[type]));
  objectTable[objectID] = o;
  o->type = type | std::int32_t(~baseObjID << 8);
  o->e = e;
  o->h = h;
  o->name = stringBuffers.front().data();
  o->parent = nullptr;
  if (!baseObjID) [[unlikely]]
    initializeObject(o, objectTable);
  return o;
}

const std::string * CE2MaterialDB::readStringParam(
    std::string& stringBuf, FileBuffer& buf, size_t len, int type)
{
  switch (type)
  {
    case 1:
      buf.readPath(stringBuf, len, "textures/", ".dds");
      break;
    default:
      buf.readString(stringBuf, len);
      break;
  }
  if (stringBuf.empty())
    return stringBuffers.front().data();
  std::uint64_t h = 0xFFFFFFFFU;
  const char  *p = stringBuf.c_str();
  const char  *endp = stringBuf.c_str() + stringBuf.length();
  // *endp is valid and always '\0'
  for ( ; (p + 7) <= endp; p = p + 8)
    hashFunctionUInt64(h, FileBuffer::readUInt64Fast(p));
  if (p < endp)
  {
    std::uint64_t tmp;
    if ((p + 3) <= endp)
    {
      tmp = FileBuffer::readUInt32Fast(p);
      if ((p + 5) <= endp)
        tmp = tmp | (std::uint64_t(FileBuffer::readUInt16Fast(p + 4)) << 32);
    }
    else
    {
      tmp = FileBuffer::readUInt16Fast(p);
    }
    hashFunctionUInt64(h, tmp);
  }
  size_t  n = size_t(h & stringHashMask);
  size_t  collisionCnt = 0;
  std::uint32_t m = storedStringParams[n];
  while (m)
  {
    const std::string *s =
        stringBuffers[m >> stringBufShift].data() + (m & stringBufMask);
    if (stringBuf == *s)
      return s;
    if (++collisionCnt >= 1024)
      errorMessage("CE2MaterialDB: internal error: stringHashMask is too low");
    n = (n + 1) & stringHashMask;
    m = storedStringParams[n];
  }
  if (stringBuffers.back().size() > stringBufMask)
  {
    stringBuffers.emplace_back();
    stringBuffers.back().reserve(stringBufMask + 1);
  }
  storedStringParams[n] =
      std::uint32_t(((stringBuffers.size() - 1) << stringBufShift)
                    | stringBuffers.back().size());
  stringBuffers.back().push_back(stringBuf);
  return &(stringBuffers.back().back());
}

CE2MaterialDB::CE2MaterialDB()
{
  objectNameMap.resize(objectNameHashMask + 1, nullptr);
  objectBuffers.emplace_back();
  objectBuffers.back().reserve(65536);
  storedStringParams.resize(stringHashMask + 1, 0U);
  stringBuffers.emplace_back();
  stringBuffers.back().reserve(stringBufMask + 1);
  stringBuffers.back().emplace_back("");
}

CE2MaterialDB::CE2MaterialDB(const BA2File& ba2File, const char *fileName)
{
  objectNameMap.resize(objectNameHashMask + 1, nullptr);
  objectBuffers.emplace_back();
  objectBuffers.back().reserve(65536);
  storedStringParams.resize(stringHashMask + 1, 0U);
  stringBuffers.emplace_back();
  stringBuffers.back().reserve(stringBufMask + 1);
  stringBuffers.back().emplace_back("");
  loadCDBFile(ba2File, fileName);
}

CE2MaterialDB::~CE2MaterialDB()
{
}

void CE2MaterialDB::loadCDBFile(CDBFile& buf)
{
  ComponentInfo componentInfo(*this, buf);
  const unsigned char *componentInfoPtr = nullptr;
  size_t  componentID = 0;
  size_t  componentCnt = 0;
  unsigned int  chunkType;
  while ((chunkType = buf.readChunk(componentInfo, 4)) != 0U)
  {
    if (chunkType == CDBFile::ChunkType_DIFF ||
        chunkType == CDBFile::ChunkType_OBJT) [[likely]]
    {
      if (componentID >= componentCnt) [[unlikely]]
      {
#if ENABLE_CDB_DEBUG
        std::fputc('\n', stdout);
#endif
        continue;
      }
      // read component
      bool    isDiff = (chunkType != CDBFile::ChunkType_OBJT);
      const unsigned char *cmpInfoPtr = componentInfoPtr + (componentID << 3);
      unsigned int  objectID = FileBuffer::readUInt32Fast(cmpInfoPtr);
      componentInfo.componentIndex = FileBuffer::readUInt16Fast(cmpInfoPtr + 4);
      componentInfo.componentType = FileBuffer::readUInt16Fast(cmpInfoPtr + 6);
      componentID++;
      componentInfo.o = const_cast< CE2MaterialObject * >(
                            findObject(componentInfo.objectTable, objectID));
      if (!componentInfo.o) [[unlikely]]
      {
#if ENABLE_CDB_DEBUG
        std::fputc('\n', stdout);
#endif
        continue;
      }
      if (componentInfo.o->type < 0) [[unlikely]]
        initializeObject(componentInfo.o, componentInfo.objectTable);
      if (componentInfo.size() < 4) [[unlikely]]
      {
#if ENABLE_CDB_DEBUG
        std::fputc('\n', stdout);
#endif
        continue;
      }
#if ENABLE_CDB_DEBUG
      std::printf(", object = 0x%08X:%d, component 0x%04X[%u]\n",
                  objectID, componentInfo.o->type,
                  componentInfo.componentType, componentInfo.componentIndex);
#  if ENABLE_CDB_DEBUG > 1
      printHexData(componentInfo.data() + 4, componentInfo.size() - 4);
#  endif
#endif
      if (!componentInfo.o)
      {
#if ENABLE_CDB_DEBUG
        std::printf("Warning: invalid object ID in material database\n");
#endif
        continue;
      }
      unsigned int  n = componentInfo.componentType - 0x0060U;
      if ((n & ~0x3FU) || !ComponentInfo::readFunctionTable[n])
      {
#if ENABLE_CDB_DEBUG
        std::printf("Warning: unrecognized component type 0x%04X\n",
                    n + 0x0060U);
#endif
        continue;
      }
      ComponentInfo::readFunctionTable[n](componentInfo, isDiff);
      continue;
    }
#if ENABLE_CDB_DEBUG
    std::fputc('\n', stdout);
#endif
    if (chunkType == CDBFile::ChunkType_STRT) [[unlikely]]      // string table
    {
      if ((buf.getPosition() - componentInfo.size()) != 24)
        errorMessage("duplicate string table in material database");
      continue;
    }
    size_t  t = CDBFile::String_Unknown;
    if (componentInfo.size() >= 4)
      t = buf.findString(FileBuffer::readUInt32Fast(componentInfo.data()));
#if ENABLE_CDB_DEBUG
    if (chunkType == CDBFile::ChunkType_MAPC)
    {
      size_t  t2 = buf.findString(componentInfo.readUInt32());
      unsigned int  n = componentInfo.readUInt32();
      std::printf("  < %s, %s >[%u]\n",
                  CDBFile::stringTable[t], CDBFile::stringTable[t2], n);
      if (t == CDBFile::String_BSResource_ID)
      {
        for ( ; n; n--)
        {
          // hash of the base name without extension
          std::printf("    0x%08X", componentInfo.readUInt32());
          // extension ("mat\0")
          std::printf(", 0x%08X", componentInfo.readUInt32());
          // hash of the directory name
          std::printf(", 0x%08X", componentInfo.readUInt32());
          // unknown hash
          std::printf("  0x%016llX\n",
                      (unsigned long long) componentInfo.readUInt64());
        }
      }
      continue;
    }
    if (chunkType == CDBFile::ChunkType_CLAS)
    {
      componentInfo.setPosition(componentInfo.getPosition() + 8);
      while ((componentInfo.getPosition() + 12ULL) <= componentInfo.size())
      {
        std::uint32_t fieldName = componentInfo.readUInt32Fast();
        std::uint32_t fieldType = componentInfo.readUInt32Fast();
        unsigned int  dataOffset = componentInfo.readUInt16Fast();
        unsigned int  dataSize = componentInfo.readUInt16Fast();
        std::printf("    %s, %s, 0x%04X, 0x%04X\n",
                    CDBFile::stringTable[buf.findString(fieldName)],
                    CDBFile::stringTable[buf.findString(fieldType)],
                    dataOffset, dataSize);
      }
      continue;
    }
#endif
    if (chunkType != CDBFile::ChunkType_LIST)
    {
#if ENABLE_CDB_DEBUG
      if (chunkType == CDBFile::ChunkType_USER ||
          chunkType == CDBFile::ChunkType_USRD)
      {
        size_t  t2 = buf.findString(componentInfo.readUInt32());
        std::printf("  %s : %s\n",
                    CDBFile::stringTable[t], CDBFile::stringTable[t2]);
#  if ENABLE_CDB_DEBUG > 1
        printHexData(componentInfo.data() + componentInfo.getPosition(),
                     componentInfo.size() - componentInfo.getPosition());
#  endif
      }
#endif
      continue;
    }
    unsigned int  n = componentInfo.readUInt32();
#if ENABLE_CDB_DEBUG
    std::printf("  %s[%u]\n", CDBFile::stringTable[t], n);
#endif
    if (t == CDBFile::String_BSComponentDB2_DBFileIndex_ObjectInfo)
    {
      if ((componentInfo.getPosition() + (std::uint64_t(n) * 21UL))
          > componentInfo.size())
      {
        errorMessage("unexpected end of LIST chunk in material database");
      }
      if (componentInfo.objectTable.begin() != componentInfo.objectTable.end())
        errorMessage("duplicate ObjectInfo list in material database");
      const unsigned char *objectInfoPtr =
          componentInfo.data() + componentInfo.getPosition();
      unsigned int  objectCnt = n;
      std::uint32_t maxObjID = 0U;
      for (const unsigned char *p = objectInfoPtr; n; n--, p = p + 21)
        maxObjID = std::max(maxObjID, FileBuffer::readUInt32Fast(p + 12));
      if (maxObjID > 0x007FFFFFU)
        errorMessage("object ID is out of range in material database");
      componentInfo.objectTable.resize(size_t(maxObjID) + 1, nullptr);
      n = objectCnt;
      for (const unsigned char *p = objectInfoPtr; n; n--, p = p + 21)
      {
        // root objects:
        //     0x7EA3660C = LayeredMaterials (object type 1)
        //     0x8EBE84FF = Blenders (object type 2)
        //     0x574A4CF3 = Layers (object type 3)
        //     0x7D1E021B = Materials (object type 4)
        //     0x06F52154 = TextureSets (object type 5)
        //     0x4298BB09 = UVStreams (object type 6)
        std::uint32_t nameHash = FileBuffer::readUInt32Fast(p);
        std::uint32_t nameExt = FileBuffer::readUInt32Fast(p + 4);
        std::uint32_t dirHash = FileBuffer::readUInt32Fast(p + 8);
        std::uint32_t objectID = FileBuffer::readUInt32Fast(p + 12);
        // initialize using defaults from this object
        std::uint32_t baseObjID = FileBuffer::readUInt32Fast(p + 16);
        // true if baseObjID is valid
        bool          hasBaseObject = bool(p[20]);
#if ENABLE_CDB_DEBUG
        std::printf("    0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, %d\n",
                    (unsigned int) nameHash, (unsigned int) nameExt,
                    (unsigned int) dirHash, (unsigned int) objectID,
                    (unsigned int) baseObjID, int(hasBaseObject));
#endif
        if (!objectID) [[unlikely]]
          continue;
        if (!hasBaseObject)
          baseObjID = 0U;
        CE2MaterialObject *o =
            allocateObject(componentInfo.objectTable, objectID, baseObjID,
                           std::uint64_t(nameHash)
                           | (std::uint64_t(dirHash) << 32), nameExt);
        if (nameExt != 0x0074616DU)             // "mat\0"
          continue;
        std::uint64_t h = 0xFFFFFFFFU;
        hashFunctionUInt64(h, o->h);
        hashFunctionUInt64(h, o->e);
        h = h & objectNameHashMask;
        size_t  collisionCnt = 0;
        for ( ; objectNameMap[h]; h = (h + 1U) & objectNameHashMask)
        {
          if (objectNameMap[h]->h == o->h &&
              objectNameMap[h]->e == o->e) [[unlikely]]
          {
            if (!((objectNameMap[h]->type ^ o->type) & 0xFF))
              break;
            errorMessage("duplicate object name hash in material database");
          }
          if (++collisionCnt >= 1024)
          {
            errorMessage("CE2MaterialDB: internal error: "
                         "objectNameHashMask is too low");
          }
        }
        objectNameMap[h] = o;
      }
    }
    else if (t == CDBFile::String_BSComponentDB2_DBFileIndex_ComponentInfo)
    {
      if ((componentInfo.getPosition() + (std::uint64_t(n) << 3))
          > componentInfo.size())
      {
        errorMessage("unexpected end of LIST chunk in material database");
      }
      if (componentInfoPtr)
        errorMessage("duplicate ComponentInfo list in material database");
      componentInfoPtr = componentInfo.data() + componentInfo.getPosition();
      componentCnt = n;
#if ENABLE_CDB_DEBUG
      while (n--)
      {
        unsigned int  objectID = componentInfo.readUInt32Fast();
        unsigned int  componentIndex = componentInfo.readUInt16Fast();
        unsigned int  componentType = componentInfo.readUInt16Fast();
        std::printf("    0x%08X, 0x%04X, 0x%04X\n",
                    objectID, componentIndex, componentType);
      }
#endif
    }
    else if (t == CDBFile::String_BSComponentDB2_DBFileIndex_EdgeInfo)
    {
      if ((componentInfo.getPosition() + (std::uint64_t(n) * 12UL))
          > componentInfo.size())
      {
        errorMessage("unexpected end of LIST chunk in material database");
      }
      const unsigned char *edgeInfoPtr =
          componentInfo.data() + componentInfo.getPosition();
      for (const unsigned char *p = edgeInfoPtr; n; n--, p = p + 12)
      {
        std::uint32_t childObjectID = FileBuffer::readUInt32Fast(p);
        std::uint32_t parentObjectID = FileBuffer::readUInt32Fast(p + 4);
        unsigned int  edgeType = FileBuffer::readUInt16Fast(p + 10);    // 0x64
#if ENABLE_CDB_DEBUG
        unsigned int  edgeIndex = FileBuffer::readUInt16Fast(p + 8);    // 0
        std::printf("    0x%08X, 0x%08X, 0x%04X, 0x%04X\n",
                    (unsigned int) childObjectID, (unsigned int) parentObjectID,
                    edgeIndex, edgeType);
#endif
        if (edgeType != 0x0064U) [[unlikely]]
          continue;             // not "BSComponentDB2::OuterEdge"
        const CE2MaterialObject *o =
            findObject(componentInfo.objectTable, childObjectID);
        if (!o)
          errorMessage("invalid object ID in EdgeInfo list in CDB file");
#if ENABLE_CDB_DEBUG
        if (o->parent)
        {
          std::printf("Warning: parent of object 0x%08X redefined\n",
                      (unsigned int) childObjectID);
        }
#endif
        const_cast< CE2MaterialObject * >(o)->parent =
            findObject(componentInfo.objectTable, parentObjectID);
      }
    }
  }
  for (std::vector< CE2MaterialObject * >::iterator
           i = componentInfo.objectTable.begin();
       i != componentInfo.objectTable.end(); i++)
  {
    if (*i && (*i)->type < 0)
      initializeObject(*i, componentInfo.objectTable);
  }
}

void CE2MaterialDB::loadCDBFile(const unsigned char *buf, size_t bufSize)
{
  CDBFile   tmpBuf(buf, bufSize);
  loadCDBFile(tmpBuf);
}

void CE2MaterialDB::loadCDBFile(const BA2File& ba2File, const char *fileName)
{
  std::vector< unsigned char >  fileBuf;
  if (!fileName || fileName[0] == '\0')
    fileName = "materials/materialsbeta.cdb";
  while (true)
  {
    size_t  len = 0;
    while (fileName[len] != ',' && fileName[len] != '\0')
      len++;
    const unsigned char *buf = nullptr;
    size_t  bufSize =
        ba2File.extractFile(buf, fileBuf, std::string(fileName, len));
    CDBFile   tmpBuf(buf, bufSize);
    loadCDBFile(tmpBuf);
    if (!fileName[len])
      break;
    fileName = fileName + (len + 1);
  }
}

const CE2Material * CE2MaterialDB::findMaterial(
    const std::string& fileName) const
{
  std::uint64_t tmp = 0U;
  std::uint32_t e = calculateHash(tmp, fileName);
  std::uint64_t h = 0xFFFFFFFFU;
  hashFunctionUInt64(h, tmp);
  hashFunctionUInt64(h, e);
  h = h & objectNameHashMask;
  for ( ; objectNameMap[h]; h = (h + 1U) & objectNameHashMask)
  {
    if (objectNameMap[h]->h == tmp && objectNameMap[h]->e == e)
      break;
  }
  const CE2MaterialObject *o = objectNameMap[h];
  if (o && o->type == 1)
    return static_cast< const CE2Material * >(o);
  return nullptr;
}

