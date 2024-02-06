
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

const char * BSReflStream::stringTable[1149] =
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
  "BGSAtmosphere::ScatteringSettings::RayleighSettings",        //   72
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
  "BGSVolumetricLightingSettings::DistantLightingSettings",     //   98
  "BGSVolumetricLightingSettings::ExteriorAndInteriorSettings", //   99
  "BGSVolumetricLightingSettings::ExteriorSettings",    //  100
  "BGSVolumetricLightingSettings::FogDensitySettings",  //  101
  "BGSVolumetricLightingSettings::FogMapSettings",      //  102
  "BGSVolumetricLightingSettings::FogThicknessSettings",        //  103
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
  "Easing",                                             //  436
  "Edge",                                               //  437
  "EdgeEffectCoefficient",                              //  438
  "EdgeEffectExponent",                                 //  439
  "EdgeFalloffEnd",                                     //  440
  "EdgeFalloffStart",                                   //  441
  "EdgeMaskContrast",                                   //  442
  "EdgeMaskDistanceMax",                                //  443
  "EdgeMaskDistanceMin",                                //  444
  "EdgeMaskMin",                                        //  445
  "Edges",                                              //  446
  "EffectLighting",                                     //  447
  "EffectSequenceMap",                                  //  448
  "EffectSequenceMapMetadata",                          //  449
  "EffectSequenceMetadataID",                           //  450
  "EffectSequenceObjectMetadata",                       //  451
  "ElevationKm",                                        //  452
  "EmissiveClipThreshold",                              //  453
  "EmissiveMaskSourceBlender",                          //  454
  "EmissiveOnlyAutomaticallyApplied",                   //  455
  "EmissiveOnlyEffect",                                 //  456
  "EmissivePaletteTex",                                 //  457
  "EmissiveSourceLayer",                                //  458
  "EmissiveTint",                                       //  459
  "Emittance",                                          //  460
  "EnableAdaptiveLimits",                               //  461
  "EnableCompensationCurve",                            //  462
  "Enabled",                                            //  463
  "End",                                                //  464
  "EndVisualEffect",                                    //  465
  "Entries",                                            //  466
  "EventName",                                          //  467
  "Events",                                             //  468
  "Exposure",                                           //  469
  "ExposureCompensationCurve",                          //  470
  "ExposureMax",                                        //  471
  "ExposureMin",                                        //  472
  "ExposureOffset",                                     //  473
  "Ext",                                                //  474
  "Exterior",                                           //  475
  "ExteriorAndInterior",                                //  476
  "ExternalColorBindingName",                           //  477
  "ExternalLuminanceBindingName",                       //  478
  "FPS",                                                //  479
  "FadeColorValue",                                     //  480
  "FadeDistanceKm",                                     //  481
  "FadeStartKm",                                        //  482
  "FallbackToRoot",                                     //  483
  "FalloffStartAngle",                                  //  484
  "FalloffStartAngles",                                 //  485
  "FalloffStartOpacities",                              //  486
  "FalloffStartOpacity",                                //  487
  "FalloffStopAngle",                                   //  488
  "FalloffStopAngles",                                  //  489
  "FalloffStopOpacities",                               //  490
  "FalloffStopOpacity",                                 //  491
  "FarFadeValue",                                       //  492
  "FarOpacityValue",                                    //  493
  "FarPlaneValue",                                      //  494
  "FarStartValue",                                      //  495
  "File",                                               //  496
  "FileName",                                           //  497
  "First",                                              //  498
  "FirstBlenderIndex",                                  //  499
  "FirstBlenderMode",                                   //  500
  "FirstLayerIndex",                                    //  501
  "FirstLayerMaskIndex",                                //  502
  "FirstLayerTint",                                     //  503
  "FixedValue",                                         //  504
  "FlipBackFaceNormalsInViewSpace",                     //  505
  "FlowExtent",                                         //  506
  "FlowIsAnimated",                                     //  507
  "FlowMap",                                            //  508
  "FlowMapAndTexturesAreFlipbooks",                     //  509
  "FlowSourceUVChannel",                                //  510
  "FlowSpeed",                                          //  511
  "FlowUVOffset",                                       //  512
  "FlowUVScale",                                        //  513
  "FoamTextureDistortion",                              //  514
  "FoamTextureScrollSpeed",                             //  515
  "Fog",                                                //  516
  "FogDensity",                                         //  517
  "FogFar",                                             //  518
  "FogFarHigh",                                         //  519
  "FogMap",                                             //  520
  "FogMapContribution",                                 //  521
  "FogNear",                                            //  522
  "FogNearHigh",                                        //  523
  "FogScaleValue",                                      //  524
  "FogThickness",                                       //  525
  "Foliage",                                            //  526
  "ForceRenderBeforeOIT",                               //  527
  "ForceStopOnDetach",                                  //  528
  "ForceStopSound",                                     //  529
  "FormFolderPath",                                     //  530
  "FrequencyMultiplier",                                //  531
  "Frosting",                                           //  532
  "FrostingBlurBias",                                   //  533
  "FrostingUnblurredBackgroundAlphaBlend",              //  534
  "GeoShareBehavior",                                   //  535
  "GlareColor",                                         //  536
  "GlobalLayerNoiseData",                               //  537
  "GreenChannel",                                       //  538
  "HDAComponentIdx",                                    //  539
  "HDAFilePath",                                        //  540
  "HDAVersion",                                         //  541
  "HableShoulderAngle",                                 //  542
  "HableShoulderLength",                                //  543
  "HableShoulderStrength",                              //  544
  "HableToeLength",                                     //  545
  "HableToeStrength",                                   //  546
  "HasData",                                            //  547
  "HasErrors",                                          //  548
  "HasOpacity",                                         //  549
  "HashMap",                                            //  550
  "Height",                                             //  551
  "HeightAboveTerrain",                                 //  552
  "HeightBlendFactor",                                  //  553
  "HeightBlendThreshold",                               //  554
  "HeightFalloffExponent",                              //  555
  "HeightKm",                                           //  556
  "HelpText",                                           //  557
  "HighFrequencyNoiseDensityScale",                     //  558
  "HighFrequencyNoiseScale",                            //  559
  "HighGUID",                                           //  560
  "HorizonFog",                                         //  561
  "HorizonScatteringBlendScale",                        //  562
  "HorizontalAngle",                                    //  563
  "HoudiniDataComponent",                               //  564
  "HurstExponent",                                      //  565
  "ID",                                                 //  566
  "ISO",                                                //  567
  "Id",                                                 //  568
  "IgnoreSmallHeadparts",                               //  569
  "IgnoreWeapons",                                      //  570
  "IgnoredBrightsPercentile",                           //  571
  "IgnoredDarksPercentile",                             //  572
  "ImageSpaceSettings",                                 //  573
  "ImageSpaceSettings::AmbientOcclusionSettings",       //  574
  "ImageSpaceSettings::BloomSettings",                  //  575
  "ImageSpaceSettings::BlurSettings",                   //  576
  "ImageSpaceSettings::CinematicSettings",              //  577
  "ImageSpaceSettings::DepthOfFieldSettings",           //  578
  "ImageSpaceSettings::ExposureSettings",               //  579
  "ImageSpaceSettings::ExposureSettings::AutoExposureSettings",         //  580
  "ImageSpaceSettings::ExposureSettings::CameraExposureSettings",       //  581
  "ImageSpaceSettings::ExposureSettings::LuminanceHistogramSettings",   //  582
  "ImageSpaceSettings::FogSettings",                    //  583
  "ImageSpaceSettings::IndirectLightingSettings",       //  584
  "ImageSpaceSettings::RadialBlurSettings",             //  585
  "ImageSpaceSettings::SunAndSkySettings",              //  586
  "ImageSpaceSettings::ToneMappingSettings",            //  587
  "ImageSpaceSettings::VolumetricLightingSettings",     //  588
  "Index",                                              //  589
  "IndirectDiffuseMultiplier",                          //  590
  "IndirectLighting",                                   //  591
  "IndirectLightingSkyScaleOverride",                   //  592
  "IndirectLightingSkyTargetEv100",                     //  593
  "IndirectLightingSkyTargetStrength",                  //  594
  "IndirectSpecRoughness",                              //  595
  "IndirectSpecularMultiplier",                         //  596
  "IndirectSpecularScale",                              //  597
  "IndirectSpecularTransmissionScale",                  //  598
  "InitialAngularVelocity",                             //  599
  "InitialAngularVelocityNoise",                        //  600
  "InitialLinearVelocity",                              //  601
  "InitialLinearVelocityNoise",                         //  602
  "InitialOffset",                                      //  603
  "InnerFadeDistance",                                  //  604
  "InorganicResources",                                 //  605
  "Input",                                              //  606
  "InputDistance",                                      //  607
  "Instances",                                          //  608
  "IntensityValue",                                     //  609
  "InterpretAs",                                        //  610
  "Interval",                                           //  611
  "IrisDepthPosition",                                  //  612
  "IrisDepthTransitionRatio",                           //  613
  "IrisSpecularity",                                    //  614
  "IrisTotalDepth",                                     //  615
  "IrisUVSize",                                         //  616
  "IsAFlipbook",                                        //  617
  "IsAlphaTested",                                      //  618
  "IsDecal",                                            //  619
  "IsDetailBlendMaskSupported",                         //  620
  "IsEmpty",                                            //  621
  "IsGlass",                                            //  622
  "IsMembrane",                                         //  623
  "IsPermanent",                                        //  624
  "IsPlanet",                                           //  625
  "IsProjected",                                        //  626
  "IsSampleInterpolating",                              //  627
  "IsSpikyHair",                                        //  628
  "IsTeeth",                                            //  629
  "Key",                                                //  630
  "KeyMaterialPathForm",                                //  631
  "Keywords",                                           //  632
  "KillSound",                                          //  633
  "LODMeshOverrides",                                   //  634
  "Label",                                              //  635
  "Layer",                                              //  636
  "LayerIndex",                                         //  637
  "Layers",                                             //  638
  "LeafAmplitude",                                      //  639
  "LeafFrequency",                                      //  640
  "LeafSecondaryMotionAmount",                          //  641
  "LeafSecondaryMotionCutOff",                          //  642
  "LensFlareAttachmentComponent",                       //  643
  "LensFlareAttachmentDefine",                          //  644
  "LensFlareCloudOcclusionStrength",                    //  645
  "LensFlareDefines",                                   //  646
  "LightAttachmentConfiguration",                       //  647
  "LightAttachmentDefine",                              //  648
  "LightAttachmentFormComponent",                       //  649
  "LightAttachmentRef",                                 //  650
  "LightLuminanceMultiplier",                           //  651
  "LightingPower",                                      //  652
  "LightingWrap",                                       //  653
  "LightningColor",                                     //  654
  "LightningDistanceMax",                               //  655
  "LightningDistanceMin",                               //  656
  "LightningFieldOfView",                               //  657
  "LightningStrikeEffect",                              //  658
  "Loop",                                               //  659
  "LoopSegment",                                        //  660
  "Loops",                                              //  661
  "LowGUID",                                            //  662
  "LowLOD",                                             //  663
  "LowLODRootMaterial",                                 //  664
  "LuminanceAtTime",                                    //  665
  "LuminanceHistogram",                                 //  666
  "LuminousEmittance",                                  //  667
  "Map",                                                //  668
  "MapMetadata",                                        //  669
  "MappingsA",                                          //  670
  "Mask",                                               //  671
  "MaskDistanceFromShoreEnd",                           //  672
  "MaskDistanceFromShoreStart",                         //  673
  "MaskDistanceRampWidth",                              //  674
  "MaskIntensityMax",                                   //  675
  "MaskIntensityMin",                                   //  676
  "MaskNoiseAmp",                                       //  677
  "MaskNoiseAnimSpeed",                                 //  678
  "MaskNoiseBias",                                      //  679
  "MaskNoiseFreq",                                      //  680
  "MaskNoiseGlobalScale",                               //  681
  "MaskWaveParallax",                                   //  682
  "Material",                                           //  683
  "MaterialMaskIntensityScale",                         //  684
  "MaterialOverallAlpha",                               //  685
  "MaterialPath",                                       //  686
  "MaterialProperty",                                   //  687
  "MaterialTypeOverride",                               //  688
  "Materials",                                          //  689
  "Max",                                                //  690
  "MaxConcentrationPlankton",                           //  691
  "MaxConcentrationSediment",                           //  692
  "MaxConcentrationYellowMatter",                       //  693
  "MaxDelay",                                           //  694
  "MaxDepthOffset",                                     //  695
  "MaxDisplacement",                                    //  696
  "MaxFogDensity",                                      //  697
  "MaxFogThickness",                                    //  698
  "MaxIndex",                                           //  699
  "MaxInput",                                           //  700
  "MaxMeanFreePath",                                    //  701
  "MaxOffsetEmittance",                                 //  702
  "MaxParralaxOcclusionSteps",                          //  703
  "MaxValue",                                           //  704
  "MeanFreePath",                                       //  705
  "MediumLODRootMaterial",                              //  706
  "MeshFileOverride",                                   //  707
  "MeshLODDistanceOverride",                            //  708
  "MeshLODs",                                           //  709
  "MeshOverrides",                                      //  710
  "Metadata",                                           //  711
  "Mie",                                                //  712
  "MieCoef",                                            //  713
  "Min",                                                //  714
  "MinDelay",                                           //  715
  "MinFogDensity",                                      //  716
  "MinFogThickness",                                    //  717
  "MinIndex",                                           //  718
  "MinInput",                                           //  719
  "MinMeanFreePath",                                    //  720
  "MinOffsetEmittance",                                 //  721
  "MinValue",                                           //  722
  "MipBase",                                            //  723
  "Misc",                                               //  724
  "Mode",                                               //  725
  "MoleculesPerUnitVolume",                             //  726
  "MoonGlare",                                          //  727
  "Moonlight",                                          //  728
  "MostSignificantLayer",                               //  729
  "MotionBlurStrengthValue",                            //  730
  "Name",                                               //  731
  "NavMeshAreaFlag",                                    //  732
  "NavMeshSplineExtraData::ChunkData",                  //  733
  "NavMeshSplineExtraData::ChunkDataRef",               //  734
  "NearFadeValue",                                      //  735
  "NearOpacityValue",                                   //  736
  "NearPlaneValue",                                     //  737
  "NearStartValue",                                     //  738
  "NestedTracks",                                       //  739
  "NiPoint3",                                           //  740
  "NightPreset",                                        //  741
  "NoHalfResOptimization",                              //  742
  "NoLAngularDamping",                                  //  743
  "NoLinearDamping",                                    //  744
  "NoSky",                                              //  745
  "Node",                                               //  746
  "NodeAttachmentConfiguration",                        //  747
  "NodeName",                                           //  748
  "Nodes",                                              //  749
  "NoiseBias",                                          //  750
  "NoiseContribution",                                  //  751
  "NoiseMaskTexture",                                   //  752
  "NoiseScale",                                         //  753
  "NoiseScrollingVelocity",                             //  754
  "NormalAffectsStrength",                              //  755
  "NormalOverride",                                     //  756
  "NormalShadowStrength",                               //  757
  "NormalTexture",                                      //  758
  "NumLODMaterials",                                    //  759
  "Object",                                             //  760
  "ObjectAttachmentConfiguration",                      //  761
  "ObjectID",                                           //  762
  "Objects",                                            //  763
  "Offset",                                             //  764
  "Op",                                                 //  765
  "OpacitySourceLayer",                                 //  766
  "OpacityTexture",                                     //  767
  "OpacityUVStream",                                    //  768
  "Optimized",                                          //  769
  "Order",                                              //  770
  "OuterFadeDistance",                                  //  771
  "OverallBlendAmount",                                 //  772
  "OverrideAttachType",                                 //  773
  "OverrideInitialVelocities",                          //  774
  "OverrideLevelCount",                                 //  775
  "OverrideMaterial",                                   //  776
  "OverrideMaterialPath",                               //  777
  "OverrideMeshLODDistance",                            //  778
  "OverrideSearchMethod",                               //  779
  "OverrideToneMapping",                                //  780
  "Overrides",                                          //  781
  "OzoneAbsorptionCoef",                                //  782
  "ParallaxOcclusionScale",                             //  783
  "ParallaxOcclusionShadows",                           //  784
  "ParamIndex",                                         //  785
  "ParameterName",                                      //  786
  "ParametersA",                                        //  787
  "Parent",                                             //  788
  "ParticleAttachmentConfiguration",                    //  789
  "ParticleFormComponent",                              //  790
  "ParticleRenderState",                                //  791
  "ParticleSystemDefine",                               //  792
  "ParticleSystemDefineOverrides",                      //  793
  "ParticleSystemDefineRef",                            //  794
  "ParticleSystemPath",                                 //  795
  "ParticleSystemReferenceDefine",                      //  796
  "Path",                                               //  797
  "PathStr",                                            //  798
  "PersistentID",                                       //  799
  "PhytoplanktonReflectanceColorB",                     //  800
  "PhytoplanktonReflectanceColorG",                     //  801
  "PhytoplanktonReflectanceColorR",                     //  802
  "PlacedWater",                                        //  803
  "Planes",                                             //  804
  "PlanetStarGlowBackgroundScale",                      //  805
  "PlanetStarfieldBackgroundGIContribution",            //  806
  "PlanetStarfieldBackgroundScale",                     //  807
  "PlanetStarfieldStarBrightnessScale",                 //  808
  "PlayMode",                                           //  809
  "PlayOnCulledNodes",                                  //  810
  "PlayerHUDEffect",                                    //  811
  "Position",                                           //  812
  "PositionOffset",                                     //  813
  "PositionOffsetEnabled",                              //  814
  "PrecipFadeIn",                                       //  815
  "PrecipFadeOut",                                      //  816
  "Precipitation",                                      //  817
  "PreferREFR",                                         //  818
  "PreferREFREnabled",                                  //  819
  "PresetData",                                         //  820
  "PreviewModelPath",                                   //  821
  "PrewarmTime",                                        //  822
  "ProjectedDecalSetting",                              //  823
  "Properties",                                         //  824
  "Property",                                           //  825
  "RadialBlur",                                         //  826
  "RampDownValue",                                      //  827
  "RampUpValue",                                        //  828
  "RandomPlaySpeedRange",                               //  829
  "RandomTimeOffset",                                   //  830
  "Range",                                              //  831
  "RaycastDirection",                                   //  832
  "RaycastLength",                                      //  833
  "Rayleigh",                                           //  834
  "RayleighCoef",                                       //  835
  "ReceiveDirectionalShadows",                          //  836
  "ReceiveNonDirectionalShadows",                       //  837
  "ReciprocalTickRate",                                 //  838
  "RedChannel",                                         //  839
  "RefID",                                              //  840
  "ReferenceDefines",                                   //  841
  "References",                                         //  842
  "ReflectanceB",                                       //  843
  "ReflectanceG",                                       //  844
  "ReflectanceR",                                       //  845
  "ReflectionProbeCellComponent",                       //  846
  "ReflectionProbeInstanceData",                        //  847
  "RefractiveIndexOfAir",                               //  848
  "RemapAlbedo",                                        //  849
  "RemapEmissive",                                      //  850
  "RemapOpacity",                                       //  851
  "RenderLayer",                                        //  852
  "Replacement",                                        //  853
  "RequiresNodeType",                                   //  854
  "ResetOnEnd",                                         //  855
  "ResetOnLoop",                                        //  856
  "ResolutionHint",                                     //  857
  "RespectSceneTransform",                              //  858
  "RevertMaterialOnSequenceEnd",                        //  859
  "RotationAngle",                                      //  860
  "RotationOffset",                                     //  861
  "RotationOffsetEnabled",                              //  862
  "Roughness",                                          //  863
  "RoundToNearest",                                     //  864
  "Route",                                              //  865
  "Rows",                                               //  866
  "SSSStrength",                                        //  867
  "SSSWidth",                                           //  868
  "Saturation",                                         //  869
  "ScaleOffset",                                        //  870
  "ScaleOffsetEnabled",                                 //  871
  "Scattering",                                         //  872
  "ScatteringFar",                                      //  873
  "ScatteringScale",                                    //  874
  "ScatteringTransition",                               //  875
  "ScatteringVolumeFar",                                //  876
  "ScatteringVolumeNear",                               //  877
  "ScleraEyeRoughness",                                 //  878
  "ScleraSpecularity",                                  //  879
  "SearchFromTopLevelFadeNode",                         //  880
  "SearchMethod",                                       //  881
  "Second",                                             //  882
  "SecondBlenderIndex",                                 //  883
  "SecondBlenderMode",                                  //  884
  "SecondLayerActive",                                  //  885
  "SecondLayerIndex",                                   //  886
  "SecondLayerMaskIndex",                               //  887
  "SecondLayerTint",                                    //  888
  "SecondMostSignificantLayer",                         //  889
  "SedimentReflectanceColorB",                          //  890
  "SedimentReflectanceColorG",                          //  891
  "SedimentReflectanceColorR",                          //  892
  "SequenceID",                                         //  893
  "SequenceName",                                       //  894
  "Sequences",                                          //  895
  "Settings",                                           //  896
  "SettingsTemplate",                                   //  897
  "Shadows",                                            //  898
  "ShakeMultiplier",                                    //  899
  "ShakeType",                                          //  900
  "ShouldRescale",                                      //  901
  "SkyLightingMultiplier",                              //  902
  "SoftEffect",                                         //  903
  "SoftFalloffDepth",                                   //  904
  "Sound",                                              //  905
  "SoundEffects",                                       //  906
  "SoundHook",                                          //  907
  "Sounds",                                             //  908
  "SourceDirection",                                    //  909
  "SourceDirectoryHash",                                //  910
  "SourceID",                                           //  911
  "SpaceGlowBackgroundScaleOverride",                   //  912
  "SpawnMethod",                                        //  913
  "SpecLobe0RoughnessScale",                            //  914
  "SpecLobe1RoughnessScale",                            //  915
  "SpecScale",                                          //  916
  "SpecularOpacityOverride",                            //  917
  "SpecularTransmissionScale",                          //  918
  "Speed",                                              //  919
  "SpellData",                                          //  920
  "SpellItems",                                         //  921
  "SplineColor_Packed",                                 //  922
  "SplineSide",                                         //  923
  "StarfieldBackgroundScaleOverride",                   //  924
  "StarfieldStarBrightnessScaleOverride",               //  925
  "Stars",                                              //  926
  "Start",                                              //  927
  "StartEvent",                                         //  928
  "StartTargetIndex",                                   //  929
  "StartValue",                                         //  930
  "StaticVisibility",                                   //  931
  "StopEvent",                                          //  932
  "StreamID",                                           //  933
  "Strength",                                           //  934
  "StrengthValue",                                      //  935
  "SubWeathers",                                        //  936
  "Sun",                                                //  937
  "SunAndSky",                                          //  938
  "SunColor",                                           //  939
  "SunDiskIlluminanceScaleOverride",                    //  940
  "SunDiskIndirectIlluminanceScaleOverride",            //  941
  "SunDiskScreenSizeMax",                               //  942
  "SunDiskScreenSizeMin",                               //  943
  "SunDiskTexture",                                     //  944
  "SunGlare",                                           //  945
  "SunGlareColor",                                      //  946
  "SunIlluminance",                                     //  947
  "Sunlight",                                           //  948
  "SurfaceHeightMap",                                   //  949
  "SwapsCollection",                                    //  950
  "TESImageSpace",                                      //  951
  "TESImageSpaceModifier",                              //  952
  "Tangent",                                            //  953
  "TangentBend",                                        //  954
  "TargetID",                                           //  955
  "TargetUVStream",                                     //  956
  "TerrainBlendGradientFactor",                         //  957
  "TerrainBlendStrength",                               //  958
  "TerrainMatch",                                       //  959
  "TexcoordScaleAndBias",                               //  960
  "TexcoordScale_XY",                                   //  961
  "TexcoordScale_XZ",                                   //  962
  "TexcoordScale_YZ",                                   //  963
  "Texture",                                            //  964
  "TextureMappingType",                                 //  965
  "TextureShadowOffset",                                //  966
  "TextureShadowStrength",                              //  967
  "Thickness",                                          //  968
  "ThicknessNoiseBias",                                 //  969
  "ThicknessNoiseScale",                                //  970
  "ThicknessTexture",                                   //  971
  "Thin",                                               //  972
  "ThirdLayerActive",                                   //  973
  "ThirdLayerIndex",                                    //  974
  "ThirdLayerMaskIndex",                                //  975
  "ThirdLayerTint",                                     //  976
  "Threshold",                                          //  977
  "ThunderFadeIn",                                      //  978
  "ThunderFadeOut",                                     //  979
  "TicksPerSecond",                                     //  980
  "Tiling",                                             //  981
  "TilingDistance",                                     //  982
  "TilingPerKm",                                        //  983
  "Time",                                               //  984
  "TimeMultiplier",                                     //  985
  "TimelineStartTime",                                  //  986
  "TimelineUnitsPerPixel",                              //  987
  "Tint",                                               //  988
  "TintColorValue",                                     //  989
  "ToneMapE",                                           //  990
  "ToneMapping",                                        //  991
  "TopBlendDistanceKm",                                 //  992
  "TopBlendStartKm",                                    //  993
  "Tracks",                                             //  994
  "TransDelta",                                         //  995
  "TransformHandling",                                  //  996
  "TransformMode",                                      //  997
  "TransitionEndAngle",                                 //  998
  "TransitionStartAngle",                               //  999
  "TransitionThreshold",                                // 1000
  "TransmissiveScale",                                  // 1001
  "TransmittanceSourceLayer",                           // 1002
  "TransmittanceWidth",                                 // 1003
  "TrunkFlexibility",                                   // 1004
  "TurbulenceDirectionAmplitude",                       // 1005
  "TurbulenceDirectionFrequency",                       // 1006
  "TurbulenceSpeedAmplitude",                           // 1007
  "TurbulenceSpeedFrequency",                           // 1008
  "Type",                                               // 1009
  "UVStreamTargetBlender",                              // 1010
  "UVStreamTargetLayer",                                // 1011
  "UniqueID",                                           // 1012
  "UseAsInteriorCriteria",                              // 1013
  "UseBoundsToScaleDissolve",                           // 1014
  "UseCustomCoefficients",                              // 1015
  "UseDetailBlendMask",                                 // 1016
  "UseDigitMask",                                       // 1017
  "UseDitheredTransparency",                            // 1018
  "UseFallOff",                                         // 1019
  "UseGBufferNormals",                                  // 1020
  "UseNodeLocalRotation",                               // 1021
  "UseNoiseMaskTexture",                                // 1022
  "UseOutdoorExposure",                                 // 1023
  "UseOutdoorLUT",                                      // 1024
  "UseOzoneAbsorptionApproximation",                    // 1025
  "UseParallaxOcclusionMapping",                        // 1026
  "UseRGBFallOff",                                      // 1027
  "UseRandomOffset",                                    // 1028
  "UseSSS",                                             // 1029
  "UseStartTargetIndex",                                // 1030
  "UseTargetForDepthOfField",                           // 1031
  "UseTargetForRadialBlur",                             // 1032
  "UseVertexAlpha",                                     // 1033
  "UseVertexColor",                                     // 1034
  "UseWorldAlignedRaycastDirection",                    // 1035
  "UserDuration",                                       // 1036
  "UsesDirectionality",                                 // 1037
  "Value",                                              // 1038
  "Variables",                                          // 1039
  "VariationStrength",                                  // 1040
  "Version",                                            // 1041
  "VertexColorBlend",                                   // 1042
  "VertexColorChannel",                                 // 1043
  "VerticalAngle",                                      // 1044
  "VerticalTiling",                                     // 1045
  "VeryLowLODRootMaterial",                             // 1046
  "VisibilityMultiplier",                               // 1047
  "VolatilityMultiplier",                               // 1048
  "VolumetricIndirectLightContribution",                // 1049
  "VolumetricLighting",                                 // 1050
  "VolumetricLightingDirectionalAnisoScale",            // 1051
  "VolumetricLightingDirectionalLightScale",            // 1052
  "WarmUp",                                             // 1053
  "WaterDepthBlur",                                     // 1054
  "WaterEdgeFalloff",                                   // 1055
  "WaterEdgeNormalFalloff",                             // 1056
  "WaterIndirectSpecularMultiplier",                    // 1057
  "WaterRefractionMagnitude",                           // 1058
  "WaterWetnessMaxDepth",                               // 1059
  "WaveAmplitude",                                      // 1060
  "WaveDistortionAmount",                               // 1061
  "WaveFlipWaveDirection",                              // 1062
  "WaveParallaxFalloffBias",                            // 1063
  "WaveParallaxFalloffScale",                           // 1064
  "WaveParallaxInnerStrength",                          // 1065
  "WaveParallaxOuterStrength",                          // 1066
  "WaveScale",                                          // 1067
  "WaveShoreFadeInnerDistance",                         // 1068
  "WaveShoreFadeOuterDistance",                         // 1069
  "WaveSpawnFadeInDistance",                            // 1070
  "WaveSpeed",                                          // 1071
  "WeatherActivateEffect",                              // 1072
  "WeatherChoice",                                      // 1073
  "Weight",                                             // 1074
  "WeightMultiplier",                                   // 1075
  "WhitePointValue",                                    // 1076
  "WindDirectionOverrideEnabled",                       // 1077
  "WindDirectionOverrideValue",                         // 1078
  "WindDirectionRange",                                 // 1079
  "WindScale",                                          // 1080
  "WindStrengthVariationMax",                           // 1081
  "WindStrengthVariationMin",                           // 1082
  "WindStrengthVariationSpeed",                         // 1083
  "WindTurbulence",                                     // 1084
  "WorldspaceScaleFactor",                              // 1085
  "WriteMask",                                          // 1086
  "XCurve",                                             // 1087
  "XMCOLOR",                                            // 1088
  "XMFLOAT2",                                           // 1089
  "XMFLOAT3",                                           // 1090
  "XMFLOAT4",                                           // 1091
  "YCurve",                                             // 1092
  "YellowMatterReflectanceColorB",                      // 1093
  "YellowMatterReflectanceColorG",                      // 1094
  "YellowMatterReflectanceColorR",                      // 1095
  "ZCurve",                                             // 1096
  "ZTest",                                              // 1097
  "ZWrite",                                             // 1098
  "a",                                                  // 1099
  "b",                                                  // 1100
  "g",                                                  // 1101
  "pArtForm",                                           // 1102
  "pClimateOverride",                                   // 1103
  "pCloudCardSequence",                                 // 1104
  "pClouds",                                            // 1105
  "pConditionForm",                                     // 1106
  "pConditions",                                        // 1107
  "pDefineCollection",                                  // 1108
  "pDisplayNameKeyword",                                // 1109
  "pEventForm",                                         // 1110
  "pExplosionForm",                                     // 1111
  "pExternalForm",                                      // 1112
  "pFalloff",                                           // 1113
  "pFormReference",                                     // 1114
  "pImageSpace",                                        // 1115
  "pImageSpaceDay",                                     // 1116
  "pImageSpaceForm",                                    // 1117
  "pImageSpaceNight",                                   // 1118
  "pImpactDataSet",                                     // 1119
  "pLensFlare",                                         // 1120
  "pLightForm",                                         // 1121
  "pLightningFX",                                       // 1122
  "pMainSpell",                                         // 1123
  "pOptionalPhotoModeEffect",                           // 1124
  "pParent",                                            // 1125
  "pParentForm",                                        // 1126
  "pPrecipitationEffect",                               // 1127
  "pPreviewForm",                                       // 1128
  "pProjectedDecalForm",                                // 1129
  "pSource",                                            // 1130
  "pSpell",                                             // 1131
  "pSunPresetOverride",                                 // 1132
  "pTimeOfDayData",                                     // 1133
  "pTransitionSpell",                                   // 1134
  "pVisualEffect",                                      // 1135
  "pVolumeticLighting",                                 // 1136
  "pWindForce",                                         // 1137
  "r",                                                  // 1138
  "spController",                                       // 1139
  "upControllers",                                      // 1140
  "upDir",                                              // 1141
  "upMap",                                              // 1142
  "upObjectToAttach",                                   // 1143
  "upObjectToSpawn",                                    // 1144
  "w",                                                  // 1145
  "x",                                                  // 1146
  "y",                                                  // 1147
  "z"                                                   // 1148
};

