
#ifndef JSONREAD_HPP_INCLUDED
#define JSONREAD_HPP_INCLUDED

#include "common.hpp"
#include "filebuf.hpp"

class JSONReader : public FileBuffer
{
 public:
  enum JSONItemType
  {
    JSONItemType_Null = 0,
    JSONItemType_Boolean = 1,
    JSONItemType_Number = 2,
    JSONItemType_String = 3,
    JSONItemType_Object = 4,
    JSONItemType_Array = 5
  };
  struct JSONItem
  {
    // 0: null, 1: boolean, 2: number, 3: string, 4: object, 5: array
    int     type;
  };
  struct JSONBoolean : public JSONItem
  {
    bool    value;
  };
  struct JSONNumber : public JSONItem
  {
    double  value;
  };
  struct JSONString : public JSONItem
  {
    std::string value;
  };
  struct JSONObject : public JSONItem
  {
    std::map< std::string, const JSONItem * > children;
  };
  struct JSONArray : public JSONItem
  {
    std::vector< const JSONItem * > children;
  };
 protected:
  JSONItem  *rootObject;
  size_t    curLine;
  std::string curToken;
  // returns empty string on EOF
  std::string& readToken();
  // returns false if s is empty
  bool parseJSONValue(JSONItem*& o, const std::string& s);
  void parseJSONData();
  void deleteJSONObjects(JSONItem *p);
 public:
  JSONReader(const char *fileName);
  JSONReader(const unsigned char *fileData, size_t fileSize);
  virtual ~JSONReader();
  inline const JSONItem *getData() const
  {
    return rootObject;
  }
};

#endif

