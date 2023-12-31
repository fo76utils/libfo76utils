
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
  ext = crcValue;
  crcValue = 0U;
  for ( ; i < extPos; i++)
  {
    // base name
    unsigned char c = (unsigned char) fileName.c_str()[i];
    if (c >= 0x41 && c <= 0x5A)         // 'A' - 'Z'
      c = c | 0x20;                     // convert to lower case
    hashFunctionCRC32(crcValue, c);
  }
  dir = crcValue;
  i++;
  if ((i + 3) <= fileName.length())
    file = FileBuffer::readUInt32Fast(fileName.c_str() + i);
  else if ((i + 2) <= fileName.length())
    file = FileBuffer::readUInt16Fast(fileName.c_str() + i);
  else if (i < fileName.length())
    file = (unsigned char) fileName.c_str()[i];
  else
    file = 0U;
  // convert extension to lower case
  file = file | ((file >> 1) & 0x20202020U);
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
  objectBuffers.back().resize(size_t((addr + bytesRequired) - addr0));
  unsigned char *p = reinterpret_cast< unsigned char * >(addr);
  return p;
}

BSMaterialsCDB::CDBObject * BSMaterialsCDB::allocateObject(
    std::uint32_t itemType, const CDBClassDef *classDef, size_t elementCnt)
{
  size_t  dataSize = sizeof(CDBObject);
  size_t  childCnt = 0;
  if (itemType == String_List || itemType == String_Map)
    childCnt = elementCnt;
  else if (itemType == String_Ref)
    childCnt = 1;
  else if (classDef)
    childCnt = classDef->fieldCnt;
  if (childCnt > 1)
    dataSize += (sizeof(CDBObject *) * (childCnt - 1));
  CDBObject *o =
      reinterpret_cast< CDBObject * >(allocateSpace(dataSize,
                                                    alignof(CDBObject)));
  o->type = std::uint16_t(itemType);
  o->childCnt = std::uint16_t(childCnt);
  o->refCnt = 0;
  if (childCnt)
  {
    for (size_t i = 0; i < childCnt; i++)
      o->data.children[i] = nullptr;
  }
  else if (itemType == String_String)
  {
    o->data.stringValue = "";
  }
  else
  {
    o->data.uint64Value = 0U;
  }
  return o;
}

void BSMaterialsCDB::copyObject(CDBObject*& o)
{
  o->refCnt++;
  for (std::uint32_t i = 0U; i < o->childCnt; i++)
  {
    if (o->data.children[i])
      copyObject(o->data.children[i]);
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
    if (tmp->className == String_BSBind_ControllerComponent) [[unlikely]]
      tmp->o = allocateObject(tmp->className, getClassDef(tmp->className), 0);
    else
#endif
    if (tmp->o) [[likely]]
      copyObject(tmp->o);
    tmp->next = nullptr;
    *prv = tmp;
    prv = &(tmp->next);
  }
}

void BSMaterialsCDB::loadItem(CDBObject*& o, Chunk& chunkBuf, bool isDiff,
                              std::uint32_t itemType)
{
  if (o && o->refCnt)
  {
    o->refCnt--;
    size_t  dataSize = sizeof(CDBObject);
    if (o->childCnt > 1)
      dataSize += (sizeof(CDBObject *) * (o->childCnt - 1));
    CDBObject *p =
        reinterpret_cast< CDBObject * >(
            allocateSpace(dataSize, alignof(CDBObject)));
    std::memcpy(p, o, dataSize);
    o = p;
    o->refCnt = 0;
  }
  const CDBClassDef *classDef = nullptr;
  if (itemType > String_Unknown)
  {
    classDef = getClassDef(itemType);
    if (!classDef)
      itemType = String_Unknown;
  }
  if (!(o && o->type == itemType))
  {
    if (!(itemType == String_List || itemType == String_Map))
      o = allocateObject(itemType, classDef, 0);
  }
  if (itemType > String_Unknown)
  {
    unsigned int  nMax = classDef->fieldCnt;
    if (classDef->isUser) [[unlikely]]
    {
      Chunk userBuf;
      unsigned int  userChunkType = readChunk(userBuf);
      if (userChunkType != ChunkType_USER && userChunkType != ChunkType_USRD)
      {
        throw FO76UtilsError("unexpected chunk type in CDB file at 0x%08x",
                             (unsigned int) getPosition());
      }
      isDiff = (userChunkType == ChunkType_USRD);
      std::uint32_t className1 = findString(userBuf.readUInt32());
      if (className1 != itemType)
      {
        throw FO76UtilsError("USER chunk has unexpected type at 0x%08x",
                             (unsigned int) getPosition());
      }
      std::uint32_t className2 = findString(userBuf.readUInt32());
      if (className2 == className1)
      {
        // no type conversion
        nMax--;
        for (unsigned int n = 0U - 1U;
             userBuf.getFieldNumber(n, nMax, isDiff); )
        {
          loadItem(o->data.children[n], userBuf, isDiff,
                   classDef->fields[n].type);
        }
      }
      else if (className2 < String_Unknown)
      {
        if (!o->childCnt)
        {
          o->childCnt++;
          o->data.children[0] = nullptr;
        }
        unsigned int  n = 0U;
        do
        {
          loadItem(o->data.children[n], userBuf, isDiff, className2);
          className2 = findString(userBuf.readUInt32());
        }
        while (++n < nMax && className2 < String_Unknown);
      }
      return;
    }
    nMax--;
    if (itemType == String_BSComponentDB2_ID) [[unlikely]]
    {
      if (!nMax && classDef->fields[0].type == String_UInt32)
      {
        std::uint32_t objectID = 0U;
        for (unsigned int n = 0U - 1U; chunkBuf.getFieldNumber(n, 0U, isDiff); )
        {
          if (!chunkBuf.readUInt32(objectID))
            break;
        }
        o->childCnt = 0;
        o->data.objectPtr = findObject(objectID);
        return;
      }
    }
    for (unsigned int n = 0U - 1U; chunkBuf.getFieldNumber(n, nMax, isDiff); )
      loadItem(o->data.children[n], chunkBuf, isDiff, classDef->fields[n].type);
    return;
  }
  FileBuffer& buf2 = *(static_cast< FileBuffer * >(&chunkBuf));
  switch (itemType)
  {
    case String_String:
      {
        unsigned int  len = buf2.readUInt16();
        if ((buf2.getPosition() + std::uint64_t(len)) > buf2.size())
          errorMessage("unexpected end of CDB file");
        char    *s =
            reinterpret_cast< char * >(allocateSpace(len + 1U, sizeof(char)));
        std::memcpy(s, buf2.getReadPtr(), len);
        s[len] = '\0';
        o->data.stringValue = s;
        buf2.setPosition(buf2.getPosition() + len);
      }
      break;
    case String_List:
    case String_Map:
      {
        Chunk listBuf;
        unsigned int  chunkType = readChunk(listBuf);
        bool    isMap = (itemType == String_Map);
        if (chunkType != (!isMap ? ChunkType_LIST : ChunkType_MAPC))
        {
          throw FO76UtilsError("unexpected chunk type in CDB file at 0x%08x",
                               (unsigned int) getPosition());
        }
        std::uint32_t classNames[2];
        classNames[0] = findString(listBuf.readUInt32());
        classNames[1] = classNames[0];
        if (isMap)
          classNames[1] = findString(listBuf.readUInt32());
        std::uint64_t listSize = 0U;
        if ((listBuf.getPosition() + 4ULL) <= listBuf.size())
          listSize = listBuf.readUInt32Fast();
        listSize = listSize << (unsigned char) isMap;
        std::uint32_t n = 0U;
        bool    appendingItems = false;
        if (isDiff && o && o->type == itemType && o->childCnt &&
            o->data.children[0]->type == classNames[0])
        {
          if (!isMap)
            appendingItems = (std::uint32_t(classNames[0] - String_Int8) > 11U);
          else
            appendingItems = (o->data.children[1]->type == classNames[1]);
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
              p->data.children[n] = o->data.children[n];
          }
          o = p;
        }
        o->childCnt = std::uint16_t(listSize);
        for ( ; n < listSize; n++)
        {
          loadItem(o->data.children[n], listBuf, isDiff, classNames[n & 1U]);
          if (isMap && !(n & 1U) && classNames[0] == String_String)
          {
            // check for duplicate keys
            for (std::uint32_t k = 0U; k < n; k = k + 2U)
            {
              if (std::strcmp(o->data.children[k]->data.stringValue,
                              o->data.children[n]->data.stringValue) == 0)
              {
                loadItem(o->data.children[k + 1U], listBuf, isDiff,
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
    case String_Ref:
      {
        std::uint32_t refType = findString(buf2.readUInt32());
        loadItem(o->data.children[0], chunkBuf, isDiff, refType);
        return;
      }
      break;
    case String_Int8:
      o->data.int8Value = std::int8_t(buf2.readUInt8());
      break;
    case String_UInt8:
      o->data.uint8Value = buf2.readUInt8();
      break;
    case String_Int16:
      o->data.int16Value = std::int16_t(buf2.readUInt16());
      break;
    case String_UInt16:
      o->data.uint16Value = buf2.readUInt16();
      break;
    case String_Int32:
      o->data.int32Value = buf2.readInt32();
      break;
    case String_UInt32:
      o->data.uint32Value = buf2.readUInt32();
      break;
    case String_Int64:
      o->data.int64Value = std::int64_t(buf2.readUInt64());
      break;
    case String_UInt64:
      o->data.uint64Value = buf2.readUInt64();
      break;
    case String_Bool:
      o->data.boolValue = bool(buf2.readUInt8());
      break;
    case String_Float:
      o->data.floatValue = buf2.readFloat();
      break;
    case String_Double:
      // FIXME: implement this in a portable way
      o->data.doubleValue =
          std::bit_cast< double, std::uint64_t >(buf2.readUInt64());
      break;
    default:
      chunkBuf.setPosition(chunkBuf.size());
      break;
  }
}

void BSMaterialsCDB::readAllChunks()
{
  objectBuffers.emplace_back();
  objectBuffers.back().reserve(65536);
  std::vector< MaterialObject * > objectTable;
  std::vector< std::pair< std::uint32_t, std::uint32_t > >  componentInfo;
  Chunk chunkBuf;
  unsigned int  chunkType;
  size_t  componentID = 0;
  size_t  componentCnt = 0;
  size_t  classCnt = 0;
  while ((chunkType = readChunk(chunkBuf)) != 0U)
  {
    std::uint32_t className = String_None;
    if (chunkType != ChunkType_TYPE && chunkBuf.size() >= 4) [[likely]]
      className = findString(chunkBuf.readUInt32Fast());
    if (chunkType == ChunkType_DIFF || chunkType == ChunkType_OBJT) [[likely]]
    {
      bool    isDiff = (chunkType == ChunkType_DIFF);
      if (componentID < componentCnt) [[likely]]
      {
        std::uint32_t dbID = componentInfo[componentID].first;
        std::uint32_t key = componentInfo[componentID].second;
        MaterialObject  *i = findObject(dbID);
        if (i && className > String_Unknown)
        {
          if (i->baseObject && i->baseObject->baseObject)
            copyBaseObject(*i);
          loadItem(findComponent(*i, key, className).o,
                   chunkBuf, isDiff, className);
        }
        componentID++;
      }
      continue;
    }
    if (chunkType == ChunkType_CLAS)
    {
      if (className < String_Unknown)
        errorMessage("invalid class ID in CDB file");
      if (className == String_Unknown)
        continue;
      if (classCnt-- < 1)
        errorMessage("unexpected CLAS chunk in CDB file");
      std::uint32_t h = 0xFFFFFFFFU;
      hashFunctionCRC32C< std::uint32_t >(h, className);
      std::uint32_t m = std::uint32_t(classes.size() - 1);
      for (h = h & m; classes[h].className; h = (h + 1U) & m)
      {
        if (classes[h].className == className)
          break;
      }
      CDBClassDef&  classDef = classes[h];
      classDef.className = className;
      (void) chunkBuf.readUInt32();     // classVersion
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
        f.name = findString(chunkBuf.readUInt32());
        if (f.name < String_Unknown)
          errorMessage("invalid field name in class definition in CDB file");
        f.type = findString(chunkBuf.readUInt32());
        (void) chunkBuf.readUInt16();   // dataOffset
        (void) chunkBuf.readUInt16();   // dataSize
      }
      continue;
    }
    if (chunkType == ChunkType_TYPE) [[unlikely]]
    {
      classCnt = chunkBuf.readUInt32();
      if (classCnt > 4096)
        errorMessage("invalid number of class definitions in CDB file");
      size_t  m = 16;
      while (m < (classCnt << 2))
        m = m << 1;
      classes.clear();
      classes.resize(m);
      for (size_t i = 0; i < m; i++)
        classes[i].className = 0U;
      continue;
    }
    if (chunkType != ChunkType_LIST)
      continue;
    std::uint32_t n = 0U;
    if (chunkBuf.size() >= 8)
      n = chunkBuf.readUInt32Fast();
    if (className == String_BSComponentDB2_DBFileIndex_ObjectInfo)
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
        persistentID.dir = chunkBuf.readUInt32();
        persistentID.file = chunkBuf.readUInt32();
        persistentID.ext = chunkBuf.readUInt32();
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
    else if (className == String_BSComponentDB2_DBFileIndex_ComponentInfo)
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
    else if (className == String_BSComponentDB2_DBFileIndex_EdgeInfo)
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
    if (!p->parent && p->persistentID.file == 0x0074616DU)      // "mat\0"
      matFileObjectMap[p->persistentID] = p;
  }
}

void BSMaterialsCDB::dumpObject(
    std::string& s, const CDBObject *o, int indentCnt) const
{
  if (!o)
  {
    printToString(s, "null");
    return;
  }
  if (o->type > String_Unknown)
  {
    const CDBClassDef *classDef = getClassDef(o->type);
    if (o->type == String_BSComponentDB2_ID) [[unlikely]]
    {
      if (classDef && classDef->fieldCnt == 1 &&
          classDef->fields[0].type == String_UInt32)
      {
        if (!o->data.objectPtr)
        {
          printToString(s, "\"\"");
        }
        else
        {
          printToString(s, "\"res:%08X:%08X:%08X\"",
                        (unsigned int) o->data.objectPtr->persistentID.ext,
                        (unsigned int) o->data.objectPtr->persistentID.dir,
                        (unsigned int) o->data.objectPtr->persistentID.file);
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
        if (!o->data.children[i])
          continue;
        int     tmpFieldNameID = String_None;
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
                    indentCnt + 4, "", stringTable[fieldNameID]);
      dumpObject(s, o->data.children[fieldNum], indentCnt + 4);
      printToString(s, ",");
    }
#else
    for (std::uint32_t fieldNum = 0U; fieldNum < o->childCnt; fieldNum++)
    {
      if (!o->data.children[fieldNum])
        continue;
      const char  *fieldNameStr = "null";
      if (classDef && fieldNum < classDef->fieldCnt)
        fieldNameStr = stringTable[classDef->fields[fieldNum].name];
      printToString(s, "\n%*s\"%s\": ", indentCnt + 4, "", fieldNameStr);
      dumpObject(s, o->data.children[fieldNum], indentCnt + 4);
      printToString(s, ",");
    }
#endif
    if (s.ends_with(','))
    {
      s[s.length() - 1] = '\n';
      printToString(s, "%*s", indentCnt + 2, "");
    }
    printToString(s, "},\n%*s\"Type\": \"%s\"\n",
                  indentCnt + 2, "", stringTable[o->type]);
    printToString(s, "%*s}", indentCnt, "");
    return;
  }
  switch (o->type)
  {
    case String_None:
      printToString(s, "null");
      break;
    case String_String:
      s += '"';
      for (size_t i = 0; o->data.stringValue[i]; i++)
      {
        char    c = o->data.stringValue[i];
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
    case String_List:
      printToString(s, "{\n%*s\"Data\": [", indentCnt + 2, "");
      for (std::uint32_t i = 0U; i < o->childCnt; i++)
      {
        printToString(s, "\n%*s", indentCnt + 4, "");
        dumpObject(s, o->data.children[i], indentCnt + 4);
        if ((i + 1U) < o->childCnt)
          printToString(s, ",");
        else
          printToString(s, "\n%*s", indentCnt + 2, "");
      }
      printToString(s, "],\n");
      if (o->childCnt)
      {
        std::uint32_t elementType = 0U;
        if (o->data.children[0])
          elementType = o->data.children[0]->type;
        const char  *elementTypeStr = stringTable[elementType];
        if (elementType && elementType < String_Ref)
        {
          elementTypeStr =
              (elementType == String_String ? "BSFixedString" : "<collection>");
        }
        printToString(s, "%*s\"ElementType\": \"%s\",\n",
                      indentCnt + 2, "", elementTypeStr);
      }
      printToString(s, "%*s\"Type\": \"<collection>\"\n", indentCnt + 2, "");
      printToString(s, "%*s}", indentCnt, "");
      break;
    case String_Map:
      printToString(s, "{\n%*s\"Data\": [", indentCnt + 2, "");
      for (std::uint32_t i = 0U; i < o->childCnt; i++)
      {
        printToString(s, "\n%*s{", indentCnt + 4, "");
        printToString(s, "\n%*s\"Data\": {", indentCnt + 6, "");
        printToString(s, "\n%*s\"Key\": ", indentCnt + 8, "");
        dumpObject(s, o->data.children[i], indentCnt + 8);
        i++;
        printToString(s, ",\n%*s\"Value\": ", indentCnt + 8, "");
        dumpObject(s, o->data.children[i], indentCnt + 8);
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
    case String_Ref:
      printToString(s, "{\n%*s\"Data\": ", indentCnt + 2, "");
      dumpObject(s, o->data.children[0], indentCnt + 2);
      printToString(s, ",\n%*s\"Type\": \"<ref>\"\n", indentCnt + 2, "");
      printToString(s, "%*s}", indentCnt, "");
      break;
#if CDB_JSON_QUOTE_NUMBERS
    case String_Int8:
      printToString(s, "\"%d\"", int(o->data.int8Value));
      break;
    case String_UInt8:
      printToString(s, "\"%u\"", (unsigned int) o->data.uint8Value);
      break;
    case String_Int16:
      printToString(s, "\"%d\"", int(o->data.int16Value));
      break;
    case String_UInt16:
      printToString(s, "\"%u\"", (unsigned int) o->data.uint16Value);
      break;
    case String_Int32:
      printToString(s, "\"%d\"", int(o->data.int32Value));
      break;
    case String_UInt32:
      printToString(s, "\"%u\"", (unsigned int) o->data.uint32Value);
      break;
    case String_Int64:
      printToString(s, "\"%lld\"", (long long) o->data.int64Value);
      break;
    case String_UInt64:
      printToString(s, "\"%llu\"", (unsigned long long) o->data.uint64Value);
      break;
    case String_Bool:
      printToString(s, "%s", (!o->data.boolValue ? "\"false\"" : "\"true\""));
      break;
    case String_Float:
      printToString(s, "\"%f\"", o->data.floatValue);
      break;
    case String_Double:
      printToString(s, "\"%f\"", o->data.doubleValue);
      break;
#else
    case String_Int8:
      printToString(s, "%d", int(o->data.int8Value));
      break;
    case String_UInt8:
      printToString(s, "%u", (unsigned int) o->data.uint8Value);
      break;
    case String_Int16:
      printToString(s, "%d", int(o->data.int16Value));
      break;
    case String_UInt16:
      printToString(s, "%u", (unsigned int) o->data.uint16Value);
      break;
    case String_Int32:
      printToString(s, "%d", int(o->data.int32Value));
      break;
    case String_UInt32:
      printToString(s, "%u", (unsigned int) o->data.uint32Value);
      break;
    case String_Int64:
      printToString(s, "\"%lld\"", (long long) o->data.int64Value);
      break;
    case String_UInt64:
      printToString(s, "\"%llu\"", (unsigned long long) o->data.uint64Value);
      break;
    case String_Bool:
      printToString(s, "%s", (!o->data.boolValue ? "false" : "true"));
      break;
    case String_Float:
      printToString(s, "%.7g", o->data.floatValue);
      break;
    case String_Double:
      printToString(s, "%.14g", o->data.doubleValue);
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
                    (unsigned int) i->persistentID.ext,
                    (unsigned int) i->persistentID.dir,
                    (unsigned int) i->persistentID.file);
    }
    const char  *parentStr = "";
    if (j)
    {
      if (j->persistentID.dir == 0x7EA3660CU)
        parentStr = "materials\\\\layered\\\\root\\\\layeredmaterials.mat";
      else if (j->persistentID.dir == 0x8EBE84FFU)
        parentStr = "materials\\\\layered\\\\root\\\\blenders.mat";
      else if (j->persistentID.dir == 0x574A4CF3U)
        parentStr = "materials\\\\layered\\\\root\\\\layers.mat";
      else if (j->persistentID.dir == 0x7D1E021BU)
        parentStr = "materials\\\\layered\\\\root\\\\materials.mat";
      else if (j->persistentID.dir == 0x06F52154U)
        parentStr = "materials\\\\layered\\\\root\\\\texturesets.mat";
      else if (j->persistentID.dir == 0x4298BB09U)
        parentStr = "materials\\\\layered\\\\root\\\\uvstreams.mat";
    }
    printToString(jsonBuf, "      \"Parent\": \"%s\"\n    },\n", parentStr);
  }
  jsonBuf.resize(jsonBuf.length() - 2);
  jsonBuf += "\n  ],\n  \"Version\": 1\n}";
}

