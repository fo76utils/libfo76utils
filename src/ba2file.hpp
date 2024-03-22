
#ifndef BA2FILE_HPP_INCLUDED
#define BA2FILE_HPP_INCLUDED

#include "common.hpp"
#include "filebuf.hpp"

class BA2File
{
 public:
  struct FileDeclaration
  {
    const unsigned char *fileData;
    // 0 if the file is uncompressed and can be accessed directly via fileData
    unsigned int  packedSize;
    unsigned int  unpackedSize;         // not valid for compressed BSA files
    //    < 0: loose file
    //      0: BA2 general
    //      1: BA2 textures
    //      2: BA2 textures in raw LZ4 format (Starfield)
    //     64: Morrowind BSA
    // >= 103: Oblivion+ BSA (version + flags, 0x40000000: compressed,
    //         0x0100: full name)
    int           archiveType;
    unsigned int  archiveFile;          // archiveFiles[] index
    std::uint32_t hashValue;            // 32-bit hash value
    std::int32_t  prv;                  // index of previous file with same hash
    std::string   fileName;             // full path in archive
    // NOTE: for loose files, if NIFSKOPE_VERSION is defined, archiveFile
    // is 0xFFFFFFFF, and fileData and packedSize are the full path on the
    // file system and its length, respectively
    inline FileDeclaration(const std::string& fName,
                           std::uint32_t h, std::int32_t p);
    inline bool compare(const std::string& fName, std::uint32_t h) const;
  };
 protected:
  enum
  {
    fileDeclBufShift = 12,
    fileDeclBufMask = 0x0FFF,
    nameHashMask = 0x001FFFFF
  };
  std::vector< std::int32_t >   fileMap;        // nameHashMask + 1 elements
  std::vector< std::vector< FileDeclaration > > fileDeclBufs;
  std::vector< FileBuffer * >   archiveFiles;
  // User defined function that returns true if the path in 's'
  // should be included.
  bool    (*fileFilterFunction)(void *p, const std::string& s);
  void    *fileFilterFunctionData;
  static inline char fixNameCharacter(unsigned char c)
  {
    if (c >= 'A' && c <= 'Z')
      return (char(c) + ('a' - 'A'));
    else if (c < 0x20 || c >= 0x7F || c == ':')
      return '_';
    else if (c == '\\')
      return '/';
    return char(c);
  }
  inline FileDeclaration& getFileDecl(std::uint32_t n)
  {
    return fileDeclBufs[n >> fileDeclBufShift][n & fileDeclBufMask];
  }
  inline const FileDeclaration& getFileDecl(std::uint32_t n) const
  {
    return fileDeclBufs[n >> fileDeclBufShift][n & fileDeclBufMask];
  }
  static inline std::uint32_t hashFunction(const std::string& s);
  FileDeclaration *addPackedFile(const std::string& fileName);
  void loadBA2General(FileBuffer& buf, size_t archiveFile, size_t hdrSize);
  void loadBA2Textures(FileBuffer& buf, size_t archiveFile, size_t hdrSize);
  void loadBSAFile(FileBuffer& buf, size_t archiveFile, int archiveType);
  void loadTES3Archive(FileBuffer& buf, size_t archiveFile);
#ifndef NIFSKOPE_VERSION
  bool loadFile(FileBuffer& buf, size_t archiveFile,
                const char *fileName, size_t nameLen, size_t prefixLen);
#else
  bool loadFile(const char *fileName, size_t nameLen, size_t prefixLen,
                size_t fileSize);
  void loadFile(std::vector< unsigned char >& buf,
                const FileDeclaration& fd) const;
#endif
  static bool checkDataDirName(const char *s, size_t len);
  static size_t findPrefixLen(const char *pathName);
  void loadArchivesFromDir(const char *pathName, size_t prefixLen);
  void loadArchiveFile(const char *fileName, size_t prefixLen);
  unsigned int getBSAUnpackedSize(const unsigned char*& dataPtr,
                                  const FileDeclaration& fd) const;
 public:
  BA2File();
  BA2File(const char *pathName,
          bool (*fileFilterFunc)(void *p, const std::string& s) = nullptr,
          void *fileFilterFuncData = nullptr);
  BA2File(const std::vector< std::string >& pathNames,
          bool (*fileFilterFunc)(void *p, const std::string& s) = nullptr,
          void *fileFilterFuncData = nullptr);
  // load a single archive file or directory
  void loadArchivePath(const char *pathName,
                       bool (*fileFilterFunc)(void *p,
                                              const std::string& s) = nullptr,
                       void *fileFilterFuncData = nullptr);
  virtual ~BA2File();
  void getFileList(std::vector< std::string >& fileList,
                   bool disableSorting = false,
                   bool (*fileFilterFunc)(void *p,
                                          const std::string& s) = nullptr,
                   void *fileFilterFuncData = nullptr) const;
  // returns pointer to file information, or NULL if the file is not found
  const FileDeclaration *findFile(const std::string& fileName) const;
  // returns -1 if the file is not found
  long getFileSize(const std::string& fileName, bool packedSize = false) const;
 protected:
  int extractBA2Texture(std::vector< unsigned char >& buf,
                        const FileDeclaration& fileDecl,
                        int mipOffset = 0) const;
  void extractBlock(std::vector< unsigned char >& buf,
                    unsigned int unpackedSize,
                    const FileDeclaration& fileDecl,
                    const unsigned char *p, unsigned int packedSize) const;
 public:
  void extractFile(std::vector< unsigned char >& buf,
                   const std::string& fileName) const;
  // returns the remaining number of mip levels to be skipped
  int extractTexture(std::vector< unsigned char >& buf,
                     const std::string& fileName, int mipOffset = 0) const;
  // extract file to buf, or get data pointer only if the file is uncompressed
  // returns the file size
  size_t extractFile(const unsigned char*& fileData,
                     std::vector< unsigned char >& buf,
                     const std::string& fileName) const;
};

#endif

