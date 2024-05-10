
#ifndef BA2FILE_HPP_INCLUDED
#define BA2FILE_HPP_INCLUDED

#include "common.hpp"
#include "filebuf.hpp"

class BA2File
{
 public:
  struct FileInfo
  {
    const unsigned char *fileData;
    // 0 if the file is uncompressed and can be accessed directly via fileData
    unsigned int  packedSize;
    unsigned int  unpackedSize;         // not valid for compressed BSA files
    //    < 0: loose file
    //      0: BA2 general
    //      1: BA2 textures (fileData points to texture info + 13 (numChunks))
    //      2: BA2 textures in raw LZ4 format (Starfield)
    //     64: Morrowind BSA
    // >= 103: Oblivion+ BSA (version + flags, 0x40000000: compressed,
    //         0x0100: full name)
    int           archiveType;
    // NOTE: for loose files, if NIFSKOPE_VERSION is defined, archiveFile
    // is 0xFFFFFFFF, and fileData and packedSize are the full path on the
    // file system and its length, respectively
    unsigned int  archiveFile;          // archiveFiles[] index
    std::uint64_t hashValue;            // hash calculated from fileName
    std::string_view  fileName;         // full path in archive, null-terminated
  };
 protected:
  FileInfo  **fileMap;
  size_t  fileMapHashMask;
  size_t  fileMapFileCnt;
  AllocBuffers  fileInfoBufs;
  std::vector< FileBuffer * >   archiveFiles;
  AllocBuffers  fileNameBufs;
  // User defined function that returns true if the path in 's'
  // should be included.
  bool    (*fileFilterFunction)(void *p, const std::string_view& s);
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
  static inline std::uint64_t hashFunction(const std::string_view& s);
  FileInfo *addPackedFile(const std::string_view& fileName);
  void allocateFileMap();
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
  void loadFile(std::vector< unsigned char >& buf, const FileInfo& fd) const;
#endif
  static bool checkDataDirName(const char *s, size_t len);
  static size_t findPrefixLen(const char *pathName);
  void loadArchivesFromDir(const char *pathName, size_t prefixLen);
  void loadArchiveFile(const char *fileName, size_t prefixLen);
  unsigned int getBSAUnpackedSize(const unsigned char*& dataPtr,
                                  const FileInfo& fd) const;
  [[noreturn]] static void findFileError(const std::string_view& fileName);
  void clear();
 public:
  BA2File();
  BA2File(const char *pathName,
          bool (*fileFilterFunc)(void *p, const std::string_view& s) = nullptr,
          void *fileFilterFuncData = nullptr);
  BA2File(const std::vector< std::string >& pathNames,
          bool (*fileFilterFunc)(void *p, const std::string_view& s) = nullptr,
          void *fileFilterFuncData = nullptr);
  // load a single archive file or directory
  void loadArchivePath(
      const char *pathName,
      bool (*fileFilterFunc)(void *p, const std::string_view& s) = nullptr,
      void *fileFilterFuncData = nullptr);
  virtual ~BA2File();
  void getFileList(std::vector< std::string_view >& fileList,
                   bool disableSorting = false,
                   bool (*fileFilterFunc)(void *p,
                                          const std::string_view& s) = nullptr,
                   void *fileFilterFuncData = nullptr) const;
  // processing stops and true is returned if fileScanFunc() returns true
  bool scanFileList(bool (*fileScanFunc)(void *p, const FileInfo& fd),
                    void *fileScanFuncData = nullptr) const;
  // returns pointer to file information, or NULL if the file is not found
  const FileInfo *findFile(const std::string_view& fileName) const;
  // returns -1 if the file is not found
  std::int64_t getFileSize(const std::string_view& fileName,
                           bool packedSize = false) const;
 protected:
  int extractBA2Texture(std::vector< unsigned char >& buf,
                        const FileInfo& fd, int mipOffset = 0) const;
  void extractBlock(std::vector< unsigned char >& buf,
                    unsigned int unpackedSize, const FileInfo& fd,
                    const unsigned char *p, unsigned int packedSize) const;
 public:
  void extractFile(std::vector< unsigned char >& buf,
                   const std::string_view& fileName) const;
  // returns the remaining number of mip levels to be skipped
  int extractTexture(std::vector< unsigned char >& buf,
                     const std::string_view& fileName, int mipOffset = 0) const;
  // extract file to buf, or get data pointer only if the file is uncompressed
  // returns the file size
  size_t extractFile(const unsigned char*& fileData,
                     std::vector< unsigned char >& buf,
                     const std::string_view& fileName) const;
  inline size_t size() const
  {
    return fileMapFileCnt;
  }
};

#endif

