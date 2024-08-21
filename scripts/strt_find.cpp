
#include "common.hpp"
#include "filebuf.hpp"
#include "ba2file.hpp"
#include "esmfile.hpp"
#include "bsrefl.hpp"

#include "ba2file.cpp"
#include "bsrefl.cpp"
#include "common.cpp"
#include "esmfile.cpp"
#include "filebuf.cpp"
#include "zlib.cpp"

static void loadStrings(std::set< std::string >& cdbStrings, FileBuffer& buf)
{
  if (buf.size() < 24)
    return;
  if (buf.readUInt64() != 0x0000000848544542ULL)        // "BETH", 8
    return;
  if (buf.readUInt32() != 4U)
    return;
  (void) buf.readUInt32();
  if (buf.readUInt32() != 0x54525453U)                  // "STRT"
    return;
  std::string s;
  size_t  n = buf.readUInt32();
  while (n--)
  {
    char    c = char(buf.readUInt8());
    if (c)
    {
      s += c;
    }
    else if (!s.empty())
    {
      cdbStrings.insert(s);
      s.clear();
    }
  }
  if (!s.empty())
    cdbStrings.insert(s);
}

static const char *predefinedCDBStrings[19] =
{
  "null",         "String",       "List",         "Map",        // -255 to -252
  "<ref>",        "Unknown_-250", "Unknown_-249", "int8_t",     // -251 to -248
  "uint8_t",      "int16_t",      "uint16_t",     "int32_t",    // -247 to -244
  "uint32_t",     "int64_t",      "uint64_t",     "bool",       // -243 to -240
  "float",        "double",       "<unknown>"                   // -239 to -237
};

int main()
{
  std::set< std::string > cdbStrings;
  for (size_t i = sizeof(predefinedCDBStrings) / sizeof(char *);
       i < (sizeof(BSReflStream::stringTable) / sizeof(char *)); i++)
  {
    cdbStrings.insert(std::string(BSReflStream::stringTable[i]));
  }
  BA2File ba2File("");
  static const char *cdbFileNames[4] =
  {
    "materials/materialsbeta.cdb",
    "materials/creations/sfbgs003/materialsbeta.cdb",
    "materials/creations/sfbgs008/materialsbeta.cdb",
    nullptr
  };
  for (size_t i = 0; cdbFileNames[i]; i++)
  {
    BA2File::UCharArray tmpBuf;
    ba2File.extractFile(tmpBuf, cdbFileNames[i]);
    FileBuffer  buf(tmpBuf.data, tmpBuf.size);
    loadStrings(cdbStrings, buf);
  }
  static const char *esmFileNames[7] =
  {
    "Starfield.esm", "BlueprintShips-Starfield.esm", "SFBGS003.esm",
    "SFBGS006.esm", "SFBGS007.esm", "SFBGS008.esm", nullptr
  };
  for (size_t j = 0; esmFileNames[j]; j++)
  {
    ESMFile esmFile(esmFileNames[j]);
    for (unsigned int i = 0xFD000000U; i != 0x10000000U; )
    {
      const ESMFile::ESMRecord  *r = esmFile.findRecord(i);
      if (r)
      {
        ESMFile::ESMField f(esmFile, *r);
        while (f.next())
          loadStrings(cdbStrings, f);
      }
      if (i == 0xFEFFFFFFU)
        i = 0U;
      else
        i++;
    }
  }
  std::set< std::string >::iterator i = cdbStrings.begin();
  for (unsigned int n = 0U; i != cdbStrings.end(); n++)
  {
    const char  *s;
    if (n < (unsigned int) (sizeof(predefinedCDBStrings) / sizeof(char *)))
    {
      s = predefinedCDBStrings[n];
    }
    else
    {
      s = i->c_str();
      i++;
    }
    std::printf("  \"%s\", ", s);
    for (size_t j = std::strlen(s); j < 50; j++)
      std::fputc(' ', stdout);
    std::printf("// %4u\n", n);
  }
  return 0;
}

