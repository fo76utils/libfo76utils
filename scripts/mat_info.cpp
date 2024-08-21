
#include "common.hpp"
#include "ba2file.hpp"
#include "material.hpp"
#include "bsmatcdb.hpp"

#include "ba2file.cpp"
#include "bsmatcdb.cpp"
#include "bsrefl.cpp"
#include "common.cpp"
#include "filebuf.cpp"
#include "jsonread.cpp"
#include "matcomps.cpp"
#include "material.cpp"
#include "mat_dump.cpp"
#include "mat_json.cpp"
#include "mat_list.cpp"
#include "zlib.cpp"

#if defined(_WIN32) || defined(_WIN64)
#  include <direct.h>
#else
#  include <sys/stat.h>
#endif

static const char *usageString =
    "Usage: mat_info [OPTIONS...] ARCHIVEPATH [FILE1.MAT [FILE2.MAT...]]\n"
    "\n"
    "Options:\n"
    "    --help          print usage\n"
    "    --              remaining options are file names\n"
    "    -cdb FILENAME   set material database file name\n"
    "    -o FILENAME     set output file name (default: stdout)\n"
    "    -list FILENAME  read list of material paths from FILENAME\n"
    "    -all            use built-in list of all known material paths\n"
    "    -dump_list      print list of .mat paths (useful with -all)\n"
    "    -dump_db        dump all reflection data\n"
    "    -json           write JSON format .mat file(s)\n";

static inline char convertNameCharacter(unsigned char c)
{
  if (c >= 'A' && c <= 'Z')
    return char(c + ('a' - 'A'));
  if (c == '\\')
    return '/';
  if (c <= (unsigned char) ' ')
    return ' ';
  return char(c);
}

static void writeFileWithPath(const char *fileName, const std::string& buf)
{
  OutputFile  *f = (OutputFile *) 0;
  try
  {
    f = new OutputFile(fileName, 0);
  }
  catch (...)
  {
    std::string pathName;
    size_t  pathOffs = 0;
    while (true)
    {
      pathName = fileName;
      pathOffs = pathName.find('/', pathOffs);
      if (pathOffs == size_t(std::string::npos))
        break;
      pathName.resize(pathOffs);
      pathOffs++;
#if defined(_WIN32) || defined(_WIN64)
      (void) _mkdir(pathName.c_str());
#else
      (void) mkdir(pathName.c_str(), 0755);
#endif
    }
    f = new OutputFile(fileName, 0);
  }
  try
  {
    f->writeData(buf.c_str(), sizeof(char) * buf.length());
  }
  catch (...)
  {
    delete f;
    throw;
  }
  delete f;
}

static int dumpCDBFiles(
    const char *outFileName, const BA2File& ba2File, const char *fileName)
{
  if (!fileName || fileName[0] == '\0')
    fileName = BSReflStream::getDefaultMaterialDBPath();
  BA2File::UCharArray cdbBuf;
  std::string stringBuf;
  std::string errMsgBuf;
  while (true)
  {
    size_t  len = 0;
    while (fileName[len] != ',' && fileName[len] != '\0')
      len++;
    ba2File.extractFile(cdbBuf, std::string_view(fileName, len));
    BSReflDump  cdbFile(cdbBuf.data, cdbBuf.size);
    try
    {
      cdbFile.readAllChunks(stringBuf, 0, true);
    }
    catch (std::exception& e)
    {
      errMsgBuf = e.what();
      break;
    }
    if (!fileName[len])
      break;
    fileName = fileName + (len + 1);
  }
  if (!outFileName)
  {
    std::fwrite(stringBuf.c_str(), sizeof(char), stringBuf.length(), stdout);
  }
  else
  {
    std::FILE *outFile = std::fopen(outFileName, "w");
    if (!outFile)
      errorMessage("error opening output file");
    if (std::fwrite(stringBuf.c_str(), sizeof(char), stringBuf.length(),
                    outFile) != stringBuf.length())
    {
      std::fclose(outFile);
      errorMessage("error writing output file");
    }
    std::fclose(outFile);
  }
  if (!errMsgBuf.empty())
  {
    std::fprintf(stderr, "mat_info: %s\n", errMsgBuf.c_str());
    return 1;
  }
  return 0;
}

static void loadMaterialPaths(
    std::vector< std::string >& args, const CE2MaterialDB& materials,
    const std::vector< std::string >& includePatterns,
    const std::vector< std::string >& excludePatterns)
{
  std::set< std::string_view >  matPaths;
  for (size_t i = 1; i < args.size(); i++)
    matPaths.emplace(args[i]);
  args.resize(1);
  AllocBuffers  stringBuf;
  materials.getMaterialList(matPaths, stringBuf);
  for (const auto& i : matPaths)
  {
    if (includePatterns.begin() != includePatterns.end())
    {
      bool    foundMatch = false;
      for (const auto& j : includePatterns)
      {
        if (i.find(j) != std::string_view::npos)
        {
          foundMatch = true;
          break;
        }
      }
      if (!foundMatch)
        continue;
    }
    bool    foundMatch = true;
    for (const auto& j : excludePatterns)
    {
      if (i.find(j) != std::string_view::npos)
      {
        foundMatch = false;
        break;
      }
    }
    if (foundMatch)
      args.emplace_back(i);
  }
}

static bool archiveFilterFunction([[maybe_unused]] void *p, const std::string_view& s)
{
  return (s.ends_with(".cdb") || s.ends_with(".mat"));
}

int main(int argc, char **argv)
{
  const char  *outFileName = nullptr;
  std::FILE *outFile = nullptr;
  bool    extractMatList = false;
  // 0: default, 1: dump_db, 2: JSON, 3: dump_list
  signed char dumpMode = 0;
  try
  {
    std::vector< std::string >  args;
    std::vector< std::string >  includePatterns;
    std::vector< std::string >  excludePatterns;
    const char  *cdbFileName = nullptr;
    const char  *listFileName = nullptr;
    bool    noOptionsFlag = false;
    for (int i = 1; i < argc; i++)
    {
      if (noOptionsFlag || argv[i][0] != '-' || std::strcmp(argv[i], "--") == 0)
      {
        if (!noOptionsFlag)
        {
          noOptionsFlag = true;
          if (std::strcmp(argv[i], "--") == 0)
            continue;
        }
        std::string s(argv[i]);
        if (args.size() > 0)
        {
          for (size_t j = 0; j < s.length(); j++)
            s[j] = convertNameCharacter((unsigned char) s[j]);
        }
        if (args.size() < 1 || !extractMatList)
        {
          args.push_back(s);
          continue;
        }
        if (!s.starts_with("-x:"))
          includePatterns.push_back(s);
        else if (s.length() > 3)
          excludePatterns.emplace_back(s, 3);
        continue;
      }
      if (std::strcmp(argv[i], "-h") == 0 ||
          std::strcmp(argv[i], "--help") == 0)
      {
        std::printf("%s", usageString);
        return 0;
      }
      if (std::strcmp(argv[i], "-cdb") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        cdbFileName = argv[i];
      }
      else if (std::strcmp(argv[i], "-o") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        outFileName = argv[i];
      }
      else if (std::strcmp(argv[i], "-list") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        listFileName = argv[i];
      }
      else if (std::strcmp(argv[i], "-all") == 0)
      {
        extractMatList = true;
      }
      else if (std::strcmp(argv[i], "-dump_list") == 0)
      {
        dumpMode = 3;
      }
      else if (std::strcmp(argv[i], "-dump_db") == 0)
      {
        dumpMode = 1;
      }
      else if (std::strcmp(argv[i], "-json") == 0)
      {
        dumpMode = 2;
      }
      else
      {
        throw FO76UtilsError("invalid option: %s", argv[i]);
      }
    }
    if (args.size() < 1)
    {
      std::fprintf(stderr, "%s", usageString);
      return 1;
    }
    if (listFileName)
    {
      FileBuffer  listFile(listFileName);
      size_t  startPos = 0;
      for (size_t i = 0; i < listFile.size(); i++)
      {
        char    c = '\0';
        if (listFile[i] >= 0x20)
          c = convertNameCharacter(listFile[i]);
        if (c == '\0')
          startPos = i + 1;
        else if (i > startPos)
          args.back() += c;
        else
          args.emplace_back(&c, 1);
      }
    }
    BA2File ba2File(args[0].c_str(), &archiveFilterFunction,
                    (dumpMode < 0 ? args.data() : nullptr));
    if (dumpMode == 1)
    {
      return dumpCDBFiles(outFileName, ba2File, cdbFileName);
    }
    CE2MaterialDB materials;
    materials.loadArchives(ba2File);
    if (extractMatList)
      loadMaterialPaths(args, materials, includePatterns, excludePatterns);
    if (dumpMode > 0)
    {
      std::string stringBuf;
      std::string errMsgBuf;
      if (dumpMode == 2)
      {
        std::string tmpBuf;
        for (size_t i = 1; i < args.size(); i++)
        {
          if (args.size() > 2)
            std::printf("%s\n", args[i].c_str());
          if (ba2File.findFile(args[i]))
          {
            try
            {
              (void) materials.loadMaterial(args[i]);
            }
            catch (FO76UtilsError& e)
            {
              std::fprintf(stderr, "mat_info: %s: error loading material: %s\n",
                           args[i].c_str(), e.what());
              continue;
            }
          }
          materials.getJSONMaterial(tmpBuf, args[i]);
          if (tmpBuf.empty())
          {
            std::fprintf(stderr, "Warning: %s: material not found\n",
                         args[i].c_str());
          }
          else
          {
            tmpBuf += '\n';
            if (args.size() <= 2)
              stringBuf += tmpBuf;
            else
              writeFileWithPath(args[i].c_str(), tmpBuf);
          }
        }
      }
      else if (dumpMode == 3)
      {
        for (size_t i = 1; i < args.size(); i++)
          printToString(stringBuf, "%s\n", args[i].c_str());
      }
      if (!(dumpMode == 2 && args.size() > 2))
      {
        if (!outFileName)
        {
          std::fwrite(stringBuf.c_str(), sizeof(char), stringBuf.length(),
                      stdout);
        }
        else
        {
          outFile = std::fopen(outFileName, "w");
          if (!outFile)
            errorMessage("error opening output file");
          if (std::fwrite(stringBuf.c_str(), sizeof(char), stringBuf.length(),
                          outFile) != stringBuf.length())
          {
            errorMessage("error writing output file");
          }
          std::fclose(outFile);
          outFile = nullptr;
        }
      }
      if (!errMsgBuf.empty())
      {
        std::fprintf(stderr, "mat_info: %s\n", errMsgBuf.c_str());
        return 1;
      }
      return 0;
    }
    if (outFileName)
    {
      outFile = std::fopen(outFileName, "w");
      if (!outFile)
        errorMessage("error opening output file");
    }
    else
    {
      outFile = stdout;
    }
    std::string stringBuf;
    for (size_t i = 1; i < args.size(); i++)
    {
      const CE2Material *o;
      try
      {
        o = materials.loadMaterial(args[i]);
      }
      catch (FO76UtilsError& e)
      {
        std::fprintf(stderr, "mat_info: %s: error loading material: %s\n",
                     args[i].c_str(), e.what());
        continue;
      }
      if (!o)
      {
        std::fprintf(stderr, "Material '%s' not found in the database\n",
                     args[i].c_str());
        continue;
      }
      std::fprintf(outFile, "==== %s ====\n", args[i].c_str());
      stringBuf.clear();
      o->printObjectInfo(stringBuf, 0, false);
      std::fwrite(stringBuf.c_str(), sizeof(char), stringBuf.length(), outFile);
    }
  }
  catch (std::exception& e)
  {
    if (outFileName && outFile)
      std::fclose(outFile);
    std::fprintf(stderr, "mat_info: %s\n", e.what());
    return 1;
  }
  if (outFileName && outFile)
    std::fclose(outFile);
  return 0;
}

