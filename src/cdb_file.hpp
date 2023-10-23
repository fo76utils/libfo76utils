
#ifndef CDB_FILE_HPP_INCLUDED
#define CDB_FILE_HPP_INCLUDED

#include "common.hpp"
#include "filebuf.hpp"

#ifndef ENABLE_CDB_DEBUG
#  define ENABLE_CDB_DEBUG  0
#endif

class CDBFile : public FileBuffer
{
 public:
  enum
  {
    ChunkType_NONE = 0,
    ChunkType_BETH = 0x48544542,
    ChunkType_STRT = 0x54525453,
    ChunkType_TYPE = 0x45505954,
    ChunkType_CLAS = 0x53414C43,
    ChunkType_LIST = 0x5453494C,
    ChunkType_MAPC = 0x4350414D,
    ChunkType_OBJT = 0x544A424F,
    ChunkType_DIFF = 0x46464944,
    ChunkType_USER = 0x52455355,
    ChunkType_USRD = 0x44525355
  };
  enum
  {
    String_None = 0,
    String_String = 1,
    String_List = 2,
    String_Map = 3,
    String_Ref = 4,
    String_Int8 = 7,
    String_UInt8 = 8,
    String_Int16 = 9,
    String_UInt16 = 10,
    String_Int32 = 11,
    String_UInt32 = 12,
    String_Int64 = 13,
    String_UInt64 = 14,
    String_Bool = 15,
    String_Float = 16,
    String_Double = 17,
    String_Unknown = 18,
    String_BSComponentDB2_DBFileIndex_ComponentInfo = 149,
    String_BSComponentDB2_DBFileIndex_ComponentTypeInfo = 150,
    String_BSComponentDB2_DBFileIndex_EdgeInfo = 151,
    String_BSComponentDB2_DBFileIndex_ObjectInfo = 152,
    String_BSResource_ID = 227
  };
  static const char *stringTable[1141];
  static int findString(const char *s);
  unsigned int  chunksRemaining;
 protected:
  std::vector< std::int16_t > stringMap;
  void readStringTable();
 public:
  CDBFile(const unsigned char *fileData, size_t fileSize)
    : FileBuffer(fileData, fileSize)
  {
    readStringTable();
  }
  CDBFile(const char *fileName)
    : FileBuffer(fileName)
  {
    readStringTable();
  }
  size_t findString(unsigned int strtOffs) const;
  // returns chunk type (ChunkType_BETH, etc.), or 0 on end of file
  inline unsigned int readChunk(FileBuffer& chunkBuf, size_t startPos = 0)
  {
    if (BRANCH_UNLIKELY(!chunksRemaining))
      return 0U;
    chunksRemaining--;
    if ((filePos + 8ULL) > fileBufSize)
      errorMessage("unexpected end of CDB file");
    unsigned int  chunkType = readUInt32Fast();
    unsigned int  chunkSize = readUInt32Fast();
    if ((filePos + std::uint64_t(chunkSize)) > fileBufSize)
      errorMessage("unexpected end of CDB file");
#if ENABLE_CDB_DEBUG
    char    chunkTypeStr[8];
    chunkTypeStr[0] = char(chunkType & 0x7FU);
    chunkTypeStr[1] = char((chunkType >> 8) & 0x7FU);
    chunkTypeStr[2] = char((chunkType >> 16) & 0x7FU);
    chunkTypeStr[3] = char((chunkType >> 24) & 0x7FU);
    chunkTypeStr[4] = '\0';
    size_t  t = String_Unknown;
    if (chunkSize >= 4U)
      t = findString(FileBuffer::readUInt32Fast(fileBuf + filePos));
    std::printf("%s (%s) at 0x%08X, size = %u bytes",
                chunkTypeStr, stringTable[t],
                (unsigned int) filePos - 8U, chunkSize);
#endif
    (void) new(&chunkBuf) FileBuffer(fileBuf + filePos, chunkSize, startPos);
    filePos = filePos + chunkSize;
    return chunkType;
  }
};

#endif

