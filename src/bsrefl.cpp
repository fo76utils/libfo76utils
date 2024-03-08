
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

const char * BSReflStream::stringTable[1151] =
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
  "IgnoresFog",                                         //  573
  "ImageSpaceSettings",                                 //  574
  "ImageSpaceSettings::AmbientOcclusionSettings",       //  575
  "ImageSpaceSettings::BloomSettings",                  //  576
  "ImageSpaceSettings::BlurSettings",                   //  577
  "ImageSpaceSettings::CinematicSettings",              //  578
  "ImageSpaceSettings::DepthOfFieldSettings",           //  579
  "ImageSpaceSettings::ExposureSettings",               //  580
  "ImageSpaceSettings::ExposureSettings::AutoExposureSettings",         //  581
  "ImageSpaceSettings::ExposureSettings::CameraExposureSettings",       //  582
  "ImageSpaceSettings::ExposureSettings::LuminanceHistogramSettings",   //  583
  "ImageSpaceSettings::FogSettings",                    //  584
  "ImageSpaceSettings::IndirectLightingSettings",       //  585
  "ImageSpaceSettings::RadialBlurSettings",             //  586
  "ImageSpaceSettings::SunAndSkySettings",              //  587
  "ImageSpaceSettings::ToneMappingSettings",            //  588
  "ImageSpaceSettings::VolumetricLightingSettings",     //  589
  "Index",                                              //  590
  "IndirectDiffuseMultiplier",                          //  591
  "IndirectLighting",                                   //  592
  "IndirectLightingSkyScaleOverride",                   //  593
  "IndirectLightingSkyTargetEv100",                     //  594
  "IndirectLightingSkyTargetStrength",                  //  595
  "IndirectSpecRoughness",                              //  596
  "IndirectSpecularMultiplier",                         //  597
  "IndirectSpecularScale",                              //  598
  "IndirectSpecularTransmissionScale",                  //  599
  "InitialAngularVelocity",                             //  600
  "InitialAngularVelocityNoise",                        //  601
  "InitialLinearVelocity",                              //  602
  "InitialLinearVelocityNoise",                         //  603
  "InitialOffset",                                      //  604
  "InnerFadeDistance",                                  //  605
  "InorganicResources",                                 //  606
  "Input",                                              //  607
  "InputDistance",                                      //  608
  "Instances",                                          //  609
  "IntensityValue",                                     //  610
  "InterpretAs",                                        //  611
  "Interval",                                           //  612
  "IrisDepthPosition",                                  //  613
  "IrisDepthTransitionRatio",                           //  614
  "IrisSpecularity",                                    //  615
  "IrisTotalDepth",                                     //  616
  "IrisUVSize",                                         //  617
  "IsAFlipbook",                                        //  618
  "IsAlphaTested",                                      //  619
  "IsDecal",                                            //  620
  "IsDetailBlendMaskSupported",                         //  621
  "IsEmpty",                                            //  622
  "IsGlass",                                            //  623
  "IsMembrane",                                         //  624
  "IsPermanent",                                        //  625
  "IsPlanet",                                           //  626
  "IsProjected",                                        //  627
  "IsSampleInterpolating",                              //  628
  "IsSpikyHair",                                        //  629
  "IsTeeth",                                            //  630
  "Key",                                                //  631
  "KeyMaterialPathForm",                                //  632
  "Keywords",                                           //  633
  "KillSound",                                          //  634
  "LODMeshOverrides",                                   //  635
  "Label",                                              //  636
  "Layer",                                              //  637
  "LayerIndex",                                         //  638
  "Layers",                                             //  639
  "LeafAmplitude",                                      //  640
  "LeafFrequency",                                      //  641
  "LeafSecondaryMotionAmount",                          //  642
  "LeafSecondaryMotionCutOff",                          //  643
  "LensFlareAttachmentComponent",                       //  644
  "LensFlareAttachmentDefine",                          //  645
  "LensFlareCloudOcclusionStrength",                    //  646
  "LensFlareDefines",                                   //  647
  "LightAttachmentConfiguration",                       //  648
  "LightAttachmentDefine",                              //  649
  "LightAttachmentFormComponent",                       //  650
  "LightAttachmentRef",                                 //  651
  "LightLuminanceMultiplier",                           //  652
  "LightingPower",                                      //  653
  "LightingWrap",                                       //  654
  "LightningColor",                                     //  655
  "LightningDistanceMax",                               //  656
  "LightningDistanceMin",                               //  657
  "LightningFieldOfView",                               //  658
  "LightningStrikeEffect",                              //  659
  "Loop",                                               //  660
  "LoopSegment",                                        //  661
  "Loops",                                              //  662
  "LowGUID",                                            //  663
  "LowLOD",                                             //  664
  "LowLODRootMaterial",                                 //  665
  "LuminanceAtTime",                                    //  666
  "LuminanceHistogram",                                 //  667
  "LuminousEmittance",                                  //  668
  "Map",                                                //  669
  "MapMetadata",                                        //  670
  "MappingsA",                                          //  671
  "Mask",                                               //  672
  "MaskDistanceFromShoreEnd",                           //  673
  "MaskDistanceFromShoreStart",                         //  674
  "MaskDistanceRampWidth",                              //  675
  "MaskIntensityMax",                                   //  676
  "MaskIntensityMin",                                   //  677
  "MaskNoiseAmp",                                       //  678
  "MaskNoiseAnimSpeed",                                 //  679
  "MaskNoiseBias",                                      //  680
  "MaskNoiseFreq",                                      //  681
  "MaskNoiseGlobalScale",                               //  682
  "MaskWaveParallax",                                   //  683
  "Material",                                           //  684
  "MaterialMaskIntensityScale",                         //  685
  "MaterialOverallAlpha",                               //  686
  "MaterialPath",                                       //  687
  "MaterialProperty",                                   //  688
  "MaterialTypeOverride",                               //  689
  "Materials",                                          //  690
  "Max",                                                //  691
  "MaxConcentrationPlankton",                           //  692
  "MaxConcentrationSediment",                           //  693
  "MaxConcentrationYellowMatter",                       //  694
  "MaxDelay",                                           //  695
  "MaxDepthOffset",                                     //  696
  "MaxDisplacement",                                    //  697
  "MaxFogDensity",                                      //  698
  "MaxFogThickness",                                    //  699
  "MaxIndex",                                           //  700
  "MaxInput",                                           //  701
  "MaxMeanFreePath",                                    //  702
  "MaxOffsetEmittance",                                 //  703
  "MaxParralaxOcclusionSteps",                          //  704
  "MaxValue",                                           //  705
  "MeanFreePath",                                       //  706
  "MediumLODRootMaterial",                              //  707
  "MeshFileOverride",                                   //  708
  "MeshLODDistanceOverride",                            //  709
  "MeshLODs",                                           //  710
  "MeshOverrides",                                      //  711
  "Metadata",                                           //  712
  "Mie",                                                //  713
  "MieCoef",                                            //  714
  "Min",                                                //  715
  "MinDelay",                                           //  716
  "MinFogDensity",                                      //  717
  "MinFogThickness",                                    //  718
  "MinIndex",                                           //  719
  "MinInput",                                           //  720
  "MinMeanFreePath",                                    //  721
  "MinOffsetEmittance",                                 //  722
  "MinValue",                                           //  723
  "MipBase",                                            //  724
  "Misc",                                               //  725
  "Mode",                                               //  726
  "MoleculesPerUnitVolume",                             //  727
  "MoonGlare",                                          //  728
  "Moonlight",                                          //  729
  "MostSignificantLayer",                               //  730
  "MotionBlurStrengthValue",                            //  731
  "Name",                                               //  732
  "NavMeshAreaFlag",                                    //  733
  "NavMeshSplineExtraData::ChunkData",                  //  734
  "NavMeshSplineExtraData::ChunkDataRef",               //  735
  "NearFadeValue",                                      //  736
  "NearOpacityValue",                                   //  737
  "NearPlaneValue",                                     //  738
  "NearStartValue",                                     //  739
  "NestedTracks",                                       //  740
  "NiPoint3",                                           //  741
  "NightPreset",                                        //  742
  "NoHalfResOptimization",                              //  743
  "NoLAngularDamping",                                  //  744
  "NoLinearDamping",                                    //  745
  "NoSky",                                              //  746
  "Node",                                               //  747
  "NodeAttachmentConfiguration",                        //  748
  "NodeName",                                           //  749
  "Nodes",                                              //  750
  "NoiseBias",                                          //  751
  "NoiseContribution",                                  //  752
  "NoiseMaskTexture",                                   //  753
  "NoiseScale",                                         //  754
  "NoiseScrollingVelocity",                             //  755
  "NormalAffectsStrength",                              //  756
  "NormalOverride",                                     //  757
  "NormalShadowStrength",                               //  758
  "NormalTexture",                                      //  759
  "NumLODMaterials",                                    //  760
  "Object",                                             //  761
  "ObjectAttachmentConfiguration",                      //  762
  "ObjectID",                                           //  763
  "Objects",                                            //  764
  "Offset",                                             //  765
  "Op",                                                 //  766
  "OpacitySourceLayer",                                 //  767
  "OpacityTexture",                                     //  768
  "OpacityUVStream",                                    //  769
  "Optimized",                                          //  770
  "Order",                                              //  771
  "OuterFadeDistance",                                  //  772
  "OverallBlendAmount",                                 //  773
  "OverrideAttachType",                                 //  774
  "OverrideInitialVelocities",                          //  775
  "OverrideLevelCount",                                 //  776
  "OverrideMaterial",                                   //  777
  "OverrideMaterialPath",                               //  778
  "OverrideMeshLODDistance",                            //  779
  "OverrideSearchMethod",                               //  780
  "OverrideToneMapping",                                //  781
  "Overrides",                                          //  782
  "OzoneAbsorptionCoef",                                //  783
  "ParallaxOcclusionScale",                             //  784
  "ParallaxOcclusionShadows",                           //  785
  "ParamIndex",                                         //  786
  "ParameterName",                                      //  787
  "ParametersA",                                        //  788
  "Parent",                                             //  789
  "ParticleAttachmentConfiguration",                    //  790
  "ParticleFormComponent",                              //  791
  "ParticleRenderState",                                //  792
  "ParticleSystemDefine",                               //  793
  "ParticleSystemDefineOverrides",                      //  794
  "ParticleSystemDefineRef",                            //  795
  "ParticleSystemPath",                                 //  796
  "ParticleSystemReferenceDefine",                      //  797
  "Path",                                               //  798
  "PathStr",                                            //  799
  "PersistentID",                                       //  800
  "PhytoplanktonReflectanceColorB",                     //  801
  "PhytoplanktonReflectanceColorG",                     //  802
  "PhytoplanktonReflectanceColorR",                     //  803
  "PlacedWater",                                        //  804
  "Planes",                                             //  805
  "PlanetStarGlowBackgroundScale",                      //  806
  "PlanetStarfieldBackgroundGIContribution",            //  807
  "PlanetStarfieldBackgroundScale",                     //  808
  "PlanetStarfieldStarBrightnessScale",                 //  809
  "PlayMode",                                           //  810
  "PlayOnCulledNodes",                                  //  811
  "PlayerHUDEffect",                                    //  812
  "Position",                                           //  813
  "PositionOffset",                                     //  814
  "PositionOffsetEnabled",                              //  815
  "PrecipFadeIn",                                       //  816
  "PrecipFadeOut",                                      //  817
  "Precipitation",                                      //  818
  "PreferREFR",                                         //  819
  "PreferREFREnabled",                                  //  820
  "PresetData",                                         //  821
  "PreviewModelPath",                                   //  822
  "PrewarmTime",                                        //  823
  "ProjectedDecalSetting",                              //  824
  "Properties",                                         //  825
  "Property",                                           //  826
  "RadialBlur",                                         //  827
  "RampDownValue",                                      //  828
  "RampUpValue",                                        //  829
  "RandomPlaySpeedRange",                               //  830
  "RandomTimeOffset",                                   //  831
  "Range",                                              //  832
  "RaycastDirection",                                   //  833
  "RaycastLength",                                      //  834
  "Rayleigh",                                           //  835
  "RayleighCoef",                                       //  836
  "ReceiveDirectionalShadows",                          //  837
  "ReceiveNonDirectionalShadows",                       //  838
  "ReciprocalTickRate",                                 //  839
  "RedChannel",                                         //  840
  "RefID",                                              //  841
  "ReferenceDefines",                                   //  842
  "References",                                         //  843
  "ReflectanceB",                                       //  844
  "ReflectanceG",                                       //  845
  "ReflectanceR",                                       //  846
  "ReflectionProbeCellComponent",                       //  847
  "ReflectionProbeInstanceData",                        //  848
  "RefractiveIndexOfAir",                               //  849
  "RemapAlbedo",                                        //  850
  "RemapEmissive",                                      //  851
  "RemapOpacity",                                       //  852
  "RenderLayer",                                        //  853
  "Replacement",                                        //  854
  "RequiresNodeType",                                   //  855
  "ResetOnEnd",                                         //  856
  "ResetOnLoop",                                        //  857
  "ResolutionHint",                                     //  858
  "RespectSceneTransform",                              //  859
  "RevertMaterialOnSequenceEnd",                        //  860
  "RotationAngle",                                      //  861
  "RotationOffset",                                     //  862
  "RotationOffsetEnabled",                              //  863
  "Roughness",                                          //  864
  "RoundToNearest",                                     //  865
  "Route",                                              //  866
  "Rows",                                               //  867
  "SSSStrength",                                        //  868
  "SSSWidth",                                           //  869
  "Saturation",                                         //  870
  "ScaleOffset",                                        //  871
  "ScaleOffsetEnabled",                                 //  872
  "Scattering",                                         //  873
  "ScatteringFar",                                      //  874
  "ScatteringScale",                                    //  875
  "ScatteringTransition",                               //  876
  "ScatteringVolumeFar",                                //  877
  "ScatteringVolumeNear",                               //  878
  "ScleraEyeRoughness",                                 //  879
  "ScleraSpecularity",                                  //  880
  "SearchFromTopLevelFadeNode",                         //  881
  "SearchMethod",                                       //  882
  "Second",                                             //  883
  "SecondBlenderIndex",                                 //  884
  "SecondBlenderMode",                                  //  885
  "SecondLayerActive",                                  //  886
  "SecondLayerIndex",                                   //  887
  "SecondLayerMaskIndex",                               //  888
  "SecondLayerTint",                                    //  889
  "SecondMostSignificantLayer",                         //  890
  "SedimentReflectanceColorB",                          //  891
  "SedimentReflectanceColorG",                          //  892
  "SedimentReflectanceColorR",                          //  893
  "SequenceID",                                         //  894
  "SequenceName",                                       //  895
  "Sequences",                                          //  896
  "Settings",                                           //  897
  "SettingsTemplate",                                   //  898
  "Shadows",                                            //  899
  "ShakeMultiplier",                                    //  900
  "ShakeType",                                          //  901
  "ShouldRescale",                                      //  902
  "SkyLightingMultiplier",                              //  903
  "SoftEffect",                                         //  904
  "SoftFalloffDepth",                                   //  905
  "Sound",                                              //  906
  "SoundEffects",                                       //  907
  "SoundHook",                                          //  908
  "Sounds",                                             //  909
  "SourceDirection",                                    //  910
  "SourceDirectoryHash",                                //  911
  "SourceID",                                           //  912
  "SpaceGlowBackgroundScaleOverride",                   //  913
  "SpawnMethod",                                        //  914
  "SpecLobe0RoughnessScale",                            //  915
  "SpecLobe1RoughnessScale",                            //  916
  "SpecScale",                                          //  917
  "SpecularOpacityOverride",                            //  918
  "SpecularTransmissionScale",                          //  919
  "Speed",                                              //  920
  "SpellData",                                          //  921
  "SpellItems",                                         //  922
  "SplineColor_Packed",                                 //  923
  "SplineSide",                                         //  924
  "StarfieldBackgroundScaleOverride",                   //  925
  "StarfieldStarBrightnessScaleOverride",               //  926
  "Stars",                                              //  927
  "Start",                                              //  928
  "StartEvent",                                         //  929
  "StartTargetIndex",                                   //  930
  "StartValue",                                         //  931
  "StaticVisibility",                                   //  932
  "StopEvent",                                          //  933
  "StreamID",                                           //  934
  "Strength",                                           //  935
  "StrengthValue",                                      //  936
  "SubWeathers",                                        //  937
  "Sun",                                                //  938
  "SunAndSky",                                          //  939
  "SunColor",                                           //  940
  "SunDiskIlluminanceScaleOverride",                    //  941
  "SunDiskIndirectIlluminanceScaleOverride",            //  942
  "SunDiskScreenSizeMax",                               //  943
  "SunDiskScreenSizeMin",                               //  944
  "SunDiskTexture",                                     //  945
  "SunGlare",                                           //  946
  "SunGlareColor",                                      //  947
  "SunIlluminance",                                     //  948
  "Sunlight",                                           //  949
  "SurfaceHeightMap",                                   //  950
  "SwapsCollection",                                    //  951
  "TESImageSpace",                                      //  952
  "TESImageSpaceModifier",                              //  953
  "Tangent",                                            //  954
  "TangentBend",                                        //  955
  "TargetID",                                           //  956
  "TargetUVStream",                                     //  957
  "TerrainBlendGradientFactor",                         //  958
  "TerrainBlendStrength",                               //  959
  "TerrainMatch",                                       //  960
  "TexcoordScaleAndBias",                               //  961
  "TexcoordScale_XY",                                   //  962
  "TexcoordScale_XZ",                                   //  963
  "TexcoordScale_YZ",                                   //  964
  "Texture",                                            //  965
  "TextureMappingType",                                 //  966
  "TextureShadowOffset",                                //  967
  "TextureShadowStrength",                              //  968
  "Thickness",                                          //  969
  "ThicknessNoiseBias",                                 //  970
  "ThicknessNoiseScale",                                //  971
  "ThicknessTexture",                                   //  972
  "Thin",                                               //  973
  "ThirdLayerActive",                                   //  974
  "ThirdLayerIndex",                                    //  975
  "ThirdLayerMaskIndex",                                //  976
  "ThirdLayerTint",                                     //  977
  "Threshold",                                          //  978
  "ThunderFadeIn",                                      //  979
  "ThunderFadeOut",                                     //  980
  "TicksPerSecond",                                     //  981
  "Tiling",                                             //  982
  "TilingDistance",                                     //  983
  "TilingPerKm",                                        //  984
  "Time",                                               //  985
  "TimeMultiplier",                                     //  986
  "TimelineStartTime",                                  //  987
  "TimelineUnitsPerPixel",                              //  988
  "Tint",                                               //  989
  "TintColorValue",                                     //  990
  "ToneMapE",                                           //  991
  "ToneMapping",                                        //  992
  "TopBlendDistanceKm",                                 //  993
  "TopBlendStartKm",                                    //  994
  "Tracks",                                             //  995
  "TransDelta",                                         //  996
  "TransformHandling",                                  //  997
  "TransformMode",                                      //  998
  "TransitionEndAngle",                                 //  999
  "TransitionStartAngle",                               // 1000
  "TransitionThreshold",                                // 1001
  "TransmissiveScale",                                  // 1002
  "TransmittanceSourceLayer",                           // 1003
  "TransmittanceWidth",                                 // 1004
  "TrunkFlexibility",                                   // 1005
  "TurbulenceDirectionAmplitude",                       // 1006
  "TurbulenceDirectionFrequency",                       // 1007
  "TurbulenceSpeedAmplitude",                           // 1008
  "TurbulenceSpeedFrequency",                           // 1009
  "Type",                                               // 1010
  "UVStreamTargetBlender",                              // 1011
  "UVStreamTargetLayer",                                // 1012
  "UniqueID",                                           // 1013
  "UseAsInteriorCriteria",                              // 1014
  "UseBoundsToScaleDissolve",                           // 1015
  "UseCustomCoefficients",                              // 1016
  "UseDetailBlendMask",                                 // 1017
  "UseDigitMask",                                       // 1018
  "UseDitheredTransparency",                            // 1019
  "UseFallOff",                                         // 1020
  "UseGBufferNormals",                                  // 1021
  "UseNodeLocalRotation",                               // 1022
  "UseNoiseMaskTexture",                                // 1023
  "UseOutdoorExposure",                                 // 1024
  "UseOutdoorLUT",                                      // 1025
  "UseOzoneAbsorptionApproximation",                    // 1026
  "UseParallaxOcclusionMapping",                        // 1027
  "UseRGBFallOff",                                      // 1028
  "UseRandomOffset",                                    // 1029
  "UseSSS",                                             // 1030
  "UseStartTargetIndex",                                // 1031
  "UseTargetForDepthOfField",                           // 1032
  "UseTargetForRadialBlur",                             // 1033
  "UseVertexAlpha",                                     // 1034
  "UseVertexColor",                                     // 1035
  "UseWorldAlignedRaycastDirection",                    // 1036
  "UserDuration",                                       // 1037
  "UsesDirectionality",                                 // 1038
  "Value",                                              // 1039
  "Variables",                                          // 1040
  "VariationStrength",                                  // 1041
  "Version",                                            // 1042
  "VertexColorBlend",                                   // 1043
  "VertexColorChannel",                                 // 1044
  "VerticalAngle",                                      // 1045
  "VerticalTiling",                                     // 1046
  "VeryLowLODRootMaterial",                             // 1047
  "VisibilityMultiplier",                               // 1048
  "Visible",                                            // 1049
  "VolatilityMultiplier",                               // 1050
  "VolumetricIndirectLightContribution",                // 1051
  "VolumetricLighting",                                 // 1052
  "VolumetricLightingDirectionalAnisoScale",            // 1053
  "VolumetricLightingDirectionalLightScale",            // 1054
  "WarmUp",                                             // 1055
  "WaterDepthBlur",                                     // 1056
  "WaterEdgeFalloff",                                   // 1057
  "WaterEdgeNormalFalloff",                             // 1058
  "WaterIndirectSpecularMultiplier",                    // 1059
  "WaterRefractionMagnitude",                           // 1060
  "WaterWetnessMaxDepth",                               // 1061
  "WaveAmplitude",                                      // 1062
  "WaveDistortionAmount",                               // 1063
  "WaveFlipWaveDirection",                              // 1064
  "WaveParallaxFalloffBias",                            // 1065
  "WaveParallaxFalloffScale",                           // 1066
  "WaveParallaxInnerStrength",                          // 1067
  "WaveParallaxOuterStrength",                          // 1068
  "WaveScale",                                          // 1069
  "WaveShoreFadeInnerDistance",                         // 1070
  "WaveShoreFadeOuterDistance",                         // 1071
  "WaveSpawnFadeInDistance",                            // 1072
  "WaveSpeed",                                          // 1073
  "WeatherActivateEffect",                              // 1074
  "WeatherChoice",                                      // 1075
  "Weight",                                             // 1076
  "WeightMultiplier",                                   // 1077
  "WhitePointValue",                                    // 1078
  "WindDirectionOverrideEnabled",                       // 1079
  "WindDirectionOverrideValue",                         // 1080
  "WindDirectionRange",                                 // 1081
  "WindScale",                                          // 1082
  "WindStrengthVariationMax",                           // 1083
  "WindStrengthVariationMin",                           // 1084
  "WindStrengthVariationSpeed",                         // 1085
  "WindTurbulence",                                     // 1086
  "WorldspaceScaleFactor",                              // 1087
  "WriteMask",                                          // 1088
  "XCurve",                                             // 1089
  "XMCOLOR",                                            // 1090
  "XMFLOAT2",                                           // 1091
  "XMFLOAT3",                                           // 1092
  "XMFLOAT4",                                           // 1093
  "YCurve",                                             // 1094
  "YellowMatterReflectanceColorB",                      // 1095
  "YellowMatterReflectanceColorG",                      // 1096
  "YellowMatterReflectanceColorR",                      // 1097
  "ZCurve",                                             // 1098
  "ZTest",                                              // 1099
  "ZWrite",                                             // 1100
  "a",                                                  // 1101
  "b",                                                  // 1102
  "g",                                                  // 1103
  "pArtForm",                                           // 1104
  "pClimateOverride",                                   // 1105
  "pCloudCardSequence",                                 // 1106
  "pClouds",                                            // 1107
  "pConditionForm",                                     // 1108
  "pConditions",                                        // 1109
  "pDefineCollection",                                  // 1110
  "pDisplayNameKeyword",                                // 1111
  "pEventForm",                                         // 1112
  "pExplosionForm",                                     // 1113
  "pExternalForm",                                      // 1114
  "pFalloff",                                           // 1115
  "pFormReference",                                     // 1116
  "pImageSpace",                                        // 1117
  "pImageSpaceDay",                                     // 1118
  "pImageSpaceForm",                                    // 1119
  "pImageSpaceNight",                                   // 1120
  "pImpactDataSet",                                     // 1121
  "pLensFlare",                                         // 1122
  "pLightForm",                                         // 1123
  "pLightningFX",                                       // 1124
  "pMainSpell",                                         // 1125
  "pOptionalPhotoModeEffect",                           // 1126
  "pParent",                                            // 1127
  "pParentForm",                                        // 1128
  "pPrecipitationEffect",                               // 1129
  "pPreviewForm",                                       // 1130
  "pProjectedDecalForm",                                // 1131
  "pSource",                                            // 1132
  "pSpell",                                             // 1133
  "pSunPresetOverride",                                 // 1134
  "pTimeOfDayData",                                     // 1135
  "pTransitionSpell",                                   // 1136
  "pVisualEffect",                                      // 1137
  "pVolumeticLighting",                                 // 1138
  "pWindForce",                                         // 1139
  "r",                                                  // 1140
  "spController",                                       // 1141
  "upControllers",                                      // 1142
  "upDir",                                              // 1143
  "upMap",                                              // 1144
  "upObjectToAttach",                                   // 1145
  "upObjectToSpawn",                                    // 1146
  "w",                                                  // 1147
  "x",                                                  // 1148
  "y",                                                  // 1149
  "z"                                                   // 1150
};

