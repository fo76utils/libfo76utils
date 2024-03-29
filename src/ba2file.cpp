
#include "common.hpp"
#include "zlib.hpp"
#include "ba2file.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

inline BA2File::FileDeclaration::FileDeclaration(
    const std::string& fName, std::uint32_t h, std::int32_t p)
  : fileData(nullptr),
    packedSize(0U),
    unpackedSize(0U),
    archiveType(0),
    archiveFile(0U),
    hashValue(h),
    prv(p),
    fileName(fName)
{
}

inline bool BA2File::FileDeclaration::compare(
    const std::string& fName, std::uint32_t h) const
{
  return (hashValue == h && fileName == fName);
}

inline std::uint32_t BA2File::hashFunction(const std::string& s)
{
  std::uint64_t h = 0xFFFFFFFFU;
  size_t  n = s.length();
  const std::uint8_t  *p = reinterpret_cast< const std::uint8_t * >(s.c_str());
  for ( ; n >= 8; n = n - 8, p = p + 8)
    hashFunctionUInt64(h, FileBuffer::readUInt64Fast(p));
  if (n)
  {
    std::uint64_t m = 0U;
    if (n & 1)
      m = p[n & 6];
    if (n & 2)
      m = (m << 16) | FileBuffer::readUInt16Fast(p + (n & 4));
    if (n & 4)
      m = (m << 32) | FileBuffer::readUInt32Fast(p);
    hashFunctionUInt64(h, m);
  }
  return std::uint32_t(h & 0xFFFFFFFFU);
}

BA2File::FileDeclaration * BA2File::addPackedFile(const std::string& fileName)
{
  if (fileFilterFunction)
  {
    if (!fileFilterFunction(fileFilterFunctionData, fileName))
      return nullptr;
  }
  std::uint32_t h = hashFunction(fileName);
  for (std::int32_t n = fileMap[h & nameHashMask]; n >= 0; )
  {
    FileDeclaration&  fd = getFileDecl(std::uint32_t(n));
    if (fd.compare(fileName, h))
      return &fd;
    n = fd.prv;
  }
  if (fileDeclBufs.size() < 1 ||
      fileDeclBufs.back().size() >= (fileDeclBufMask + 1U))
  {
    fileDeclBufs.emplace_back();
    fileDeclBufs.back().reserve(fileDeclBufMask + 1U);
  }
  std::vector< FileDeclaration >& fdBuf = fileDeclBufs.back();
  size_t  n = ((fileDeclBufs.size() - 1) << fileDeclBufShift) | fdBuf.size();
  fdBuf.emplace_back(fileName, h, fileMap[h & nameHashMask]);
  fileMap[h & nameHashMask] = std::int32_t(n);
  return &(fdBuf.back());
}

void BA2File::loadBA2General(
    FileBuffer& buf, size_t archiveFile, size_t hdrSize)
{
  size_t  fileCnt = buf.readUInt32();
  size_t  nameOffs = buf.readUInt64();
  if (nameOffs > buf.size() || (fileCnt * 36ULL + hdrSize) > nameOffs)
    errorMessage("invalid BA2 file header");
  std::vector< FileDeclaration * >  fileDecls(fileCnt, nullptr);
  buf.setPosition(nameOffs);
  std::string fileName;
  for (size_t i = 0; i < fileCnt; i++)
  {
    fileName.clear();
    size_t  nameLen = buf.readUInt16();
    if ((buf.getPosition() + nameLen) > buf.size())
      errorMessage("end of input file");
    while (nameLen--)
      fileName += fixNameCharacter(buf.readUInt8Fast());
    fileDecls[i] = addPackedFile(fileName);
  }
  for (size_t i = 0; i < fileCnt; i++)
  {
    if (!fileDecls[i])
      continue;
    FileDeclaration&  fileDecl = *(fileDecls[i]);
    buf.setPosition(i * 36 + hdrSize);
    (void) buf.readUInt32Fast();        // unknown
    (void) buf.readUInt32Fast();        // extension
    (void) buf.readUInt32Fast();        // unknown
    (void) buf.readUInt32Fast();        // flags
    fileDecl.fileData = buf.data() + buf.readUInt64();
    fileDecl.packedSize = buf.readUInt32Fast();
    fileDecl.unpackedSize = buf.readUInt32Fast();
    fileDecl.archiveType = 0;
    fileDecl.archiveFile = (unsigned int) archiveFile;
    (void) buf.readUInt32Fast();        // 0xBAADF00D
  }
}

void BA2File::loadBA2Textures(
    FileBuffer& buf, size_t archiveFile, size_t hdrSize)
{
  size_t  fileCnt = buf.readUInt32();
  size_t  nameOffs = buf.readUInt64();
  if (nameOffs > buf.size() || (fileCnt * 48ULL + hdrSize) > nameOffs)
    errorMessage("invalid BA2 file header");
  std::vector< FileDeclaration * >  fileDecls(fileCnt, nullptr);
  buf.setPosition(nameOffs);
  std::string fileName;
  for (size_t i = 0; i < fileCnt; i++)
  {
    fileName.clear();
    size_t  nameLen = buf.readUInt16();
    if ((buf.getPosition() + nameLen) > buf.size())
      errorMessage("end of input file");
    while (nameLen--)
      fileName += fixNameCharacter(buf.readUInt8Fast());
    fileDecls[i] = addPackedFile(fileName);
  }
  buf.setPosition(hdrSize);
  for (size_t i = 0; i < fileCnt; i++)
  {
    if ((buf.getPosition() + 24) > buf.size())
      errorMessage("end of input file");
    (void) buf.readUInt32Fast();        // unknown
    (void) buf.readUInt32Fast();        // extension ("dds\0")
    (void) buf.readUInt32Fast();        // unknown
    (void) buf.readUInt8Fast();         // unknown
    const unsigned char *fileData = buf.getReadPtr();
    size_t  chunkCnt = buf.readUInt8Fast();
    (void) buf.readUInt16Fast();        // chunk header size
    (void) buf.readUInt16Fast();        // texture width
    (void) buf.readUInt16Fast();        // texture height
    (void) buf.readUInt8Fast();         // number of mipmaps
    (void) buf.readUInt8Fast();         // format
    (void) buf.readUInt16Fast();        // 0x0800
    unsigned int  packedSize = 0;
    unsigned int  unpackedSize = 148;
    if ((buf.getPosition() + (chunkCnt * 24)) > buf.size())
      errorMessage("end of input file");
    for (size_t j = 0; j < chunkCnt; j++)
    {
      (void) buf.readUInt64();          // file offset of chunk data
      packedSize = packedSize + buf.readUInt32Fast();
      unpackedSize = unpackedSize + buf.readUInt32Fast();
      (void) buf.readUInt16Fast();      // start mipmap
      (void) buf.readUInt16Fast();      // end mipmap
      (void) buf.readUInt32Fast();      // 0xBAADF00D
    }
    if (fileDecls[i])
    {
      FileDeclaration&  fileDecl = *(fileDecls[i]);
      fileDecl.fileData = fileData;
      fileDecl.packedSize = packedSize;
      fileDecl.unpackedSize = unpackedSize;
      fileDecl.archiveType = (hdrSize != 36 ? 1 : 2);
      fileDecl.archiveFile = (unsigned int) archiveFile;
    }
  }
}

void BA2File::loadBSAFile(FileBuffer& buf, size_t archiveFile, int archiveType)
{
  unsigned int  flags = buf.readUInt32();
  if (archiveType < 104)
    flags = flags & ~0x0700U;
  if ((flags & ~0x01BCU) != 0x0003)
    errorMessage("unsupported archive file format");
  size_t  folderCnt = buf.readUInt32();
  size_t  fileCnt = buf.readUInt32();   // total number of files
  (void) buf.readUInt32();              // total length of all folder names
  (void) buf.readUInt32();              // total length of all file names
  (void) buf.readUInt16();              // file flags
  (void) buf.readUInt16();              // padding
  // dataSize + (packedFlag << 30) + (dataOffset << 32)
  std::vector< unsigned long long > fileDecls(fileCnt, 0ULL);
  std::vector< unsigned int > folderFileCnts(folderCnt);
  std::vector< std::string >  folderNames(folderCnt);
  for (size_t i = 0; i < folderCnt; i++)
  {
    buf.setPosition(i * (archiveType < 105 ? 16U : 24U) + 36);
    (void) buf.readUInt64();            // hash
    folderFileCnts[i] = buf.readUInt32();
  }
  buf.setPosition(folderCnt * (archiveType < 105 ? 16U : 24U) + 36);
  size_t  n = 0;
  for (size_t i = 0; i < folderCnt; i++)
  {
    std::string folderName;
    for (size_t j = buf.readUInt8(); j-- > 0; )
    {
      unsigned char c = buf.readUInt8();
      if (folderName.empty() && (c == '.' || c == '/' || c == '\\'))
        continue;
      if (c)
        folderName += fixNameCharacter(c);
    }
    if (folderName.length() > 0 && folderName.back() != '/')
      folderName += '/';
    folderNames[i] = folderName;
    for (size_t j = folderFileCnts[i]; j-- > 0; n++)
    {
      (void) buf.readUInt64();          // hash
      unsigned long long  tmp = buf.readUInt64();       // data size, offset
      fileDecls[n] = tmp ^ ((flags & 0x04U) << 28);
    }
  }
  if (n != fileCnt)
    errorMessage("invalid file count in BSA archive");
  n = 0;
  std::string fileName;
  for (size_t i = 0; i < folderCnt; i++)
  {
    for (size_t j = 0; j < folderFileCnts[i]; j++, n++)
    {
      fileName = folderNames[i];
      unsigned char c;
      while ((c = buf.readUInt8()) != '\0')
        fileName += fixNameCharacter(c);
      FileDeclaration *fileDecl = addPackedFile(fileName);
      if (fileDecl)
      {
        fileDecl->fileData = buf.data() + size_t(fileDecls[n] >> 32);
        fileDecl->packedSize = 0;
        fileDecl->unpackedSize = (unsigned int) (fileDecls[n] & 0x7FFFFFFFU);
        fileDecl->archiveType =
            archiveType
            | int((flags & 0x0100) | (fileDecl->unpackedSize & 0x40000000));
        fileDecl->archiveFile = archiveFile;
        if (fileDecl->unpackedSize & 0x40000000)
        {
          fileDecl->packedSize = fileDecl->unpackedSize & 0x3FFFFFFFU;
          fileDecl->unpackedSize = 0;
        }
      }
    }
  }
}

void BA2File::loadTES3Archive(FileBuffer& buf, size_t archiveFile)
{
  buf.setPosition(4);
  std::uint64_t fileDataOffs = buf.readUInt32();        // hash table offset
  size_t  fileCnt = buf.readUInt32();   // total number of files
  fileDataOffs = fileDataOffs + (std::uint64_t(fileCnt) << 3) + 12UL;
  // dataSize + (dataOffset << 32), name offset
  std::vector< std::pair< std::uint64_t, std::uint32_t > >  fileDecls;
  for (size_t i = 0; i < fileCnt; i++)
    fileDecls.emplace_back(buf.readUInt64(), 0U);
  for (size_t i = 0; i < fileCnt; i++)
    fileDecls[i].second = buf.readUInt32();
  size_t  nameTableOffs = buf.getPosition();
  std::string fileName;
  for (size_t i = 0; i < fileCnt; i++)
  {
    fileName.clear();
    size_t  nameOffs = fileDecls[i].second;
    if ((nameTableOffs + std::uint64_t(nameOffs)) >= buf.size())
      errorMessage("invalid file name offset in Morrowind BSA file");
    buf.setPosition(nameTableOffs + nameOffs);
    unsigned char c;
    while ((c = buf.readUInt8()) != '\0')
      fileName += fixNameCharacter(c);
    FileDeclaration *fileDecl = addPackedFile(fileName);
    if (fileDecl)
    {
      std::uint64_t dataOffs = fileDataOffs + (fileDecls[i].first >> 32);
      std::uint32_t dataSize = std::uint32_t(fileDecls[i].first & 0xFFFFFFFFU);
      if ((dataOffs + dataSize) > buf.size())
        errorMessage("invalid file data offset in Morrowind BSA file");
      fileDecl->fileData = buf.data() + dataOffs;
      fileDecl->packedSize = 0;
      fileDecl->unpackedSize = dataSize;
      fileDecl->archiveType = 64;
      fileDecl->archiveFile = archiveFile;
    }
  }
}

bool BA2File::loadFile(FileBuffer& buf, size_t archiveFile,
                       const char *fileName, size_t nameLen, size_t prefixLen)
{
  std::string fileName2;
  fileName2.reserve(nameLen);
  for (size_t i = 0; i < nameLen; i++)
  {
    char    c = fileName[i];
    if (c >= 'A' && c <= 'Z')
      c = c + ('a' - 'A');
    else if (c == '\\')
      c = '/';
    if (c == '/' && fileName2.length() > 0 && fileName2.back() == '/')
      continue;
    fileName2 += c;
  }
  if (prefixLen > 0)
    fileName2.erase(0, prefixLen);
  while (fileName2.starts_with("./"))
    fileName2.erase(0, 2);
  while (fileName2.starts_with("../"))
    fileName2.erase(0, 3);
  if (fileName2.empty())
    return false;
  FileDeclaration *fileDecl = addPackedFile(fileName2);
  if (!fileDecl)
    return false;
  fileDecl->fileData = buf.data();
  fileDecl->packedSize = 0;
  fileDecl->unpackedSize = (unsigned int) buf.size();
  fileDecl->archiveType = -0x80000000;
  fileDecl->archiveFile = (unsigned int) archiveFile;
  return true;
}

bool BA2File::checkDataDirName(const char *s, size_t len)
{
  static const char *dataDirNames[14] =
  {
    "geometries", "icons", "interface", "materials", "meshes",
    "particles", "planetdata", "scripts", "shadersfx", "sound",
    "strings", "terrain", "textures", "vis"
  };
  size_t  n0 = 0;
  size_t  n2 = sizeof(dataDirNames) / sizeof(char *);
  while (n2 > n0)
  {
    size_t  n1 = n0 + ((n2 - n0) >> 1);
    const char  *t = dataDirNames[n1];
    int     d = 0;
    for (size_t i = 0; i < len && !d; i++)
      d = int((unsigned char) s[i]) - int((unsigned char) t[i]);
    if (!d || n1 == n0)
      return !d;
    if (d < 0)
      n2 = n1;
    else
      n0 = n1;
  }
  return false;
}

size_t BA2File::findPrefixLen(const char *pathName)
{
  if (!pathName) [[unlikely]]
    return 0;
  size_t  n = 0;
  while (pathName[n])
    n++;
  while (n > 0)
  {
    size_t  n2 = n;
#if defined(_WIN32) || defined(_WIN64)
    while (n2 > 0 && pathName[n2 - 1] != '/' && pathName[n2 - 1] != '\\' &&
           !(n2 == 2 && pathName[n2 - 1] == ':'))
#else
    while (n2 > 0 && pathName[n2 - 1] != '/')
#endif
    {
      n2--;
    }
    if (n > n2 && checkDataDirName(pathName + n2, n - n2))
      return n2;
    n = n2 - size_t(n2 > 0);
  }
  return 0;
}

void BA2File::loadArchivesFromDir(const char *pathName, size_t prefixLen)
{
  DIR     *d = opendir(pathName);
  if (!d)
    errorMessage("error opening archive directory");
  try
  {
    std::set< std::string > archiveNames;
    std::string dirName(pathName);
#if defined(_WIN32) || defined(_WIN64)
    if (dirName.back() != '/' && dirName.back() != '\\' &&
        !(dirName.length() == 2 && dirName[1] == ':'))
#else
    if (dirName.back() != '/')
#endif
    {
      dirName += '/';
    }
    std::string baseName;
    // 0: game archive, 1: DLC or update archive, 2: mod archive, 3: loose file
    std::string fullName("0");
    fullName += dirName;
    struct dirent *e;
    while (bool(e = readdir(d)))
    {
      baseName = e->d_name;
      if (baseName.length() < 1 || baseName == "." || baseName == "..")
        continue;
      fullName.resize(dirName.length() + 1);
      fullName += baseName;
      for (size_t i = 0; i < baseName.length(); i++)
      {
        if (baseName[i] >= 'A' && baseName[i] <= 'Z')
          baseName[i] = baseName[i] + ('a' - 'A');
      }
      {
#if defined(_WIN32) || defined(_WIN64)
        struct __stat64 st;
        if (_stat64(fullName.c_str() + 1, &st) == 0 &&
            (st.st_mode & _S_IFMT) == _S_IFDIR)
#else
        struct stat st;
        if (stat(fullName.c_str() + 1, &st) == 0 &&
            (st.st_mode & S_IFMT) == S_IFDIR)
#endif
        {
          if ((prefixLen && prefixLen < dirName.length()) ||
              checkDataDirName(baseName.c_str(), baseName.length()))
          {
            fullName[0] = '3';
            archiveNames.insert(fullName);
          }
          continue;
        }
      }
      size_t  n = baseName.rfind('.');
      if (n == std::string::npos ||
          ((((baseName.length() - n) - 4) & ~1ULL) &&
           !baseName.ends_with("strings")))
      {
        continue;
      }
      const char  *s = baseName.c_str() + (baseName.length() - 4);
      std::uint32_t fileType = FileBuffer::readUInt32Fast(s);
      switch (fileType)
      {
        case 0x3261622EU:               // ".ba2"
        case 0x6173622EU:               // ".bsa"
        case 0x6474622EU:               // ".btd"
        case 0x6F74622EU:               // ".bto"
        case 0x7274622EU:               // ".btr"
        case 0x6264632EU:               // ".cdb"
        case 0x7364642EU:               // ".dds"
        case 0x74616D2EU:               // ".mat"
        case 0x66696E2EU:               // ".nif"
        case 0x6D656762U:               // "bgem"
        case 0x6D736762U:               // "bgsm"
        case 0x6873656DU:               // "mesh"
        case 0x73676E69U:               // "ings"
          break;
        default:
          continue;
      }
      char    c = '3';
      if (fileType == 0x3261622EU || fileType == 0x6173622EU)   // .ba2 or .bsa
      {
        c = '2';
        if (baseName == "morrowind.bsa" ||
            baseName.starts_with("oblivion") ||
            baseName.starts_with("fallout") || baseName.starts_with("skyrim") ||
            baseName.starts_with("seventysix") ||
            baseName.starts_with("starfield"))
        {
          c = (baseName.find("update") == std::string::npos &&
               !baseName.ends_with("patch.ba2") ? '0' : '1');
        }
        else if (baseName.starts_with("dlc"))
        {
          c = '1';
        }
      }
      fullName[0] = c;
      archiveNames.insert(fullName);
    }
    closedir(d);
    d = nullptr;
    if (!prefixLen)
      prefixLen = dirName.length();
    for (std::set< std::string >::iterator i = archiveNames.begin();
         i != archiveNames.end(); i++)
    {
      loadArchiveFile(i->c_str() + 1, prefixLen);
    }
  }
  catch (...)
  {
    if (d)
      closedir(d);
    throw;
  }
}

void BA2File::loadArchiveFile(const char *fileName, size_t prefixLen)
{
  if (!fileName || *fileName == '\0') [[unlikely]]
  {
    std::string dataPath;
    if (!FileBuffer::getDefaultDataPath(dataPath))
      errorMessage("empty input file name");
    loadArchivesFromDir(dataPath.c_str(), findPrefixLen(dataPath.c_str()));
    return;
  }
  size_t  nameLen = std::strlen(fileName);
  {
    if (!prefixLen) [[unlikely]]
      prefixLen = findPrefixLen(fileName);
#if defined(_WIN32) || defined(_WIN64)
    char    c = fileName[nameLen - 1];
    if (c == '/' || c == '\\')
    {
      loadArchivesFromDir(fileName, prefixLen);
      return;
    }
    struct __stat64 st;
    if (_stat64(fileName, &st) != 0)
#else
    struct stat st;
    if (stat(fileName, &st) != 0)
#endif
    {
      throw FO76UtilsError("error opening archive file or directory '%s'",
                           fileName);
    }
#if defined(_WIN32) || defined(_WIN64)
    if ((st.st_mode & _S_IFMT) == _S_IFDIR)
#else
    if ((st.st_mode & S_IFMT) == S_IFDIR)
#endif
    {
      loadArchivesFromDir(fileName, prefixLen);
      return;
    }
  }

  std::uint32_t ext = 0;
  if (nameLen >= 4)
    ext = FileBuffer::readUInt32Fast(fileName + (nameLen - 4)) | 0x20202000U;

  FileBuffer  *bufp = new FileBuffer(fileName);
  try
  {
    FileBuffer&   buf = *bufp;
    size_t  archiveFile = archiveFiles.size();
    // loose file: -1, Morrowind BSA: 128, Oblivion+ BSA: 103 to 105,
    // BA2: header size (+ 1 if DX10)
    int           archiveType = -1;
    if (buf.size() >= 12)
    {
      unsigned int  hdr1 = buf.readUInt32Fast();
      unsigned int  hdr2 = buf.readUInt32Fast();
      unsigned int  hdr3 = buf.readUInt32Fast();
      if (hdr1 == 0x58445442 && hdr2 && hdr2 <= 3U)     // "BTDX", version <= 3
      {
        if (hdr3 == 0x4C524E47)                         // "GNRL"
          archiveType = (hdr2 == 1 ? 24 : 32);
        else if (hdr3 == 0x30315844)                    // "DX10"
          archiveType = (hdr2 == 1 ? 25 : (hdr2 == 2 ? 33 : 37));
      }
      else if (hdr1 == 0x00415342 && hdr3 == 36)        // "BSA\0", header size
      {
        if (hdr2 >= 103 && hdr2 <= 105)
          archiveType = int(hdr2);
      }
      else if (hdr1 == 0x00000100 && ext == 0x6173622E) // ".bsa"
      {
        archiveType = 128;
      }
    }
    switch (archiveType & 0xC1)
    {
      case 0:
        loadBA2General(buf, archiveFile, size_t(archiveType));
        break;
      case 1:
        loadBA2Textures(buf, archiveFile, size_t(archiveType & 0x3E));
        break;
      case 0x40:
      case 0x41:
        loadBSAFile(buf, archiveFile, archiveType);
        break;
      case 0x80:
        loadTES3Archive(buf, archiveFile);
        break;
      default:
        if (!loadFile(buf, archiveFile,
                      fileName, nameLen, prefixLen)) [[unlikely]]
        {
          delete bufp;
          return;
        }
        break;
    }
    archiveFiles.push_back(bufp);
  }
  catch (...)
  {
    delete bufp;
    throw;
  }
}

unsigned int BA2File::getBSAUnpackedSize(const unsigned char*& dataPtr,
                                         const FileDeclaration& fd) const
{
  const FileBuffer& buf = *(archiveFiles[fd.archiveFile]);
  size_t  offs = size_t(dataPtr - buf.data());
  if (fd.archiveType & 0x00000100)
    offs = offs + (size_t(buf.readUInt8(offs)) + 1);
  unsigned int  unpackedSize = fd.unpackedSize;
  if (fd.archiveType & 0x40000000)
  {
    unpackedSize = buf.readUInt32(offs);
    offs = offs + 4;
  }
  dataPtr = buf.data() + offs;
  return unpackedSize;
}

BA2File::BA2File()
  : fileMap(size_t(nameHashMask + 1), std::int32_t(-1)),
    fileFilterFunction(nullptr),
    fileFilterFunctionData(nullptr)
{
}

BA2File::BA2File(const char *pathName,
                 bool (*fileFilterFunc)(void *p, const std::string& s),
                 void *fileFilterFuncData)
  : fileMap(size_t(nameHashMask + 1), std::int32_t(-1)),
    fileFilterFunction(fileFilterFunc),
    fileFilterFunctionData(fileFilterFuncData)
{
  loadArchiveFile(pathName, 0);
}

BA2File::BA2File(const std::vector< std::string >& pathNames,
                 bool (*fileFilterFunc)(void *p, const std::string& s),
                 void *fileFilterFuncData)
  : fileMap(size_t(nameHashMask + 1), std::int32_t(-1)),
    fileFilterFunction(fileFilterFunc),
    fileFilterFunctionData(fileFilterFuncData)
{
  for (size_t i = 0; i < pathNames.size(); i++)
    loadArchiveFile(pathNames[i].c_str(), 0);
}

void BA2File::loadArchivePath(
    const char *pathName,
    bool (*fileFilterFunc)(void *p, const std::string& s),
    void *fileFilterFuncData)
{
  fileFilterFunction = fileFilterFunc;
  fileFilterFunctionData = fileFilterFuncData;
  loadArchiveFile(pathName, 0);
}

BA2File::~BA2File()
{
  for (size_t i = 0; i < archiveFiles.size(); i++)
    delete archiveFiles[i];
}

void BA2File::getFileList(
    std::vector< std::string >& fileList, bool disableSorting,
    bool (*fileFilterFunc)(void *p, const std::string& s),
    void *fileFilterFuncData) const
{
  fileList.clear();
  for (size_t i = 0; i < fileDeclBufs.size(); i++)
  {
    if (!fileFilterFunc)
    {
      for (size_t j = 0; j < fileDeclBufs[i].size(); j++)
        fileList.emplace_back(fileDeclBufs[i][j].fileName);
    }
    else
    {
      for (size_t j = 0; j < fileDeclBufs[i].size(); j++)
      {
        const std::string&  s = fileDeclBufs[i][j].fileName;
        if (fileFilterFunc(fileFilterFuncData, s))
          fileList.emplace_back(s);
      }
    }
  }
  if (fileList.size() > 1 && !disableSorting)
    std::sort(fileList.begin(), fileList.end());
}

const BA2File::FileDeclaration * BA2File::findFile(
    const std::string& fileName) const
{
  std::uint32_t h = hashFunction(fileName);
  for (std::int32_t n = fileMap[h & nameHashMask]; n >= 0; )
  {
    const FileDeclaration&  fd = getFileDecl(std::uint32_t(n));
    if (fd.compare(fileName, h))
      return &fd;
    n = fd.prv;
  }
  return nullptr;
}

long BA2File::getFileSize(const std::string& fileName, bool packedSize) const
{
  const FileDeclaration *fd = findFile(fileName);
  if (!fd)
    return -1L;
  if (packedSize && fd->packedSize)
    return long(fd->packedSize);
  if (fd->archiveType & 0x40000000)     // compressed BSA
  {
    const unsigned char *p = fd->fileData;
    return long(getBSAUnpackedSize(p, *fd));
  }
  return long(fd->unpackedSize);
}

int BA2File::extractBA2Texture(std::vector< unsigned char >& buf,
                               const FileDeclaration& fileDecl,
                               int mipOffset) const
{
  FileBuffer  fileBuf(archiveFiles[fileDecl.archiveFile]->data(),
                      archiveFiles[fileDecl.archiveFile]->size());
  const unsigned char *p = fileDecl.fileData;
  size_t  chunkCnt = p[0];
  unsigned int  height = FileBuffer::readUInt16Fast(p + 3);
  unsigned int  width = FileBuffer::readUInt16Fast(p + 5);
  int     mipCnt = p[7];
  unsigned char dxgiFormat = p[8];
  bool    isCubeMap = bool(p[9] & 1);
  size_t  offs = size_t(p - fileBuf.data()) + 11;
  buf.resize(148);
  for ( ; chunkCnt-- > 0; offs = offs + 24)
  {
    fileBuf.setPosition(offs);
    size_t  chunkOffset = fileBuf.readUInt64();
    size_t  chunkSizePacked = fileBuf.readUInt32Fast();
    size_t  chunkSizeUnpacked = fileBuf.readUInt32Fast();
    int     chunkMipCnt = fileBuf.readUInt16Fast();
    chunkMipCnt = int(fileBuf.readUInt16Fast()) + 1 - chunkMipCnt;
    if (chunkMipCnt > 0 &&
        (chunkMipCnt < mipOffset || (chunkMipCnt == mipOffset && chunkCnt > 0)))
    {
      do
      {
        mipOffset--;
        width = (width + 1) >> 1;
        height = (height + 1) >> 1;
        mipCnt--;
      }
      while (--chunkMipCnt > 0);
    }
    else
    {
      extractBlock(buf, chunkSizeUnpacked, fileDecl,
                   fileBuf.data() + chunkOffset, chunkSizePacked);
    }
  }
  // write DDS header
  if (!FileBuffer::writeDDSHeader(buf.data(), dxgiFormat,
                                  int(width), int(height), mipCnt, isCubeMap))
  {
    throw FO76UtilsError("unsupported DXGI_FORMAT 0x%02X",
                         (unsigned int) dxgiFormat);
  }
  // return the remaining number of mip levels to be skipped
  return mipOffset;
}

void BA2File::extractBlock(
    std::vector< unsigned char >& buf, unsigned int unpackedSize,
    const FileDeclaration& fileDecl,
    const unsigned char *p, unsigned int packedSize) const
{
  size_t  n = buf.size();
  buf.resize(n + unpackedSize);
  const FileBuffer& fileBuf = *(archiveFiles[fileDecl.archiveFile]);
  size_t  offs = size_t(p - fileBuf.data());
  if (!packedSize)
  {
    if (offs >= fileBuf.size() || (offs + unpackedSize) > fileBuf.size())
      errorMessage("invalid packed data offset or size");
    std::memcpy(buf.data() + n, p, unpackedSize);
  }
  else
  {
    if (offs >= fileBuf.size() || (offs + packedSize) > fileBuf.size())
      errorMessage("invalid packed data offset or size");
    if (fileDecl.archiveType == 2) [[unlikely]]
    {
      n = ZLibDecompressor::decompressLZ4Raw(
              buf.data() + n, unpackedSize, p, packedSize);
    }
    else
    {
      n = ZLibDecompressor::decompressData(
              buf.data() + n, unpackedSize, p, packedSize);
    }
    if (n != unpackedSize)
      errorMessage("invalid or corrupt compressed data in archive");
  }
}

void BA2File::extractFile(std::vector< unsigned char >& buf,
                          const std::string& fileName) const
{
  buf.clear();
  const FileDeclaration *fd = findFile(fileName);
  if (!fd)
    throw FO76UtilsError("file %s not found in archive", fileName.c_str());
  const FileDeclaration&  fileDecl = *fd;
  const unsigned char *p = fileDecl.fileData;
  unsigned int  packedSize = fileDecl.packedSize;
  unsigned int  unpackedSize = fileDecl.unpackedSize;
  int     archiveType = fileDecl.archiveType;
  if (archiveType & 0x40000100)         // BSA with compression or full names
  {
    unpackedSize = getBSAUnpackedSize(p, fileDecl);
    packedSize = packedSize - (unsigned int) (p - fileDecl.fileData);
  }
  if (!unpackedSize)
    return;
  buf.reserve(unpackedSize);

  if (archiveType == 1 || archiveType == 2)
  {
    (void) extractBA2Texture(buf, fileDecl);
    return;
  }

  extractBlock(buf, unpackedSize, fileDecl, p, packedSize);
}

int BA2File::extractTexture(std::vector< unsigned char >& buf,
                            const std::string& fileName, int mipOffset) const
{
  buf.clear();
  const FileDeclaration *fd = findFile(fileName);
  if (!fd)
    throw FO76UtilsError("file %s not found in archive", fileName.c_str());
  const FileDeclaration&  fileDecl = *fd;
  const unsigned char *p = fileDecl.fileData;
  unsigned int  packedSize = fileDecl.packedSize;
  unsigned int  unpackedSize = fileDecl.unpackedSize;
  int     archiveType = fileDecl.archiveType;
  if (archiveType & 0x40000100)         // BSA with compression or full names
  {
    unpackedSize = getBSAUnpackedSize(p, fileDecl);
    packedSize = packedSize - (unsigned int) (p - fileDecl.fileData);
  }
  if (!unpackedSize)
    return mipOffset;
  buf.reserve(unpackedSize);

  if (archiveType == 1 || archiveType == 2)
    return extractBA2Texture(buf, fileDecl, mipOffset);

  extractBlock(buf, unpackedSize, fileDecl, p, packedSize);
  return mipOffset;
}

size_t BA2File::extractFile(
    const unsigned char*& fileData, std::vector< unsigned char >& buf,
    const std::string& fileName) const
{
  fileData = nullptr;
  buf.clear();
  const FileDeclaration *fd = findFile(fileName);
  if (!fd)
    throw FO76UtilsError("file %s not found in archive", fileName.c_str());
  const FileDeclaration&  fileDecl = *fd;
  const unsigned char *p = fileDecl.fileData;
  unsigned int  packedSize = fileDecl.packedSize;
  unsigned int  unpackedSize = fileDecl.unpackedSize;
  int     archiveType = fileDecl.archiveType;
  if (archiveType & 0x40000100)         // BSA with compression or full names
  {
    unpackedSize = getBSAUnpackedSize(p, fileDecl);
    packedSize = packedSize - (unsigned int) (p - fileDecl.fileData);
  }
  if (!unpackedSize)
    return 0;
  if (!(packedSize || archiveType == 1 || archiveType == 2))
  {
    const FileBuffer& fileBuf = *(archiveFiles[fileDecl.archiveFile]);
    size_t  offs = size_t(p - fileBuf.data());
    if (offs >= fileBuf.size() || (offs + unpackedSize) > fileBuf.size())
      errorMessage("invalid packed data offset or size");
    fileData = p;
    return unpackedSize;
  }

  buf.reserve(unpackedSize);
  if (archiveType == 1 || archiveType == 2)
    (void) extractBA2Texture(buf, fileDecl);
  else
    extractBlock(buf, unpackedSize, fileDecl, p, packedSize);
  fileData = buf.data();
  return buf.size();
}

