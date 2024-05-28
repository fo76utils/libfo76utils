
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
  // transmissive, curvature, mask
  0xFF000000U, 0xFF808080U, 0xFF000000U, 0xFF808080U,
  0xFF000000U, 0xFF000000U, 0xFF808080U, 0xFF808080U,
  0xFF808080U, 0xFF000000U, 0xFF000000U, 0xFFFFFFFFU,
  0xFF808080U
};

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

const std::string * CE2MaterialDB::storeStdString(const std::string& s)
{
  return &(*(storedStdStrings.insert(s).first));
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
    switch (q->persistentID.file)
    {
      case 0x7EA3660C:                  // "layeredmaterials"
        o = BSMaterialsCDB::allocateObjects< CE2Material >(1);
        o->type = 1;
        break;
      case 0x8EBE84FF:                  // "blenders"
        o = BSMaterialsCDB::allocateObjects< CE2Material::Blender >(1);
        o->type = 2;
        break;
      case 0x574A4CF3:                  // "layers"
        o = BSMaterialsCDB::allocateObjects< CE2Material::Layer >(1);
        o->type = 3;
        break;
      case 0x7D1E021B:                  // "materials"
        o = BSMaterialsCDB::allocateObjects< CE2Material::Material >(1);
        o->type = 4;
        break;
      case 0x06F52154:                  // "texturesets"
        o = BSMaterialsCDB::allocateObjects< CE2Material::TextureSet >(1);
        o->type = 5;
        break;
      case 0x4298BB09:                  // "uvstreams"
        o = BSMaterialsCDB::allocateObjects< CE2Material::UVStream >(1);
        o->type = 6;
        break;
    }
  }
  if (!o) [[unlikely]]
    return nullptr;
  o->cdbObject = p;
  o->name = BSMaterialsCDB::storeString(nullptr, 0);
  // initialize with defaults
  const std::string *emptyString = &(*(storedStdStrings.cbegin()));
  switch (o->type)
  {
    case 1:
      {
        CE2Material *q = static_cast< CE2Material * >(o);
        for (size_t i = 0; i < CE2Material::maxLayers; i++)
          q->layers[i] = nullptr;
        q->alphaThreshold = 1.0f / 3.0f;
        q->shaderModel = 31;            // "BaseMaterial"
        q->alphaHeightBlendThreshold = 0.0f;
        q->alphaHeightBlendFactor = 0.05f;
        q->alphaPosition = 0.5f;
        q->alphaContrast = 0.0f;
        q->alphaUVStream = nullptr;
        q->opacityLayer2 = 1;           // "MATERIAL_LAYER_1"
        q->opacityLayer3 = 2;           // "MATERIAL_LAYER_2"
        q->opacityBlender2 = 1;         // "BLEND_LAYER_1"
        q->specularOpacityOverride = 0.0f;
        for (size_t i = 0; i < CE2Material::maxBlenders; i++)
          q->blenders[i] = nullptr;
        for (size_t i = 0; i < CE2Material::maxLODMaterials; i++)
          q->lodMaterials[i] = nullptr;
        q->effectSettings = nullptr;
        q->emissiveSettings = nullptr;
        q->layeredEmissiveSettings = nullptr;
        q->translucencySettings = nullptr;
        q->decalSettings = nullptr;
        q->vegetationSettings = nullptr;
        q->detailBlenderSettings = nullptr;
        q->layeredEdgeFalloff = nullptr;
        q->waterSettings = nullptr;
        q->globalLayerData = nullptr;
      }
      break;
    case 2:
      {
        CE2Material::Blender  *q = static_cast< CE2Material::Blender * >(o);
        q->uvStream = nullptr;
        q->texturePath = emptyString;
        q->textureReplacement = 0xFFFFFFFFU;
        for (size_t i = 0; i < CE2Material::Blender::maxFloatParams; i++)
          q->floatParams[i] = 0.5f;
      }
      break;
    case 3:
      {
        CE2Material::Layer  *q = static_cast< CE2Material::Layer * >(o);
        q->material = nullptr;
        q->uvStream = nullptr;
      }
      break;
    case 4:
      {
        CE2Material::Material *q = static_cast< CE2Material::Material * >(o);
        q->color = FloatVector4(1.0f);
        q->flipbookColumns = 1;
        q->flipbookRows = 1;
        q->flipbookFPS = 30.0f;
        q->textureSet = nullptr;
      }
      break;
    case 5:
      {
        CE2Material::TextureSet *q =
            static_cast< CE2Material::TextureSet * >(o);
        q->floatParam = 0.5f;
        for (size_t i = 0; i < CE2Material::TextureSet::maxTexturePaths; i++)
          q->texturePaths[i] = emptyString;
        for (size_t i = 0; i < CE2Material::TextureSet::maxTexturePaths; i++)
          q->textureReplacements[i] = defaultTextureRepl[i];
      }
      break;
    case 6:
      {
        CE2Material::UVStream *q = static_cast< CE2Material::UVStream * >(o);
        q->scaleAndOffset = FloatVector4(1.0f, 1.0f, 0.0f, 0.0f);
        q->channel = 1;                 // "One"
      }
      break;
  }
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
  materialDBMutex.lock();
  try
  {
    if (ba2File)
      clear();
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
      BA2File::UCharArray jsonBuf;
      const unsigned char *jsonData = nullptr;
      size_t  jsonSize = 0;
      const BA2File::FileInfo *fd;
      const MaterialObject  *p = BSMaterialsCDB::getMaterial(objectID);
      // only load JSON materials that replace CDB materials from loose files
      if (ba2File && (fd = ba2File->findFile(materialPath)) != nullptr &&
          (fd->archiveType < 0 || !p))
      {
        jsonSize = ba2File->extractFile(jsonData, jsonBuf, materialPath);
      }
      if (jsonSize > 0)
        BSMaterialsCDB::loadJSONFile(jsonData, jsonSize, materialPath);
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
    ba2File = nullptr;
    bool    constructFlag =
        (storedStdStrings.begin() == storedStdStrings.end());
    storedStdStrings.clear();
    materialObjectMap.clear();
    stringBuf.clear();
    storedStdStrings.emplace("");
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

