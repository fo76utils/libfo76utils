
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
    std::map< const BSMaterialsCDB::MaterialObject *,
              CE2MaterialObject * >::iterator i = materialObjectMap.find(p);
    if (i != materialObjectMap.end())
      return i->second;
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
        o = reinterpret_cast< CE2MaterialObject * >(
                BSMaterialsCDB::allocateSpace(sizeof(CE2Material),
                                              alignof(CE2Material)));
        o->type = 1;
        break;
      case 0x8EBE84FF:                  // "blenders"
        o = reinterpret_cast< CE2MaterialObject * >(
                BSMaterialsCDB::allocateSpace(sizeof(CE2Material::Blender),
                                              alignof(CE2Material::Blender)));
        o->type = 2;
        break;
      case 0x574A4CF3:                  // "layers"
        o = reinterpret_cast< CE2MaterialObject * >(
                BSMaterialsCDB::allocateSpace(sizeof(CE2Material::Layer),
                                              alignof(CE2Material::Layer)));
        o->type = 3;
        break;
      case 0x7D1E021B:                  // "materials"
        o = reinterpret_cast< CE2MaterialObject * >(
                BSMaterialsCDB::allocateSpace(sizeof(CE2Material::Material),
                                              alignof(CE2Material::Material)));
        o->type = 4;
        break;
      case 0x06F52154:                  // "texturesets"
        o = reinterpret_cast< CE2MaterialObject * >(
                BSMaterialsCDB::allocateSpace(
                    sizeof(CE2Material::TextureSet),
                    alignof(CE2Material::TextureSet)));
        o->type = 5;
        break;
      case 0x4298BB09:                  // "uvstreams"
        o = reinterpret_cast< CE2MaterialObject * >(
                BSMaterialsCDB::allocateSpace(sizeof(CE2Material::UVStream),
                                              alignof(CE2Material::UVStream)));
        o->type = 6;
        break;
    }
  }
  if (!o) [[unlikely]]
    return nullptr;
  o->cdbObject = p;
  o->name = BSMaterialsCDB::storeString(nullptr, 0);
  o->parent = findMaterialObject(p->parent);
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
  // load components
  ComponentInfo componentInfo(*this, o);
  for (const BSMaterialsCDB::MaterialComponent *
           q = p->components; q; q = q->next)
  {
    componentInfo.loadComponent(q);
  }
  (void) materialObjectMap.emplace(p, o);
  return o;
}

CE2MaterialDB::CE2MaterialDB()
{
  clear();
}

CE2MaterialDB::~CE2MaterialDB()
{
}

void CE2MaterialDB::loadArchives(
    const BA2File& archive1, const BA2File *archive2)
{
  materialDBMutex.lock();
  try
  {
    // TODO
  }
  catch (...)
  {
    materialDBMutex.unlock();
    throw;
  }
  materialDBMutex.unlock();
}

const CE2Material * CE2MaterialDB::loadMaterial(const std::string& materialPath)
{
  materialDBMutex.lock();
  try
  {
    // TODO
  }
  catch (...)
  {
    materialDBMutex.unlock();
    throw;
  }
  materialDBMutex.unlock();
  return nullptr;
}

void CE2MaterialDB::clear()
{
  materialDBMutex.lock();
  try
  {
    objectNameMap = nullptr;
    ba2File1 = nullptr;
    ba2File2 = nullptr;
    storedStdStrings.clear();
    materialObjectMap.clear();
    stringBuf.clear();
    storedStdStrings.emplace("");
    BSMaterialsCDB::clear();
    objectNameMap =
        reinterpret_cast< CE2MaterialObject ** >(
            BSMaterialsCDB::allocateSpace(
                sizeof(CE2MaterialObject *) * (objectHashMask + 1),
                alignof(CE2MaterialObject *)));
    for (size_t i = 0; i <= objectHashMask; i++)
      objectNameMap[i] = nullptr;
  }
  catch (...)
  {
    materialDBMutex.unlock();
    throw;
  }
  materialDBMutex.unlock();
}

