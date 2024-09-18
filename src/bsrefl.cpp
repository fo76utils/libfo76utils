
#include "common.hpp"
#include "bsrefl.hpp"

void BSReflStream::readStringTable()
{
  if (fileBufSize < 24 ||
      readUInt64() != 0x0000000848544542ULL)            // "BETH", 8
  {
    errorMessage("invalid reflection stream header");
  }
  if (readUInt32Fast() != 4U)
    errorMessage("unsupported reflection stream version");
  chunksRemaining = readUInt32Fast();
  if (chunksRemaining < 2U || readUInt32Fast() != ChunkType_STRT)
    errorMessage("missing string table in reflection stream");
  chunksRemaining = chunksRemaining - 2U;
  // create string table
  unsigned int  n = readUInt32Fast();
  if ((filePos + std::uint64_t(n)) > fileBufSize)
    errorMessage("unexpected end of reflection stream");
  stringMap.resize(n, std::int16_t(-1));
  for ( ; n; n--)
  {
    unsigned int  strtOffs = (unsigned int) (filePos - 24);
    const char  *s = reinterpret_cast< const char * >(fileBuf + filePos);
    for ( ; readUInt8Fast(); n--)
    {
      if (!n)
        errorMessage("string table is not terminated in reflection stream");
      stringMap[strtOffs] = std::int16_t(findString(s));
      if (stringMap[strtOffs] < 0)
      {
        std::fprintf(stderr,
                     "Warning: unrecognized string in reflection stream: "
                     "'%s'\n", s);
      }
    }
  }
}

int BSReflStream::findString(const char *s)
{
  size_t  n0 = 19;
  size_t  n2 = sizeof(stringTable) / sizeof(char *);
  while (n2 > (n0 + 1))
  {
    size_t  n1 = (n0 + n2) >> 1;
    if (std::strcmp(s, stringTable[n1]) < 0)
      n2 = n1;
    else
      n0 = n1;
  }
  if (n2 > n0 && std::strcmp(s, stringTable[n0]) == 0)
    return int(n0);
  return -1;
}

std::uint32_t BSReflStream::findString(unsigned int strtOffs) const
{
  if (strtOffs < stringMap.size())
  {
    int     n = stringMap[strtOffs];
    if (n >= 0)
      return size_t(n);
  }
  unsigned int  n = strtOffs - 0xFFFFFF01U;
  return std::uint32_t(std::min(n, 18U));
}

bool BSReflStream::Chunk::readEnum(unsigned char& n, const char *t)
{
  if ((filePos + 2ULL) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
  unsigned int  len = readUInt16Fast();
  if ((filePos + std::uint64_t(len)) > fileBufSize) [[unlikely]]
  {
    filePos = fileBufSize;
    return false;
  }
  const char  *s = reinterpret_cast< const char * >(fileBuf + filePos);
  filePos = filePos + len;
  while (len > 0U && s[len - 1U] == '\0')
    len--;
  for (unsigned int i = 0U; *t; i++)
  {
    unsigned int  len2 = (unsigned char) *t;
    const char  *s2 = t + 1;
    t = s2 + len2;
    if (len2 != len)
      continue;
    for (unsigned int j = 0U; len2 && s2[j] == s[j]; j++, len2--)
      ;
    if (!len2)
    {
      n = (unsigned char) i;
      break;
    }
  }
  return true;
}

void BSReflDump::dumpItem(std::string& s, Chunk& chunkBuf, bool isDiff,
                          std::uint32_t itemType, int indentCnt)
{
  const CDBClassDef *classDef = nullptr;
  if (itemType > String_Unknown)
  {
    std::map< std::uint32_t, CDBClassDef >::const_iterator
        i = classes.find(itemType);
    if (i != classes.end()) [[likely]]
      classDef = &(i->second);
    else
      itemType = String_Unknown;
  }
  if (itemType > String_Unknown)
  {
    printToString(s, "{\n%*s\"Data\": {", indentCnt + 1, "");
    unsigned int  nMax = (unsigned int) classDef->fields.size();
    bool    firstField = true;
    if (classDef->isUser)
    {
      Chunk userBuf;
      unsigned int  userChunkType = readChunk(userBuf);
      if (userChunkType != ChunkType_USER && userChunkType != ChunkType_USRD)
      {
        throw FO76UtilsError("unexpected chunk type in reflection stream "
                             "at 0x%08x", (unsigned int) getPosition());
      }
      isDiff = (userChunkType == ChunkType_USRD);
      std::uint32_t className1 = findString(userBuf.readUInt32());
      if (className1 != itemType)
      {
        throw FO76UtilsError("USER chunk has unexpected type at 0x%08x",
                             (unsigned int) getPosition());
      }
      std::uint32_t className2 = findString(userBuf.readUInt32());
      if (className2 == className1)
      {
        // no type conversion
        nMax--;
        for (unsigned int n = 0U - 1U;
             userBuf.getFieldNumber(n, nMax, isDiff); )
        {
          printToString(s, (!firstField ? ",\n%*s\"%s\": " : "\n%*s\"%s\": "),
                        indentCnt + 2, "",
                        stringTable[classDef->fields[n].first]);
          dumpItem(s, userBuf, isDiff, classDef->fields[n].second,
                   indentCnt + 2);
          firstField = false;
        }
      }
      else if (className2 < String_Unknown)
      {
        unsigned int  n = 0U;
        do
        {
          const char  *fieldNameStr = "null";
          if (nMax) [[likely]]
            fieldNameStr = stringTable[classDef->fields[n].first];
          printToString(s, (!firstField ? ",\n%*s\"%s\": " : "\n%*s\"%s\": "),
                        indentCnt + 2, "", fieldNameStr);
          dumpItem(s, userBuf, isDiff, className2, indentCnt + 2);
          firstField = false;
          className2 = findString(userBuf.readUInt32());
        }
        while (++n < nMax && className2 < String_Unknown);
      }
    }
    else
    {
      nMax--;
      for (unsigned int n = 0U - 1U; chunkBuf.getFieldNumber(n, nMax, isDiff); )
      {
        printToString(s, (!firstField ? ",\n%*s\"%s\": " : "\n%*s\"%s\": "),
                      indentCnt + 2, "",
                      stringTable[classDef->fields[n].first]);
        dumpItem(s, chunkBuf, isDiff, classDef->fields[n].second,
                 indentCnt + 2);
        firstField = false;
      }
    }
    if (!s.ends_with('{'))
      printToString(s, "\n%*s", indentCnt + 1, "");
    printToString(s, "},\n%*s\"Type\": \"%s\"\n",
                  indentCnt + 1, "", stringTable[itemType]);
    printToString(s, "%*s}", indentCnt, "");
    return;
  }
  FileBuffer& buf2 = *(static_cast< FileBuffer * >(&chunkBuf));
  switch (itemType)
  {
    case String_None:
      printToString(s, "null");
      break;
    case String_String:
      {
        unsigned int  len = buf2.readUInt16();
        bool    endOfString = false;
        s += '"';
        while (len--)
        {
          char    c = char(buf2.readUInt8());
          if (!endOfString)
          {
            if (!c)
            {
              endOfString = true;
              continue;
            }
            if ((unsigned char) c < 0x20 || c == '"' || c == '\\')
            {
              s += '\\';
              switch (c)
              {
                case '\b':
                  c = 'b';
                  break;
                case '\t':
                  c = 't';
                  break;
                case '\n':
                  c = 'n';
                  break;
                case '\f':
                  c = 'f';
                  break;
                case '\r':
                  c = 'r';
                  break;
                default:
                  if ((unsigned char) c < 0x20)
                  {
                    printToString(s, "u%04X", (unsigned int) c);
                    continue;
                  }
                  break;
              }
            }
            s += c;
          }
        }
        s += '"';
      }
      break;
    case String_List:
      {
        Chunk listBuf;
        unsigned int  chunkType = readChunk(listBuf);
        if (chunkType != ChunkType_LIST)
        {
          throw FO76UtilsError("unexpected chunk type in reflection stream "
                               "at 0x%08x", (unsigned int) getPosition());
        }
        std::uint32_t elementType = findString(listBuf.readUInt32());
        std::uint32_t listSize = 0U;
        if ((listBuf.getPosition() + 4ULL) <= listBuf.size())
          listSize = listBuf.readUInt32();
        printToString(s, "{\n%*s\"Data\": [", indentCnt + 1, "");
        for (std::uint32_t i = 0U; i < listSize; i++)
        {
          printToString(s, "\n%*s", indentCnt + 2, "");
          dumpItem(s, listBuf, isDiff, elementType, indentCnt + 2);
          if ((i + 1U) < listSize)
            printToString(s, ",");
          else
            printToString(s, "\n%*s", indentCnt + 1, "");
        }
        printToString(s, "],\n");
        if (listSize)
        {
          const char  *elementTypeStr = stringTable[elementType];
          if (elementType && elementType < String_Ref)
          {
            elementTypeStr = (elementType == String_String ?
                              "BSFixedString" : "<collection>");
          }
          printToString(s, "%*s\"ElementType\": \"%s\",\n",
                        indentCnt + 1, "", elementTypeStr);
        }
        printToString(s, "%*s\"Type\": \"<collection>\"\n", indentCnt + 1, "");
        printToString(s, "%*s}", indentCnt, "");
      }
      break;
    case String_Map:
      {
        Chunk mapBuf;
        unsigned int  chunkType = readChunk(mapBuf);
        if (chunkType != ChunkType_MAPC)
        {
          throw FO76UtilsError("unexpected chunk type in reflection stream "
                               "at 0x%08x", (unsigned int) getPosition());
        }
        std::uint32_t keyClassName = findString(mapBuf.readUInt32());
        std::uint32_t valueClassName = findString(mapBuf.readUInt32());
        std::uint32_t mapSize = 0U;
        if ((mapBuf.getPosition() + 4ULL) <= mapBuf.size())
          mapSize = mapBuf.readUInt32();
        printToString(s, "{\n%*s\"Data\": [", indentCnt + 1, "");
        for (std::uint32_t i = 0U; i < mapSize; i++)
        {
          printToString(s, "\n%*s{", indentCnt + 2, "");
          printToString(s, "\n%*s\"Data\": {", indentCnt + 3, "");
          printToString(s, "\n%*s\"Key\": ", indentCnt + 4, "");
          dumpItem(s, mapBuf, false, keyClassName, indentCnt + 4);
          printToString(s, ",\n%*s\"Value\": ", indentCnt + 4, "");
          dumpItem(s, mapBuf, isDiff, valueClassName, indentCnt + 4);
          printToString(s, "\n%*s},", indentCnt + 3, "");
          printToString(s, "\n%*s\"Type\": \"StdMapType::Pair\"\n",
                        indentCnt + 3, "");
          printToString(s, "%*s}", indentCnt + 2, "");
          if ((i + 1U) < mapSize)
            printToString(s, ",");
          else
            printToString(s, "\n%*s", indentCnt + 1, "");
        }
        printToString(s, "],\n%*s\"ElementType\": \"StdMapType::Pair\",\n",
                      indentCnt + 1, "");
        printToString(s, "%*s\"Type\": \"<collection>\"\n", indentCnt + 1, "");
        printToString(s, "%*s}", indentCnt, "");
      }
      break;
    case String_Ref:
      {
        std::uint32_t refType = findString(buf2.readUInt32());
        printToString(s, "{\n%*s\"Data\": ", indentCnt + 1, "");
        dumpItem(s, chunkBuf, isDiff, refType, indentCnt + 1);
        printToString(s, ",\n%*s\"Type\": \"<ref>\"\n", indentCnt + 1, "");
        printToString(s, "%*s}", indentCnt, "");
      }
      break;
    case String_Int8:
      printToString(s, "%d", int(std::int8_t(buf2.readUInt8())));
      break;
    case String_UInt8:
      printToString(s, "%u", (unsigned int) buf2.readUInt8());
      break;
    case String_Int16:
      printToString(s, "%d", int(std::int16_t(buf2.readUInt16())));
      break;
    case String_UInt16:
      printToString(s, "%u", (unsigned int) buf2.readUInt16());
      break;
    case String_Int32:
      printToString(s, "%d", int(buf2.readInt32()));
      break;
    case String_UInt32:
      printToString(s, "%u", (unsigned int) buf2.readUInt32());
      break;
    case String_Int64:
      printToString(s, "\"%lld\"", (long long) std::int64_t(buf2.readUInt64()));
      break;
    case String_UInt64:
      printToString(s, "\"%llu\"", (unsigned long long) buf2.readUInt64());
      break;
    case String_Bool:
      printToString(s, "%s", (!buf2.readUInt8() ? "false" : "true"));
      break;
    case String_Float:
      printToString(s, "%.7g", buf2.readFloat());
      break;
    case String_Double:
      // FIXME: implement this in a portable way
      printToString(s, "%.14g",
                    std::bit_cast< double, std::uint64_t >(buf2.readUInt64()));
      break;
    default:
      printToString(s, "<unknown>");
      chunkBuf.setPosition(chunkBuf.size());
      break;
  }
}

void BSReflDump::readAllChunks(std::string& s, int indentCnt, bool verboseMode)
{
  Chunk   chunkBuf;
  unsigned int  chunkType;
  bool    isArray = false;
  bool    firstObject = true;
  size_t  objectCnt = 0;
  for (size_t i = filePos; (i + 8ULL) <= fileBufSize; )
  {
    chunkType = FileBuffer::readUInt32Fast(fileBuf + i);
    size_t  chunkSize = FileBuffer::readUInt32Fast(fileBuf + (i + 4));
    if (chunkType == ChunkType_DIFF || chunkType == ChunkType_OBJT ||
        (verboseMode && chunkType == ChunkType_CLAS))
    {
      if (++objectCnt > 1)
      {
        isArray = true;
        break;
      }
    }
    if ((i + (std::uint64_t(chunkSize) + 8ULL)) >= fileBufSize)
      break;
    i = i + (chunkSize + 8);
  }
  if (isArray)
  {
    indentCnt++;
    printToString(s, "{ \"Objects\": [\n%*s", indentCnt, "");
  }
  while ((chunkType = readChunk(chunkBuf)) != 0U)
  {
    if (chunkType == ChunkType_CLAS)
    {
      std::uint32_t className = findString(chunkBuf.readUInt32());
      if (className < String_Unknown)
        errorMessage("invalid class ID in reflection stream");
      unsigned int  classVersion = chunkBuf.readUInt32();
      unsigned int  classFlags = chunkBuf.readUInt16();
      (void) chunkBuf.readUInt16();     // number of fields
      unsigned int  fieldCnt = 0U;
      if (verboseMode)
      {
        if (!firstObject)
          printToString(s, ",\n%*s", indentCnt, "");
        firstObject = false;
        printToString(s, "{\n%*s\"Fields\": [", indentCnt + 1, "");
      }
      CDBClassDef&  classDef = classes[className];
      classDef.isUser = bool(classFlags & 4U);
      for ( ; (chunkBuf.getPosition() + 12ULL) <= chunkBuf.size(); fieldCnt++)
      {
        std::uint32_t fieldName = findString(chunkBuf.readUInt32Fast());
        if (fieldName < String_Unknown)
        {
          errorMessage("invalid field name in class definition "
                       "in reflection stream");
        }
        std::uint32_t fieldType = findString(chunkBuf.readUInt32Fast());
        unsigned int  dataOffset = chunkBuf.readUInt16Fast();
        unsigned int  dataSize = chunkBuf.readUInt16Fast();
        if (verboseMode)
        {
          printToString(s, (!fieldCnt ? "\n%*s" : ",\n%*s"), indentCnt + 2, "");
          printToString(s, "{\n%*s\"Name\": \"%s\",",
                        indentCnt + 3, "", stringTable[fieldName]);
          printToString(s, "\n%*s\"Offset\": %u,",
                        indentCnt + 3, "", dataOffset);
          printToString(s, "\n%*s\"Size\": %u,", indentCnt + 3, "", dataSize);
          printToString(s, "\n%*s\"Type\": \"%s\"",
                        indentCnt + 3, "", stringTable[fieldType]);
          printToString(s, "\n%*s}", indentCnt + 2, "");
        }
        classDef.fields.emplace_back(fieldName, fieldType);
      }
      if (verboseMode)
      {
        if (fieldCnt)
          printToString(s, "\n%*s", indentCnt + 1, "");
        printToString(s, "],\n%*s\"Flags\": %u,",
                      indentCnt + 1, "", classFlags);
        printToString(s, "\n%*s\"Name\": \"%s\",",
                      indentCnt + 1, "", stringTable[className]);
        printToString(s, "\n%*s\"Type\": \"<class>\",", indentCnt + 1, "");
        printToString(s, "\n%*s\"Version\": %u",
                      indentCnt + 1, "", classVersion);
        printToString(s, "\n%*s}", indentCnt, "");
      }
      continue;
    }
    if (chunkType == ChunkType_DIFF || chunkType == ChunkType_OBJT) [[likely]]
    {
      if (!firstObject)
        printToString(s, ",\n%*s", indentCnt, "");
      std::uint32_t className = findString(chunkBuf.readUInt32());
      bool    isDiff = (chunkType == ChunkType_DIFF);
      dumpItem(s, chunkBuf, isDiff, className, indentCnt);
      firstObject = false;
      continue;
    }
    if (chunkType != ChunkType_TYPE)
    {
      throw FO76UtilsError("unexpected reflection stream chunk type 0x%08X",
                           chunkType);
    }
  }
  if (!firstObject)
    printToString(s, "\n");
  if (isArray)
  {
    indentCnt--;
    if (s.ends_with('\n'))
      printToString(s, "%*s] }\n", indentCnt, "");
    else
      printToString(s, "] }\n");
  }
  bool    isIndent = true;
  for (size_t i = 0; i < s.length(); i++)
  {
    if (s[i] == ' ' && isIndent)
      s[i] = '\t';
    else
      isIndent = (s[i] == '\n');
  }
}

const char * BSReflStream::stringTable[1156] =
{
  "null",                                               //    0
  "String",                                             //    1
  "List",                                               //    2
  "Map",                                                //    3
  "<ref>",                                              //    4
  "Unknown_-250",                                       //    5
  "Unknown_-249",                                       //    6
  "int8_t",                                             //    7
  "uint8_t",                                            //    8
  "int16_t",                                            //    9
  "uint16_t",                                           //   10
  "int32_t",                                            //   11
  "uint32_t",                                           //   12
  "int64_t",                                            //   13
  "uint64_t",                                           //   14
  "bool",                                               //   15
  "float",                                              //   16
  "double",                                             //   17
  "<unknown>",                                          //   18
  "AOVertexColorChannel",                               //   19
  "Absolute",                                           //   20
  "ActiveLayersMask",                                   //   21
  "ActorValueBindings",                                 //   22
  "ActorValueSnapshot",                                 //   23
  "ActorValueSnapshot::ActorValueBinding",              //   24
  "AdaptSpeedDown",                                     //   25
  "AdaptSpeedUp",                                       //   26
  "AdaptiveEmittance",                                  //   27
  "Address",                                            //   28
  "AerosolAbsorbtion",                                  //   29
  "AerosolDensity",                                     //   30
  "AerosolPhaseFunction",                               //   31
  "Albedo",                                             //   32
  "AlbedoPaletteTex",                                   //   33
  "AlbedoTint",                                         //   34
  "AlbedoTintColor",                                    //   35
  "AllowReapply",                                       //   36
  "Alpha",                                              //   37
  "AlphaAdd",                                           //   38
  "AlphaBias",                                          //   39
  "AlphaChannel",                                       //   40
  "AlphaDistance",                                      //   41
  "AlphaMultiply",                                      //   42
  "AlphaPaletteTex",                                    //   43
  "AlphaTestThreshold",                                 //   44
  "AlphaTint",                                          //   45
  "AmbientOcclusion",                                   //   46
  "Anchor",                                             //   47
  "AnimatedDecalIgnoresTAA",                            //   48
  "Anisotropy",                                         //   49
  "Aperture",                                           //   50
  "ApplyExternalEmittanceToAllMaterials",               //   51
  "ApplyFlowOnANMR",                                    //   52
  "ApplyFlowOnEmissivity",                              //   53
  "ApplyFlowOnOpacity",                                 //   54
  "ApplyNodeRotation",                                  //   55
  "ApplyToAllChildren",                                 //   56
  "AssetDataA",                                         //   57
  "AssetName",                                          //   58
  "AtmosphereType",                                     //   59
  "AttachSoundToRefr",                                  //   60
  "AttachType",                                         //   61
  "Attachment",                                         //   62
  "AutoExposure",                                       //   63
  "AutoPlay",                                           //   64
  "BGSAtmosphere",                                      //   65
  "BGSAtmosphere::AtmosphereSettings",                  //   66
  "BGSAtmosphere::CelestialBodySettings",               //   67
  "BGSAtmosphere::MiscSettings",                        //   68
  "BGSAtmosphere::OverrideSettings",                    //   69
  "BGSAtmosphere::ScatteringSettings",                  //   70
  "BGSAtmosphere::ScatteringSettings::MieSettings",     //   71
  "BGSAtmosphere::ScatteringSettings::RayleighSettings", //   72
  "BGSAtmosphere::StarSettings",                        //   73
  "BGSAudio::WwiseGUID",                                //   74
  "BGSAudio::WwiseSoundHook",                           //   75
  "BGSCloudForm",                                       //   76
  "BGSCloudForm::CloudLayer",                           //   77
  "BGSCloudForm::CloudPlane",                           //   78
  "BGSCloudForm::ShadowParams",                         //   79
  "BGSCurve3DForm",                                     //   80
  "BGSCurveForm",                                       //   81
  "BGSEffectSequenceForm",                              //   82
  "BGSEffectSequenceFormComponent",                     //   83
  "BGSFogVolumeForm",                                   //   84
  "BGSForceData",                                       //   85
  "BGSFormFolderKeywordList",                           //   86
  "BGSLayeredMaterialSwap",                             //   87
  "BGSLayeredMaterialSwap::Entry",                      //   88
  "BGSLodOwnerComponent",                               //   89
  "BGSMaterialPathForm",                                //   90
  "BGSMaterialPropertyComponent",                       //   91
  "BGSMaterialPropertyComponent::Entry",                //   92
  "BGSParticleSystemDefineCollection",                  //   93
  "BGSSpacePhysicsFormComponent",                       //   94
  "BGSTimeOfDayData",                                   //   95
  "BGSVolumetricLighting",                              //   96
  "BGSVolumetricLightingSettings",                      //   97
  "BGSVolumetricLightingSettings::DistantLightingSettings", //   98
  "BGSVolumetricLightingSettings::ExteriorAndInteriorSettings", //   99
  "BGSVolumetricLightingSettings::ExteriorSettings",    //  100
  "BGSVolumetricLightingSettings::FogDensitySettings",  //  101
  "BGSVolumetricLightingSettings::FogMapSettings",      //  102
  "BGSVolumetricLightingSettings::FogThicknessSettings", //  103
  "BGSVolumetricLightingSettings::HorizonFogSettings",  //  104
  "BGSWeatherSettingsForm",                             //  105
  "BGSWeatherSettingsForm::ColorSettings",              //  106
  "BGSWeatherSettingsForm::FoliageSettings",            //  107
  "BGSWeatherSettingsForm::MagicEffect",                //  108
  "BGSWeatherSettingsForm::PrecipitationSettings",      //  109
  "BGSWeatherSettingsForm::SoundEffectSettings",        //  110
  "BGSWeatherSettingsForm::SpellEffect",                //  111
  "BGSWeatherSettingsForm::SpellSettings",              //  112
  "BGSWeatherSettingsForm::WeatherChoiceSettings",      //  113
  "BGSWeatherSettingsForm::WeatherSound",               //  114
  "BSAttachConfig::ArtObjectAttach",                    //  115
  "BSAttachConfig::AttachmentConfiguration",            //  116
  "BSAttachConfig::LensFlareAttachment",                //  117
  "BSAttachConfig::LightAttachment",                    //  118
  "BSAttachConfig::NodeAttachment",                     //  119
  "BSAttachConfig::NodeName",                           //  120
  "BSAttachConfig::ObjectAttachment",                   //  121
  "BSAttachConfig::ParticleAttachment",                 //  122
  "BSAttachConfig::SearchRootNode",                     //  123
  "BSAttachConfig::SearchSingleNameSingleNode",         //  124
  "BSAttachConfig::SoundAttachment",                    //  125
  "BSBind::Address",                                    //  126
  "BSBind::ColorCurveController",                       //  127
  "BSBind::ColorLerpController",                        //  128
  "BSBind::ComponentProperty",                          //  129
  "BSBind::ControllerComponent",                        //  130
  "BSBind::Controllers",                                //  131
  "BSBind::Controllers::Mapping",                       //  132
  "BSBind::Directory",                                  //  133
  "BSBind::DirectoryComponent",                         //  134
  "BSBind::Float2DCurveController",                     //  135
  "BSBind::Float2DLerpController",                      //  136
  "BSBind::Float3DCurveController",                     //  137
  "BSBind::Float3DLerpController",                      //  138
  "BSBind::FloatCurveController",                       //  139
  "BSBind::FloatLerpController",                        //  140
  "BSBind::Multiplex",                                  //  141
  "BSBind::Snapshot",                                   //  142
  "BSBind::Snapshot::Entry",                            //  143
  "BSBind::TimerController",                            //  144
  "BSBlendable::ColorValue",                            //  145
  "BSBlendable::FloatValue",                            //  146
  "BSColorCurve",                                       //  147
  "BSComponentDB2::DBFileIndex",                        //  148
  "BSComponentDB2::DBFileIndex::ComponentInfo",         //  149
  "BSComponentDB2::DBFileIndex::ComponentTypeInfo",     //  150
  "BSComponentDB2::DBFileIndex::EdgeInfo",              //  151
  "BSComponentDB2::DBFileIndex::ObjectInfo",            //  152
  "BSComponentDB2::ID",                                 //  153
  "BSComponentDB::CTName",                              //  154
  "BSFloat2DCurve",                                     //  155
  "BSFloat3DCurve",                                     //  156
  "BSFloatCurve",                                       //  157
  "BSFloatCurve::Control",                              //  158
  "BSGalaxy::BGSSunPresetForm",                         //  159
  "BSGalaxy::BGSSunPresetForm::DawnDuskSettings",       //  160
  "BSGalaxy::BGSSunPresetForm::NightSettings",          //  161
  "BSHoudini::HoudiniAssetData",                        //  162
  "BSHoudini::HoudiniAssetData::Parameter",             //  163
  "BSMaterial::AlphaBlenderSettings",                   //  164
  "BSMaterial::AlphaSettingsComponent",                 //  165
  "BSMaterial::BlendModeComponent",                     //  166
  "BSMaterial::BlendParamFloat",                        //  167
  "BSMaterial::BlenderID",                              //  168
  "BSMaterial::Channel",                                //  169
  "BSMaterial::CollisionComponent",                     //  170
  "BSMaterial::Color",                                  //  171
  "BSMaterial::ColorChannelTypeComponent",              //  172
  "BSMaterial::ColorRemapSettingsComponent",            //  173
  "BSMaterial::DecalSettingsComponent",                 //  174
  "BSMaterial::DetailBlenderSettings",                  //  175
  "BSMaterial::DetailBlenderSettingsComponent",         //  176
  "BSMaterial::DistortionComponent",                    //  177
  "BSMaterial::EffectSettingsComponent",                //  178
  "BSMaterial::EmissiveSettingsComponent",              //  179
  "BSMaterial::EmittanceSettings",                      //  180
  "BSMaterial::EyeSettingsComponent",                   //  181
  "BSMaterial::FlipbookComponent",                      //  182
  "BSMaterial::FlowSettingsComponent",                  //  183
  "BSMaterial::GlobalLayerDataComponent",               //  184
  "BSMaterial::GlobalLayerNoiseSettings",               //  185
  "BSMaterial::HairSettingsComponent",                  //  186
  "BSMaterial::Internal::CompiledDB",                   //  187
  "BSMaterial::Internal::CompiledDB::FilePair",         //  188
  "BSMaterial::LODMaterialID",                          //  189
  "BSMaterial::LayerID",                                //  190
  "BSMaterial::LayeredEdgeFalloffComponent",            //  191
  "BSMaterial::LayeredEmissivityComponent",             //  192
  "BSMaterial::LevelOfDetailSettings",                  //  193
  "BSMaterial::MRTextureFile",                          //  194
  "BSMaterial::MaterialID",                             //  195
  "BSMaterial::MaterialOverrideColorTypeComponent",     //  196
  "BSMaterial::MaterialParamFloat",                     //  197
  "BSMaterial::MipBiasSetting",                         //  198
  "BSMaterial::MouthSettingsComponent",                 //  199
  "BSMaterial::Offset",                                 //  200
  "BSMaterial::OpacityComponent",                       //  201
  "BSMaterial::ParamBool",                              //  202
  "BSMaterial::PhysicsMaterialType",                    //  203
  "BSMaterial::ProjectedDecalSettings",                 //  204
  "BSMaterial::Scale",                                  //  205
  "BSMaterial::ShaderModelComponent",                   //  206
  "BSMaterial::ShaderRouteComponent",                   //  207
  "BSMaterial::SourceTextureWithReplacement",           //  208
  "BSMaterial::StarmapBodyEffectComponent",             //  209
  "BSMaterial::TerrainSettingsComponent",               //  210
  "BSMaterial::TerrainTintSettingsComponent",           //  211
  "BSMaterial::TextureAddressModeComponent",            //  212
  "BSMaterial::TextureFile",                            //  213
  "BSMaterial::TextureReplacement",                     //  214
  "BSMaterial::TextureResolutionSetting",               //  215
  "BSMaterial::TextureSetID",                           //  216
  "BSMaterial::TextureSetKindComponent",                //  217
  "BSMaterial::TranslucencySettings",                   //  218
  "BSMaterial::TranslucencySettingsComponent",          //  219
  "BSMaterial::UVStreamID",                             //  220
  "BSMaterial::UVStreamParamBool",                      //  221
  "BSMaterial::VegetationSettingsComponent",            //  222
  "BSMaterial::WaterFoamSettingsComponent",             //  223
  "BSMaterial::WaterGrimeSettingsComponent",            //  224
  "BSMaterial::WaterSettingsComponent",                 //  225
  "BSMaterialBinding::MaterialPropertyNode",            //  226
  "BSMaterialBinding::MaterialUVStreamPropertyNode",    //  227
  "BSResource::ID",                                     //  228
  "BSSequence::AnimationEvent",                         //  229
  "BSSequence::AnimationTrack",                         //  230
  "BSSequence::CameraShakeEvent",                       //  231
  "BSSequence::CameraShakeStrengthTrack",               //  232
  "BSSequence::CameraShakeTrack",                       //  233
  "BSSequence::ColorCurveEvent",                        //  234
  "BSSequence::ColorLerpEvent",                         //  235
  "BSSequence::ColorTriggerEvent",                      //  236
  "BSSequence::CullEvent",                              //  237
  "BSSequence::DissolveEvent",                          //  238
  "BSSequence::DissolveFrequencyScaleTrack",            //  239
  "BSSequence::DissolveOffsetTrack",                    //  240
  "BSSequence::DissolveTrack",                          //  241
  "BSSequence::ExplosionObjectSpawn",                   //  242
  "BSSequence::Float2LerpEvent",                        //  243
  "BSSequence::Float2TriggerEvent",                     //  244
  "BSSequence::FloatCurveEvent",                        //  245
  "BSSequence::FloatLerpEvent",                         //  246
  "BSSequence::FloatNoiseEvent",                        //  247
  "BSSequence::FloatTriggerEvent",                      //  248
  "BSSequence::ImageSpaceLifetimeEvent",                //  249
  "BSSequence::ImageSpaceStrengthTrack",                //  250
  "BSSequence::ImageSpaceTrack",                        //  251
  "BSSequence::ImpactEffectEvent",                      //  252
  "BSSequence::ImpactEffectTrack",                      //  253
  "BSSequence::LightColorTrack",                        //  254
  "BSSequence::LightEffectReferenceTrack",              //  255
  "BSSequence::LightEffectTrack",                       //  256
  "BSSequence::LightIntensityTrack",                    //  257
  "BSSequence::LightLensFlareVisiblityTrack",           //  258
  "BSSequence::LightRadiusTrack",                       //  259
  "BSSequence::LightSpawnEvent",                        //  260
  "BSSequence::LoopMarker",                             //  261
  "BSSequence::MaterialFlipbookIndexGeneratorEvent",    //  262
  "BSSequence::MaterialFlipbookIndexTrack",             //  263
  "BSSequence::MaterialPropertyTrack",                  //  264
  "BSSequence::MaterialTrack",                          //  265
  "BSSequence::NoteEvent",                              //  266
  "BSSequence::NoteTrack",                              //  267
  "BSSequence::ObjectAttachmentTrack",                  //  268
  "BSSequence::ObjectSpawnEvent",                       //  269
  "BSSequence::ObjectSpawnTrack",                       //  270
  "BSSequence::ParticleEffectReferenceTrack",           //  271
  "BSSequence::ParticleEffectTrack",                    //  272
  "BSSequence::ParticleEvent",                          //  273
  "BSSequence::ParticleParameterTrack",                 //  274
  "BSSequence::PlaySubSequenceEvent",                   //  275
  "BSSequence::PositionTrack",                          //  276
  "BSSequence::ProjectedDecalAlphaTrack",               //  277
  "BSSequence::ProjectedDecalSpawnEvent",               //  278
  "BSSequence::ProjectedDecalTrack",                    //  279
  "BSSequence::PropertyControllerEvent",                //  280
  "BSSequence::PropertyLerpControllerEvent",            //  281
  "BSSequence::ReferenceSpawnEvent",                    //  282
  "BSSequence::RevertMaterialOverrideEvent",            //  283
  "BSSequence::RotationTrack",                          //  284
  "BSSequence::ScaleTrack",                             //  285
  "BSSequence::SceneNodeTrack",                         //  286
  "BSSequence::Sequence",                               //  287
  "BSSequence::SetPropertyEvent",                       //  288
  "BSSequence::SoundEvent",                             //  289
  "BSSequence::SoundTrack",                             //  290
  "BSSequence::SubSequenceTrack",                       //  291
  "BSSequence::TrackGroup",                             //  292
  "BSSequence::TriggerMaterialSwap",                    //  293
  "BSSequence::VectorCurveEvent",                       //  294
  "BSSequence::VectorLerpEvent",                        //  295
  "BSSequence::VectorNoiseEvent",                       //  296
  "BSSequence::VectorTriggerEvent",                     //  297
  "BSSequence::VisibilityTrack",                        //  298
  "BSTRange<float>",                                    //  299
  "BSTRange<uint32_t>",                                 //  300
  "BackLightingEnable",                                 //  301
  "BackLightingTintColor",                              //  302
  "BacklightingScale",                                  //  303
  "BacklightingSharpness",                              //  304
  "BacklightingTransparencyFactor",                     //  305
  "BackscatterStrength",                                //  306
  "BackscatterWrap",                                    //  307
  "BaseDistance",                                       //  308
  "BaseFrequency",                                      //  309
  "BeginVisualEffect",                                  //  310
  "Bias",                                               //  311
  "Binding",                                            //  312
  "BindingType",                                        //  313
  "BlendAmount",                                        //  314
  "BlendContrast",                                      //  315
  "BlendMaskContrast",                                  //  316
  "BlendMaskPosition",                                  //  317
  "BlendMode",                                          //  318
  "BlendNormalsAdditively",                             //  319
  "BlendPosition",                                      //  320
  "BlendSoftness",                                      //  321
  "Blender",                                            //  322
  "BlendingMode",                                       //  323
  "BlockClothWindIfInterior",                           //  324
  "Bloom",                                              //  325
  "BloomRangeScale",                                    //  326
  "BloomScale",                                         //  327
  "BloomThresholdOffset",                               //  328
  "BlueChannel",                                        //  329
  "Blur",                                               //  330
  "BlurRadiusValue",                                    //  331
  "BlurStrength",                                       //  332
  "BottomBlendDistanceKm",                              //  333
  "BottomBlendStartKm",                                 //  334
  "BranchFlexibility",                                  //  335
  "Brightness",                                         //  336
  "BuildVersion",                                       //  337
  "CameraDistanceFade",                                 //  338
  "CameraExposure",                                     //  339
  "CameraExposureMode",                                 //  340
  "Category",                                           //  341
  "CelestialBodies",                                    //  342
  "CelestialBodyIlluminanceScale",                      //  343
  "CelestialBodyIlluminanceScaleOverride",              //  344
  "CelestialBodyIndirectIlluminanceScaleOverride",      //  345
  "CenterXValue",                                       //  346
  "CenterYValue",                                       //  347
  "Channel",                                            //  348
  "Children",                                           //  349
  "Cinematic",                                          //  350
  "Circular",                                           //  351
  "Class",                                              //  352
  "ClassReference",                                     //  353
  "ClearDissolveOnEnd",                                 //  354
  "CloudDirectLightingContribution",                    //  355
  "CloudIndirectLightingContribution",                  //  356
  "Collapsed",                                          //  357
  "Collisions",                                         //  358
  "Color",                                              //  359
  "ColorAtTime",                                        //  360
  "ColorGradingAmount",                                 //  361
  "ColorGradingTexture",                                //  362
  "ColorTexture",                                       //  363
  "Colors",                                             //  364
  "Columns",                                            //  365
  "ComponentIndex",                                     //  366
  "ComponentType",                                      //  367
  "ComponentTypes",                                     //  368
  "Components",                                         //  369
  "Config",                                             //  370
  "ConfigurableLODData<3, 3>",                          //  371
  "ConfigurableLODData<4, 4>",                          //  372
  "ContactShadowSoftening",                             //  373
  "Contrast",                                           //  374
  "Controller",                                         //  375
  "Controllers",                                        //  376
  "Controls",                                           //  377
  "CorneaEyeRoughness",                                 //  378
  "CorneaSpecularity",                                  //  379
  "Coverage",                                           //  380
  "Culled",                                             //  381
  "Curve",                                              //  382
  "CurveType",                                          //  383
  "DBID",                                               //  384
  "DEPRECATEDTerrainBlendGradientFactor",               //  385
  "DEPRECATEDTerrainBlendStrength",                     //  386
  "DecalSize",                                          //  387
  "DefaultValue",                                       //  388
  "DefineOverridesMap",                                 //  389
  "Defines",                                            //  390
  "DelayUntilAllTextureDependenciesReady",              //  391
  "Density",                                            //  392
  "DensityDistanceExponent",                            //  393
  "DensityFullDistance",                                //  394
  "DensityNoiseBias",                                   //  395
  "DensityNoiseScale",                                  //  396
  "DensityStartDistance",                               //  397
  "DepolarizationFactor",                               //  398
  "DepthBiasInUlp",                                     //  399
  "DepthMVFixup",                                       //  400
  "DepthMVFixupEdgesOnly",                              //  401
  "DepthOfField",                                       //  402
  "DepthOffsetMaskVertexColorChannel",                  //  403
  "DepthScale",                                         //  404
  "DesiredLODLevelCount",                               //  405
  "DetailBlendMask",                                    //  406
  "DetailBlendMaskUVStream",                            //  407
  "DetailBlenderSettings",                              //  408
  "DiffuseTransmissionScale",                           //  409
  "DigitMask",                                          //  410
  "Dir",                                                //  411
  "DirectTransmissionScale",                            //  412
  "Direction",                                          //  413
  "DirectionalColor",                                   //  414
  "DirectionalIlluminance",                             //  415
  "DirectionalLightIlluminanceOverride",                //  416
  "DirectionalityIntensity",                            //  417
  "DirectionalitySaturation",                           //  418
  "DirectionalityScale",                                //  419
  "DisableMipBiasHint",                                 //  420
  "DisableSimulatedVisibility",                         //  421
  "DisplacementMidpoint",                               //  422
  "DisplayName",                                        //  423
  "DisplayTypeName",                                    //  424
  "Dissolve",                                           //  425
  "DistanceKm",                                         //  426
  "DistantLODs",                                        //  427
  "DistantLighting",                                    //  428
  "DitherDistanceMax",                                  //  429
  "DitherDistanceMin",                                  //  430
  "DitherScale",                                        //  431
  "DoubleVisionStrengthValue",                          //  432
  "DownStartValue",                                     //  433
  "Duration",                                           //  434
  "DuskDawnPreset",                                     //  435
  "DynamicState",                                       //  436
  "Easing",                                             //  437
  "Edge",                                               //  438
  "EdgeEffectCoefficient",                              //  439
  "EdgeEffectExponent",                                 //  440
  "EdgeFalloffEnd",                                     //  441
  "EdgeFalloffStart",                                   //  442
  "EdgeMaskContrast",                                   //  443
  "EdgeMaskDistanceMax",                                //  444
  "EdgeMaskDistanceMin",                                //  445
  "EdgeMaskMin",                                        //  446
  "Edges",                                              //  447
  "EffectLighting",                                     //  448
  "EffectSequenceMap",                                  //  449
  "EffectSequenceMapMetadata",                          //  450
  "EffectSequenceMetadataID",                           //  451
  "EffectSequenceObjectMetadata",                       //  452
  "ElevationKm",                                        //  453
  "EmissiveClipThreshold",                              //  454
  "EmissiveMaskSourceBlender",                          //  455
  "EmissiveOnlyAutomaticallyApplied",                   //  456
  "EmissiveOnlyEffect",                                 //  457
  "EmissivePaletteTex",                                 //  458
  "EmissiveSourceLayer",                                //  459
  "EmissiveTint",                                       //  460
  "Emittance",                                          //  461
  "EnableAdaptiveLimits",                               //  462
  "EnableCompensationCurve",                            //  463
  "Enabled",                                            //  464
  "End",                                                //  465
  "EndVisualEffect",                                    //  466
  "Entries",                                            //  467
  "EventName",                                          //  468
  "Events",                                             //  469
  "Exposure",                                           //  470
  "ExposureCompensationCurve",                          //  471
  "ExposureMax",                                        //  472
  "ExposureMin",                                        //  473
  "ExposureOffset",                                     //  474
  "Ext",                                                //  475
  "Exterior",                                           //  476
  "ExteriorAndInterior",                                //  477
  "ExternalColorBindingName",                           //  478
  "ExternalLuminanceBindingName",                       //  479
  "FPS",                                                //  480
  "FadeColorValue",                                     //  481
  "FadeDistanceKm",                                     //  482
  "FadeStartKm",                                        //  483
  "FallbackToRoot",                                     //  484
  "FalloffStartAngle",                                  //  485
  "FalloffStartAngles",                                 //  486
  "FalloffStartOpacities",                              //  487
  "FalloffStartOpacity",                                //  488
  "FalloffStopAngle",                                   //  489
  "FalloffStopAngles",                                  //  490
  "FalloffStopOpacities",                               //  491
  "FalloffStopOpacity",                                 //  492
  "FarFadeValue",                                       //  493
  "FarOpacityValue",                                    //  494
  "FarPlaneValue",                                      //  495
  "FarStartValue",                                      //  496
  "File",                                               //  497
  "FileName",                                           //  498
  "First",                                              //  499
  "FirstBlenderIndex",                                  //  500
  "FirstBlenderMode",                                   //  501
  "FirstLayerIndex",                                    //  502
  "FirstLayerMaskIndex",                                //  503
  "FirstLayerTint",                                     //  504
  "FixedValue",                                         //  505
  "FlipBackFaceNormalsInViewSpace",                     //  506
  "FlowExtent",                                         //  507
  "FlowIsAnimated",                                     //  508
  "FlowMap",                                            //  509
  "FlowMapAndTexturesAreFlipbooks",                     //  510
  "FlowSourceUVChannel",                                //  511
  "FlowSpeed",                                          //  512
  "FlowUVOffset",                                       //  513
  "FlowUVScale",                                        //  514
  "FoamTextureDistortion",                              //  515
  "FoamTextureScrollSpeed",                             //  516
  "Fog",                                                //  517
  "FogDensity",                                         //  518
  "FogFar",                                             //  519
  "FogFarHigh",                                         //  520
  "FogMap",                                             //  521
  "FogMapContribution",                                 //  522
  "FogNear",                                            //  523
  "FogNearHigh",                                        //  524
  "FogScaleValue",                                      //  525
  "FogThickness",                                       //  526
  "Foliage",                                            //  527
  "ForceRenderBeforeClouds",                            //  528
  "ForceRenderBeforeOIT",                               //  529
  "ForceStopOnDetach",                                  //  530
  "ForceStopSound",                                     //  531
  "FormFolderPath",                                     //  532
  "FrequencyMultiplier",                                //  533
  "Frosting",                                           //  534
  "FrostingBlurBias",                                   //  535
  "FrostingUnblurredBackgroundAlphaBlend",              //  536
  "GeoShareBehavior",                                   //  537
  "GlareColor",                                         //  538
  "GlobalLayerNoiseData",                               //  539
  "GreenChannel",                                       //  540
  "HDAComponentIdx",                                    //  541
  "HDAFilePath",                                        //  542
  "HDAVersion",                                         //  543
  "HableShoulderAngle",                                 //  544
  "HableShoulderLength",                                //  545
  "HableShoulderStrength",                              //  546
  "HableToeLength",                                     //  547
  "HableToeStrength",                                   //  548
  "HasData",                                            //  549
  "HasErrors",                                          //  550
  "HasOpacity",                                         //  551
  "HashMap",                                            //  552
  "Height",                                             //  553
  "HeightAboveTerrain",                                 //  554
  "HeightBlendFactor",                                  //  555
  "HeightBlendThreshold",                               //  556
  "HeightFalloffExponent",                              //  557
  "HeightKm",                                           //  558
  "HelpText",                                           //  559
  "HighFrequencyNoiseDensityScale",                     //  560
  "HighFrequencyNoiseScale",                            //  561
  "HighGUID",                                           //  562
  "HorizonFog",                                         //  563
  "HorizonScatteringBlendScale",                        //  564
  "HorizontalAngle",                                    //  565
  "HoudiniDataComponent",                               //  566
  "HurstExponent",                                      //  567
  "ID",                                                 //  568
  "ISO",                                                //  569
  "Id",                                                 //  570
  "IgnoreSmallHeadparts",                               //  571
  "IgnoreWeapons",                                      //  572
  "IgnoredBrightsPercentile",                           //  573
  "IgnoredDarksPercentile",                             //  574
  "IgnoresFog",                                         //  575
  "ImageSpaceSettings",                                 //  576
  "ImageSpaceSettings::AmbientOcclusionSettings",       //  577
  "ImageSpaceSettings::BloomSettings",                  //  578
  "ImageSpaceSettings::BlurSettings",                   //  579
  "ImageSpaceSettings::CinematicSettings",              //  580
  "ImageSpaceSettings::DepthOfFieldSettings",           //  581
  "ImageSpaceSettings::ExposureSettings",               //  582
  "ImageSpaceSettings::ExposureSettings::AutoExposureSettings", //  583
  "ImageSpaceSettings::ExposureSettings::CameraExposureSettings", //  584
  "ImageSpaceSettings::ExposureSettings::LuminanceHistogramSettings", //  585
  "ImageSpaceSettings::FogSettings",                    //  586
  "ImageSpaceSettings::IndirectLightingSettings",       //  587
  "ImageSpaceSettings::RadialBlurSettings",             //  588
  "ImageSpaceSettings::SunAndSkySettings",              //  589
  "ImageSpaceSettings::ToneMappingSettings",            //  590
  "ImageSpaceSettings::VolumetricLightingSettings",     //  591
  "Index",                                              //  592
  "IndirectDiffuseMultiplier",                          //  593
  "IndirectLighting",                                   //  594
  "IndirectLightingSkyScaleOverride",                   //  595
  "IndirectLightingSkyTargetEv100",                     //  596
  "IndirectLightingSkyTargetStrength",                  //  597
  "IndirectSpecRoughness",                              //  598
  "IndirectSpecularMultiplier",                         //  599
  "IndirectSpecularScale",                              //  600
  "IndirectSpecularTransmissionScale",                  //  601
  "InitialAngularVelocity",                             //  602
  "InitialAngularVelocityNoise",                        //  603
  "InitialLinearVelocity",                              //  604
  "InitialLinearVelocityNoise",                         //  605
  "InitialOffset",                                      //  606
  "InnerFadeDistance",                                  //  607
  "InorganicResources",                                 //  608
  "Input",                                              //  609
  "InputDistance",                                      //  610
  "Instances",                                          //  611
  "IntensityValue",                                     //  612
  "InterpretAs",                                        //  613
  "Interval",                                           //  614
  "IrisDepthPosition",                                  //  615
  "IrisDepthTransitionRatio",                           //  616
  "IrisSpecularity",                                    //  617
  "IrisTotalDepth",                                     //  618
  "IrisUVSize",                                         //  619
  "IsAFlipbook",                                        //  620
  "IsAlphaTested",                                      //  621
  "IsDecal",                                            //  622
  "IsDetailBlendMaskSupported",                         //  623
  "IsEmpty",                                            //  624
  "IsGlass",                                            //  625
  "IsMembrane",                                         //  626
  "IsPermanent",                                        //  627
  "IsPlanet",                                           //  628
  "IsProjected",                                        //  629
  "IsSampleInterpolating",                              //  630
  "IsSpikyHair",                                        //  631
  "IsTeeth",                                            //  632
  "Key",                                                //  633
  "KeyMaterialPathForm",                                //  634
  "Keywords",                                           //  635
  "KillSound",                                          //  636
  "LODMeshOverrides",                                   //  637
  "Label",                                              //  638
  "Layer",                                              //  639
  "LayerIndex",                                         //  640
  "Layers",                                             //  641
  "LeafAmplitude",                                      //  642
  "LeafFrequency",                                      //  643
  "LeafSecondaryMotionAmount",                          //  644
  "LeafSecondaryMotionCutOff",                          //  645
  "LensFlareAttachmentComponent",                       //  646
  "LensFlareAttachmentDefine",                          //  647
  "LensFlareCloudOcclusionStrength",                    //  648
  "LensFlareDefines",                                   //  649
  "LightAttachmentConfiguration",                       //  650
  "LightAttachmentDefine",                              //  651
  "LightAttachmentFormComponent",                       //  652
  "LightAttachmentRef",                                 //  653
  "LightLuminanceMultiplier",                           //  654
  "LightingPower",                                      //  655
  "LightingWrap",                                       //  656
  "LightningColor",                                     //  657
  "LightningDistanceMax",                               //  658
  "LightningDistanceMin",                               //  659
  "LightningFieldOfView",                               //  660
  "LightningStrikeEffect",                              //  661
  "Loop",                                               //  662
  "LoopSegment",                                        //  663
  "Loops",                                              //  664
  "LowGUID",                                            //  665
  "LowLOD",                                             //  666
  "LowLODRootMaterial",                                 //  667
  "LuminanceAtTime",                                    //  668
  "LuminanceHistogram",                                 //  669
  "LuminousEmittance",                                  //  670
  "Map",                                                //  671
  "MapMetadata",                                        //  672
  "MappingsA",                                          //  673
  "Mask",                                               //  674
  "MaskDistanceFromShoreEnd",                           //  675
  "MaskDistanceFromShoreStart",                         //  676
  "MaskDistanceRampWidth",                              //  677
  "MaskIntensityMax",                                   //  678
  "MaskIntensityMin",                                   //  679
  "MaskNoiseAmp",                                       //  680
  "MaskNoiseAnimSpeed",                                 //  681
  "MaskNoiseBias",                                      //  682
  "MaskNoiseFreq",                                      //  683
  "MaskNoiseGlobalScale",                               //  684
  "MaskWaveParallax",                                   //  685
  "Material",                                           //  686
  "MaterialMaskIntensityScale",                         //  687
  "MaterialOverallAlpha",                               //  688
  "MaterialPath",                                       //  689
  "MaterialProperty",                                   //  690
  "MaterialTypeOverride",                               //  691
  "Materials",                                          //  692
  "Max",                                                //  693
  "MaxConcentrationPlankton",                           //  694
  "MaxConcentrationSediment",                           //  695
  "MaxConcentrationYellowMatter",                       //  696
  "MaxDelay",                                           //  697
  "MaxDepthOffset",                                     //  698
  "MaxDisplacement",                                    //  699
  "MaxFogDensity",                                      //  700
  "MaxFogThickness",                                    //  701
  "MaxIndex",                                           //  702
  "MaxInput",                                           //  703
  "MaxMeanFreePath",                                    //  704
  "MaxOffsetEmittance",                                 //  705
  "MaxParralaxOcclusionSteps",                          //  706
  "MaxValue",                                           //  707
  "MeanFreePath",                                       //  708
  "MediumLODRootMaterial",                              //  709
  "MeshFileOverride",                                   //  710
  "MeshLODDistanceOverride",                            //  711
  "MeshLODs",                                           //  712
  "MeshOverrides",                                      //  713
  "Metadata",                                           //  714
  "Mie",                                                //  715
  "MieCoef",                                            //  716
  "Min",                                                //  717
  "MinDelay",                                           //  718
  "MinFogDensity",                                      //  719
  "MinFogThickness",                                    //  720
  "MinIndex",                                           //  721
  "MinInput",                                           //  722
  "MinMeanFreePath",                                    //  723
  "MinOffsetEmittance",                                 //  724
  "MinValue",                                           //  725
  "MipBase",                                            //  726
  "Misc",                                               //  727
  "Mode",                                               //  728
  "MoleculesPerUnitVolume",                             //  729
  "MoonGlare",                                          //  730
  "Moonlight",                                          //  731
  "MostSignificantLayer",                               //  732
  "MotionBlurStrengthValue",                            //  733
  "Name",                                               //  734
  "NavMeshAreaFlag",                                    //  735
  "NavMeshSplineExtraData::ChunkData",                  //  736
  "NavMeshSplineExtraData::ChunkDataRef",               //  737
  "NearFadeValue",                                      //  738
  "NearOpacityValue",                                   //  739
  "NearPlaneValue",                                     //  740
  "NearStartValue",                                     //  741
  "NestedTracks",                                       //  742
  "NiPoint3",                                           //  743
  "NightPreset",                                        //  744
  "NoHalfResOptimization",                              //  745
  "NoLAngularDamping",                                  //  746
  "NoLinearDamping",                                    //  747
  "NoSky",                                              //  748
  "Node",                                               //  749
  "NodeAttachmentConfiguration",                        //  750
  "NodeName",                                           //  751
  "Nodes",                                              //  752
  "NoiseBias",                                          //  753
  "NoiseContribution",                                  //  754
  "NoiseMaskTexture",                                   //  755
  "NoiseScale",                                         //  756
  "NoiseScrollingVelocity",                             //  757
  "NormalAffectsStrength",                              //  758
  "NormalOverride",                                     //  759
  "NormalShadowStrength",                               //  760
  "NormalTexture",                                      //  761
  "NumLODMaterials",                                    //  762
  "Object",                                             //  763
  "ObjectAttachmentConfiguration",                      //  764
  "ObjectID",                                           //  765
  "Objects",                                            //  766
  "Offset",                                             //  767
  "Op",                                                 //  768
  "OpacitySourceLayer",                                 //  769
  "OpacityTexture",                                     //  770
  "OpacityUVStream",                                    //  771
  "Optimized",                                          //  772
  "Order",                                              //  773
  "OuterFadeDistance",                                  //  774
  "OverallBlendAmount",                                 //  775
  "OverrideAttachType",                                 //  776
  "OverrideInitialVelocities",                          //  777
  "OverrideLevelCount",                                 //  778
  "OverrideMaterial",                                   //  779
  "OverrideMaterialPath",                               //  780
  "OverrideMeshLODDistance",                            //  781
  "OverrideSearchMethod",                               //  782
  "OverrideToneMapping",                                //  783
  "Overrides",                                          //  784
  "OzoneAbsorptionCoef",                                //  785
  "ParallaxOcclusionScale",                             //  786
  "ParallaxOcclusionShadows",                           //  787
  "ParamIndex",                                         //  788
  "ParameterName",                                      //  789
  "ParametersA",                                        //  790
  "Parent",                                             //  791
  "ParentPersistentID",                                 //  792
  "ParticleAttachmentConfiguration",                    //  793
  "ParticleFormComponent",                              //  794
  "ParticleRenderState",                                //  795
  "ParticleSystemDefine",                               //  796
  "ParticleSystemDefineOverrides",                      //  797
  "ParticleSystemDefineRef",                            //  798
  "ParticleSystemPath",                                 //  799
  "ParticleSystemReferenceDefine",                      //  800
  "Path",                                               //  801
  "PathStr",                                            //  802
  "PersistentID",                                       //  803
  "PhytoplanktonReflectanceColorB",                     //  804
  "PhytoplanktonReflectanceColorG",                     //  805
  "PhytoplanktonReflectanceColorR",                     //  806
  "PlacedWater",                                        //  807
  "Planes",                                             //  808
  "PlanetStarGlowBackgroundScale",                      //  809
  "PlanetStarfieldBackgroundGIContribution",            //  810
  "PlanetStarfieldBackgroundScale",                     //  811
  "PlanetStarfieldStarBrightnessScale",                 //  812
  "PlayMode",                                           //  813
  "PlayOnCulledNodes",                                  //  814
  "PlayerHUDEffect",                                    //  815
  "Position",                                           //  816
  "PositionOffset",                                     //  817
  "PositionOffsetEnabled",                              //  818
  "PrecipFadeIn",                                       //  819
  "PrecipFadeOut",                                      //  820
  "Precipitation",                                      //  821
  "PreferREFR",                                         //  822
  "PreferREFREnabled",                                  //  823
  "PresetData",                                         //  824
  "PreviewModelPath",                                   //  825
  "PrewarmTime",                                        //  826
  "ProjectedDecalSetting",                              //  827
  "Properties",                                         //  828
  "Property",                                           //  829
  "RadialBlur",                                         //  830
  "RampDownValue",                                      //  831
  "RampUpValue",                                        //  832
  "RandomPlaySpeedRange",                               //  833
  "RandomTimeOffset",                                   //  834
  "Range",                                              //  835
  "RaycastDirection",                                   //  836
  "RaycastLength",                                      //  837
  "Rayleigh",                                           //  838
  "RayleighCoef",                                       //  839
  "ReceiveDirectionalShadows",                          //  840
  "ReceiveNonDirectionalShadows",                       //  841
  "ReciprocalTickRate",                                 //  842
  "RedChannel",                                         //  843
  "RefID",                                              //  844
  "ReferenceDefines",                                   //  845
  "References",                                         //  846
  "ReflectanceB",                                       //  847
  "ReflectanceG",                                       //  848
  "ReflectanceR",                                       //  849
  "ReflectionProbeCellComponent",                       //  850
  "ReflectionProbeInstanceData",                        //  851
  "RefractiveIndexOfAir",                               //  852
  "RemapAlbedo",                                        //  853
  "RemapEmissive",                                      //  854
  "RemapOpacity",                                       //  855
  "RenderLayer",                                        //  856
  "Replacement",                                        //  857
  "RequiresNodeType",                                   //  858
  "ResetOnEnd",                                         //  859
  "ResetOnLoop",                                        //  860
  "ResolutionHint",                                     //  861
  "RespectSceneTransform",                              //  862
  "RevertMaterialOnSequenceEnd",                        //  863
  "RotationAngle",                                      //  864
  "RotationOffset",                                     //  865
  "RotationOffsetEnabled",                              //  866
  "Roughness",                                          //  867
  "RoundToNearest",                                     //  868
  "Route",                                              //  869
  "Rows",                                               //  870
  "SSSStrength",                                        //  871
  "SSSWidth",                                           //  872
  "Saturation",                                         //  873
  "ScaleOffset",                                        //  874
  "ScaleOffsetEnabled",                                 //  875
  "Scattering",                                         //  876
  "ScatteringFar",                                      //  877
  "ScatteringScale",                                    //  878
  "ScatteringTransition",                               //  879
  "ScatteringVolumeFar",                                //  880
  "ScatteringVolumeNear",                               //  881
  "ScleraEyeRoughness",                                 //  882
  "ScleraSpecularity",                                  //  883
  "SearchFromTopLevelFadeNode",                         //  884
  "SearchMethod",                                       //  885
  "Second",                                             //  886
  "SecondBlenderIndex",                                 //  887
  "SecondBlenderMode",                                  //  888
  "SecondLayerActive",                                  //  889
  "SecondLayerIndex",                                   //  890
  "SecondLayerMaskIndex",                               //  891
  "SecondLayerTint",                                    //  892
  "SecondMostSignificantLayer",                         //  893
  "SedimentReflectanceColorB",                          //  894
  "SedimentReflectanceColorG",                          //  895
  "SedimentReflectanceColorR",                          //  896
  "SequenceID",                                         //  897
  "SequenceName",                                       //  898
  "Sequences",                                          //  899
  "Settings",                                           //  900
  "SettingsTemplate",                                   //  901
  "Shadows",                                            //  902
  "ShakeMultiplier",                                    //  903
  "ShakeType",                                          //  904
  "ShouldRescale",                                      //  905
  "SkyLightingMultiplier",                              //  906
  "SoftEffect",                                         //  907
  "SoftFalloffDepth",                                   //  908
  "Sound",                                              //  909
  "SoundEffects",                                       //  910
  "SoundHook",                                          //  911
  "Sounds",                                             //  912
  "SourceDirection",                                    //  913
  "SourceDirectoryHash",                                //  914
  "SourceID",                                           //  915
  "SpaceGlowBackgroundScaleOverride",                   //  916
  "Span",                                               //  917
  "SpawnMethod",                                        //  918
  "SpecLobe0RoughnessScale",                            //  919
  "SpecLobe1RoughnessScale",                            //  920
  "SpecScale",                                          //  921
  "SpecularOpacityOverride",                            //  922
  "SpecularTransmissionScale",                          //  923
  "Speed",                                              //  924
  "SpellData",                                          //  925
  "SpellItems",                                         //  926
  "SplineColor_Packed",                                 //  927
  "SplineSide",                                         //  928
  "StarfieldBackgroundScaleOverride",                   //  929
  "StarfieldStarBrightnessScaleOverride",               //  930
  "Stars",                                              //  931
  "Start",                                              //  932
  "StartEvent",                                         //  933
  "StartTargetIndex",                                   //  934
  "StartValue",                                         //  935
  "StaticVisibility",                                   //  936
  "StopEvent",                                          //  937
  "StreamID",                                           //  938
  "Strength",                                           //  939
  "StrengthValue",                                      //  940
  "SubWeathers",                                        //  941
  "Sun",                                                //  942
  "SunAndSky",                                          //  943
  "SunColor",                                           //  944
  "SunDiskIlluminanceScaleOverride",                    //  945
  "SunDiskIndirectIlluminanceScaleOverride",            //  946
  "SunDiskScreenSizeMax",                               //  947
  "SunDiskScreenSizeMin",                               //  948
  "SunDiskTexture",                                     //  949
  "SunGlare",                                           //  950
  "SunGlareColor",                                      //  951
  "SunIlluminance",                                     //  952
  "Sunlight",                                           //  953
  "SurfaceHeightMap",                                   //  954
  "SwapsCollection",                                    //  955
  "TESImageSpace",                                      //  956
  "TESImageSpaceModifier",                              //  957
  "Tangent",                                            //  958
  "TangentBend",                                        //  959
  "TargetID",                                           //  960
  "TargetUVStream",                                     //  961
  "TerrainBlendGradientFactor",                         //  962
  "TerrainBlendStrength",                               //  963
  "TerrainMatch",                                       //  964
  "TexcoordScaleAndBias",                               //  965
  "TexcoordScale_XY",                                   //  966
  "TexcoordScale_XZ",                                   //  967
  "TexcoordScale_YZ",                                   //  968
  "Texture",                                            //  969
  "TextureMappingType",                                 //  970
  "TextureShadowOffset",                                //  971
  "TextureShadowStrength",                              //  972
  "Thickness",                                          //  973
  "ThicknessNoiseBias",                                 //  974
  "ThicknessNoiseScale",                                //  975
  "ThicknessTexture",                                   //  976
  "Thin",                                               //  977
  "ThirdLayerActive",                                   //  978
  "ThirdLayerIndex",                                    //  979
  "ThirdLayerMaskIndex",                                //  980
  "ThirdLayerTint",                                     //  981
  "Threshold",                                          //  982
  "ThunderFadeIn",                                      //  983
  "ThunderFadeOut",                                     //  984
  "TicksPerSecond",                                     //  985
  "Tiling",                                             //  986
  "TilingDistance",                                     //  987
  "TilingPerKm",                                        //  988
  "Time",                                               //  989
  "TimeMultiplier",                                     //  990
  "TimelineStartTime",                                  //  991
  "TimelineUnitsPerPixel",                              //  992
  "Tint",                                               //  993
  "TintColorValue",                                     //  994
  "ToneMapE",                                           //  995
  "ToneMapping",                                        //  996
  "TopBlendDistanceKm",                                 //  997
  "TopBlendStartKm",                                    //  998
  "Tracks",                                             //  999
  "TransDelta",                                         // 1000
  "TransformHandling",                                  // 1001
  "TransformMode",                                      // 1002
  "TransitionEndAngle",                                 // 1003
  "TransitionStartAngle",                               // 1004
  "TransitionThreshold",                                // 1005
  "TransmissiveScale",                                  // 1006
  "TransmittanceSourceLayer",                           // 1007
  "TransmittanceWidth",                                 // 1008
  "TrunkFlexibility",                                   // 1009
  "TurbulenceDirectionAmplitude",                       // 1010
  "TurbulenceDirectionFrequency",                       // 1011
  "TurbulenceSpeedAmplitude",                           // 1012
  "TurbulenceSpeedFrequency",                           // 1013
  "Type",                                               // 1014
  "UVStreamTargetBlender",                              // 1015
  "UVStreamTargetLayer",                                // 1016
  "UniqueID",                                           // 1017
  "UseAsInteriorCriteria",                              // 1018
  "UseBoundsToScaleDissolve",                           // 1019
  "UseCustomCoefficients",                              // 1020
  "UseDetailBlendMask",                                 // 1021
  "UseDigitMask",                                       // 1022
  "UseDitheredTransparency",                            // 1023
  "UseFallOff",                                         // 1024
  "UseGBufferNormals",                                  // 1025
  "UseNodeLocalRotation",                               // 1026
  "UseNoiseMaskTexture",                                // 1027
  "UseOutdoorExposure",                                 // 1028
  "UseOutdoorLUT",                                      // 1029
  "UseOzoneAbsorptionApproximation",                    // 1030
  "UseParallaxOcclusionMapping",                        // 1031
  "UseRGBFallOff",                                      // 1032
  "UseRandomOffset",                                    // 1033
  "UseSSS",                                             // 1034
  "UseStartTargetIndex",                                // 1035
  "UseTargetForDepthOfField",                           // 1036
  "UseTargetForRadialBlur",                             // 1037
  "UseVertexAlpha",                                     // 1038
  "UseVertexColor",                                     // 1039
  "UseWorldAlignedRaycastDirection",                    // 1040
  "UserDuration",                                       // 1041
  "UsesDirectionality",                                 // 1042
  "Value",                                              // 1043
  "Variables",                                          // 1044
  "VariationStrength",                                  // 1045
  "Version",                                            // 1046
  "VertexColorBlend",                                   // 1047
  "VertexColorChannel",                                 // 1048
  "VerticalAngle",                                      // 1049
  "VerticalTiling",                                     // 1050
  "VeryLowLODRootMaterial",                             // 1051
  "VisibilityMultiplier",                               // 1052
  "Visible",                                            // 1053
  "VolatilityMultiplier",                               // 1054
  "VolumetricIndirectLightContribution",                // 1055
  "VolumetricLighting",                                 // 1056
  "VolumetricLightingDirectionalAnisoScale",            // 1057
  "VolumetricLightingDirectionalLightScale",            // 1058
  "WarmUp",                                             // 1059
  "WaterDepthBlur",                                     // 1060
  "WaterEdgeFalloff",                                   // 1061
  "WaterEdgeNormalFalloff",                             // 1062
  "WaterIndirectSpecularMultiplier",                    // 1063
  "WaterRefractionMagnitude",                           // 1064
  "WaterWetnessMaxDepth",                               // 1065
  "WaveAmplitude",                                      // 1066
  "WaveDistortionAmount",                               // 1067
  "WaveFlipWaveDirection",                              // 1068
  "WaveParallaxFalloffBias",                            // 1069
  "WaveParallaxFalloffScale",                           // 1070
  "WaveParallaxInnerStrength",                          // 1071
  "WaveParallaxOuterStrength",                          // 1072
  "WaveScale",                                          // 1073
  "WaveShoreFadeInnerDistance",                         // 1074
  "WaveShoreFadeOuterDistance",                         // 1075
  "WaveSpawnFadeInDistance",                            // 1076
  "WaveSpeed",                                          // 1077
  "WeatherActivateEffect",                              // 1078
  "WeatherChoice",                                      // 1079
  "Weight",                                             // 1080
  "WeightMultiplier",                                   // 1081
  "Weighted",                                           // 1082
  "WhitePointValue",                                    // 1083
  "WindDirectionOverrideEnabled",                       // 1084
  "WindDirectionOverrideValue",                         // 1085
  "WindDirectionRange",                                 // 1086
  "WindScale",                                          // 1087
  "WindStrengthVariationMax",                           // 1088
  "WindStrengthVariationMin",                           // 1089
  "WindStrengthVariationSpeed",                         // 1090
  "WindTurbulence",                                     // 1091
  "WorldspaceScaleFactor",                              // 1092
  "WriteMask",                                          // 1093
  "XCurve",                                             // 1094
  "XMCOLOR",                                            // 1095
  "XMFLOAT2",                                           // 1096
  "XMFLOAT3",                                           // 1097
  "XMFLOAT4",                                           // 1098
  "YCurve",                                             // 1099
  "YellowMatterReflectanceColorB",                      // 1100
  "YellowMatterReflectanceColorG",                      // 1101
  "YellowMatterReflectanceColorR",                      // 1102
  "ZCurve",                                             // 1103
  "ZTest",                                              // 1104
  "ZWrite",                                             // 1105
  "a",                                                  // 1106
  "b",                                                  // 1107
  "g",                                                  // 1108
  "pArtForm",                                           // 1109
  "pClimateOverride",                                   // 1110
  "pCloudCardSequence",                                 // 1111
  "pClouds",                                            // 1112
  "pConditionForm",                                     // 1113
  "pConditions",                                        // 1114
  "pDefineCollection",                                  // 1115
  "pDisplayNameKeyword",                                // 1116
  "pEventForm",                                         // 1117
  "pExplosionForm",                                     // 1118
  "pExternalForm",                                      // 1119
  "pFalloff",                                           // 1120
  "pFormReference",                                     // 1121
  "pImageSpace",                                        // 1122
  "pImageSpaceDay",                                     // 1123
  "pImageSpaceForm",                                    // 1124
  "pImageSpaceNight",                                   // 1125
  "pImpactDataSet",                                     // 1126
  "pLensFlare",                                         // 1127
  "pLightForm",                                         // 1128
  "pLightningFX",                                       // 1129
  "pMainSpell",                                         // 1130
  "pOptionalPhotoModeEffect",                           // 1131
  "pParent",                                            // 1132
  "pParentForm",                                        // 1133
  "pPrecipitationEffect",                               // 1134
  "pPreviewForm",                                       // 1135
  "pProjectedDecalForm",                                // 1136
  "pSource",                                            // 1137
  "pSpell",                                             // 1138
  "pSunPresetOverride",                                 // 1139
  "pTimeOfDayData",                                     // 1140
  "pTransitionSpell",                                   // 1141
  "pVisualEffect",                                      // 1142
  "pVolumeticLighting",                                 // 1143
  "pWindForce",                                         // 1144
  "r",                                                  // 1145
  "spController",                                       // 1146
  "upControllers",                                      // 1147
  "upDir",                                              // 1148
  "upMap",                                              // 1149
  "upObjectToAttach",                                   // 1150
  "upObjectToSpawn",                                    // 1151
  "w",                                                  // 1152
  "x",                                                  // 1153
  "y",                                                  // 1154
  "z"                                                   // 1155
};

