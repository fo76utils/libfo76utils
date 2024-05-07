
#include "common.hpp"
#include "zlib.hpp"
#include "ba2file.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>
#else
#  include <dirent.h>
#endif

inline BA2File::FileInfo::FileInfo(
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

inline bool BA2File::FileInfo::compare(
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

BA2File::FileInfo * BA2File::addPackedFile(const std::string& fileName)
{
  if (fileFilterFunction)
  {
    if (!fileFilterFunction(fileFilterFunctionData, fileName))
      return nullptr;
  }
  std::uint32_t h = hashFunction(fileName);
  for (std::int32_t n = fileMap[h & nameHashMask]; n >= 0; )
  {
    FileInfo& fd = getFileInfo(std::uint32_t(n));
    if (fd.compare(fileName, h))
      return &fd;
    n = fd.prv;
  }
  if (fileInfoBufs.size() < 1 ||
      fileInfoBufs.back().size() >= (fileInfoBufMask + 1U))
  {
    fileInfoBufs.emplace_back();
    fileInfoBufs.back().reserve(fileInfoBufMask + 1U);
  }
  std::vector< FileInfo >&  fdBuf = fileInfoBufs.back();
  size_t  n = ((fileInfoBufs.size() - 1) << fileInfoBufShift) | fdBuf.size();
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
  std::vector< FileInfo * > fileList(fileCnt, nullptr);
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
    fileList[i] = addPackedFile(fileName);
  }
  for (size_t i = 0; i < fileCnt; i++)
  {
    if (!fileList[i])
      continue;
    FileInfo& fileInfo = *(fileList[i]);
    buf.setPosition(i * 36 + hdrSize);
    (void) buf.readUInt32Fast();        // unknown
    (void) buf.readUInt32Fast();        // extension
    (void) buf.readUInt32Fast();        // unknown
    (void) buf.readUInt32Fast();        // flags
    fileInfo.fileData = buf.data() + buf.readUInt64();
    fileInfo.packedSize = buf.readUInt32Fast();
    fileInfo.unpackedSize = buf.readUInt32Fast();
    fileInfo.archiveType = 0;
    fileInfo.archiveFile = (unsigned int) archiveFile;
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
  std::vector< FileInfo * > fileList(fileCnt, nullptr);
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
    fileList[i] = addPackedFile(fileName);
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
    if (fileList[i])
    {
      FileInfo& fileInfo = *(fileList[i]);
      fileInfo.fileData = fileData;
      fileInfo.packedSize = packedSize;
      fileInfo.unpackedSize = unpackedSize;
      fileInfo.archiveType = (hdrSize != 36 ? 1 : 2);
      fileInfo.archiveFile = (unsigned int) archiveFile;
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
  std::vector< unsigned long long > fileList(fileCnt, 0ULL);
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
      fileList[n] = tmp ^ ((flags & 0x04U) << 28);
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
      FileInfo  *fileInfo = addPackedFile(fileName);
      if (fileInfo)
      {
        fileInfo->fileData = buf.data() + size_t(fileList[n] >> 32);
        fileInfo->packedSize = 0;
        fileInfo->unpackedSize = (unsigned int) (fileList[n] & 0x7FFFFFFFU);
        fileInfo->archiveType =
            archiveType
            | int((flags & 0x0100) | (fileInfo->unpackedSize & 0x40000000));
        fileInfo->archiveFile = archiveFile;
        if (fileInfo->unpackedSize & 0x40000000)
        {
          fileInfo->packedSize = fileInfo->unpackedSize & 0x3FFFFFFFU;
          fileInfo->unpackedSize = 0;
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
  std::vector< std::pair< std::uint64_t, std::uint32_t > >  fileList;
  for (size_t i = 0; i < fileCnt; i++)
    fileList.emplace_back(buf.readUInt64(), 0U);
  for (size_t i = 0; i < fileCnt; i++)
    fileList[i].second = buf.readUInt32();
  size_t  nameTableOffs = buf.getPosition();
  std::string fileName;
  for (size_t i = 0; i < fileCnt; i++)
  {
    fileName.clear();
    size_t  nameOffs = fileList[i].second;
    if ((nameTableOffs + std::uint64_t(nameOffs)) >= buf.size())
      errorMessage("invalid file name offset in Morrowind BSA file");
    buf.setPosition(nameTableOffs + nameOffs);
    unsigned char c;
    while ((c = buf.readUInt8()) != '\0')
      fileName += fixNameCharacter(c);
    FileInfo  *fileInfo = addPackedFile(fileName);
    if (fileInfo)
    {
      std::uint64_t dataOffs = fileDataOffs + (fileList[i].first >> 32);
      std::uint32_t dataSize = std::uint32_t(fileList[i].first & 0xFFFFFFFFU);
      if ((dataOffs + dataSize) > buf.size())
        errorMessage("invalid file data offset in Morrowind BSA file");
      fileInfo->fileData = buf.data() + dataOffs;
      fileInfo->packedSize = 0;
      fileInfo->unpackedSize = dataSize;
      fileInfo->archiveType = 64;
      fileInfo->archiveFile = archiveFile;
    }
  }
}

#ifndef NIFSKOPE_VERSION
bool BA2File::loadFile(FileBuffer& buf, size_t archiveFile,
                       const char *fileName, size_t nameLen, size_t prefixLen)
#else
bool BA2File::loadFile(const char *fileName, size_t nameLen, size_t prefixLen,
                       size_t fileSize)
#endif
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
  FileInfo  *fileInfo = addPackedFile(fileName2);
  if (!fileInfo)
    return false;
#ifdef NIFSKOPE_VERSION
  if (fileInfoBufs.size() < 1 ||
      fileInfoBufs.back().size() >= (fileInfoBufMask + 1U))
  {
    fileInfoBufs.emplace_back();
    fileInfoBufs.back().reserve(fileInfoBufMask + 1U);
  }
  std::vector< FileInfo >&  fdBuf = fileInfoBufs.back();
  fdBuf.emplace_back(std::string(fileName), 0U, 0);
  fileInfo->fileData =
      reinterpret_cast< const unsigned char * >(fdBuf.back().fileName.c_str());
  fileInfo->packedSize = (unsigned int) nameLen;
  fileInfo->unpackedSize = (unsigned int) fileSize;
  fileInfo->archiveType = -0x80000000;
  fileInfo->archiveFile = 0xFFFFFFFFU;
#else
  fileInfo->fileData = buf.data();
  fileInfo->packedSize = 0;
  fileInfo->unpackedSize = (unsigned int) buf.size();
  fileInfo->archiveType = -0x80000000;
  fileInfo->archiveFile = (unsigned int) archiveFile;
#endif
  return true;
}

#ifdef NIFSKOPE_VERSION
void BA2File::loadFile(
    std::vector< unsigned char >& buf, const FileInfo& fd) const
{
  const char  *fileName = reinterpret_cast< const char * >(fd.fileData);
  FileBuffer  f(fileName);
  if (f.size() != fd.unpackedSize)
  {
    throw FO76UtilsError("BA2File: unexpected change to size of loose file %s",
                         fileName);
  }
  if (fd.unpackedSize) [[likely]]
  {
    buf.resize(fd.unpackedSize);
    std::memcpy(buf.data(), f.data(), fd.unpackedSize);
  }
}
#endif

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
      d = int((unsigned char) s[i] | 0x20) - int((unsigned char) t[i]);
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

struct ArchiveDirListItem
{
  //   -4: game archive
  //   -3: DLC or update archive
  //   -2: mod archive
  //   -1: sub-directory
  // >= 0: size of loose file
  std::int64_t  fileSize;
  std::string   baseName;
  inline bool operator<(const ArchiveDirListItem& r) const
  {
    std::int64_t  size1 = std::min< std::int64_t >(fileSize, -1);
    std::int64_t  size2 = std::min< std::int64_t >(r.fileSize, -1);
    return (size1 < size2 || (size1 == size2 && baseName < r.baseName));
  }
};

void BA2File::loadArchivesFromDir(const char *pathName, size_t prefixLen)
{
  std::string dirName(pathName);
#if defined(_WIN32) || defined(_WIN64)
  dirName += ((dirName.back() != '/' && dirName.back() != '\\' &&
               !(dirName.length() == 2 && dirName[1] == ':')) ? "\\*" : "*");
  __finddata64_t  e;
  std::intptr_t   d = _findfirst64(dirName.c_str(), &e);
  if (d < 0)
    errorMessage("error opening archive directory");
  dirName.resize(dirName.length() - 1);
#else
  if (dirName.back() != '/')
    dirName += '/';
  DIR     *d = opendir(pathName);
  if (!d)
    errorMessage("error opening archive directory");
#endif
  if (!prefixLen)
    prefixLen = dirName.length();

  try
  {
    std::set< ArchiveDirListItem >  fileList;
    ArchiveDirListItem  f;
    std::string baseNameL;
    std::string fullName(dirName);

#if defined(_WIN32) || defined(_WIN64)
    do
    {
      f.baseName = e.name;
#else
    struct dirent *e;
    while (bool(e = readdir(d)))
    {
      f.baseName = e->d_name;
#endif
      if (f.baseName.empty() || f.baseName == "." || f.baseName == "..")
        continue;
      {
#if defined(_WIN32) || defined(_WIN64)
        f.fileSize = std::int64_t(e.size);
        bool    isDir = bool(e.attrib & _A_SUBDIR);
#else
        fullName.resize(dirName.length());
        fullName += f.baseName;
        struct stat st;
        if (stat(fullName.c_str(), &st) != 0)
          continue;
        f.fileSize = std::int64_t(st.st_size);
        bool    isDir = ((st.st_mode & S_IFMT) == S_IFDIR);
#endif
        if (isDir)
        {
          if (prefixLen < dirName.length() ||
              checkDataDirName(f.baseName.c_str(), f.baseName.length()))
          {
            f.fileSize = -1;
            fileList.insert(f);
          }
          continue;
        }
      }
      if (f.fileSize < 1)               // ignore empty files
        continue;
      size_t  n = f.baseName.rfind('.');
      if (n == std::string::npos || (f.baseName.length() - n) > 10)
        continue;
      std::uint64_t fileType = 0U;
      for (unsigned char b = 48; ++n < f.baseName.length(); b = b - 6)
      {
        unsigned char c = (unsigned char) f.baseName[n];
        if (c < 0x20 || c > 0x7F)
          c = 0xFF;
        c = (c & 0x1F) | ((c & 0x40) >> 1);
        fileType = fileType | (std::uint64_t(c) << b);
      }
      switch (fileType)
      {
        case 0x22852000000000ULL:       // "ba2"
        case 0x22CE1000000000ULL:       // "bsa"
          baseNameL = f.baseName;
          for (char& c : baseNameL)
          {
            if (c >= 'A' && c <= 'Z')
              c = c + ('a' - 'A');
          }
          f.fileSize = -2;
          if (baseNameL == "morrowind.bsa" ||
              baseNameL.starts_with("oblivion") ||
              baseNameL.starts_with("fallout") ||
              baseNameL.starts_with("skyrim") ||
              baseNameL.starts_with("seventysix") ||
              baseNameL.starts_with("starfield"))
          {
            f.fileSize = (baseNameL.find("update") == std::string::npos &&
                          !baseNameL.ends_with("patch.ba2") ? -4 : -3);
          }
          else if (baseNameL.starts_with("dlc"))
          {
            f.fileSize = -3;
          }
          [[fallthrough]];
        case 0x229E5B40000000ULL:       // "bgem"
        case 0x229F3B40000000ULL:       // "bgsm"
        case 0x22B70000000000ULL:       // "bmp"
        case 0x22D24000000000ULL:       // "btd"
        case 0x22D2F000000000ULL:       // "bto"
        case 0x22D32000000000ULL:       // "btr"
        case 0x23922000000000ULL:       // "cdb"
        case 0x24933000000000ULL:       // "dds"
        case 0x24B33D32A6E9F3ULL:       // "dlstrings"
        case 0x28932000000000ULL:       // "hdr"
        case 0x29B33D32A6E9F3ULL:       // "ilstrings"
        case 0x2D874000000000ULL:       // "mat"
        case 0x2D973A00000000ULL:       // "mesh"
        case 0x2EA66000000000ULL:       // "nif"
        case 0x33D32A6E9F3000ULL:       // "strings"
        case 0x349E1000000000ULL:       // "tga"
          fileList.insert(f);
          break;
      }
    }
#if defined(_WIN32) || defined(_WIN64)
    while (_findnext64(d, &e) >= 0);
    _findclose(d);
    d = -1;
#else
    closedir(d);
    d = nullptr;
#endif
    for (const auto& i : fileList)
    {
      fullName.resize(dirName.length());
      fullName += i.baseName;
#ifdef NIFSKOPE_VERSION
      if (i.fileSize >= 0)
      {
        // create list of loose files without opening them
        (void) loadFile(fullName.c_str(), fullName.length(),
                        prefixLen, size_t(i.fileSize));
      }
      else
#endif
      {
        loadArchiveFile(fullName.c_str(), prefixLen);
      }
    }
  }
  catch (...)
  {
#if defined(_WIN32) || defined(_WIN64)
    if (d >= 0)
      _findclose(d);
#else
    if (d)
      closedir(d);
#endif
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
#ifdef NIFSKOPE_VERSION
  size_t  fileSize;
#endif
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
#ifdef NIFSKOPE_VERSION
    fileSize = size_t(std::max< std::int64_t >(std::int64_t(st.st_size), 0));
#endif
  }

  std::uint32_t ext = 0;
  if (nameLen >= 4)
    ext = FileBuffer::readUInt32Fast(fileName + (nameLen - 4)) | 0x20202000U;
#ifdef NIFSKOPE_VERSION
  if (!(fileSize >= 12 && (ext == 0x3261622E || ext == 0x6173622E)))
  {                                     // not ".ba2" or ".bsa"
    (void) loadFile(fileName, nameLen, prefixLen, fileSize);
    return;
  }
#endif

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
      if (hdr1 == 0x58445442 && hdr2 <= 15U && ((hdr2 = (1U << hdr2)) & 0x018E))
      {                                 // "BTDX", version 1, 2, 3, 7 or 8
        if (hdr3 == 0x4C524E47)                         // "GNRL"
          archiveType = (!(hdr2 & 0x0C) ? 24 : 32);
        else if (hdr3 == 0x30315844)                    // "DX10"
          archiveType = (!(hdr2 & 0x0C) ? 25 : (!(hdr2 & 8) ? 33 : 37));
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
#ifdef NIFSKOPE_VERSION
        delete bufp;
        (void) loadFile(fileName, nameLen, prefixLen, fileSize);
        return;
#else
        if (!loadFile(buf, archiveFile,
                      fileName, nameLen, prefixLen)) [[unlikely]]
        {
          delete bufp;
          return;
        }
        break;
#endif
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
                                         const FileInfo& fd) const
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
  for (const auto& i : fileInfoBufs)
  {
    if (!fileFilterFunc)
    {
      for (const auto& fd : i)
        fileList.emplace_back(fd.fileName);
    }
    else
    {
      for (const auto& fd : i)
      {
        const std::string&  s = fd.fileName;
        if (fileFilterFunc(fileFilterFuncData, s))
          fileList.emplace_back(s);
      }
    }
  }
  if (fileList.size() > 1 && !disableSorting)
    std::sort(fileList.begin(), fileList.end());
}

bool BA2File::scanFileList(
    bool (*fileScanFunc)(void *p, const FileInfo& fd),
    void *fileScanFuncData) const
{
  for (const auto& i : fileInfoBufs)
  {
    for (const auto& fd : i)
    {
      if (fileScanFunc(fileScanFuncData, fd))
        return true;
    }
  }
  return false;
}

const BA2File::FileInfo * BA2File::findFile(const std::string& fileName) const
{
  std::uint32_t h = hashFunction(fileName);
  for (std::int32_t n = fileMap[h & nameHashMask]; n >= 0; )
  {
    const FileInfo& fd = getFileInfo(std::uint32_t(n));
    if (fd.compare(fileName, h))
      return &fd;
    n = fd.prv;
  }
  return nullptr;
}

long BA2File::getFileSize(const std::string& fileName, bool packedSize) const
{
  const FileInfo  *fd = findFile(fileName);
  if (!fd)
    return -1L;
  if (packedSize && fd->packedSize)
  {
#ifdef NIFSKOPE_VERSION
    if (fd->archiveType < 0) [[unlikely]]
      return long(fd->unpackedSize);
#endif
    return long(fd->packedSize);
  }
  if (fd->archiveType & 0x40000000)     // compressed BSA
  {
    const unsigned char *p = fd->fileData;
    return long(getBSAUnpackedSize(p, *fd));
  }
  return long(fd->unpackedSize);
}

int BA2File::extractBA2Texture(std::vector< unsigned char >& buf,
                               const FileInfo& fd, int mipOffset) const
{
  FileBuffer  fileBuf(archiveFiles[fd.archiveFile]->data(),
                      archiveFiles[fd.archiveFile]->size());
  const unsigned char *p = fd.fileData;
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
      extractBlock(buf, chunkSizeUnpacked, fd,
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
    const FileInfo& fd, const unsigned char *p, unsigned int packedSize) const
{
  size_t  n = buf.size();
  buf.resize(n + unpackedSize);
  const FileBuffer& fileBuf = *(archiveFiles[fd.archiveFile]);
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
    if (fd.archiveType == 2) [[unlikely]]
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
  const FileInfo  *fdPtr = findFile(fileName);
  if (!fdPtr)
    throw FO76UtilsError("file %s not found in archive", fileName.c_str());
  const FileInfo& fd = *fdPtr;
  const unsigned char *p = fd.fileData;
  unsigned int  packedSize = fd.packedSize;
  unsigned int  unpackedSize = fd.unpackedSize;
  int     archiveType = fd.archiveType;
#ifdef NIFSKOPE_VERSION
  if (archiveType < 0) [[unlikely]]
  {
    loadFile(buf, fd);
    return;
  }
#endif
  if (archiveType & 0x40000100)         // BSA with compression or full names
  {
    unpackedSize = getBSAUnpackedSize(p, fd);
    packedSize = packedSize - (unsigned int) (p - fd.fileData);
  }
  if (!unpackedSize)
    return;
  buf.reserve(unpackedSize);

  if (archiveType == 1 || archiveType == 2)
  {
    (void) extractBA2Texture(buf, fd);
    return;
  }

  extractBlock(buf, unpackedSize, fd, p, packedSize);
}

int BA2File::extractTexture(std::vector< unsigned char >& buf,
                            const std::string& fileName, int mipOffset) const
{
  buf.clear();
  const FileInfo  *fdPtr = findFile(fileName);
  if (!fdPtr)
    throw FO76UtilsError("file %s not found in archive", fileName.c_str());
  const FileInfo& fd = *fdPtr;
  const unsigned char *p = fd.fileData;
  unsigned int  packedSize = fd.packedSize;
  unsigned int  unpackedSize = fd.unpackedSize;
  int     archiveType = fd.archiveType;
#ifdef NIFSKOPE_VERSION
  if (archiveType < 0) [[unlikely]]
  {
    loadFile(buf, fd);
    return mipOffset;
  }
#endif
  if (archiveType & 0x40000100)         // BSA with compression or full names
  {
    unpackedSize = getBSAUnpackedSize(p, fd);
    packedSize = packedSize - (unsigned int) (p - fd.fileData);
  }
  if (!unpackedSize)
    return mipOffset;
  buf.reserve(unpackedSize);

  if (archiveType == 1 || archiveType == 2)
    return extractBA2Texture(buf, fd, mipOffset);

  extractBlock(buf, unpackedSize, fd, p, packedSize);
  return mipOffset;
}

size_t BA2File::extractFile(
    const unsigned char*& fileData, std::vector< unsigned char >& buf,
    const std::string& fileName) const
{
  fileData = nullptr;
  buf.clear();
  const FileInfo  *fdPtr = findFile(fileName);
  if (!fdPtr)
    throw FO76UtilsError("file %s not found in archive", fileName.c_str());
  const FileInfo& fd = *fdPtr;
  const unsigned char *p = fd.fileData;
  unsigned int  packedSize = fd.packedSize;
  unsigned int  unpackedSize = fd.unpackedSize;
  int     archiveType = fd.archiveType;
#ifdef NIFSKOPE_VERSION
  if (archiveType < 0) [[unlikely]]
  {
    loadFile(buf, fd);
    fileData = buf.data();
    return buf.size();
  }
#endif
  if (archiveType & 0x40000100)         // BSA with compression or full names
  {
    unpackedSize = getBSAUnpackedSize(p, fd);
    packedSize = packedSize - (unsigned int) (p - fd.fileData);
  }
  if (!unpackedSize)
    return 0;
  if (!(packedSize || archiveType == 1 || archiveType == 2))
  {
    const FileBuffer& fileBuf = *(archiveFiles[fd.archiveFile]);
    size_t  offs = size_t(p - fileBuf.data());
    if (offs >= fileBuf.size() || (offs + unpackedSize) > fileBuf.size())
      errorMessage("invalid packed data offset or size");
    fileData = p;
    return unpackedSize;
  }

  buf.reserve(unpackedSize);
  if (archiveType == 1 || archiveType == 2)
    (void) extractBA2Texture(buf, fd);
  else
    extractBlock(buf, unpackedSize, fd, p, packedSize);
  fileData = buf.data();
  return buf.size();
}

