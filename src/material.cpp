
#include "common.hpp"
#include "material.hpp"
#include "bsmatcdb.hpp"

#include <new>

static const std::uint32_t
    defaultTextureRepl[CE2Material::TextureSet::maxTexturePaths] =
{
  // color, normal, opacity, rough
  0xFF000000U, 0xFFFF8080U, 0xFFFFFFFFU, 0xFF000000U,
  // metal, ao, height, emissive
  0xFF000000U, 0xFFFFFFFFU, 0xFF000000U, 0xFF000000U,
  // transmissive, curvature, mask, unknown
  0xFF000000U, 0xFF808080U, 0xFF000000U, 0xFF808080U,
  // Z offset, unknown, overlay color, overlay roughness
  0xFF000000U, 0xFF000000U, 0xFF808080U, 0xFF808080U,
  // overlay metalness, unknown, unknown, unknown
  0xFF808080U, 0xFF000000U, 0xFF000000U, 0xFFFFFFFFU,
  // ID
  0xFF808080U
};

const std::string_view  CE2Material::emptyStringView("", 0);

CE2Material::CE2Material()
  : CE2MaterialObject(1),
    flags(0U),
    layerMask(0U),
    alphaThreshold(1.0f / 3.0f),
    shaderModel(31),            // "BaseMaterial"
    alphaSourceLayer(0),
    alphaBlendMode(0),          // "Linear"
    alphaVertexColorChannel(0), // "Red"
    alphaHeightBlendThreshold(0.0f),
    alphaHeightBlendFactor(0.05f),
    alphaPosition(0.5f),
    alphaContrast(0.0f),
    alphaUVStream(nullptr),
    shaderRoute(0),             // "Deferred"
    opacityLayer1(0),           // "MATERIAL_LAYER_0"
    opacityLayer2(1),           // "MATERIAL_LAYER_1"
    opacityBlender1(0),         // "BLEND_LAYER_0"
    opacityBlender1Mode(0),     // "Lerp"
    opacityLayer3(2),           // "MATERIAL_LAYER_2"
    opacityBlender2(1),         // "BLEND_LAYER_1"
    opacityBlender2Mode(0),     // "Lerp"
    specularOpacityOverride(0.0f),
    physicsMaterialType(0),     // "None"
    effectSettings(nullptr),
    emissiveSettings(nullptr),
    layeredEmissiveSettings(nullptr),
    translucencySettings(nullptr),
    decalSettings(nullptr),
    vegetationSettings(nullptr),
    detailBlenderSettings(nullptr),
    layeredEdgeFalloff(nullptr),
    waterSettings(nullptr),
    globalLayerData(nullptr),
    hairSettings(nullptr)
{
  for (size_t i = 0; i < maxLayers; i++)
    layers[i] = nullptr;
  for (size_t i = 0; i < maxBlenders; i++)
    blenders[i] = nullptr;
  for (size_t i = 0; i < maxLODMaterials; i++)
    lodMaterials[i] = nullptr;
}

CE2Material::Blender::Blender()
  : CE2MaterialObject(2),
    uvStream(nullptr),
    texturePath(&emptyStringView),
    textureReplacement(0xFFFFFFFFU),
    textureReplacementEnabled(false),
    blendMode(0),               // "Linear"
    colorChannel(0)             // "Red"
{
  floatParams[0] = 0.5f;        // height blend threshold
  floatParams[1] = 0.5f;        // height blend factor
  floatParams[2] = 0.5f;        // position
  floatParams[3] = 1.0f;        // 1.0 - contrast
  floatParams[4] = 1.0f;        // mask intensity
  boolParams[0] = true;         // blend color
  boolParams[1] = true;         // blend metalness
  boolParams[2] = true;         // blend roughness
  boolParams[3] = true;         // blend normals
  boolParams[4] = true;         // blend normals additively
  boolParams[5] = false;        // use vertex color
  boolParams[6] = true;         // blend ambient occlusion
  boolParams[7] = false;        // use detail blend mask
}

CE2Material::Layer::Layer()
  : CE2MaterialObject(3),
    material(nullptr),
    uvStream(nullptr)
{
}

CE2Material::Material::Material()
  : CE2MaterialObject(4),
    color(FloatVector4(1.0f, 1.0f, 1.0f, 0.0f)),
    colorModeFlags(1),          // "Lerp"
    flipbookFlags(0),
    flipbookColumns(1),
    flipbookRows(1),
    flipbookFPS(30.0f),
    textureSet(nullptr)
{
}

CE2Material::TextureSet::TextureSet()
  : CE2MaterialObject(5),
    texturePathMask(0U),
    floatParam(1.0f),           // normal map intensity
    textureReplacementMask(0U),
    resolutionHint(0),          // "Tiling"
    disableMipBiasHint(false)
{
  for (size_t i = 0; i < maxTexturePaths; i++)
    texturePaths[i] = &emptyStringView;
  for (size_t i = 0; i < maxTexturePaths; i++)
    textureReplacements[i] = defaultTextureRepl[i];
}

CE2Material::UVStream::UVStream()
  : CE2MaterialObject(6),
    scaleAndOffset(FloatVector4(1.0f, 1.0f, 0.0f, 0.0f)),
    textureAddressMode(0),      // "Wrap"
    channel(1)                  // "One"
{
}

CE2MaterialDB::StoredStdStringHashMap::StoredStdStringHashMap()
  : buf(nullptr),
    hashMask(0),
    size(0)
{
}

CE2MaterialDB::StoredStdStringHashMap::~StoredStdStringHashMap()
{
  std::free(buf);
}

void CE2MaterialDB::StoredStdStringHashMap::clear()
{
  static const std::string_view emptyString("");
  hashMask = 0;
  size = 0;
  expandBuffer();
  buf[0] = &emptyString;
  size = 1;
}

const std::string_view * CE2MaterialDB::StoredStdStringHashMap::insert(
    BSMaterialsCDB& cdb, const std::string& s)
{
  size_t  m = hashMask;
  size_t  i = ~(hashFunctionUInt32(s.c_str(), s.length())) & m;
  const std::string_view  *p;
  for ( ; (p = buf[i]) != nullptr; i = (i + 1) & m)
  {
    if (*p == s)
      return p;
  }
  std::string_view  *v = cdb.allocateObjects< std::string_view >(1);
  char    *t = cdb.allocateObjects< char >(s.length() + 1);
  std::memcpy(t, s.c_str(), s.length() + 1);
  buf[i] = new(v) std::string_view(t, s.length());
  size++;
  if ((size * std::uint64_t(3)) > (m * std::uint64_t(2))) [[unlikely]]
    expandBuffer();
  return v;
}

void CE2MaterialDB::StoredStdStringHashMap::expandBuffer()
{
  size_t  m = (hashMask << 1) | 0xFF;
  const std::string_view  **newBuf =
      reinterpret_cast< const std::string_view ** >(
          std::calloc(m + 1, sizeof(std::string_view *)));
  if (!newBuf)
    throw std::bad_alloc();
#if !(defined(__i386__) || defined(__x86_64__) || defined(__x86_64))
  for (size_t i = 0; i <= m; i++)
    newBuf[i] = nullptr;
#endif
  if (size)
  {
    for (size_t i = 0; i <= hashMask; i++)
    {
      const std::string_view  *s = buf[i];
      if (!s)
        continue;
      size_t  h = ~(hashFunctionUInt32(s->data(), s->length())) & m;
      while (newBuf[h])
        h = (h + 1) & m;
      newBuf[h] = s;
    }
  }
  std::free(buf);
  buf = newBuf;
  hashMask = m;
}

CE2MaterialDB::CE2MatObjectHashMap::CE2MatObjectHashMap()
  : buf(nullptr),
    hashMask(0),
    size(0)
{
  expandBuffer();
}

CE2MaterialDB::CE2MatObjectHashMap::~CE2MatObjectHashMap()
{
  std::free(buf);
}

void CE2MaterialDB::CE2MatObjectHashMap::clear()
{
  hashMask = 0;
  size = 0;
  expandBuffer();
}

void CE2MaterialDB::CE2MatObjectHashMap::storeObject(const CE2MaterialObject *o)
{
  BSResourceID  objectID(o->cdbObject->persistentID);
  size_t  m = hashMask;
  size_t  i = objectID.hashFunction() & m;
  const CE2MaterialObject *p;
  for ( ; (p = buf[i]) != nullptr; i = (i + 1) & m)
  {
    if (p->cdbObject->persistentID == objectID)
    {
      buf[i] = o;
      return;
    }
  }
  buf[i] = o;
  size++;
  if ((size * std::uint64_t(3)) > (m * std::uint64_t(2))) [[unlikely]]
    expandBuffer();
}

inline const CE2MaterialObject *
    CE2MaterialDB::CE2MatObjectHashMap::findObject(BSResourceID objectID) const
{
  size_t  m = hashMask;
  size_t  i = objectID.hashFunction() & m;
  const CE2MaterialObject *o;
  for ( ; (o = buf[i]) != nullptr; i = (i + 1) & m)
  {
    if (o->cdbObject->persistentID == objectID)
      return o;
  }
  return nullptr;
}

void CE2MaterialDB::CE2MatObjectHashMap::expandBuffer()
{
  size_t  m = (hashMask << 1) | 0x0FFF;
  const CE2MaterialObject **newBuf =
      reinterpret_cast< const CE2MaterialObject ** >(
          std::calloc(m + 1, sizeof(CE2MaterialObject *)));
  if (!newBuf)
    throw std::bad_alloc();
#if !(defined(__i386__) || defined(__x86_64__) || defined(__x86_64))
  for (size_t i = 0; i <= m; i++)
    newBuf[i] = nullptr;
#endif
  if (size)
  {
    for (size_t i = 0; i <= hashMask; i++)
    {
      const CE2MaterialObject *o = buf[i];
      if (!o)
        continue;
      size_t  h = o->cdbObject->persistentID.hashFunction() & m;
      while (newBuf[h])
        h = (h + 1) & m;
      newBuf[h] = o;
    }
  }
  std::free(buf);
  buf = newBuf;
  hashMask = m;
}

CE2MaterialObject * CE2MaterialDB::findMaterialObject(
    const BSMaterialsCDB::MaterialObject *p)
{
  if (!p) [[unlikely]]
    return nullptr;
  {
    const CE2MaterialObject *o = materialObjectMap.findObject(p->persistentID);
    if (o)
      return const_cast< CE2MaterialObject * >(o);
  }
  CE2MaterialObject *o = nullptr;
  {
    const BSMaterialsCDB::MaterialObject  *q = p;
    while (q->baseObject)
      q = q->baseObject;
    if (q->persistentID.dir != 0x1D95562F ||    // "materials\\layered\\root"
        q->persistentID.ext != 0x0074616D) [[unlikely]] // "mat\0"
    {
      return nullptr;
    }
    // allocate object and initialize with defaults
    switch (q->persistentID.file)
    {
      case 0x7EA3660C:                  // "layeredmaterials"
        o = BSMaterialsCDB::constructObject< CE2Material >();
        break;
      case 0x8EBE84FF:                  // "blenders"
        o = BSMaterialsCDB::constructObject< CE2Material::Blender >();
        break;
      case 0x574A4CF3:                  // "layers"
        o = BSMaterialsCDB::constructObject< CE2Material::Layer >();
        break;
      case 0x7D1E021B:                  // "materials"
        o = BSMaterialsCDB::constructObject< CE2Material::Material >();
        break;
      case 0x06F52154:                  // "texturesets"
        o = BSMaterialsCDB::constructObject< CE2Material::TextureSet >();
        break;
      case 0x4298BB09:                  // "uvstreams"
        o = BSMaterialsCDB::constructObject< CE2Material::UVStream >();
        break;
    }
  }
  if (!o) [[unlikely]]
    return nullptr;
  o->cdbObject = p;
  o->name = BSMaterialsCDB::storeString(nullptr, 0);
  materialObjectMap.storeObject(o);
  o->parent = findMaterialObject(p->parent);
  // load components
  ComponentInfo componentInfo(*this, o);
  for (const BSMaterialsCDB::MaterialComponent *
           q = p->components; q; q = q->next)
  {
    componentInfo.loadComponent(q);
  }
  return o;
}

CE2MaterialDB::CE2MaterialDB()
{
  clear();
}

CE2MaterialDB::~CE2MaterialDB()
{
}

static bool cdbFileNameFilterFunc(
    [[maybe_unused]] void *p, const std::string_view& s)
{
  return (s.ends_with(".cdb") && s.starts_with("materials/"));
}

void CE2MaterialDB::loadArchives(const BA2File& archive)
{
  if (ba2File && !parentDB)
    clear();
  materialDBMutex.lock();
  try
  {
    ba2File = &archive;
    if (!ba2File) [[unlikely]]
    {
      materialDBMutex.unlock();
      return;
    }
    std::vector< std::string_view > cdbPaths;
    BA2File::UCharArray cdbBuf;
    ba2File->getFileList(cdbPaths, false, &cdbFileNameFilterFunc);
    for (size_t j = 1; j < cdbPaths.size(); j++)
    {
      if (cdbPaths[j] == BSReflStream::getDefaultMaterialDBPath())
      {
        std::swap(cdbPaths[0], cdbPaths[j]);
        break;
      }
    }
    for (size_t j = 0; j < cdbPaths.size(); j++)
    {
      const unsigned char *cdbData = nullptr;
      size_t  cdbSize = ba2File->extractFile(cdbData, cdbBuf, cdbPaths[j]);
      if (cdbSize > 0)
        BSMaterialsCDB::loadCDBFile(cdbData, cdbSize);
    }
  }
  catch (...)
  {
    materialDBMutex.unlock();
    throw;
  }
  materialDBMutex.unlock();
}

const CE2Material * CE2MaterialDB::loadMaterial(
    const std::string_view& materialPath)
{
  if (materialPath.empty()) [[unlikely]]
    return nullptr;
  BSMaterialsCDB::BSResourceID  objectID(materialPath);
  const CE2MaterialObject *o = nullptr;
  materialDBMutex.lock();
  try
  {
    o = materialObjectMap.findObject(objectID);
    if (!o) [[unlikely]]
    {
      const MaterialObject  *p = findMatFileObject(objectID);
      if (!(p && p->isJSON()))
        loadJSONFile(materialPath, objectID, 1);
      o = findMaterialObject(BSMaterialsCDB::getMaterial(objectID));
    }
  }
  catch (...)
  {
    materialDBMutex.unlock();
    throw;
  }
  materialDBMutex.unlock();
  if (!(o && o->type == 1)) [[unlikely]]
    return nullptr;
  return static_cast< const CE2Material * >(o);
}

void CE2MaterialDB::clear()
{
  materialDBMutex.lock();
  try
  {
    bool    constructFlag = !storedStdStrings.buf;
    storedStdStrings.clear();
    materialObjectMap.clear();
    stringBuf.clear();
    if (!constructFlag)
      BSMaterialsCDB::clear();
  }
  catch (...)
  {
    materialDBMutex.unlock();
    throw;
  }
  materialDBMutex.unlock();
}

