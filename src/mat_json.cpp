
#include "common.hpp"
#include "bsmatcdb.hpp"

std::uint32_t BSMaterialsCDB::findJSONItemType(const std::string& s)
{
  size_t  n0 = BSReflStream::String_Unknown + 1;
  size_t  n2 = sizeof(BSReflStream::stringTable) / sizeof(char *);
  std::uint32_t itemType = 0U;
  while (true)
  {
    size_t  n1 = n0 + ((n2 - n0) >> 1);
    int     tmp = s.compare(BSReflStream::stringTable[n1]);
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
  if (!itemType)
  {
    static const char *typeNameTable[13] =
    {
      "<ref>", "BSFixedString", "bool", "double", "float", "int16_t", "int32_t",
      "int64_t", "int8_t", "uint16_t", "uint32_t", "uint64_t", "uint8_t"
    };
    static const std::uint8_t typeIndexTable[13] =
    {
      BSReflStream::String_Ref, BSReflStream::String_String,
      BSReflStream::String_Bool, BSReflStream::String_Double,
      BSReflStream::String_Float, BSReflStream::String_Int16,
      BSReflStream::String_Int32, BSReflStream::String_Int64,
      BSReflStream::String_Int8, BSReflStream::String_UInt16,
      BSReflStream::String_UInt32, BSReflStream::String_UInt64,
      BSReflStream::String_UInt8
    };
    n0 = 0;
    n2 = 13;
    while (true)
    {
      size_t  n1 = n0 + ((n2 - n0) >> 1);
      int     tmp = s.compare(typeNameTable[n1]);
      if (!tmp || n1 == n0) [[unlikely]]
      {
        if (!tmp)
          itemType = typeIndexTable[n1];
        break;
      }
      if (tmp < 0)
        n2 = n1;
      else
        n0 = n1;
    }
  }
  return itemType;
}

static inline void addParentLink(
    BSMaterialsCDB::MaterialObject *o, const BSMaterialsCDB::MaterialObject *p)
{
  for (const BSMaterialsCDB::MaterialObject *q = p; q; q = q->parent)
  {
    // avoid circular links
    if (q == o)
      return;
  }
  o->parent = p;
}

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
  if (itemType > BSReflStream::String_Unknown ||
      (itemType >= BSReflStream::String_List &&
       itemType <= BSReflStream::String_Ref))
  {
    // compound types (structure, list, map, reference)
    if (itemType == BSReflStream::String_BSComponentDB2_ID) [[unlikely]]
    {
      if (jsonItem->type != JSONReader::JSONItemType_String)
        return;
      const std::string&  itemValue =
          static_cast< const JSONReader::JSONString * >(jsonItem)->value;
      if (!itemValue.empty())
      {
        BSResourceID  objectID;
        if (itemValue == "<this>")
        {
          for (const MaterialObject *p = materialObject; true; p = p->parent)
          {
            if (!p->parent)
            {
              objectID = p->persistentID;
              break;
            }
          }
        }
        else
        {
          objectID.fromJSONString(itemValue);
        }
        const MaterialObject  *p = findMatFileObject(objectID);
        std::map< BSResourceID, MaterialObject * >::iterator  i =
            objectMap.find(objectID);
        if (i != objectMap.end())
        {
          static_cast< CDBObject_Link * >(o)->objectPtr = i->second;
          if (!i->second->parent)
            addParentLink(i->second, materialObject);
          if (!p)
            return;
        }
        static_cast< CDBObject_Link * >(o)->objectPtr = p;
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
    j = jsonObject->children.find("Data");
    if (j == jsonObject->children.end() || !j->second)
      return;
    if (itemType < BSReflStream::String_Unknown) [[unlikely]]
    {
      if (itemType == BSReflStream::String_Ref)
      {
        // reference
        if (j->second->type != JSONReader::JSONItemType_Object ||
            itemTypeStr != "<ref>")
        {
          return;
        }
        const JSONReader::JSONObject  *refObject =
            static_cast< const JSONReader::JSONObject * >(j->second);
        j = refObject->children.find("Type");
        if (j == refObject->children.end() ||
            !j->second || j->second->type != JSONReader::JSONItemType_String)
        {
          return;
        }
        std::uint32_t refItemType =
            findJSONItemType(static_cast< const JSONReader::JSONString * >(
                                 j->second)->value);
        if (refItemType > BSReflStream::String_Unknown &&
            getClassDef(refItemType))
        {
          loadJSONItem(o->children()[0], refObject, refItemType,
                       materialObject, objectMap);
        }
        return;
      }
      // list or map
      if (j->second->type != JSONReader::JSONItemType_Array ||
          itemTypeStr != "<collection>")
      {
        return;
      }
      const JSONReader::JSONArray *collectionData =
          static_cast< const JSONReader::JSONArray * >(j->second);
      j = jsonObject->children.find("ElementType");
      if (j == jsonObject->children.end() ||
          !j->second || j->second->type != JSONReader::JSONItemType_String)
      {
        return;
      }
      size_t  elementCnt = collectionData->children.size();
      const std::string&  elementTypeStr =
          static_cast< const JSONReader::JSONString * >(j->second)->value;
      std::uint32_t elementType = 0U;
      std::uint8_t  isMap = std::uint8_t(itemType == BSReflStream::String_Map);
      if (!isMap)
      {
        elementType = findJSONItemType(elementTypeStr);
        if (!elementType || (elementType > BSReflStream::String_Unknown &&
                             !getClassDef(elementType)))
        {
          return;
        }
      }
      else if (elementTypeStr != "StdMapType::Pair")
      {
        return;
      }
      std::uint64_t listSize = std::uint64_t(elementCnt) << isMap;
      std::uint32_t n = 0U;
      bool    appendingItems = false;
      if (o && o->type == itemType && o->childCnt)
      {
        appendingItems =
            (isMap ||
             (o->children()[0]->type == elementType &&
              std::uint32_t(elementType - BSReflStream::String_Int8) > 11U));
        if (appendingItems)
        {
          if (!listSize) [[unlikely]]
            return;
          listSize = listSize + o->childCnt;
        }
      }
      if (appendingItems ||
          !(o && o->type == itemType && o->childCnt >= listSize))
      {
        listSize = std::min< std::uint64_t >(listSize, 0xFFFFU - isMap);
        CDBObject *p = allocateObject(itemType, classDef, size_t(listSize));
        if (appendingItems)
        {
          std::uint32_t prvSize = o->childCnt;
          for ( ; n < prvSize && n < listSize; n++)
            p->children()[n] = o->children()[n];
        }
        o = p;
      }
      o->childCnt = std::uint16_t(n);
      CDBObject *key = nullptr;
      for (size_t m = 0; n < listSize; m++, n++)
      {
        CDBObject_Compound  *p = static_cast< CDBObject_Compound * >(o);
        CDBObject *value = nullptr;
        const JSONReader::JSONItem  *q = collectionData->children[m >> isMap];
        if (isMap)
        {
          const JSONReader::JSONObject  *tmp = nullptr;
          if (q->type == JSONReader::JSONItemType_Object)
            tmp = static_cast< const JSONReader::JSONObject * >(q);
          q = nullptr;
          if (tmp)
          {
            j = tmp->children.find("Data");
            if (j != tmp->children.end() &&
                j->second && j->second->type == JSONReader::JSONItemType_Object)
            {
              tmp = static_cast< const JSONReader::JSONObject * >(j->second);
              j = tmp->children.find(!(m & 1) ? "Key" : "Value");
              if (j != tmp->children.end() && j->second)
              {
                q = j->second;
                if (q->type == JSONReader::JSONItemType_Object)
                  elementType = BSReflStream::String_Ref;
                else if (q->type == JSONReader::JSONItemType_String)
                  elementType = BSReflStream::String_String;
                else if (q->type == JSONReader::JSONItemType_Number)
                  elementType = BSReflStream::String_Float;
                else if (q->type == JSONReader::JSONItemType_Boolean)
                  elementType = BSReflStream::String_Bool;
                else
                  q = nullptr;
              }
            }
          }
        }
        if (q) [[likely]]
          loadJSONItem(value, q, elementType, materialObject, objectMap);
        if (!isMap)
          p->insertListItem(value, listSize);
        else if (!(m & 1))
          key = value;
        else
          p->insertMapItem(key, value, listSize);
      }
      return;
    }
    // structure
    if (j->second->type != JSONReader::JSONItemType_Object ||
        itemTypeStr != BSReflStream::stringTable[itemType])
    {
      return;
    }
    const JSONReader::JSONObject  *jsonObjectData =
        static_cast< const JSONReader::JSONObject * >(j->second);
    for (const auto& i : jsonObjectData->children)
    {
      const std::string&  fieldName = i.first;
      int     fieldNum = -1;
      for (size_t l = 0; l < classDef->fieldCnt; l++)
      {
        if (fieldName == BSReflStream::stringTable[classDef->fields[l].name])
        {
          fieldNum = int(l);
          break;
        }
      }
      if (fieldNum < 0 || !i.second)
      {
        if (classDef->isUser && !classDef->fieldCnt && fieldName == "null" &&
            i.second && i.second->type == JSONReader::JSONItemType_String)
        {
          // work around ClassReference being defined with 0 fields
          if (o->childCnt < 1)
            o->childCnt = 1;
          loadJSONItem(o->children()[0], i.second, BSReflStream::String_String,
                       materialObject, objectMap);
        }
        continue;
      }
      std::uint32_t fieldType = classDef->fields[fieldNum].type;
      loadJSONItem(o->children()[fieldNum],
                   i.second, fieldType, materialObject, objectMap);
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
        static_cast< CDBObject_String * >(o)->value =
            storeString(s.c_str(), s.length());
      }
      else
      {
        static_cast< CDBObject_String * >(o)->value = "";
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
                std::uint64_t(std::strtoull(itemValue.c_str(), &endp, 10));
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
    const JSONReader& matFile, const std::string_view& materialPath)
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
  BSResourceID  matObjectID;
  matObjectID.fromJSONString(materialPath);
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
    BSResourceID  objectID = matObjectID;
    j = jsonObject->children.find("ID");
    if (j != jsonObject->children.end() &&
        j->second && j->second->type == JSONReader::JSONItemType_String &&
        static_cast< const JSONReader::JSONString * >(j->second)->value
        != "<this>")
    {
      objectID.fromJSONString(
          static_cast< const JSONReader::JSONString * >(j->second)->value);
    }
    if (!(objectID.file | objectID.ext | objectID.dir))
      continue;
    const MaterialObject  *parentPtr = findMatFileObject(parentID);
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
        o->dbID = std::uint32_t(objectMap.size() + 0x01000000);
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
      std::uint32_t itemType = findJSONItemType(itemTypeStr);
      if (itemType <= BSReflStream::String_Unknown || !getClassDef(itemType))
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
    MaterialObject  *o = const_cast< MaterialObject * >(
                             findMatFileObject(i->second->persistentID));
    if (o)
    {
      if (o->parent)
      {
        // remove old edges from this object ID
        MaterialObject  *q = const_cast< MaterialObject * >(o->parent);
        const MaterialObject  **prvPtr = &(q->children);
        for (const MaterialObject *r = q->children; r; r = r->next)
        {
          if (r->persistentID == o->persistentID)
            *prvPtr = r->next;
          else
            prvPtr = const_cast< const MaterialObject ** >(&(r->next));
        }
      }
      *o = *(i->second);
      i->second = o;
    }
    else
    {
      storeMatFileObject(i->second);
    }
  }

  // load edges
  for (size_t i = 0; i < objects.size(); i++)
  {
    if (!objects[i])
      continue;
    const JSONReader::JSONObject  *jsonObject =
        static_cast< const JSONReader::JSONObject * >(jsonObjects->children[i]);
    j = jsonObject->children.find("Edges");
    if (j == jsonObject->children.end() ||
        !j->second || j->second->type != JSONReader::JSONItemType_Array)
    {
      continue;
    }

    const JSONReader::JSONArray   *jsonEdges =
        static_cast< const JSONReader::JSONArray * >(j->second);
    for (size_t l = 0; l < jsonEdges->children.size(); l++)
    {
      if (!(jsonEdges->children[l] &&
            jsonEdges->children[l]->type == JSONReader::JSONItemType_Object))
      {
        continue;
      }
      const JSONReader::JSONObject  *jsonEdge =
          static_cast< const JSONReader::JSONObject * >(jsonEdges->children[l]);
      j = jsonEdge->children.find("Type");
      if (j == jsonEdge->children.end() ||
          !j->second || j->second->type != JSONReader::JSONItemType_String ||
          static_cast< const JSONReader::JSONString * >(j->second)->value
          != "BSComponentDB2::OuterEdge")
      {
        continue;
      }
      j = jsonEdge->children.find("To");
      if (j == jsonEdge->children.end() ||
          !j->second || j->second->type != JSONReader::JSONItemType_String)
      {
        continue;
      }
      const std::string&  edgeToStr =
          static_cast< const JSONReader::JSONString * >(j->second)->value;
      BSResourceID  edgeID;
      if (edgeToStr == "<this>")
        edgeID = matObjectID;
      else
        edgeID.fromJSONString(edgeToStr);
      if (!(edgeID.file | edgeID.ext | edgeID.dir))
        continue;
      const MaterialObject  *o = findMatFileObject(objects[i]->persistentID);
      const MaterialObject  *q = findMatFileObject(edgeID);
      if (o && q)
      {
        addParentLink(const_cast< MaterialObject * >(o), q);
        break;
      }
    }
  }

  // create edge links
  for (std::map< BSResourceID, MaterialObject * >::iterator
           i = objectMap.begin(); i != objectMap.end(); i++)
  {
    MaterialObject  *o = i->second;
    MaterialObject  *q = const_cast< MaterialObject * >(o->parent);
    if (q)
    {
      o->next = q->children;
      q->children = o;
    }
  }
}

void BSMaterialsCDB::loadJSONFile(
    const unsigned char *fileData, size_t fileSize,
    const std::string_view& materialPath)
{
  JSONReader  matFile(fileData, fileSize);
  loadJSONFile(matFile, materialPath);
}

void BSMaterialsCDB::loadJSONFile(
    const char *fileName, const std::string_view& materialPath)
{
  JSONReader  matFile(fileName);
  loadJSONFile(
      matFile,
      (!materialPath.empty() ? materialPath : std::string_view(fileName)));
}

