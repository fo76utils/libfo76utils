
#include "common.hpp"
#include "bsmatcdb.hpp"

void BSMaterialsCDB::loadJSONItem(
    CDBObject*& o, const JSONReader::JSONItem *jsonItem, std::uint32_t itemType,
    MaterialObject *materialObject,
    std::map< BSResourceID, MaterialObject * >& objectMap)
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
    // structure
    if (itemType == BSReflStream::String_BSComponentDB2_ID) [[unlikely]]
    {
      if (jsonItem->type != JSONReader::JSONItemType_String)
        return;
      BSResourceID  objectID;
      objectID.fromJSONString(
          static_cast< const JSONReader::JSONString * >(jsonItem)->value);
      MaterialObject  *p = nullptr;
      std::map< BSResourceID, MaterialObject * >::iterator  i =
          objectMap.find(objectID);
      if (i != objectMap.end())
        p = i->second;
      for (MaterialObject *q = materialObject; q;
           q = const_cast< MaterialObject * >(q->parent))
      {
        if (p == q)
        {
          // circular links
          p = nullptr;
          break;
        }
      }
      static_cast< CDBObject_Link * >(o)->objectPtr = p;
      if (p)
      {
        p->parent = materialObject;
        p->next = materialObject->children;
        materialObject->children = p;
      }
      return;
    }
    if (jsonItem->type != JSONReader::JSONItemType_Object)
      return;
    const JSONReader::JSONObject  *jsonObject =
        static_cast< const JSONReader::JSONObject * >(jsonItem);
    std::map< std::string, const JSONReader::JSONItem * >::const_iterator j =
        jsonObject->children.find("Type");
    if (j == jsonObject->children.end() ||
        !j->second || j->second->type != JSONReader::JSONItemType_String)
    {
      return;
    }
    const std::string&  itemTypeStr =
        static_cast< const JSONReader::JSONString * >(j->second)->value;
    if (itemTypeStr != BSReflStream::stringTable[itemType])
      return;
    j = jsonObject->children.find("Data");
    if (j == jsonObject->children.end() ||
        !j->second || j->second->type != JSONReader::JSONItemType_Object)
    {
      return;
    }
    const JSONReader::JSONObject  *jsonObjectData =
        static_cast< const JSONReader::JSONObject * >(j->second);
    for (std::map< std::string, const JSONReader::JSONItem * >::const_iterator
             i = jsonObjectData->children.begin();
         i != jsonObjectData->children.end(); i++)
    {
      const std::string&  fieldName = i->first;
      int     fieldNum = -1;
      for (size_t l = 0; l < classDef->fieldCnt; l++)
      {
        if (fieldName == BSReflStream::stringTable[classDef->fields[l].name])
        {
          fieldNum = int(l);
          break;
        }
      }
      if (fieldNum < 0 || !i->second)
        continue;
      std::uint32_t fieldType = classDef->fields[fieldNum].type;
      loadJSONItem(o->children()[fieldNum],
                   i->second, fieldType, materialObject, objectMap);
    }
    return;
  }

  switch (itemType)
  {
    case BSReflStream::String_String:
      if (jsonItem->type == JSONReader::JSONItemType_String)
      {
        const std::string&  s =
            static_cast< const JSONReader::JSONString * >(jsonItem)->value;
        char    *t = reinterpret_cast< char * >(
                         allocateSpace(s.length() + 1, sizeof(char)));
        std::memcpy(t, s.c_str(), s.length() + 1);
        static_cast< CDBObject_String * >(o)->value = t;
      }
      else
      {
        static_cast< CDBObject_String * >(o)->value = "";
      }
      break;
    case BSReflStream::String_List:
    case BSReflStream::String_Map:
      {
        // TODO
      }
      break;
    case BSReflStream::String_Ref:
      {
        // TODO
      }
      break;
    case BSReflStream::String_Int8:
    case BSReflStream::String_UInt8:
    case BSReflStream::String_Int16:
    case BSReflStream::String_UInt16:
    case BSReflStream::String_Int32:
    case BSReflStream::String_UInt32:
    case BSReflStream::String_Int64:
    case BSReflStream::String_UInt64:
    case BSReflStream::String_Float:
    case BSReflStream::String_Double:
      if (jsonItem->type == JSONReader::JSONItemType_Number)
      {
        double  itemValue =
            static_cast< const JSONReader::JSONNumber * >(jsonItem)->value;
        switch (itemType)
        {
          case BSReflStream::String_Int8:
            static_cast< CDBObject_Int * >(o)->value =
                std::int8_t(roundDouble(itemValue));
            break;
          case BSReflStream::String_UInt8:
            static_cast< CDBObject_UInt * >(o)->value =
                std::uint8_t(roundDouble(itemValue));
            break;
          case BSReflStream::String_Int16:
            static_cast< CDBObject_Int * >(o)->value =
                std::int16_t(roundDouble(itemValue));
            break;
          case BSReflStream::String_UInt16:
            static_cast< CDBObject_UInt * >(o)->value =
                std::uint16_t(roundDouble(itemValue));
            break;
          case BSReflStream::String_Int32:
            static_cast< CDBObject_Int * >(o)->value =
                std::int32_t(roundDouble(itemValue));
            break;
          case BSReflStream::String_UInt32:
            static_cast< CDBObject_UInt * >(o)->value =
                std::uint32_t(std::llrint(itemValue));
            break;
          case BSReflStream::String_Int64:
            static_cast< CDBObject_Int64 * >(o)->value =
                std::int64_t(std::llrint(itemValue));
            break;
          case BSReflStream::String_UInt64:
            static_cast< CDBObject_UInt64 * >(o)->value =
                std::uint64_t(std::llrint(itemValue));
            break;
          case BSReflStream::String_Float:
            static_cast< CDBObject_Float * >(o)->value = float(itemValue);
            break;
          case BSReflStream::String_Double:
            static_cast< CDBObject_Double * >(o)->value = itemValue;
            break;
        }
      }
      else if (jsonItem->type == JSONReader::JSONItemType_String)
      {
        const std::string&  itemValue =
            static_cast< const JSONReader::JSONString * >(jsonItem)->value;
        char    *endp = nullptr;
        switch (itemType)
        {
          case BSReflStream::String_Int8:
            static_cast< CDBObject_Int * >(o)->value =
                std::int8_t(std::strtol(itemValue.c_str(), &endp, 10));
            break;
          case BSReflStream::String_UInt8:
            static_cast< CDBObject_UInt * >(o)->value =
                std::uint8_t(std::strtol(itemValue.c_str(), &endp, 10));
            break;
          case BSReflStream::String_Int16:
            static_cast< CDBObject_Int * >(o)->value =
                std::int16_t(std::strtol(itemValue.c_str(), &endp, 10));
            break;
          case BSReflStream::String_UInt16:
            static_cast< CDBObject_UInt * >(o)->value =
                std::uint16_t(std::strtol(itemValue.c_str(), &endp, 10));
            break;
          case BSReflStream::String_Int32:
            static_cast< CDBObject_Int * >(o)->value =
                std::int32_t(std::strtol(itemValue.c_str(), &endp, 10));
            break;
          case BSReflStream::String_UInt32:
            static_cast< CDBObject_UInt * >(o)->value =
                std::uint32_t(std::strtoll(itemValue.c_str(), &endp, 10));
            break;
          case BSReflStream::String_Int64:
            static_cast< CDBObject_Int64 * >(o)->value =
                std::int64_t(std::strtoll(itemValue.c_str(), &endp, 10));
            break;
          case BSReflStream::String_UInt64:
            static_cast< CDBObject_UInt64 * >(o)->value =
                std::uint64_t(std::strtoll(itemValue.c_str(), &endp, 10));
            break;
          case BSReflStream::String_Float:
            static_cast< CDBObject_Float * >(o)->value =
                float(std::strtod(itemValue.c_str(), &endp));
            break;
          case BSReflStream::String_Double:
            static_cast< CDBObject_Double * >(o)->value =
                std::strtod(itemValue.c_str(), &endp);
            break;
        }
      }
      break;
    case BSReflStream::String_Bool:
      if (jsonItem->type == JSONReader::JSONItemType_Boolean)
      {
        static_cast< CDBObject_Bool * >(o)->value =
            static_cast< const JSONReader::JSONBoolean * >(jsonItem)->value;
      }
      else if (jsonItem->type == JSONReader::JSONItemType_String)
      {
        const std::string&  s =
            static_cast< const JSONReader::JSONString * >(jsonItem)->value;
        static_cast< CDBObject_Bool * >(o)->value =
            !(s == "false" || s == "False" || s == "0" || s.empty());
      }
      break;
    default:
      break;
  }
}

void BSMaterialsCDB::loadJSONFile(
    const JSONReader& matFile, const char *materialPath)
{
  const JSONReader::JSONItem  *p = matFile.getData();
  if (!(p && p->type == JSONReader::JSONItemType_Object))
    return;
  const JSONReader::JSONObject  *jsonData =
      static_cast< const JSONReader::JSONObject * >(p);
  std::map< std::string, const JSONReader::JSONItem * >::const_iterator j =
      jsonData->children.find("Objects");
  if (j == jsonData->children.end() ||
      !j->second || j->second->type != JSONReader::JSONItemType_Array)
  {
    return;
  }
  const JSONReader::JSONArray *jsonObjects =
      static_cast< const JSONReader::JSONArray * >(j->second);
  j = jsonData->children.find("Version");
  if (j == jsonData->children.end() ||
      !j->second || j->second->type != JSONReader::JSONItemType_Number ||
      static_cast< const JSONReader::JSONNumber * >(j->second)->value != 1.0)
  {
    return;
  }

  // load objects
  std::map< BSResourceID, MaterialObject * >  objectMap;
  std::vector< MaterialObject * > objects(jsonObjects->children.size(),
                                          nullptr);
  for (size_t i = 0; i < jsonObjects->children.size(); i++)
  {
    if (!(jsonObjects->children[i] &&
          jsonObjects->children[i]->type == JSONReader::JSONItemType_Object))
    {
      continue;
    }
    const JSONReader::JSONObject  *jsonObject =
        static_cast< const JSONReader::JSONObject * >(jsonObjects->children[i]);
    j = jsonObject->children.find("Parent");
    if (j == jsonObject->children.end() ||
        !j->second || j->second->type != JSONReader::JSONItemType_String)
    {
      continue;
    }
    BSResourceID  parentID;
    parentID.fromJSONString(
        static_cast< const JSONReader::JSONString * >(j->second)->value);
    BSResourceID  objectID;
    j = jsonObject->children.find("ID");
    if (j == jsonObject->children.end() ||
        !j->second || j->second->type != JSONReader::JSONItemType_String)
    {
      objectID.fromJSONString(std::string(materialPath ? materialPath : ""));
    }
    else
    {
      objectID.fromJSONString(
          static_cast< const JSONReader::JSONString * >(j->second)->value);
    }
    const MaterialObject  *parentPtr = nullptr;
    {
      std::map< BSResourceID, const MaterialObject * >::const_iterator  k =
          matFileObjectMap.find(parentID);
      if (k != matFileObjectMap.end())
        parentPtr = k->second;
    }
    if (!parentPtr)
      continue;
    j = jsonObject->children.find("Components");
    if (j == jsonObject->children.end() ||
        !j->second || j->second->type != JSONReader::JSONItemType_Array)
    {
      continue;
    }
    MaterialObject  *o = nullptr;
    {
      std::map< BSResourceID, MaterialObject * >::iterator  k =
          objectMap.find(objectID);
      if (k == objectMap.end())
      {
        o = reinterpret_cast< MaterialObject * >(
                allocateSpace(sizeof(MaterialObject), alignof(MaterialObject)));
        o->persistentID = objectID;
        o->dbID = std::uint32_t(i + 0x01000000);
        o->baseObject = parentPtr;
        o->components = nullptr;
        o->parent = nullptr;
        o->children = nullptr;
        o->next = nullptr;
        objectMap[objectID] = o;
      }
      else
      {
        o = k->second;
      }
    }
    if (o->baseObject && o->baseObject->baseObject)
      copyBaseObject(*o);
    objects[i] = o;
  }

  // load components
  for (size_t i = 0; i < objects.size(); i++)
  {
    if (!objects[i])
      continue;
    const JSONReader::JSONObject  *jsonObject =
        static_cast< const JSONReader::JSONObject * >(jsonObjects->children[i]);
    const JSONReader::JSONArray   *jsonComponents =
        static_cast< const JSONReader::JSONArray * >(
            jsonObject->children.find("Components")->second);
    MaterialObject  *o = objects[i];

    for (size_t l = 0; l < jsonComponents->children.size(); l++)
    {
      if (!(jsonComponents->children[l] &&
            jsonComponents->children[l]->type
            == JSONReader::JSONItemType_Object))
      {
        continue;
      }
      const JSONReader::JSONObject  *jsonComponent =
          static_cast< const JSONReader::JSONObject * >(
              jsonComponents->children[l]);
      j = jsonComponent->children.find("Index");
      if (j == jsonComponent->children.end() ||
          !j->second || j->second->type != JSONReader::JSONItemType_Number)
      {
        continue;
      }
      double  itemIndex_f =
          static_cast< const JSONReader::JSONNumber * >(j->second)->value;
      int     itemIndex = roundDouble(itemIndex_f);
      if ((itemIndex & ~0xFFFF) || double(itemIndex) != itemIndex_f)
        continue;
      j = jsonComponent->children.find("Type");
      if (j == jsonComponent->children.end() ||
          !j->second || j->second->type != JSONReader::JSONItemType_String)
      {
        continue;
      }
      const std::string&  itemTypeStr =
          static_cast< const JSONReader::JSONString * >(j->second)->value;
      j = jsonComponent->children.find("Data");
      if (j == jsonComponent->children.end() ||
          !j->second || j->second->type != JSONReader::JSONItemType_Object)
      {
        continue;
      }
      // find component type
      size_t  n0 = BSReflStream::String_Unknown + 1;
      size_t  n2 = sizeof(BSReflStream::stringTable) / sizeof(char *);
      std::uint32_t itemType = 0U;
      while (n2 > n0)
      {
        size_t  n1 = n0 + ((n2 - n0) >> 1);
        int     tmp = itemTypeStr.compare(BSReflStream::stringTable[n1]);
        if (!tmp || n1 == n0) [[unlikely]]
        {
          if (!tmp)
            itemType = std::uint32_t(n1);
          break;
        }
        if (tmp < 0)
          n2 = n1;
        else
          n0 = n1;
      }
      if (!(itemType && getClassDef(itemType)))
        continue;
      std::uint32_t key = (itemType << 16) | std::uint32_t(itemIndex);
      loadJSONItem(findComponent(*o, key, itemType).o,
                   jsonComponent, itemType, o, objectMap);
    }
  }

  // add materials to database
  for (std::map< BSResourceID, MaterialObject * >::iterator
           i = objectMap.begin(); i != objectMap.end(); i++)
  {
    if (i->second && !i->second->parent && i->first.ext == 0x0074616DU)
      matFileObjectMap[i->first] = i->second;           // "mat\0"
  }
}

void BSMaterialsCDB::loadJSONFile(
    const unsigned char *fileData, size_t fileSize, const char *materialPath)
{
  JSONReader  matFile(fileData, fileSize);
  loadJSONFile(matFile, materialPath);
}

void BSMaterialsCDB::loadJSONFile(
    const char *fileName, const char *materialPath)
{
  if (!(materialPath && *materialPath))
    materialPath = fileName;
  JSONReader  matFile(fileName);
  loadJSONFile(matFile, materialPath);
}

