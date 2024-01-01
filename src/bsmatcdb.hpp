
#ifndef BSMATCDB_HPP_INCLUDED
#define BSMATCDB_HPP_INCLUDED

#include "common.hpp"
#include "filebuf.hpp"
#include "bsrefl.hpp"

class BSMaterialsCDB : public BSReflStream
{
 public:
  struct BSResourceID
  {
    std::uint32_t dir;
    std::uint32_t file;
    std::uint32_t ext;
    inline BSResourceID()
    {
    }
    BSResourceID(const std::string& fileName);
    inline bool operator<(const BSResourceID& r) const
    {
      return (dir < r.dir || (dir == r.dir && file < r.file) ||
              (dir == r.dir && file == r.file && ext < r.ext));
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
    // Size of data.children for structure, reference, list and map types.
    // For maps, childCnt = 2 * elements, and data.children[N * 2] and
    // data.children[N * 2 + 1] contain the key and value for element N.
    std::uint16_t childCnt;
    std::uint32_t refCnt;
    union
    {
      CDBObject     *children[1];
      bool          boolValue;          // valid if type == String_Bool,
      std::int8_t   int8Value;          //   String_Int8,
      std::uint8_t  uint8Value;         //   String_UInt8,
      std::int16_t  int16Value;         //   String_Int16,
      std::uint16_t uint16Value;        //   String_UInt16,
      std::int32_t  int32Value;         //   String_Int32,
      std::uint32_t uint32Value;        //   String_UInt32,
      std::int64_t  int64Value;         //   String_Int64,
      std::uint64_t uint64Value;        //   String_UInt64,
      float         floatValue;         //   String_Float,
      double        doubleValue;        //   String_Double,
      const char    *stringValue;       //   String_String,
      const MaterialObject  *objectPtr; //   or String_BSComponentDB2_ID
    }
    data;
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
  std::vector< CDBClassDef >  classes;
  MaterialObject  **objectTablePtr;
  size_t  objectTableSize;
  std::map< BSResourceID, const MaterialObject * >  matFileObjectMap;
  std::vector< std::vector< unsigned char > > objectBuffers;
  MaterialComponent& findComponent(MaterialObject& o,
                                   std::uint32_t key, std::uint32_t className);
  inline MaterialObject *findObject(std::uint32_t dbID);
  inline const MaterialObject *findObject(std::uint32_t dbID) const;
  void *allocateSpace(size_t nBytes, size_t alignBytes = 16);
  CDBObject *allocateObject(std::uint32_t itemType, const CDBClassDef *classDef,
                            size_t elementCnt = 0);
  void copyObject(CDBObject*& o);
  void copyBaseObject(MaterialObject& o);
  void loadItem(CDBObject*& o, Chunk& chunkBuf, bool isDiff,
                std::uint32_t itemType);
  void readAllChunks();
  void dumpObject(std::string& s, const CDBObject *o, int indentCnt) const;
 public:
  BSMaterialsCDB(const unsigned char *fileData, size_t fileSize)
    : BSReflStream(fileData, fileSize)
  {
    readAllChunks();
  }
  BSMaterialsCDB(const char *fileName)
    : BSReflStream(fileName)
  {
    readAllChunks();
  }
  const CDBClassDef *getClassDef(std::uint32_t type) const;
  const MaterialObject *getMaterial(const std::string& materialPath) const;
  void getJSONMaterial(std::string& jsonBuf,
                       const std::string& materialPath) const;
};

#endif

