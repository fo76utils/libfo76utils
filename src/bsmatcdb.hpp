
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
    std::uint16_t refCnt;
    std::uint32_t childCnt;
    union
    {
      CDBObject     *children[1];
      bool          boolValue;
      std::int8_t   int8Value;
      std::uint8_t  uint8Value;
      std::int16_t  int16Value;
      std::uint16_t uint16Value;
      std::int32_t  int32Value;
      std::uint32_t uint32Value;
      std::int64_t  int64Value;
      std::uint64_t uint64Value;
      float         floatValue;
      double        doubleValue;
      const char    *stringValue;
      // for type == String_BSComponentDB2_ID
      const MaterialObject  *objectPtr;
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

