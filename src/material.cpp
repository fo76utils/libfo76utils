
#include "common.hpp"
#include "material.hpp"
#include "filebuf.hpp"

unsigned int CE2MaterialDB::hashFunction(const void *p, size_t n)
{
  unsigned int  crcValue = 0U;
  for (size_t i = 0; i < n; i++)
  {
    unsigned char c = reinterpret_cast< const unsigned char * >(p)[i];
    if (c >= 0x41 && c <= 0x5A)         // 'A' - 'Z'
      c = c | 0x20;                     // convert to lower case
    else if (c == 0x2F)                 // '/'
      c = 0x5C;                         // convert to '\\'
    crcValue = (crcValue >> 8) ^ crc32Table[(crcValue ^ c) & 0xFFU];
  }
  return crcValue;
}

int CE2MaterialDB::findString(const std::string& s)
{
  size_t  n0 = 0;
  size_t  n2 = sizeof(stringTable) / sizeof(char *);
  while (n2 > (n0 + 1))
  {
    size_t  n1 = (n0 + n2) >> 1;
    if (s.compare(stringTable[n1]) < 0)
      n2 = n1;
    else
      n0 = n1;
  }
  if (n2 > n0 && s == stringTable[n0])
    return int(n0);
  return -1;
}

int CE2MaterialDB::findString(unsigned int strtOffs) const
{
  size_t  n0 = 0;
  size_t  n2 = stringMap.size();
  while (n2 > (n0 + 1))
  {
    size_t  n1 = (n0 + n2) >> 1;
    if (strtOffs < (stringMap[n1] & 0xFFFFFFFFU))
      n2 = n1;
    else
      n0 = n1;
  }
  if (n2 > n0 && strtOffs == (stringMap[n0] & 0xFFFFFFFFU))
    return int(stringMap[n0] >> 32);
  return -1;
}

CE2MaterialDB::CE2MaterialDB(const BA2File& ba2File, const char *fileName)
{
  std::vector< unsigned char >  fileBuf;
  std::string stringBuf;
  if (!fileName || fileName[0] == '\0')
    fileName = "materials/materialsbeta.cdb";
  ba2File.extractFile(fileBuf, std::string(fileName));
  FileBuffer  buf(fileBuf.data(), fileBuf.size());
  if (buf.readUInt64() != 0x0000000848544542ULL)        // "BETH", 8
    errorMessage("invalid or unsupported material database file");
  (void) buf.readUInt32();              // unknown, 4
  for (unsigned int chunkCnt = buf.readUInt32(); chunkCnt > 1U; chunkCnt--)
  {
    unsigned int  chunkType = buf.readUInt32();
    unsigned int  chunkSize = buf.readUInt32();
    std::printf("Chunk type = %c%c%c%c, size = %8u bytes\n",
                char(chunkType & 0x7FU), char((chunkType >> 8) & 0x7FU),
                char((chunkType >> 16) & 0x7FU),
                char((chunkType >> 24) & 0x7FU), chunkSize);
    if ((buf.getPosition() + std::uint64_t(chunkSize)) > buf.size())
      errorMessage("unexpected end of material database file");
    FileBuffer  buf2(buf.data() + buf.getPosition(), chunkSize);
    buf.setPosition(buf.getPosition() + chunkSize);
    if (chunkType == 0x46464944U)               // "DIFF"
    {
      int     t = findString(buf2.readUInt32());
      if (t >= 0)
        std::printf("    %s\n", stringTable[t]);
      else
        std::printf("Warning: unrecognized string ID in DIFF chunk\n");
      if (t == 39 ||                            // "BSComponentDB::CTName"
          t == 74)                              // "BSMaterial::MRTextureFile"
      {
        std::printf("    0x%04X", (unsigned int) buf2.readUInt16());
        unsigned int  len = buf2.readUInt16();
        if (t == 74)
          buf2.readPath(stringBuf, len, "textures/", ".dds");
        else
          buf2.readString(stringBuf, len);
        std::printf(", \"%s\"\n", stringBuf.c_str());
      }
      else if (chunkSize == 16U)
      {
        std::printf("    0x%08X", buf2.readUInt32());
        std::printf(", 0x%08X", buf2.readUInt32());
        std::printf(", 0x%08X\n", buf2.readUInt32());
      }
    }
    else if (chunkType == 0x5453494CU)          // "LIST"
    {
      int     t = findString(buf2.readUInt32());
      if (t >= 0)
        std::printf("    %s\n", stringTable[t]);
      else
        std::printf("Warning: unrecognized string ID in LIST chunk\n");
      if (t == 37)              // "BSComponentDB2::DBFileIndex::ObjectInfo"
      {
        for (unsigned int n = buf2.readUInt32(); n; n--)
        {
          // hash of the base name without extension
          std::printf("    0x%08X", buf2.readUInt32());
          // extension ("mat\0")
          std::printf(", 0x%08X", buf2.readUInt32());
          // hash of the directory name
          std::printf(", 0x%08X", buf2.readUInt32());
          // object ID
          std::printf(", 0x%08X", buf2.readUInt32());
          // parent object ID?
          std::printf(", 0x%08X", buf2.readUInt32());
          // 0 if the previous value is 0, 1 otherwise
          std::printf(", 0x%02X\n", buf2.readUInt8());
        }
      }
    }
    else if (chunkType == 0x4350414DU)          // "MAPC"
    {
      int     t = findString(buf2.readUInt32());
      if (t >= 0)
        std::printf("    %s\n", stringTable[t]);
      else
        std::printf("Warning: unrecognized string ID in MAPC chunk\n");
      if (t == 107)                             // "BSResource::ID"
      {
        (void) buf2.readUInt32();
        for (unsigned int n = buf2.readUInt32(); n; n--)
        {
          // hash of the base name without extension
          std::printf("    0x%08X", buf2.readUInt32());
          // extension ("mat\0")
          std::printf(", 0x%08X", buf2.readUInt32());
          // hash of the directory name
          std::printf(", 0x%08X", buf2.readUInt32());
          // unknown hash
          std::printf(", 0x%08X", buf2.readUInt32());
          // unknown hash
          std::printf(", 0x%08X\n", buf2.readUInt32());
        }
      }
    }
    else if (chunkType == 0x54525453U)          // "STRT" (string table)
    {
      while (buf2.getPosition() < buf2.size())
      {
        unsigned int  strtOffs = (unsigned int) buf2.getPosition();
        buf2.readString(stringBuf);
        int     stringNum = findString(stringBuf);
        if (stringNum >= 0)
        {
          stringMap.push_back((std::uint64_t(stringNum) << 32) | strtOffs);
        }
        else
        {
          std::printf("Warning: unrecognized string in material database: %s\n",
                      stringBuf.c_str());
        }
      }
    }
  }
}

CE2MaterialDB::~CE2MaterialDB()
{
}

#if 0
void CE2MaterialDB::printTables(const BA2File& ba2File, const char *fileName)
{
  std::printf("const std::uint32_t CE2MaterialDB::crc32Table[256] =\n");
  std::printf("{\n");
  for (size_t i = 0; i < 256; i++)
  {
    unsigned int  crcValue = (unsigned int) i;
    for (size_t j = 0; j < 8; j++)
      crcValue = (crcValue >> 1) ^ (0xEDB88320U & (0U - (crcValue & 1U)));
    if ((i % 6) == 0)
      std::fputc(' ', stdout);
    std::printf(" 0x%08X", crcValue);
    if (i == 255)
      std::fputc('\n', stdout);
    else if ((i % 6) != 5)
      std::fputc(',', stdout);
    else
      std::printf(",\n");
  }
  std::printf("};\n\n");

  std::vector< unsigned char >  fileBuf;
  if (!fileName || fileName[0] == '\0')
    fileName = "materials/materialsbeta.cdb";
  ba2File.extractFile(fileBuf, std::string(fileName));
  FileBuffer  buf(fileBuf.data(), fileBuf.size());
  if (buf.readUInt64() != 0x0000000848544542ULL)        // "BETH", 8
    errorMessage("invalid or unsupported material database file");
  (void) buf.readUInt32();              // unknown, 4
  std::vector< std::string >  stringTable;
  for (unsigned int chunkCnt = buf.readUInt32(); chunkCnt > 1U; chunkCnt--)
  {
    unsigned int  chunkType = buf.readUInt32();
    unsigned int  chunkSize = buf.readUInt32();
    if ((buf.getPosition() + std::uint64_t(chunkSize)) > buf.size())
      errorMessage("unexpected end of material database file");
    FileBuffer  buf2(buf.data() + buf.getPosition(), chunkSize);
    buf.setPosition(buf.getPosition() + chunkSize);
    if (chunkType == 0x54525453U)               // "STRT" (string table)
    {
      while (buf2.getPosition() < buf2.size())
      {
        stringTable.emplace_back();
        buf2.readString(stringTable.back());
      }
    }
  }
  std::sort(stringTable.begin(), stringTable.end());
  std::printf("const char * CE2MaterialDB::stringTable[%d] =\n",
              int(stringTable.size()));
  std::printf("{\n");
  for (size_t i = 0; i < stringTable.size(); i++)
  {
    std::printf("  \"%s\"%c",
                stringTable[i].c_str(),
                ((i + 1) < stringTable.size() ? ',' : ' '));
    for (size_t j = stringTable[i].length(); j < 51; j++)
      std::fputc(' ', stdout);
    std::printf("// %3d\n", int(i));
  }
  std::printf("};\n");
}
#endif

const std::uint32_t CE2MaterialDB::crc32Table[256] =
{
  0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
  0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
  0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
  0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
  0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
  0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
  0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
  0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
  0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
  0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
  0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
  0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
  0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
  0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
  0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
  0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
  0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
  0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
  0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
  0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
  0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
  0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
  0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
  0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
  0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
  0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
  0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
  0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
  0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
  0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
  0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
  0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
  0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
  0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
  0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
  0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
  0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
  0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
  0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
  0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
  0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
  0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
  0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

const char * CE2MaterialDB::stringTable[451] =
{
  "AOVertexColorChannel",                               //   0
  "ActiveLayersMask",                                   //   1
  "AdaptiveEmittance",                                  //   2
  "Address",                                            //   3
  "AlbedoPaletteTex",                                   //   4
  "AlbedoTint",                                         //   5
  "AlbedoTintColor",                                    //   6
  "AlphaBias",                                          //   7
  "AlphaChannel",                                       //   8
  "AlphaDistance",                                      //   9
  "AlphaPaletteTex",                                    //  10
  "AlphaTestThreshold",                                 //  11
  "AlphaTint",                                          //  12
  "AnimatedDecalIgnoresTAA",                            //  13
  "ApplyFlowOnANMR",                                    //  14
  "ApplyFlowOnEmissivity",                              //  15
  "ApplyFlowOnOpacity",                                 //  16
  "BSBind::Address",                                    //  17
  "BSBind::ColorCurveController",                       //  18
  "BSBind::ComponentProperty",                          //  19
  "BSBind::ControllerComponent",                        //  20
  "BSBind::Controllers",                                //  21
  "BSBind::Controllers::Mapping",                       //  22
  "BSBind::Directory",                                  //  23
  "BSBind::DirectoryComponent",                         //  24
  "BSBind::Float2DCurveController",                     //  25
  "BSBind::Float2DLerpController",                      //  26
  "BSBind::Float3DCurveController",                     //  27
  "BSBind::FloatCurveController",                       //  28
  "BSBind::FloatLerpController",                        //  29
  "BSBind::Multiplex",                                  //  30
  "BSBind::TimerController",                            //  31
  "BSColorCurve",                                       //  32
  "BSComponentDB2::DBFileIndex",                        //  33
  "BSComponentDB2::DBFileIndex::ComponentInfo",         //  34
  "BSComponentDB2::DBFileIndex::ComponentTypeInfo",     //  35
  "BSComponentDB2::DBFileIndex::EdgeInfo",              //  36
  "BSComponentDB2::DBFileIndex::ObjectInfo",            //  37
  "BSComponentDB2::ID",                                 //  38
  "BSComponentDB::CTName",                              //  39
  "BSFloat2DCurve",                                     //  40
  "BSFloat3DCurve",                                     //  41
  "BSFloatCurve",                                       //  42
  "BSFloatCurve::Control",                              //  43
  "BSMaterial::AlphaBlenderSettings",                   //  44
  "BSMaterial::AlphaSettingsComponent",                 //  45
  "BSMaterial::BlendModeComponent",                     //  46
  "BSMaterial::BlendParamFloat",                        //  47
  "BSMaterial::BlenderID",                              //  48
  "BSMaterial::Channel",                                //  49
  "BSMaterial::CollisionComponent",                     //  50
  "BSMaterial::Color",                                  //  51
  "BSMaterial::ColorChannelTypeComponent",              //  52
  "BSMaterial::ColorRemapSettingsComponent",            //  53
  "BSMaterial::DecalSettingsComponent",                 //  54
  "BSMaterial::DetailBlenderSettings",                  //  55
  "BSMaterial::DetailBlenderSettingsComponent",         //  56
  "BSMaterial::DistortionComponent",                    //  57
  "BSMaterial::EffectSettingsComponent",                //  58
  "BSMaterial::EmissiveSettingsComponent",              //  59
  "BSMaterial::EmittanceSettings",                      //  60
  "BSMaterial::EyeSettingsComponent",                   //  61
  "BSMaterial::FlipbookComponent",                      //  62
  "BSMaterial::FlowSettingsComponent",                  //  63
  "BSMaterial::GlobalLayerDataComponent",               //  64
  "BSMaterial::GlobalLayerNoiseSettings",               //  65
  "BSMaterial::HairSettingsComponent",                  //  66
  "BSMaterial::Internal::CompiledDB",                   //  67
  "BSMaterial::Internal::CompiledDB::FilePair",         //  68
  "BSMaterial::LODMaterialID",                          //  69
  "BSMaterial::LayerID",                                //  70
  "BSMaterial::LayeredEdgeFalloffComponent",            //  71
  "BSMaterial::LayeredEmissivityComponent",             //  72
  "BSMaterial::LevelOfDetailSettings",                  //  73
  "BSMaterial::MRTextureFile",                          //  74
  "BSMaterial::MaterialID",                             //  75
  "BSMaterial::MaterialOverrideColorTypeComponent",     //  76
  "BSMaterial::MaterialParamFloat",                     //  77
  "BSMaterial::MouthSettingsComponent",                 //  78
  "BSMaterial::Offset",                                 //  79
  "BSMaterial::OpacityComponent",                       //  80
  "BSMaterial::ParamBool",                              //  81
  "BSMaterial::PhysicsMaterialType",                    //  82
  "BSMaterial::ProjectedDecalSettings",                 //  83
  "BSMaterial::Scale",                                  //  84
  "BSMaterial::ShaderModelComponent",                   //  85
  "BSMaterial::ShaderRouteComponent",                   //  86
  "BSMaterial::SourceTextureWithReplacement",           //  87
  "BSMaterial::StarmapBodyEffectComponent",             //  88
  "BSMaterial::TerrainSettingsComponent",               //  89
  "BSMaterial::TerrainTintSettingsComponent",           //  90
  "BSMaterial::TextureAddressModeComponent",            //  91
  "BSMaterial::TextureFile",                            //  92
  "BSMaterial::TextureReplacement",                     //  93
  "BSMaterial::TextureResolutionSetting",               //  94
  "BSMaterial::TextureSetID",                           //  95
  "BSMaterial::TextureSetKindComponent",                //  96
  "BSMaterial::TranslucencySettings",                   //  97
  "BSMaterial::TranslucencySettingsComponent",          //  98
  "BSMaterial::UVStreamID",                             //  99
  "BSMaterial::UVStreamParamBool",                      // 100
  "BSMaterial::VegetationSettingsComponent",            // 101
  "BSMaterial::WaterFoamSettingsComponent",             // 102
  "BSMaterial::WaterGrimeSettingsComponent",            // 103
  "BSMaterial::WaterSettingsComponent",                 // 104
  "BSMaterialBinding::MaterialPropertyNode",            // 105
  "BSMaterialBinding::MaterialUVStreamPropertyNode",    // 106
  "BSResource::ID",                                     // 107
  "BackLightingEnable",                                 // 108
  "BackLightingTintColor",                              // 109
  "BacklightingScale",                                  // 110
  "BacklightingSharpness",                              // 111
  "BacklightingTransparencyFactor",                     // 112
  "BackscatterStrength",                                // 113
  "BackscatterWrap",                                    // 114
  "BaseFrequency",                                      // 115
  "Bias",                                               // 116
  "Binding",                                            // 117
  "BindingType",                                        // 118
  "BlendContrast",                                      // 119
  "BlendMaskContrast",                                  // 120
  "BlendMaskPosition",                                  // 121
  "BlendMode",                                          // 122
  "BlendNormalsAdditively",                             // 123
  "BlendPosition",                                      // 124
  "BlendSoftness",                                      // 125
  "Blender",                                            // 126
  "BlendingMode",                                       // 127
  "BlueChannel",                                        // 128
  "BlurStrength",                                       // 129
  "BranchFlexibility",                                  // 130
  "BuildVersion",                                       // 131
  "CameraDistanceFade",                                 // 132
  "Children",                                           // 133
  "Circular",                                           // 134
  "Class",                                              // 135
  "ClassReference",                                     // 136
  "Collisions",                                         // 137
  "Color",                                              // 138
  "Columns",                                            // 139
  "ComponentIndex",                                     // 140
  "ComponentType",                                      // 141
  "ComponentTypes",                                     // 142
  "Components",                                         // 143
  "ContactShadowSoftening",                             // 144
  "Contrast",                                           // 145
  "Controller",                                         // 146
  "Controls",                                           // 147
  "CorneaEyeRoughness",                                 // 148
  "CorneaSpecularity",                                  // 149
  "Curve",                                              // 150
  "DBID",                                               // 151
  "DEPRECATEDTerrainBlendGradientFactor",               // 152
  "DEPRECATEDTerrainBlendStrength",                     // 153
  "DefaultValue",                                       // 154
  "DepthBiasInUlp",                                     // 155
  "DepthMVFixup",                                       // 156
  "DepthMVFixupEdgesOnly",                              // 157
  "DepthOffsetMaskVertexColorChannel",                  // 158
  "DepthScale",                                         // 159
  "DetailBlendMask",                                    // 160
  "DetailBlendMaskUVStream",                            // 161
  "DetailBlenderSettings",                              // 162
  "DiffuseTransmissionScale",                           // 163
  "Dir",                                                // 164
  "DirectTransmissionScale",                            // 165
  "DirectionalityIntensity",                            // 166
  "DirectionalitySaturation",                           // 167
  "DirectionalityScale",                                // 168
  "DisplacementMidpoint",                               // 169
  "DitherDistanceMax",                                  // 170
  "DitherDistanceMin",                                  // 171
  "DitherScale",                                        // 172
  "Duration",                                           // 173
  "Easing",                                             // 174
  "Edge",                                               // 175
  "EdgeMaskContrast",                                   // 176
  "EdgeMaskDistanceMax",                                // 177
  "EdgeMaskDistanceMin",                                // 178
  "EdgeMaskMin",                                        // 179
  "Edges",                                              // 180
  "EmissiveClipThreshold",                              // 181
  "EmissiveMaskSourceBlender",                          // 182
  "EmissiveOnlyAutomaticallyApplied",                   // 183
  "EmissiveOnlyEffect",                                 // 184
  "EmissivePaletteTex",                                 // 185
  "EmissiveSourceLayer",                                // 186
  "EmissiveTint",                                       // 187
  "EnableAdaptiveLimits",                               // 188
  "Enabled",                                            // 189
  "End",                                                // 190
  "ExposureOffset",                                     // 191
  "Ext",                                                // 192
  "FPS",                                                // 193
  "FalloffStartAngle",                                  // 194
  "FalloffStartAngles",                                 // 195
  "FalloffStartOpacities",                              // 196
  "FalloffStartOpacity",                                // 197
  "FalloffStopAngle",                                   // 198
  "FalloffStopAngles",                                  // 199
  "FalloffStopOpacities",                               // 200
  "FalloffStopOpacity",                                 // 201
  "FarFadeValue",                                       // 202
  "File",                                               // 203
  "FileName",                                           // 204
  "First",                                              // 205
  "FirstBlenderIndex",                                  // 206
  "FirstBlenderMode",                                   // 207
  "FirstLayerIndex",                                    // 208
  "FirstLayerMaskIndex",                                // 209
  "FirstLayerTint",                                     // 210
  "FlipBackFaceNormalsInViewSpace",                     // 211
  "FlowExtent",                                         // 212
  "FlowIsAnimated",                                     // 213
  "FlowMap",                                            // 214
  "FlowMapAndTexturesAreFlipbooks",                     // 215
  "FlowSourceUVChannel",                                // 216
  "FlowSpeed",                                          // 217
  "FlowUVOffset",                                       // 218
  "FlowUVScale",                                        // 219
  "FoamTextureDistortion",                              // 220
  "FoamTextureScrollSpeed",                             // 221
  "ForceRenderBeforeOIT",                               // 222
  "FrequencyMultiplier",                                // 223
  "Frosting",                                           // 224
  "FrostingBlurBias",                                   // 225
  "FrostingUnblurredBackgroundAlphaBlend",              // 226
  "GlobalLayerNoiseData",                               // 227
  "GreenChannel",                                       // 228
  "HasData",                                            // 229
  "HasOpacity",                                         // 230
  "HashMap",                                            // 231
  "HeightBlendFactor",                                  // 232
  "HeightBlendThreshold",                               // 233
  "HurstExponent",                                      // 234
  "ID",                                                 // 235
  "Index",                                              // 236
  "IndirectSpecRoughness",                              // 237
  "IndirectSpecularScale",                              // 238
  "IndirectSpecularTransmissionScale",                  // 239
  "Input",                                              // 240
  "InputDistance",                                      // 241
  "IrisDepthPosition",                                  // 242
  "IrisDepthTransitionRatio",                           // 243
  "IrisSpecularity",                                    // 244
  "IrisTotalDepth",                                     // 245
  "IrisUVSize",                                         // 246
  "IsAFlipbook",                                        // 247
  "IsAlphaTested",                                      // 248
  "IsDecal",                                            // 249
  "IsDetailBlendMaskSupported",                         // 250
  "IsEmpty",                                            // 251
  "IsGlass",                                            // 252
  "IsPlanet",                                           // 253
  "IsProjected",                                        // 254
  "IsSampleInterpolating",                              // 255
  "IsSpikyHair",                                        // 256
  "IsTeeth",                                            // 257
  "LayerIndex",                                         // 258
  "LeafAmplitude",                                      // 259
  "LeafFrequency",                                      // 260
  "LightingPower",                                      // 261
  "LightingWrap",                                       // 262
  "Loop",                                               // 263
  "Loops",                                              // 264
  "LowLOD",                                             // 265
  "LowLODRootMaterial",                                 // 266
  "LuminousEmittance",                                  // 267
  "MappingsA",                                          // 268
  "Mask",                                               // 269
  "MaskDistanceFromShoreEnd",                           // 270
  "MaskDistanceFromShoreStart",                         // 271
  "MaskDistanceRampWidth",                              // 272
  "MaskIntensityMax",                                   // 273
  "MaskIntensityMin",                                   // 274
  "MaskNoiseAmp",                                       // 275
  "MaskNoiseAnimSpeed",                                 // 276
  "MaskNoiseBias",                                      // 277
  "MaskNoiseFreq",                                      // 278
  "MaskNoiseGlobalScale",                               // 279
  "MaskWaveParallax",                                   // 280
  "MaterialMaskIntensityScale",                         // 281
  "MaterialOverallAlpha",                               // 282
  "MaterialTypeOverride",                               // 283
  "MaxConcentrationPlankton",                           // 284
  "MaxConcentrationSediment",                           // 285
  "MaxConcentrationYellowMatter",                       // 286
  "MaxDepthOffset",                                     // 287
  "MaxDisplacement",                                    // 288
  "MaxInput",                                           // 289
  "MaxOffsetEmittance",                                 // 290
  "MaxParralaxOcclusionSteps",                          // 291
  "MaxValue",                                           // 292
  "MediumLODRootMaterial",                              // 293
  "MinInput",                                           // 294
  "MinOffsetEmittance",                                 // 295
  "MinValue",                                           // 296
  "MipBase",                                            // 297
  "Mode",                                               // 298
  "MostSignificantLayer",                               // 299
  "Name",                                               // 300
  "NearFadeValue",                                      // 301
  "NoHalfResOptimization",                              // 302
  "Nodes",                                              // 303
  "NoiseMaskTexture",                                   // 304
  "NormalAffectsStrength",                              // 305
  "NormalOverride",                                     // 306
  "NumLODMaterials",                                    // 307
  "Object",                                             // 308
  "ObjectID",                                           // 309
  "Objects",                                            // 310
  "OpacitySourceLayer",                                 // 311
  "OpacityUVStream",                                    // 312
  "Optimized",                                          // 313
  "ParallaxOcclusionScale",                             // 314
  "ParallaxOcclusionShadows",                           // 315
  "Parent",                                             // 316
  "Path",                                               // 317
  "PathStr",                                            // 318
  "PersistentID",                                       // 319
  "PhytoplanktonReflectanceColorB",                     // 320
  "PhytoplanktonReflectanceColorG",                     // 321
  "PhytoplanktonReflectanceColorR",                     // 322
  "PlacedWater",                                        // 323
  "Position",                                           // 324
  "ProjectedDecalSetting",                              // 325
  "ReceiveDirectionalShadows",                          // 326
  "ReceiveNonDirectionalShadows",                       // 327
  "RedChannel",                                         // 328
  "ReflectanceB",                                       // 329
  "ReflectanceG",                                       // 330
  "ReflectanceR",                                       // 331
  "RemapAlbedo",                                        // 332
  "RemapEmissive",                                      // 333
  "RemapOpacity",                                       // 334
  "RenderLayer",                                        // 335
  "Replacement",                                        // 336
  "ResolutionHint",                                     // 337
  "RotationAngle",                                      // 338
  "Roughness",                                          // 339
  "Route",                                              // 340
  "Rows",                                               // 341
  "SSSStrength",                                        // 342
  "SSSWidth",                                           // 343
  "ScleraEyeRoughness",                                 // 344
  "ScleraSpecularity",                                  // 345
  "Second",                                             // 346
  "SecondBlenderIndex",                                 // 347
  "SecondBlenderMode",                                  // 348
  "SecondLayerActive",                                  // 349
  "SecondLayerIndex",                                   // 350
  "SecondLayerMaskIndex",                               // 351
  "SecondLayerTint",                                    // 352
  "SecondMostSignificantLayer",                         // 353
  "SedimentReflectanceColorB",                          // 354
  "SedimentReflectanceColorG",                          // 355
  "SedimentReflectanceColorR",                          // 356
  "Settings",                                           // 357
  "SoftEffect",                                         // 358
  "SoftFalloffDepth",                                   // 359
  "SourceDirection",                                    // 360
  "SourceDirectoryHash",                                // 361
  "SourceID",                                           // 362
  "SpecLobe0RoughnessScale",                            // 363
  "SpecLobe1RoughnessScale",                            // 364
  "SpecScale",                                          // 365
  "SpecularOpacityOverride",                            // 366
  "SpecularTransmissionScale",                          // 367
  "Start",                                              // 368
  "StreamID",                                           // 369
  "Strength",                                           // 370
  "SurfaceHeightMap",                                   // 371
  "Tangent",                                            // 372
  "TangentBend",                                        // 373
  "TargetID",                                           // 374
  "TargetUVStream",                                     // 375
  "TerrainBlendGradientFactor",                         // 376
  "TerrainBlendStrength",                               // 377
  "TexcoordScaleAndBias",                               // 378
  "TexcoordScale_XY",                                   // 379
  "TexcoordScale_XZ",                                   // 380
  "TexcoordScale_YZ",                                   // 381
  "Texture",                                            // 382
  "TextureMappingType",                                 // 383
  "Thin",                                               // 384
  "ThirdLayerActive",                                   // 385
  "ThirdLayerIndex",                                    // 386
  "ThirdLayerMaskIndex",                                // 387
  "ThirdLayerTint",                                     // 388
  "TilingDistance",                                     // 389
  "TransmissiveScale",                                  // 390
  "TransmittanceSourceLayer",                           // 391
  "TransmittanceWidth",                                 // 392
  "TrunkFlexibility",                                   // 393
  "Type",                                               // 394
  "UVStreamTargetBlender",                              // 395
  "UVStreamTargetLayer",                                // 396
  "UseDetailBlendMask",                                 // 397
  "UseDitheredTransparency",                            // 398
  "UseFallOff",                                         // 399
  "UseGBufferNormals",                                  // 400
  "UseNoiseMaskTexture",                                // 401
  "UseParallaxOcclusionMapping",                        // 402
  "UseRGBFallOff",                                      // 403
  "UseRandomOffset",                                    // 404
  "UseSSS",                                             // 405
  "UseVertexAlpha",                                     // 406
  "UseVertexColor",                                     // 407
  "UsesDirectionality",                                 // 408
  "Value",                                              // 409
  "VariationStrength",                                  // 410
  "Version",                                            // 411
  "VertexColorBlend",                                   // 412
  "VertexColorChannel",                                 // 413
  "VeryLowLODRootMaterial",                             // 414
  "WaterDepthBlur",                                     // 415
  "WaterEdgeFalloff",                                   // 416
  "WaterEdgeNormalFalloff",                             // 417
  "WaterRefractionMagnitude",                           // 418
  "WaterWetnessMaxDepth",                               // 419
  "WaveAmplitude",                                      // 420
  "WaveDistortionAmount",                               // 421
  "WaveFlipWaveDirection",                              // 422
  "WaveParallaxFalloffBias",                            // 423
  "WaveParallaxFalloffScale",                           // 424
  "WaveParallaxInnerStrength",                          // 425
  "WaveParallaxOuterStrength",                          // 426
  "WaveScale",                                          // 427
  "WaveShoreFadeInnerDistance",                         // 428
  "WaveShoreFadeOuterDistance",                         // 429
  "WaveSpawnFadeInDistance",                            // 430
  "WaveSpeed",                                          // 431
  "WorldspaceScaleFactor",                              // 432
  "WriteMask",                                          // 433
  "XCurve",                                             // 434
  "XMFLOAT2",                                           // 435
  "XMFLOAT3",                                           // 436
  "XMFLOAT4",                                           // 437
  "YCurve",                                             // 438
  "YellowMatterReflectanceColorB",                      // 439
  "YellowMatterReflectanceColorG",                      // 440
  "YellowMatterReflectanceColorR",                      // 441
  "ZCurve",                                             // 442
  "ZTest",                                              // 443
  "ZWrite",                                             // 444
  "upControllers",                                      // 445
  "upDir",                                              // 446
  "w",                                                  // 447
  "x",                                                  // 448
  "y",                                                  // 449
  "z"                                                   // 450
};
