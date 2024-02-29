
#include "common.hpp"
#include "bsmatcdb.hpp"

#define CDB_COMPONENTS_SORTED   0
#define CDB_JSON_QUOTE_NUMBERS  1
#define CDB_SORT_STRUCT_MEMBERS 1
#define CDB_COPY_CONTROLLERS    0

BSMaterialsCDB::BSResourceID::BSResourceID(const std::string& fileName)
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
    // directory name
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
  dir = crcValue;
  crcValue = 0U;
  for ( ; i < extPos; i++)
  {
    // base name
    unsigned char c = (unsigned char) fileName.c_str()[i];
    if (c >= 0x41 && c <= 0x5A)         // 'A' - 'Z'
      c = c | 0x20;                     // convert to lower case
    hashFunctionCRC32(crcValue, c);
  }
  file = crcValue;
  i++;
  if ((i + 3) <= fileName.length())
    ext = FileBuffer::readUInt32Fast(fileName.c_str() + i);
  else if ((i + 2) <= fileName.length())
    ext = FileBuffer::readUInt16Fast(fileName.c_str() + i);
  else if (i < fileName.length())
    ext = (unsigned char) fileName.c_str()[i];
  else
    ext = 0U;
  // convert extension to lower case
  ext = ext | ((ext >> 1) & 0x20202020U);
}

void BSMaterialsCDB::BSResourceID::fromJSONString(const std::string& s)
{
  if (s.length() == 30 && s[12] == ':' && s[21] == ':' && s[26] != '.')
  {
    std::uint32_t tmp = FileBuffer::readUInt32Fast(s.c_str());
    if ((tmp | ((tmp >> 1) & 0x20202020)) == 0x3A736572)        // "res:"
    {
      file = 0;
      ext = 0;
      dir = 0;
      for (size_t i = 4; true; i++)
      {
        if (i == 12 || i == 21)
          continue;
        char    c = s[i];
        if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
          c = char(c + 9);
        else if (!(c >= '0' && c <= '9'))
          break;
        dir = (dir << 4) | ((file >> 28) & 0x0F);
        file = ((file & 0x0FFFFFFF) << 4) | ((ext >> 28) & 0x0F);
        ext = ((ext & 0x0FFFFFFF) << 4) | std::uint32_t(c & 0x0F);
        if (i == 29)
          return;
      }
    }
  }
  (void) new(this) BSResourceID(s);
}

BSMaterialsCDB::MaterialComponent& BSMaterialsCDB::findComponent(
    MaterialObject& o, std::uint32_t key, std::uint32_t className)
{
  MaterialComponent *prv = nullptr;
  MaterialComponent *i = o.components;
#if CDB_COMPONENTS_SORTED
  for ( ; i && i->key < key; i = i->next)
#else
  for ( ; i && i->key != key; i = i->next)
#endif
    prv = i;
  if (i && i->key == key)
    return *i;
  MaterialComponent *p =
      reinterpret_cast< MaterialComponent * >(
          allocateSpace(sizeof(MaterialComponent), alignof(MaterialComponent)));
  p->key = key;
  p->className = className;
  p->o = nullptr;
  p->next = i;
  if (!prv)
    o.components = p;
  else
    prv->next = p;
  return *p;
}

inline const BSMaterialsCDB::MaterialObject *
    BSMaterialsCDB::findObject(std::uint32_t dbID) const
{
  if (!dbID || dbID >= objectTableSize) [[unlikely]]
    return nullptr;
  return objectTablePtr[dbID];
}

inline BSMaterialsCDB::MaterialObject *
    BSMaterialsCDB::findObject(std::uint32_t dbID)
{
  const BSMaterialsCDB *p = this;
  return const_cast< MaterialObject * >(p->findObject(dbID));
}

void * BSMaterialsCDB::allocateSpace(size_t nBytes, size_t alignBytes)
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
    bufBytes = std::max(bufBytes, bytesRequired);
    objectBuffers.emplace_back();
    objectBuffers.back().reserve(size_t(bufBytes));
    addr0 = reinterpret_cast< std::uintptr_t >(objectBuffers.back().data());
    addr = (addr0 + alignMask) & ~alignMask;
  }
  objectBuffers.back().resize(size_t((addr + bytesRequired) - addr0), 0);
  unsigned char *p = reinterpret_cast< unsigned char * >(addr);
  return p;
}

const std::uint8_t BSMaterialsCDB::cdbObjectSizeAlignTable[38] =
{
  std::uint8_t(sizeof(CDBObject)), std::uint8_t(alignof(CDBObject)),
  std::uint8_t(sizeof(CDBObject_String)),
  std::uint8_t(alignof(CDBObject_String)),
  std::uint8_t(sizeof(CDBObject_Compound)),
  std::uint8_t(alignof(CDBObject_Compound)),
  std::uint8_t(sizeof(CDBObject_Compound)),
  std::uint8_t(alignof(CDBObject_Compound)),
  std::uint8_t(sizeof(CDBObject_Compound)),
  std::uint8_t(alignof(CDBObject_Compound)),
  std::uint8_t(sizeof(CDBObject)), std::uint8_t(alignof(CDBObject)),
  std::uint8_t(sizeof(CDBObject)), std::uint8_t(alignof(CDBObject)),
  std::uint8_t(sizeof(CDBObject_Int)), std::uint8_t(alignof(CDBObject_Int)),
  std::uint8_t(sizeof(CDBObject_UInt)), std::uint8_t(alignof(CDBObject_UInt)),
  std::uint8_t(sizeof(CDBObject_Int)), std::uint8_t(alignof(CDBObject_Int)),
  std::uint8_t(sizeof(CDBObject_UInt)), std::uint8_t(alignof(CDBObject_UInt)),
  std::uint8_t(sizeof(CDBObject_Int)), std::uint8_t(alignof(CDBObject_Int)),
  std::uint8_t(sizeof(CDBObject_UInt)), std::uint8_t(alignof(CDBObject_UInt)),
  std::uint8_t(sizeof(CDBObject_Int64)), std::uint8_t(alignof(CDBObject_Int64)),
  std::uint8_t(sizeof(CDBObject_UInt64)),
  std::uint8_t(alignof(CDBObject_UInt64)),
  std::uint8_t(sizeof(CDBObject_Bool)), std::uint8_t(alignof(CDBObject_Bool)),
  std::uint8_t(sizeof(CDBObject_Float)), std::uint8_t(alignof(CDBObject_Float)),
  std::uint8_t(sizeof(CDBObject_Double)),
  std::uint8_t(alignof(CDBObject_Double)),
  std::uint8_t(sizeof(CDBObject)), std::uint8_t(alignof(CDBObject))
};

BSMaterialsCDB::CDBObject * BSMaterialsCDB::allocateObject(
    std::uint32_t itemType, const CDBClassDef *classDef, size_t elementCnt)
{
  size_t  dataSize, alignSize;
  size_t  childCnt = 0;
  if (itemType > BSReflStream::String_Unknown ||
      (itemType >= BSReflStream::String_List &&
       itemType <= BSReflStream::String_Ref))
  {
    if (itemType > BSReflStream::String_Unknown && classDef)
      childCnt = classDef->fieldCnt;
    else if (itemType < BSReflStream::String_Ref)
      childCnt = elementCnt;
    else
      childCnt = 1;
    dataSize = sizeof(CDBObject_Compound);
    alignSize = alignof(CDBObject_Compound);
    if (childCnt > 1)
      dataSize += (sizeof(CDBObject *) * (childCnt - 1));
  }
  else
  {
    dataSize = cdbObjectSizeAlignTable[itemType << 1];
    alignSize = cdbObjectSizeAlignTable[(itemType << 1) + 1];
  }
  CDBObject *o =
      reinterpret_cast< CDBObject * >(allocateSpace(dataSize, alignSize));
  o->type = std::uint16_t(itemType);
  o->childCnt = std::uint16_t(childCnt);
  if (itemType == BSReflStream::String_String)
    static_cast< CDBObject_String * >(o)->value = "";
#if !(defined(__i386__) || defined(__x86_64__) || defined(__x86_64))
  else if (itemType == BSReflStream::String_Float)
    static_cast< CDBObject_Float * >(o)->value = 0.0f;
  else if (itemType == BSReflStream::String_Double)
    static_cast< CDBObject_Double * >(o)->value = 0.0;
  else if (itemType == BSReflStream::String_BSComponentDB2_ID)
    static_cast< CDBObject_Link * >(o)->objectPtr = nullptr;
  for (size_t i = 0; i < childCnt; i++)
    o->children()[i] = nullptr;
#endif
  return o;
}

void BSMaterialsCDB::copyObject(CDBObject*& o)
{
  o->refCnt++;
  for (std::uint32_t i = 0U; i < o->childCnt; i++)
  {
    if (o->children()[i])
      copyObject(o->children()[i]);
  }
}

void BSMaterialsCDB::copyBaseObject(MaterialObject& o)
{
  MaterialObject  *p = const_cast< MaterialObject * >(o.baseObject);
  if (p->baseObject->baseObject)
    copyBaseObject(*p);
  o.baseObject = p->baseObject;
  o.components = nullptr;
  MaterialComponent **prv = &(o.components);
  for (MaterialComponent *i = p->components; i; i = i->next)
  {
    MaterialComponent *tmp = reinterpret_cast< MaterialComponent * >(
                                 allocateSpace(sizeof(MaterialComponent),
                                               alignof(MaterialComponent)));
    tmp->key = i->key;
    tmp->className = i->className;
    tmp->o = i->o;
#if !CDB_COPY_CONTROLLERS
    if (tmp->className
        == BSReflStream::String_BSBind_ControllerComponent) [[unlikely]]
    {
      tmp->o = allocateObject(tmp->className, getClassDef(tmp->className), 0);
    }
    else
#endif
    if (tmp->o) [[likely]]
      copyObject(tmp->o);
    tmp->next = nullptr;
    *prv = tmp;
    prv = &(tmp->next);
  }
}

void BSMaterialsCDB::loadItem(
    CDBObject*& o, BSReflStream& cdbFile, BSReflStream::Chunk& chunkBuf,
    bool isDiff, std::uint32_t itemType)
{
  if (o && o->refCnt)
  {
    o->refCnt--;
    size_t  dataSize, alignSize;
    if (o->type > BSReflStream::String_Unknown)
    {
      dataSize = sizeof(CDBObject_Compound);
      alignSize = alignof(CDBObject_Compound);
    }
    else
    {
      dataSize = cdbObjectSizeAlignTable[o->type << 1];
      alignSize = cdbObjectSizeAlignTable[(o->type << 1) + 1];
    }
    size_t  childCnt = o->childCnt;
    if (childCnt > 1)
      dataSize += (sizeof(CDBObject *) * (childCnt - 1));
    CDBObject *p =
        reinterpret_cast< CDBObject * >(allocateSpace(dataSize, alignSize));
    std::memcpy(p, o, dataSize);
    o = p;
    o->refCnt = 0;
  }
  const CDBClassDef *classDef = nullptr;
  if (itemType > BSReflStream::String_Unknown)
  {
    classDef = getClassDef(itemType);
    if (!classDef)
      itemType = BSReflStream::String_Unknown;
  }
  if (!(o && o->type == itemType))
  {
    if (!(itemType == BSReflStream::String_List ||
          itemType == BSReflStream::String_Map))
    {
      o = allocateObject(itemType, classDef, 0);
    }
  }
  if (itemType > BSReflStream::String_Unknown)
  {
    unsigned int  nMax = classDef->fieldCnt;
    if (classDef->isUser) [[unlikely]]
    {
      BSReflStream::Chunk userBuf;
      unsigned int  userChunkType = cdbFile.readChunk(userBuf);
      if (userChunkType != BSReflStream::ChunkType_USER &&
          userChunkType != BSReflStream::ChunkType_USRD)
      {
        throw FO76UtilsError("unexpected chunk type in CDB file at 0x%08x",
                             (unsigned int) cdbFile.getPosition());
      }
      isDiff = (userChunkType == BSReflStream::ChunkType_USRD);
      std::uint32_t className1 = cdbFile.findString(userBuf.readUInt32());
      if (className1 != itemType)
      {
        throw FO76UtilsError("USER chunk has unexpected type at 0x%08x",
                             (unsigned int) cdbFile.getPosition());
      }
      std::uint32_t className2 = cdbFile.findString(userBuf.readUInt32());
      if (className2 == className1)
      {
        // no type conversion
        nMax--;
        for (unsigned int n = 0U - 1U;
             userBuf.getFieldNumber(n, nMax, isDiff); )
        {
          loadItem(o->children()[n], cdbFile, userBuf, isDiff,
                   classDef->fields[n].type);
        }
      }
      else if (className2 < BSReflStream::String_Unknown)
      {
        if (!o->childCnt)
        {
          o->childCnt++;
          o->children()[0] = nullptr;
        }
        unsigned int  n = 0U;
        do
        {
          loadItem(o->children()[n], cdbFile, userBuf, isDiff, className2);
          className2 = cdbFile.findString(userBuf.readUInt32());
        }
        while (++n < nMax && className2 < BSReflStream::String_Unknown);
      }
      return;
    }
    nMax--;
    if (itemType == BSReflStream::String_BSComponentDB2_ID) [[unlikely]]
    {
      if (!nMax && classDef->fields[0].type == BSReflStream::String_UInt32)
      {
        std::uint32_t objectID = 0U;
        for (unsigned int n = 0U - 1U; chunkBuf.getFieldNumber(n, 0U, isDiff); )
        {
          if (!chunkBuf.readUInt32(objectID))
            break;
        }
        o->childCnt = 0;
        static_cast< CDBObject_Link * >(o)->objectPtr = findObject(objectID);
      }
      return;
    }
    for (unsigned int n = 0U - 1U; chunkBuf.getFieldNumber(n, nMax, isDiff); )
    {
      loadItem(o->children()[n], cdbFile, chunkBuf, isDiff,
               classDef->fields[n].type);
    }
    return;
  }
  FileBuffer& buf2 = *(static_cast< FileBuffer * >(&chunkBuf));
  switch (itemType)
  {
    case BSReflStream::String_String:
      {
        unsigned int  len = buf2.readUInt16();
        if ((buf2.getPosition() + std::uint64_t(len)) > buf2.size())
          errorMessage("unexpected end of CDB file");
        char    *s =
            reinterpret_cast< char * >(allocateSpace(len + 1U, sizeof(char)));
        std::memcpy(s, buf2.getReadPtr(), len);
        s[len] = '\0';
        static_cast< CDBObject_String * >(o)->value = s;
        buf2.setPosition(buf2.getPosition() + len);
      }
      break;
    case BSReflStream::String_List:
    case BSReflStream::String_Map:
      {
        BSReflStream::Chunk listBuf;
        unsigned int  chunkType = cdbFile.readChunk(listBuf);
        bool    isMap = (itemType == BSReflStream::String_Map);
        if (chunkType != (!isMap ? BSReflStream::ChunkType_LIST
                                   : BSReflStream::ChunkType_MAPC))
        {
          throw FO76UtilsError("unexpected chunk type in CDB file at 0x%08x",
                               (unsigned int) cdbFile.getPosition());
        }
        std::uint32_t classNames[2];
        classNames[0] = cdbFile.findString(listBuf.readUInt32());
        classNames[1] = classNames[0];
        if (isMap)
          classNames[1] = cdbFile.findString(listBuf.readUInt32());
        std::uint64_t listSize = 0U;
        if ((listBuf.getPosition() + 4ULL) <= listBuf.size())
          listSize = listBuf.readUInt32Fast();
        listSize = listSize << (unsigned char) isMap;
        std::uint32_t n = 0U;
        bool    appendingItems = false;
        if (isDiff && o && o->type == itemType && o->childCnt &&
            o->children()[0]->type == classNames[0])
        {
          if (!isMap)
          {
            appendingItems = (std::uint32_t(classNames[0]
                                            - BSReflStream::String_Int8) > 11U);
          }
          else
          {
            appendingItems = (o->children()[1]->type == classNames[1]);
          }
          if (appendingItems)
            listSize = listSize + o->childCnt;
        }
        if (appendingItems ||
            !(o && o->type == itemType && o->childCnt >= listSize))
        {
          if (listSize > 0xFFFFU)
            errorMessage("invalid list or map size in CDB file");
          CDBObject *p = allocateObject(itemType, classDef, size_t(listSize));
          if (appendingItems)
          {
            std::uint32_t prvSize = o->childCnt;
            for ( ; n < prvSize; n++)
              p->children()[n] = o->children()[n];
          }
          o = p;
        }
        o->childCnt = std::uint16_t(listSize);
        for ( ; n < listSize; n++)
        {
          loadItem(o->children()[n], cdbFile, listBuf, isDiff,
                   classNames[n & 1U]);
          if (isMap && !(n & 1U) &&
              classNames[0] == BSReflStream::String_String)
          {
            // check for duplicate keys
            for (std::uint32_t k = 0U; k < n; k = k + 2U)
            {
              if (std::strcmp(o->children()[k]->stringValue(),
                              o->children()[n]->stringValue()) == 0)
              {
                loadItem(o->children()[k + 1U], cdbFile, listBuf, isDiff,
                         classNames[1]);
                listSize = listSize - 2U;
                o->childCnt = std::uint16_t(listSize);
                n--;
                break;
              }
            }
          }
        }
        return;
      }
      break;
    case BSReflStream::String_Ref:
      {
        std::uint32_t refType = cdbFile.findString(buf2.readUInt32());
        loadItem(o->children()[0], cdbFile, chunkBuf, isDiff, refType);
        return;
      }
      break;
    case BSReflStream::String_Int8:
      static_cast< CDBObject_Int * >(o)->value = std::int8_t(buf2.readUInt8());
      break;
    case BSReflStream::String_UInt8:
      static_cast< CDBObject_UInt * >(o)->value = buf2.readUInt8();
      break;
    case BSReflStream::String_Int16:
      static_cast< CDBObject_Int * >(o)->value =
          std::int16_t(buf2.readUInt16());
      break;
    case BSReflStream::String_UInt16:
      static_cast< CDBObject_UInt * >(o)->value = buf2.readUInt16();
      break;
    case BSReflStream::String_Int32:
      static_cast< CDBObject_Int * >(o)->value = buf2.readInt32();
      break;
    case BSReflStream::String_UInt32:
      static_cast< CDBObject_UInt * >(o)->value = buf2.readUInt32();
      break;
    case BSReflStream::String_Int64:
      static_cast< CDBObject_Int64 * >(o)->value =
          std::int64_t(buf2.readUInt64());
      break;
    case BSReflStream::String_UInt64:
      static_cast< CDBObject_UInt64 * >(o)->value = buf2.readUInt64();
      break;
    case BSReflStream::String_Bool:
      static_cast< CDBObject_Bool * >(o)->value = bool(buf2.readUInt8());
      break;
    case BSReflStream::String_Float:
      static_cast< CDBObject_Float * >(o)->value = buf2.readFloat();
      break;
    case BSReflStream::String_Double:
      // FIXME: implement this in a portable way
      static_cast< CDBObject_Double * >(o)->value =
          std::bit_cast< double, std::uint64_t >(buf2.readUInt64());
      break;
    default:
      chunkBuf.setPosition(chunkBuf.size());
      break;
  }
}

void BSMaterialsCDB::readAllChunks(BSReflStream& cdbFile)
{
  if (classes.size() < 1)
  {
    classes.resize(512);
    for (size_t i = 0; i < 512; i++)
      classes[i].className = 0U;
  }
  if (objectBuffers.size() < 1)
  {
    objectBuffers.emplace_back();
    objectBuffers.back().reserve(65536);
  }

  std::vector< MaterialObject * > objectTable;
  std::vector< std::pair< std::uint32_t, std::uint32_t > >  componentInfo;
  BSReflStream::Chunk chunkBuf;
  unsigned int  chunkType;
  size_t  componentID = 0;
  size_t  componentCnt = 0;
  while ((chunkType = cdbFile.readChunk(chunkBuf)) != 0U)
  {
    std::uint32_t className = BSReflStream::String_None;
    if (chunkType != BSReflStream::ChunkType_TYPE &&
        chunkBuf.size() >= 4) [[likely]]
    {
      className = cdbFile.findString(chunkBuf.readUInt32Fast());
    }
    if (chunkType == BSReflStream::ChunkType_DIFF ||
        chunkType == BSReflStream::ChunkType_OBJT) [[likely]]
    {
      bool    isDiff = (chunkType == BSReflStream::ChunkType_DIFF);
      if (componentID < componentCnt) [[likely]]
      {
        std::uint32_t dbID = componentInfo[componentID].first;
        std::uint32_t key = componentInfo[componentID].second;
        MaterialObject  *i = findObject(dbID);
        if (i && className > BSReflStream::String_Unknown)
        {
          if (i->baseObject && i->baseObject->baseObject)
            copyBaseObject(*i);
          loadItem(findComponent(*i, key, className).o,
                   cdbFile, chunkBuf, isDiff, className);
        }
        componentID++;
      }
      continue;
    }
    if (chunkType == BSReflStream::ChunkType_TYPE) [[unlikely]]
    {
      for (size_t classCnt = chunkBuf.readUInt32(); classCnt > 0; classCnt--)
      {
        if ((chunkType = cdbFile.readChunk(chunkBuf))
            != BSReflStream::ChunkType_CLAS)
        {
          throw FO76UtilsError("unexpected chunk type in CDB file at 0x%08x",
                               (unsigned int) cdbFile.getPosition());
        }
        className = BSReflStream::String_None;
        if (chunkBuf.size() >= 4) [[likely]]
          className = cdbFile.findString(chunkBuf.readUInt32Fast());
        if (className < BSReflStream::String_Unknown)
          errorMessage("invalid class ID in CDB file");
        if (className == BSReflStream::String_Unknown)
          continue;
        CDBClassDef&  classDef = allocateClassDef(className);
        if (classDef.className)
          continue;
        classDef.className = className;
        (void) chunkBuf.readUInt32();   // classVersion
        unsigned int  classFlags = chunkBuf.readUInt16();
        classDef.isUser = bool(classFlags & 4U);
        classDef.fieldCnt = chunkBuf.readUInt16();
        classDef.fields =
            reinterpret_cast< CDBClassDef::Field * >(
                allocateSpace(sizeof(CDBClassDef::Field) * classDef.fieldCnt,
                              alignof(CDBClassDef::Field)));
        for (size_t i = 0; i < classDef.fieldCnt; i++)
        {
          CDBClassDef::Field& f =
              const_cast< CDBClassDef::Field * >(classDef.fields)[i];
          f.name = cdbFile.findString(chunkBuf.readUInt32());
          if (f.name < BSReflStream::String_Unknown)
            errorMessage("invalid field name in class definition in CDB file");
          f.type = cdbFile.findString(chunkBuf.readUInt32());
          (void) chunkBuf.readUInt16(); // dataOffset
          (void) chunkBuf.readUInt16(); // dataSize
        }
      }
      continue;
    }
    if (chunkType != BSReflStream::ChunkType_LIST)
      continue;
    std::uint32_t n = 0U;
    if (chunkBuf.size() >= 8)
      n = chunkBuf.readUInt32Fast();
    if (className == BSReflStream::String_BSComponentDB2_DBFileIndex_ObjectInfo)
    {
      if (n > ((chunkBuf.size() - chunkBuf.getPosition()) / 21))
        errorMessage("unexpected end of LIST chunk in material database");
      const unsigned char *objectInfoPtr = chunkBuf.getReadPtr();
      std::uint32_t maxObjID = 0U;
      for (std::uint32_t i = 0U; i < n; i++)
      {
        std::uint32_t dbID =
            FileBuffer::readUInt32Fast(objectInfoPtr + (i * 21U + 12U));
        maxObjID = std::max(maxObjID, dbID);
      }
      if (maxObjID > 0x007FFFFFU)
        errorMessage("object ID is out of range in material database");
      objectTable.resize(maxObjID + 1U, nullptr);
      objectTablePtr = objectTable.data();
      objectTableSize = objectTable.size();
      for (std::uint32_t i = 0U; i < n; i++)
      {
        BSResourceID  persistentID;
        persistentID.file = chunkBuf.readUInt32();
        persistentID.ext = chunkBuf.readUInt32();
        persistentID.dir = chunkBuf.readUInt32();
        std::uint32_t dbID = chunkBuf.readUInt32();
        if (!(dbID && dbID <= maxObjID))
          errorMessage("invalid object ID in material database");
        MaterialObject  *o =
            reinterpret_cast< MaterialObject * >(
                allocateSpace(sizeof(MaterialObject), alignof(MaterialObject)));
        if (objectTable[dbID])
          errorMessage("duplicate object ID in material database");
        objectTable[dbID] = o;
        o->persistentID = persistentID;
        o->dbID = dbID;
        o->baseObject = findObject(chunkBuf.readUInt32());
        (void) static_cast< FileBuffer * >(&chunkBuf)->readUInt8(); // HasData
        o->components = nullptr;
        o->parent = nullptr;
        o->children = nullptr;
        o->next = nullptr;
      }
    }
    else if (className
             == BSReflStream::String_BSComponentDB2_DBFileIndex_ComponentInfo)
    {
      componentID = 0;
      componentCnt = 0;
      while (n--)
      {
        std::uint32_t dbID = chunkBuf.readUInt32();
        std::uint32_t key = chunkBuf.readUInt32();      // (type << 16) | index
        componentInfo.emplace_back(dbID, key);
        componentCnt++;
      }
    }
    else if (className
             == BSReflStream::String_BSComponentDB2_DBFileIndex_EdgeInfo)
    {
      if (n > ((chunkBuf.size() - chunkBuf.getPosition()) / 12))
        errorMessage("unexpected end of LIST chunk in material database");
      const unsigned char *edgeInfoPtr = chunkBuf.getReadPtr();
      for (std::uint32_t i = 0U; i < n; i++)
      {
        // uint32_t sourceID, uint32_t targetID, uint16_t index, uint16_t type
        std::uint32_t sourceID =
            FileBuffer::readUInt32Fast(edgeInfoPtr + (i * 12U));
        std::uint32_t targetID =
            FileBuffer::readUInt32Fast(edgeInfoPtr + (i * 12U + 4U));
        MaterialObject  *o = findObject(sourceID);
        MaterialObject  *p = findObject(targetID);
        if (o && p)
        {
          if (o->parent)
          {
            throw FO76UtilsError("object 0x%08X has multiple parents in "
                                 "material database", (unsigned int) sourceID);
          }
          o->parent = p;
          o->next = p->children;
          p->children = o;
        }
      }
    }
  }
  for (std::vector< MaterialObject * >::iterator
           i = objectTable.begin(); i != objectTable.end(); i++)
  {
    MaterialObject  *p = *i;
    if (!p)
      continue;
    if (p->baseObject && p->baseObject->baseObject)
      copyBaseObject(*p);
    if (!p->parent && p->persistentID.ext == 0x0074616DU)       // "mat\0"
      matFileObjectMap[p->persistentID] = p;
  }
}

BSMaterialsCDB::CDBClassDef& BSMaterialsCDB::allocateClassDef(
    std::uint32_t className)
{
  std::uint32_t h = 0xFFFFFFFFU;
  hashFunctionCRC32C< std::uint32_t >(h, className);
  size_t  n = classes.size();
  std::uint32_t m = std::uint32_t(n - 1);
  for (h = h & m; true; n--, h = (h + 1U) & m)
  {
    if (n < 1) [[unlikely]]
    {
      errorMessage("BSMaterialsCDB: internal error: "
                   "too many class definitions");
    }
    if (!classes[h].className || classes[h].className == className)
      break;
  }
  return classes[h];
}

void BSMaterialsCDB::dumpObject(
    std::string& s, const CDBObject *o, int indentCnt) const
{
  if (!o)
  {
    printToString(s, "null");
    return;
  }
  if (o->type > BSReflStream::String_Unknown)
  {
    const CDBClassDef *classDef = getClassDef(o->type);
    if (o->type == BSReflStream::String_BSComponentDB2_ID) [[unlikely]]
    {
      const MaterialObject  *p = o->linkedObject();
      if (classDef && classDef->fieldCnt == 1 &&
          classDef->fields[0].type == BSReflStream::String_UInt32)
      {
        if (!p)
        {
          printToString(s, "\"\"");
        }
        else
        {
          printToString(s, "\"res:%08X:%08X:%08X\"",
                        (unsigned int) p->persistentID.dir,
                        (unsigned int) p->persistentID.file,
                        (unsigned int) p->persistentID.ext);
        }
        return;
      }
    }
    // structure
    printToString(s, "{\n%*s\"Data\": {", indentCnt + 2, "");
#if CDB_SORT_STRUCT_MEMBERS
    int     prvFieldNameID = -1;
    while (true)
    {
      int     fieldNameID = 0x7FFFFFFF;
      int     fieldNum = -1;
      for (std::uint32_t i = 0U; i < o->childCnt; i++)
      {
        if (!o->children()[i])
          continue;
        int     tmpFieldNameID = BSReflStream::String_None;
        if (classDef && i < classDef->fieldCnt)
          tmpFieldNameID = int(classDef->fields[i].name);
        if (tmpFieldNameID > prvFieldNameID && tmpFieldNameID < fieldNameID)
        {
          fieldNameID = tmpFieldNameID;
          fieldNum = int(i);
        }
      }
      if (fieldNum < 0)
        break;
      prvFieldNameID = fieldNameID;
      printToString(s, "\n%*s\"%s\": ",
                    indentCnt + 4, "", BSReflStream::stringTable[fieldNameID]);
      dumpObject(s, o->children()[fieldNum], indentCnt + 4);
      printToString(s, ",");
    }
#else
    for (std::uint32_t fieldNum = 0U; fieldNum < o->childCnt; fieldNum++)
    {
      if (!o->children()[fieldNum])
        continue;
      const char  *fieldNameStr = "null";
      if (classDef && fieldNum < classDef->fieldCnt)
      {
        fieldNameStr =
            BSReflStream::stringTable[classDef->fields[fieldNum].name];
      }
      printToString(s, "\n%*s\"%s\": ", indentCnt + 4, "", fieldNameStr);
      dumpObject(s, o->children()[fieldNum], indentCnt + 4);
      printToString(s, ",");
    }
#endif
    if (s.ends_with(','))
    {
      s[s.length() - 1] = '\n';
      printToString(s, "%*s", indentCnt + 2, "");
    }
    printToString(s, "},\n%*s\"Type\": \"%s\"\n",
                  indentCnt + 2, "", BSReflStream::stringTable[o->type]);
    printToString(s, "%*s}", indentCnt, "");
    return;
  }
  switch (o->type)
  {
    case BSReflStream::String_None:
      printToString(s, "null");
      break;
    case BSReflStream::String_String:
      s += '"';
      for (size_t i = 0; o->stringValue()[i]; i++)
      {
        char    c = o->stringValue()[i];
        if ((unsigned char) c < 0x20 || c == '"' || c == '\\')
        {
          s += '\\';
          switch (c)
          {
            case '\b':
              c = 'b';
              break;
            case '\t':
              c = 't';
              break;
            case '\n':
              c = 'n';
              break;
            case '\f':
              c = 'f';
              break;
            case '\r':
              c = 'r';
              break;
            default:
              if ((unsigned char) c < 0x20)
              {
                printToString(s, "u%04X", (unsigned int) c);
                continue;
              }
              break;
          }
        }
        s += c;
      }
      s += '"';
      break;
    case BSReflStream::String_List:
      printToString(s, "{\n%*s\"Data\": [", indentCnt + 2, "");
      for (std::uint32_t i = 0U; i < o->childCnt; i++)
      {
        printToString(s, "\n%*s", indentCnt + 4, "");
        dumpObject(s, o->children()[i], indentCnt + 4);
        if ((i + 1U) < o->childCnt)
          printToString(s, ",");
        else
          printToString(s, "\n%*s", indentCnt + 2, "");
      }
      printToString(s, "],\n");
      if (o->childCnt)
      {
        std::uint32_t elementType = 0U;
        if (o->children()[0])
          elementType = o->children()[0]->type;
        const char  *elementTypeStr = BSReflStream::stringTable[elementType];
        if (elementType && elementType < BSReflStream::String_Ref)
        {
          elementTypeStr = (elementType == BSReflStream::String_String ?
                            "BSFixedString" : "<collection>");
        }
        printToString(s, "%*s\"ElementType\": \"%s\",\n",
                      indentCnt + 2, "", elementTypeStr);
      }
      printToString(s, "%*s\"Type\": \"<collection>\"\n", indentCnt + 2, "");
      printToString(s, "%*s}", indentCnt, "");
      break;
    case BSReflStream::String_Map:
      printToString(s, "{\n%*s\"Data\": [", indentCnt + 2, "");
      for (std::uint32_t i = 0U; i < o->childCnt; i++)
      {
        printToString(s, "\n%*s{", indentCnt + 4, "");
        printToString(s, "\n%*s\"Data\": {", indentCnt + 6, "");
        printToString(s, "\n%*s\"Key\": ", indentCnt + 8, "");
        dumpObject(s, o->children()[i], indentCnt + 8);
        i++;
        printToString(s, ",\n%*s\"Value\": ", indentCnt + 8, "");
        dumpObject(s, o->children()[i], indentCnt + 8);
        printToString(s, "\n%*s},", indentCnt + 6, "");
        printToString(s, "\n%*s\"Type\": \"StdMapType::Pair\"\n",
                      indentCnt + 6, "");
        printToString(s, "%*s}", indentCnt + 4, "");
        if ((i + 1U) < o->childCnt)
          printToString(s, ",");
        else
          printToString(s, "\n%*s", indentCnt + 2, "");
      }
      printToString(s, "],\n%*s\"ElementType\": \"StdMapType::Pair\",\n",
                    indentCnt + 2, "");
      printToString(s, "%*s\"Type\": \"<collection>\"\n", indentCnt + 2, "");
      printToString(s, "%*s}", indentCnt, "");
      break;
    case BSReflStream::String_Ref:
      printToString(s, "{\n%*s\"Data\": ", indentCnt + 2, "");
      dumpObject(s, o->children()[0], indentCnt + 2);
      printToString(s, ",\n%*s\"Type\": \"<ref>\"\n", indentCnt + 2, "");
      printToString(s, "%*s}", indentCnt, "");
      break;
#if CDB_JSON_QUOTE_NUMBERS
    case BSReflStream::String_Int8:
    case BSReflStream::String_Int16:
    case BSReflStream::String_Int32:
      printToString(s, "\"%d\"", int(o->intValue()));
      break;
    case BSReflStream::String_UInt8:
    case BSReflStream::String_UInt16:
    case BSReflStream::String_UInt32:
      printToString(s, "\"%u\"", (unsigned int) o->uintValue());
      break;
    case BSReflStream::String_Int64:
      printToString(s, "\"%lld\"", (long long) o->int64Value());
      break;
    case BSReflStream::String_UInt64:
      printToString(s, "\"%llu\"", (unsigned long long) o->uint64Value());
      break;
    case BSReflStream::String_Bool:
      printToString(s, "%s", (!o->boolValue() ? "\"false\"" : "\"true\""));
      break;
    case BSReflStream::String_Float:
      printToString(s, "\"%f\"", o->floatValue());
      break;
    case BSReflStream::String_Double:
      printToString(s, "\"%f\"", o->doubleValue());
      break;
#else
    case BSReflStream::String_Int8:
    case BSReflStream::String_Int16:
    case BSReflStream::String_Int32:
      printToString(s, "%d", int(o->intValue()));
      break;
    case BSReflStream::String_UInt8:
    case BSReflStream::String_UInt16:
    case BSReflStream::String_UInt32:
      printToString(s, "%u", (unsigned int) o->uintValue());
      break;
    case BSReflStream::String_Int64:
      printToString(s, "\"%lld\"", (long long) o->int64Value());
      break;
    case BSReflStream::String_UInt64:
      printToString(s, "\"%llu\"", (unsigned long long) o->uint64Value());
      break;
    case BSReflStream::String_Bool:
      printToString(s, "%s", (!o->boolValue() ? "false" : "true"));
      break;
    case BSReflStream::String_Float:
      printToString(s, "%.7g", o->floatValue());
      break;
    case BSReflStream::String_Double:
      printToString(s, "%.14g", o->doubleValue());
      break;
#endif
    default:
      printToString(s, "\"<unknown>\"");
      break;
  }
}

const BSMaterialsCDB::CDBClassDef *
    BSMaterialsCDB::getClassDef(std::uint32_t type) const
{
  std::uint32_t h = 0xFFFFFFFFU;
  hashFunctionCRC32C< std::uint32_t >(h, type);
  if (classes.size() < 1) [[unlikely]]
    return nullptr;
  std::uint32_t m = std::uint32_t(classes.size() - 1);
  for (h = h & m; classes[h].className; h = (h + 1U) & m)
  {
    if (classes[h].className == type)
      return &(classes[h]);
  }
  return nullptr;
}

const BSMaterialsCDB::MaterialObject *
    BSMaterialsCDB::getMaterial(const std::string& materialPath) const
{
  std::map< BSResourceID, const MaterialObject * >::const_iterator  i =
      matFileObjectMap.find(BSResourceID(materialPath));
  if (i != matFileObjectMap.end())
    return i->second;
  return nullptr;
}

void BSMaterialsCDB::getJSONMaterial(
    std::string& jsonBuf, const std::string& materialPath) const
{
  jsonBuf.clear();
  if (materialPath.empty())
    return;
  const MaterialObject  *i = getMaterial(materialPath);
  if (!i)
    return;
  jsonBuf = "{\n  \"Objects\": [\n";
  for ( ; i; i = i->getNextChildObject())
  {
    printToString(jsonBuf, "    {\n      \"Components\": [\n");
    for (const MaterialComponent *j = i->components; j; j = j->next)
    {
      jsonBuf += "        ";
#if CDB_SORT_STRUCT_MEMBERS
      size_t  prvLen = jsonBuf.length();
      dumpObject(jsonBuf, j->o, 8);
      size_t  n = jsonBuf.rfind("\n          \"Type\"");
      if (n != std::string::npos && n > prvLen)
      {
        char    tmpBuf[64];
        std::sprintf(tmpBuf, "\n          \"Index\": %u,",
                     (unsigned int) (j->key & 0xFFFFU));
        jsonBuf.insert(n, tmpBuf);
        printToString(jsonBuf, ",\n");
      }
#else
      dumpObject(jsonBuf, j->o, 8);
      if (jsonBuf.ends_with("\n        }"))
      {
        jsonBuf.resize(jsonBuf.length() - 10);
        printToString(jsonBuf, ",\n          \"Index\": %u\n",
                      (unsigned int) (j->key & 0xFFFFU));
        printToString(jsonBuf, "        },\n");
      }
#endif
    }
    if (jsonBuf.ends_with(",\n"))
    {
      jsonBuf.resize(jsonBuf.length() - 1);
      jsonBuf[jsonBuf.length() - 1] = '\n';
    }
    printToString(jsonBuf, "      ],\n");
    const MaterialObject  *j = i->baseObject;
    if (i->parent)
    {
      printToString(jsonBuf, "      \"ID\": \"res:%08X:%08X:%08X\",\n",
                    (unsigned int) i->persistentID.dir,
                    (unsigned int) i->persistentID.file,
                    (unsigned int) i->persistentID.ext);
    }
    const char  *parentStr = "";
    if (j)
    {
      if (j->persistentID.file == 0x7EA3660CU)
        parentStr = "materials\\\\layered\\\\root\\\\layeredmaterials.mat";
      else if (j->persistentID.file == 0x8EBE84FFU)
        parentStr = "materials\\\\layered\\\\root\\\\blenders.mat";
      else if (j->persistentID.file == 0x574A4CF3U)
        parentStr = "materials\\\\layered\\\\root\\\\layers.mat";
      else if (j->persistentID.file == 0x7D1E021BU)
        parentStr = "materials\\\\layered\\\\root\\\\materials.mat";
      else if (j->persistentID.file == 0x06F52154U)
        parentStr = "materials\\\\layered\\\\root\\\\texturesets.mat";
      else if (j->persistentID.file == 0x4298BB09U)
        parentStr = "materials\\\\layered\\\\root\\\\uvstreams.mat";
    }
    printToString(jsonBuf, "      \"Parent\": \"%s\"\n    },\n", parentStr);
  }
  jsonBuf.resize(jsonBuf.length() - 2);
  jsonBuf += "\n  ],\n  \"Version\": 1\n}";
}

