
#ifndef BSMATCDB_HPP_INCLUDED
#define BSMATCDB_HPP_INCLUDED

#include "common.hpp"
#include "filebuf.hpp"
#include "bsrefl.hpp"
#include "jsonread.hpp"
#include "ba2file.hpp"

class BSMaterialsCDB
{
  friend class CE2MaterialDB;
 public:
  struct BSResourceID
  {
    std::uint32_t file; // CRC32 of base name (not including the extension)
    std::uint32_t ext;  // extension (0x0074616D for "mat\0")
    std::uint32_t dir;  // CRC32 of directory name (e.g. "materials\\test")
    inline BSResourceID()
    {
    }
    inline BSResourceID(std::uint32_t d, std::uint32_t f, std::uint32_t e)
      : file(f), ext(e), dir(d)
    {
    }
    BSResourceID(const std::string_view& fileName);
    // convert .mat file path or "res:dir:file:ext" format resource ID
    void fromJSONString(std::string_view s);
    inline bool operator<(const BSResourceID& r) const
    {
      return (file < r.file || (file == r.file && ext < r.ext) ||
              (file == r.file && ext == r.ext && dir < r.dir));
    }
    inline bool operator==(const BSResourceID& r) const
    {
      return (file == r.file && ext == r.ext && dir == r.dir);
    }
    inline operator bool() const
    {
      return bool(file | ext | dir);
    }
    inline std::uint32_t hashFunction() const
    {
      std::uint32_t h = 0xFFFFFFFFU;
#if ENABLE_X86_64_SIMD
      hashFunctionCRC32C< std::uint64_t >(h, (std::uint64_t(ext) << 32) | file);
#else
      hashFunctionUInt32(h, file);
      hashFunctionUInt32(h, ext);
#endif
      hashFunctionUInt32(h, dir);
      return h;
    }
  };
  struct CDBClassDef
  {
    struct Field
    {
      std::uint32_t name;
      std::uint32_t type;
    };
    std::uint32_t className;
    bool    isUser;
    std::uint16_t fieldCnt;
    const Field   *fields;
  };
  struct MaterialObject;
  struct CDBObject
  {
    std::uint16_t type;                 // BSReflStream::stringTable[] index
    // Size of children[] for compound types (struct, ref, list and map).
    // For maps, childCnt = 2 * elements, and data.children[N * 2] and
    // data.children[N * 2 + 1] contain the key and value for element N.
    std::uint16_t childCnt;
    std::uint32_t refCnt;
    // for type == BSReflStream::String_Bool
    inline bool boolValue() const;
    // for type == String_Int8, String_Int16 and String_Int32
    inline std::int32_t intValue() const;
    // for type == String_UInt8, String_UInt16 and String_UInt32
    inline std::uint32_t uintValue() const;
    // for type == String_Int64
    inline std::int64_t int64Value() const;
    // for type == String_UInt64
    inline std::uint64_t uint64Value() const;
    // for type == String_Float
    inline float floatValue() const;
    // for type == String_Double
    inline double doubleValue() const;
    // for type == String_String
    inline const char *stringValue() const;
    // for type == String_List, String_Map, String_Ref and structures
    inline CDBObject **children();
    inline const CDBObject * const *children() const;
    // for type == String_BSComponentDB2_ID
    inline const MaterialObject *linkedObject() const;
  };
  struct CDBObject_Bool : public CDBObject
  {
    bool    value;
  };
  struct CDBObject_Int : public CDBObject
  {
    std::int32_t  value;
  };
  struct CDBObject_UInt : public CDBObject
  {
    std::uint32_t value;
  };
  struct CDBObject_Int64 : public CDBObject
  {
    std::int64_t  value;
  };
  struct CDBObject_UInt64 : public CDBObject
  {
    std::uint64_t value;
  };
  struct CDBObject_Float : public CDBObject
  {
    float   value;
  };
  struct CDBObject_Double : public CDBObject
  {
    double  value;
  };
  struct CDBObject_String : public CDBObject
  {
    const char  *value;
  };
  struct CDBObject_Compound : public CDBObject
  {
    CDBObject *children[1];
    inline void insertListItem(CDBObject *value, size_t listCapacity)
    {
      if (childCnt < listCapacity && value) [[likely]]
      {
        children[childCnt] = value;
        childCnt++;
      }
    }
    void insertMapItem(CDBObject *key, CDBObject *value, size_t mapCapacity);
  };
  struct CDBObject_Link : public CDBObject
  {
    const MaterialObject  *objectPtr;
  };
  struct MaterialComponent
  {
    std::uint32_t key;          // (type << 16) | index
    std::uint32_t className;
    CDBObject *o;
    MaterialComponent *next;
    inline bool operator<(const MaterialComponent& r) const
    {
      return (key < r.key);
    }
  };
  struct MaterialObject
  {
    BSResourceID  persistentID;
    std::uint32_t dbID;
    const MaterialObject  *baseObject;
    MaterialComponent     *components;
    const MaterialObject  *parent;
    const MaterialObject  *children;
    const MaterialObject  *next;
    inline bool isJSON() const
    {
      // returns true if this object was loaded from a JSON format .mat file
      return (dbID >= 0x01000000U);
    }
    inline const MaterialObject *getNextChildObject() const
    {
      const MaterialObject  *i = children;
      if (i)
        return i;
      for (i = this; !i->next && i->parent; i = i->parent)
        ;
      return i->next;
    }
  };
 protected:
  enum
  {
    classHashMask = 0x000001FF
  };
  struct StoredStringHashMap
  {
    const char  **buf;
    size_t  hashMask;
    size_t  size;
    StoredStringHashMap();
    ~StoredStringHashMap();
    void clear();
    const char *storeString(BSMaterialsCDB& cdb, const char *s, size_t len);
    void expandBuffer(size_t minMask = 0x0FFF);
  };
  struct MatObjectHashMap
  {
    MaterialObject  **buf;
    size_t  hashMask;
    size_t  size;
    MatObjectHashMap();
    ~MatObjectHashMap();
    void clear();
    void storeObject(MaterialObject *o);
    const MaterialObject *findObject(BSResourceID objectID) const;
    void expandBuffer();
  };
  struct JSONMaterialHashMap
  {
    BSResourceID  *buf;
    size_t  hashMask;
    size_t  size;
    JSONMaterialHashMap();
    ~JSONMaterialHashMap();
    void clear();
    bool insert(BSResourceID objectID);
    void expandBuffer();
  };
  static const std::uint8_t cdbObjectSizeAlignTable[38];
  CDBClassDef     *classes;             // classHashMask + 1 elements
  MaterialObject  **objectTablePtr;
  size_t          objectTableSize;
  StoredStringHashMap storedStrings;
  MatObjectHashMap    matFileObjectMap;
  AllocBuffers    objectBuffers[3];     // for align bytes <= 2, 4 and >= 8
  BSMaterialsCDB  *parentDB = nullptr;  // valid if copyFrom() was called
  const BA2File   *ba2File;
  JSONMaterialHashMap jsonMaterialsLoaded;
  MaterialComponent& findComponent(MaterialObject& o,
                                   std::uint32_t key, std::uint32_t className);
  inline MaterialObject *findObject(std::uint32_t dbID);
  inline const MaterialObject *findObject(std::uint32_t dbID) const;
  inline void *allocateSpace(size_t nBytes, size_t alignBytes = 16)
  {
    size_t  n = size_t(std::bit_width(std::uintptr_t(alignBytes)));
    n = std::min< size_t >(std::max< size_t >(n, 2), 4) - 2;
    return objectBuffers[n].allocateSpace(nBytes, alignBytes);
  }
  template< typename T >
  inline T *allocateObjects(size_t n)
  {
    return reinterpret_cast< T * >(allocateSpace(sizeof(T) * n, alignof(T)));
  }
  template< typename T >
  inline T *constructObject()
  {
    return new(allocateObjects< T >(1)) T();
  }
  CDBObject *allocateObject(std::uint32_t itemType, const CDBClassDef *classDef,
                            size_t elementCnt = 0);
  void copyObject(CDBObject& o);
  void copyBaseObject(MaterialObject& o);
  // decrement the reference count of all child objects
  void deleteObject(CDBObject& o);
  void deleteObject(MaterialObject& o);
  inline const char *storeString(const char *s, size_t len)
  {
    return storedStrings.storeString(*this, s, len);
  }
  void loadItem(CDBObject*& o,
                BSReflStream& cdbFile, BSReflStream::Chunk& chunkBuf,
                bool isDiff, std::uint32_t itemType);
  void readAllChunks(BSReflStream& cdbFile);
  CDBClassDef& allocateClassDef(std::uint32_t className);
  inline void storeMatFileObject(MaterialObject *o)
  {
    matFileObjectMap.storeObject(o);
  }
  inline const MaterialObject *findMatFileObject(
      const BSResourceID& objectID) const
  {
    return matFileObjectMap.findObject(objectID);
  }
  void dumpObject(std::string& s, const CDBObject *o, int indentCnt) const;
  void dumpObject(std::string& jsonBuf, const MaterialObject *o,
                  BSResourceID matObjectID) const;
  static std::uint32_t findJSONItemType(const std::string& s);
  void loadJSONItem(CDBObject*& o, const JSONReader::JSONItem *jsonItem,
                    std::uint32_t itemType, MaterialObject *materialObject,
                    std::map< BSResourceID, MaterialObject * >& objectMap);
 public:
  void loadCDBFile(const unsigned char *fileData, size_t fileSize)
  {
    BSReflStream  cdbFile(fileData, fileSize);
    readAllChunks(cdbFile);
  }
  void loadCDBFile(const char *fileName)
  {
    BSReflStream  cdbFile(fileName);
    readAllChunks(cdbFile);
  }
  BSMaterialsCDB()
  {
    clear();
  }
  BSMaterialsCDB(const unsigned char *fileData, size_t fileSize)
  {
    clear();
    loadCDBFile(fileData, fileSize);
  }
  BSMaterialsCDB(const char *fileName)
  {
    clear();
    loadCDBFile(fileName);
  }
  ~BSMaterialsCDB()
  {
    if (parentDB)
      clear();
  }
  // clone an existing material database
  // clear() must be called to remove references to 'r' before &r is deleted
  void copyFrom(BSMaterialsCDB& r);
  void loadJSONFile(const JSONReader& matFile,
                    const std::string_view& materialPath);
  void loadJSONFile(const unsigned char *fileData, size_t fileSize,
                    const std::string_view& materialPath);
  void loadJSONFile(const char *fileName, const std::string_view& materialPath);
  // Load .mat file from archives, the return value is true on success.
  // flags & 1: ignore missing file
  // flags & 2: ignore errors
  // flags & 4: skip file if loading it was previously attempted
  bool loadJSONFile(const std::string_view& materialPath, BSResourceID objectID,
                    int flags = 0);
  void clear();
  const CDBClassDef *getClassDef(std::uint32_t type) const;
  const MaterialObject *getMaterial(BSResourceID objectID) const
  {
    return findMatFileObject(objectID);
  }
  const MaterialObject *getMaterial(const std::string_view& materialPath) const
  {
    return findMatFileObject(BSResourceID(materialPath));
  }
  void getMaterials(std::vector< const MaterialObject * >& materials) const;
  void getJSONMaterial(std::string& jsonBuf,
                       const std::string_view& materialPath) const;
  inline void setArchives(const BA2File *archive)
  {
    ba2File = archive;
  }
};

inline bool BSMaterialsCDB::CDBObject::boolValue() const
{
  return static_cast< const CDBObject_Bool * >(this)->value;
}

inline std::int32_t BSMaterialsCDB::CDBObject::intValue() const
{
  return static_cast< const CDBObject_Int * >(this)->value;
}

inline std::uint32_t BSMaterialsCDB::CDBObject::uintValue() const
{
  return static_cast< const CDBObject_UInt * >(this)->value;
}

inline std::int64_t BSMaterialsCDB::CDBObject::int64Value() const
{
  return static_cast< const CDBObject_Int64 * >(this)->value;
}

inline std::uint64_t BSMaterialsCDB::CDBObject::uint64Value() const
{
  return static_cast< const CDBObject_UInt64 * >(this)->value;
}

inline float BSMaterialsCDB::CDBObject::floatValue() const
{
  return static_cast< const CDBObject_Float * >(this)->value;
}

inline double BSMaterialsCDB::CDBObject::doubleValue() const
{
  return static_cast< const CDBObject_Double * >(this)->value;
}

inline const char * BSMaterialsCDB::CDBObject::stringValue() const
{
  return static_cast< const CDBObject_String * >(this)->value;
}

inline BSMaterialsCDB::CDBObject ** BSMaterialsCDB::CDBObject::children()
{
  return static_cast< CDBObject_Compound * >(this)->children;
}

inline const BSMaterialsCDB::CDBObject * const *
    BSMaterialsCDB::CDBObject::children() const
{
  return static_cast< const CDBObject_Compound * >(this)->children;
}

inline const BSMaterialsCDB::MaterialObject *
    BSMaterialsCDB::CDBObject::linkedObject() const
{
  return static_cast< const CDBObject_Link * >(this)->objectPtr;
}

#endif

