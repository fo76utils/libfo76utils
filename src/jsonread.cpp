
#include "common.hpp"
#include "jsonread.hpp"

std::string& JSONReader::readToken()
{
  curToken.clear();
  bool    skippingWhitespace = true;
  bool    readingQuotedString = false;
  while (filePos < fileBufSize) [[likely]]
  {
    char    c = char(fileBuf[filePos]);
    filePos++;
    if ((unsigned char) c <= 0x20)
    {
      if (!c)
      {
        throw FO76UtilsError("invalid character in JSON file at line %lu",
                             (unsigned long) curLine);
      }
      if (readingQuotedString)
      {
        if (c == '\n')
        {
          throw FO76UtilsError("unexpected end of line %lu in JSON file",
                               (unsigned long) curLine);
        }
        else if (!(c == '\t' || c == ' '))
        {
          throw FO76UtilsError("invalid character in JSON file at line %lu",
                               (unsigned long) curLine);
        }
        curToken += c;
        continue;
      }
      if (!skippingWhitespace)
      {
        if (c == '\n')
          filePos--;
        return curToken;
      }
      if (c == '\n')
        curLine++;
      continue;
    }
    if (skippingWhitespace)
    {
      skippingWhitespace = false;
      readingQuotedString = (c == '"');
    }
    else if (c == '"')
    {
      if (!readingQuotedString)
      {
        throw FO76UtilsError("syntax error in JSON file at line %lu",
                             (unsigned long) curLine);
      }
      curToken += c;
      return curToken;
    }
    if (!readingQuotedString)
    {
      if (c == ',' || c == ':' || c == '[' || c == ']' || c == '{' || c == '}')
      {
        if (curToken.empty())
          curToken = c;
        else
          filePos--;
        return curToken;
      }
    }
    else if (c == '\\')
    {
      c = '\0';
      if (filePos < fileBufSize) [[likely]]
      {
        c = char(fileBuf[filePos]);
        filePos++;
      }
      switch (c)
      {
        case 'b':
          c = '\b';
          break;
        case 'f':
          c = '\f';
          break;
        case 'n':
          c = '\n';
          break;
        case 'r':
          c = '\r';
          break;
        case 't':
          c = '\t';
          break;
        case 'u':
          {
            unsigned int  tmp = 0U;
            for (int i = 0; i < 4; i++)
            {
              if (filePos >= fileBufSize)
              {
                throw FO76UtilsError("unexpected end of JSON file at line %lu",
                                     (unsigned long) curLine);
              }
              c = char(fileBuf[filePos]);
              filePos++;
              if (c >= '0' && c <= '9')
              {
                tmp = (tmp << 4) | (unsigned int) (c & 0x0F);
              }
              else if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
              {
                tmp = (tmp << 4) | (unsigned int) ((c & 0x0F) + 9);
              }
              else
              {
                throw FO76UtilsError("syntax error in JSON file at line %lu",
                                     (unsigned long) curLine);
              }
            }
            if (!tmp)
            {
              throw FO76UtilsError("invalid character in JSON file at line %lu",
                                   (unsigned long) curLine);
            }
            if (tmp >= 0x0080U)
            {
              // encode UTF-8 character
              if (tmp >= 0x0800U)
              {
                curToken += char(((tmp >> 12) & 0x000FU) | 0x00E0U);
                curToken += char(((tmp >> 6) & 0x003FU) | 0x0080U);
              }
              else
              {
                curToken += char(((tmp >> 6) & 0x001FU) | 0x00C0U);
              }
              tmp = (tmp & 0x003FU) | 0x0080U;
            }
            c = char(tmp);
          }
          break;
        case '"':
        case '/':
        case '\\':
          break;
        default:
          throw FO76UtilsError("syntax error in JSON file at line %lu",
                               (unsigned long) curLine);
      }
    }
    curToken += c;
  }
  return curToken;
}

bool JSONReader::parseJSONValue(JSONItem*& o, const std::string& s)
{
  if (s.empty())
    return false;
  if (s.starts_with("\"") && s.ends_with("\""))
  {
    // string
    JSONString  *p = new JSONString;
    o = p;
    p->type = JSONItemType_String;
    if (s.length() > 2)
      p->value = std::string(s, 1, s.length() - 2);
  }
  else if (s == "{")
  {
    // structure
    JSONObject  *p = new JSONObject;
    o = p;
    p->type = JSONItemType_Object;
    const JSONItem  **curItem = nullptr;
    bool    braceExpected = true;
    bool    commaExpected = false;
    while (true)
    {
      std::string&  t = readToken();
      if (t.empty())
      {
        throw FO76UtilsError("unexpected end of JSON file at line %lu",
                             (unsigned long) curLine);
      }
      if (!curItem)
      {
        if (t == "}" && braceExpected)
          break;
        if (t == "," && commaExpected)
        {
          braceExpected = false;
          commaExpected = false;
          continue;
        }
        if (!((t.starts_with("\"") && t.ends_with("\"") && t.length() >= 2) ||
              t == "null"))
        {
          throw FO76UtilsError("syntax error in JSON file at line %lu",
                               (unsigned long) curLine);
        }
        if (t[0] == '"' && t.length() > 2)
          curItem = &(p->children[std::string(t, 1, t.length() - 2)]);
        else
          curItem = &(p->children[std::string()]);
      }
      else
      {
        if (t == ":")
        {
          if (parseJSONValue(*(const_cast< JSONItem ** >(curItem)),
                             readToken()))
          {
            curItem = nullptr;
            braceExpected = true;
            commaExpected = true;
            continue;
          }
        }
        throw FO76UtilsError("syntax error in JSON file at line %lu",
                             (unsigned long) curLine);
      }
    }
  }
  else if (s == "[")
  {
    // array
    JSONArray *p = new JSONArray;
    o = p;
    p->type = JSONItemType_Array;
    bool    bracketExpected = true;
    bool    commaExpected = false;
    while (true)
    {
      std::string&  t = readToken();
      if (t.empty())
      {
        throw FO76UtilsError("unexpected end of JSON file at line %lu",
                             (unsigned long) curLine);
      }
      if (t == "]" && bracketExpected)
        break;
      if ((t == ",") != commaExpected)
      {
        throw FO76UtilsError("syntax error in JSON file at line %lu",
                             (unsigned long) curLine);
      }
      bracketExpected = !commaExpected;
      commaExpected = !commaExpected;
      if (!commaExpected)
        continue;
      p->children.push_back(nullptr);
      (void) parseJSONValue(*(const_cast< JSONItem ** >(&(p->children.back()))),
                            t);
    }
  }
  else if ((s[0] >= '0' && s[0] <= '9') ||
           s[0] == '+' || s[0] == '-' || s[0] == '.')
  {
    // number
    char    *endp = nullptr;
    double  tmp = std::strtod(s.c_str(), &endp);
    if (endp != (s.c_str() + s.length()))
    {
      throw FO76UtilsError("invalid number syntax in JSON file at line %lu",
                           (unsigned long) curLine);
    }
    JSONNumber  *p = new JSONNumber;
    o = p;
    p->type = JSONItemType_Number;
    p->value = tmp;
  }
  else if (s == "false" || s == "true")
  {
    // boolean
    JSONBoolean *p = new JSONBoolean;
    o = p;
    p->type = JSONItemType_Boolean;
    p->value = (s[0] == 't');
  }
  else if (s == "null")
  {
    // null
    o = new JSONItem;
    o->type = JSONItemType_Null;
  }
  else
  {
    throw FO76UtilsError("syntax error in JSON file at line %lu",
                         (unsigned long) curLine);
  }
  return true;
}

void JSONReader::parseJSONData()
{
  filePos = 0;
  if (fileBufSize >= 3 &&
      readUInt16Fast(fileBuf) == 0xBBEFU && fileBuf[2] == 0xBF)
  {
    filePos = 3;
  }
  if (rootObject)
  {
    deleteJSONObjects(rootObject);
    rootObject = nullptr;
  }
  curLine = 1;
  (void) parseJSONValue(rootObject, readToken());
}

void JSONReader::deleteJSONObjects(JSONItem *p)
{
  switch (p->type)
  {
    case JSONItemType_Boolean:
      delete static_cast< JSONBoolean * >(p);
      break;
    case JSONItemType_Number:
      delete static_cast< JSONNumber * >(p);
      break;
    case JSONItemType_String:
      delete static_cast< JSONString * >(p);
      break;
    case JSONItemType_Object:
      {
        JSONObject& o = *(static_cast< JSONObject * >(p));
        for (auto& i : o.children)
        {
          if (i.second)
            deleteJSONObjects(const_cast< JSONItem * >(i.second));
        }
        delete &o;
      }
      break;
    case JSONItemType_Array:
      {
        JSONArray&  o = *(static_cast< JSONArray * >(p));
        for (auto& i : o.children)
        {
          if (i)
            deleteJSONObjects(const_cast< JSONItem * >(i));
        }
        delete &o;
      }
      break;
    default:
      delete p;
      break;
  }
}

JSONReader::JSONReader(const char *fileName)
  : FileBuffer(fileName),
    rootObject(nullptr)
{
  try
  {
    parseJSONData();
  }
  catch (...)
  {
    if (rootObject)
      deleteJSONObjects(rootObject);
    throw;
  }
}

JSONReader::JSONReader(const unsigned char *fileData, size_t fileSize)
  : FileBuffer(fileData, fileSize),
    rootObject(nullptr)
{
  try
  {
    parseJSONData();
  }
  catch (...)
  {
    if (rootObject)
      deleteJSONObjects(rootObject);
    throw;
  }
}

JSONReader::~JSONReader()
{
  if (rootObject)
    deleteJSONObjects(rootObject);
}

