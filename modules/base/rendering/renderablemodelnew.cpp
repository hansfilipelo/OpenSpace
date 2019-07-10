/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2019                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <modules/base/rendering/renderablemodelnew.h>

#include <modules/base/basemodule.h>
#include <modules/base/rendering/modelgeometrynew.h>
#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/engine/globals.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/time.h>
#include <openspace/util/updatestructures.h>
#include <openspace/scene/scene.h>
#include <openspace/scene/lightsource.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/file.h>
#include <ghoul/misc/invariants.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
//#include <stb_image.h>
#include <ghoul/io/texture/texturereader.h>

#include <map>

namespace {
    constexpr const char* _loggerCat = "RenderableModelNew";
    constexpr const char* ProgramName = "ModelProgram";
    constexpr const char* KeyModelFile = "ModelFile";

    constexpr const std::array<const char*, 11> UniformNames = {
        "opacity", "nLightSources", "lightDirectionsViewSpace", "lightIntensities",
        "modelViewTransform", "projectionTransform", "performShading", "texture1",
        "ambientIntensity", "diffuseIntensity", "specularIntensity"
    };

    constexpr openspace::properties::Property::PropertyInfo TextureInfo = {
        "ColorTexture",
        "Color Texture",
        "This value points to a color texture file that is applied to the geometry "
        "rendered in this object."
    };

    constexpr openspace::properties::Property::PropertyInfo AmbientIntensityInfo = {
        "AmbientIntensity",
        "Ambient Intensity",
        "A multiplier for ambient lighting."
    };

    constexpr openspace::properties::Property::PropertyInfo DiffuseIntensityInfo = {
        "DiffuseIntensity",
        "Diffuse Intensity",
        "A multiplier for diffuse lighting."
    };

    constexpr openspace::properties::Property::PropertyInfo SpecularIntensityInfo = {
        "SpecularIntensity",
        "Specular Intensity",
        "A multiplier for specular lighting."
    };

    constexpr openspace::properties::Property::PropertyInfo ShadingInfo = {
        "PerformShading",
        "Perform Shading",
        "This value determines whether this model should be shaded by using the position "
        "of the Sun."
    };

    constexpr openspace::properties::Property::PropertyInfo DisableFaceCullingInfo = {
        "DisableFaceCulling",
        "Disable Face Culling",
        "Disable OpenGL automatic face culling optimization."
    };

    constexpr openspace::properties::Property::PropertyInfo ModelTransformInfo = {
        "ModelTransform",
        "Model Transform",
        "This value specifies the model transform that is applied to the model before "
        "all other transformations are applied."
    };

    constexpr openspace::properties::Property::PropertyInfo RotationVecInfo = {
        "RotationVector",
        "Rotation Vector",
        "Rotation Vector using degrees"
    };

    constexpr openspace::properties::Property::PropertyInfo LightSourcesInfo = {
        "LightSources",
        "Light Sources",
        "A list of light sources that this model should accept light from."
    };

    class TextureDimensions
    {
    public:
        TextureDimensions(unsigned int width, unsigned int height, unsigned int dimensions, ghoul::opengl::Texture::Format format)
            : _width(width)
            , _height(height)
            , _dimensions(dimensions)
            , _format(format)
        {}

        bool operator<(const TextureDimensions& other) const
        {
            if (this->_width < other._width)
            {
                return true;
            }
            if (this->_width == other._width)
            {
                if (this->_height < other._height)
                {
                    return true;
                }
                if (this->_height == other._height)
                {
                    if (this->_dimensions < other._dimensions)
                    {
                        return true;
                    }
                    if (this->_dimensions == other._dimensions)
                    {
                        if (this->_format < other._format)
                        {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        const unsigned int _width;
        const unsigned int _height;
        const unsigned int _dimensions;
        const ghoul::opengl::Texture::Format _format;
    };
} // namespace

namespace openspace {

documentation::Documentation RenderableModelNew::Documentation() {
    using namespace documentation;
    return {
        "RenderableModelNew",
        "base_renderable_model",
        {
            {
                AmbientIntensityInfo.identifier,
                new DoubleVerifier,
                Optional::Yes,
                AmbientIntensityInfo.description
            },
            {
                DiffuseIntensityInfo.identifier,
                new DoubleVerifier,
                Optional::Yes,
                DiffuseIntensityInfo.description
            },
            {
                SpecularIntensityInfo.identifier,
                new DoubleVerifier,
                Optional::Yes,
                SpecularIntensityInfo.description
            },
            {
                ShadingInfo.identifier,
                new BoolVerifier,
                Optional::Yes,
                ShadingInfo.description
            },
            {
                DisableFaceCullingInfo.identifier,
                new BoolVerifier,
                Optional::Yes,
                DisableFaceCullingInfo.description
            },
            {
                ModelTransformInfo.identifier,
                new DoubleMatrix3Verifier,
                Optional::Yes,
                ModelTransformInfo.description
            },
           {
                RotationVecInfo.identifier,
                new DoubleVector3Verifier,
                Optional::Yes,
                RotationVecInfo.description
            },
            {
                LightSourcesInfo.identifier,
                new TableVerifier({
                    {
                        "*",
                        new ReferencingVerifier("core_light_source"),
                        Optional::Yes
                    }
                }),
                Optional::Yes,
                LightSourcesInfo.description
            }
        }
    };
}

RenderableModelNew::RenderableModelNew(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _ambientIntensity(AmbientIntensityInfo, 0.2f, 0.f, 1.f)
    , _diffuseIntensity(DiffuseIntensityInfo, 1.f, 0.f, 1.f)
    , _specularIntensity(SpecularIntensityInfo, 1.f, 0.f, 1.f)
    , _performShading(ShadingInfo, true)
    , _disableFaceCulling(DisableFaceCullingInfo, false)
    , _modelTransform(
        ModelTransformInfo,
        glm::dmat3(1.0),
        glm::dmat3(-1.0),
        glm::dmat3(1.0)
    )
    , _rotationVec(RotationVecInfo, glm::dvec3(0.0), glm::dvec3(0.0), glm::dvec3(360.0))
    , _lightSourcePropertyOwner({ "LightSources", "Light Sources" })
{
    documentation::testSpecificationAndThrow(
        Documentation(),
        dictionary,
        "RenderableModelNew"
    );

    addProperty(_opacity);
    registerUpdateRenderBinFromOpacity();

    std::string file = absPath(dictionary.value<std::string>(KeyModelFile));
    loadModel(file);

    if (dictionary.hasKey(ModelTransformInfo.identifier)) {
        _modelTransform = dictionary.value<glm::dmat3>(ModelTransformInfo.identifier);
    }

    if (dictionary.hasKey(AmbientIntensityInfo.identifier)) {
        _ambientIntensity = dictionary.value<float>(AmbientIntensityInfo.identifier);
    }
    if (dictionary.hasKey(DiffuseIntensityInfo.identifier)) {
        _diffuseIntensity = dictionary.value<float>(DiffuseIntensityInfo.identifier);
    }
    if (dictionary.hasKey(SpecularIntensityInfo.identifier)) {
        _specularIntensity = dictionary.value<float>(SpecularIntensityInfo.identifier);
    }

    if (dictionary.hasKey(ShadingInfo.identifier)) {
        _performShading = dictionary.value<bool>(ShadingInfo.identifier);
    }

    if (dictionary.hasKey(DisableFaceCullingInfo.identifier)) {
        _disableFaceCulling = dictionary.value<bool>(DisableFaceCullingInfo.identifier);
    }

    if (dictionary.hasKey(LightSourcesInfo.identifier)) {
        const ghoul::Dictionary& lsDictionary =
            dictionary.value<ghoul::Dictionary>(LightSourcesInfo.identifier);

        for (const std::string& k : lsDictionary.keys()) {
            std::unique_ptr<LightSource> lightSource = LightSource::createFromDictionary(
                lsDictionary.value<ghoul::Dictionary>(k)
            );
            _lightSourcePropertyOwner.addPropertySubOwner(lightSource.get());
            _lightSources.push_back(std::move(lightSource));
        }
    }

    addPropertySubOwner(_lightSourcePropertyOwner);

    for (int i = 0; i < _geometries.size(); ++i)
    {
        addPropertySubOwner(_geometries[i].get());
    }

    addProperty(_ambientIntensity);
    addProperty(_diffuseIntensity);
    addProperty(_specularIntensity);
    addProperty(_performShading);
    addProperty(_disableFaceCulling);
    addProperty(_modelTransform);
    addProperty(_rotationVec);

    _rotationVec.onChange([this]() {
        glm::vec3 degreeVector = _rotationVec;
        glm::vec3 radianVector = glm::vec3(
            glm::radians(degreeVector.x),
            glm::radians(degreeVector.y),
            glm::radians(degreeVector.z)
        );
        _modelTransform = glm::mat4_cast(glm::quat(radianVector));
    });
}

namespace {
std::unique_ptr<ghoul::opengl::Texture> createDummyTexture()
{
    constexpr unsigned int dimensions = 2;
    std::unique_ptr<ghoul::opengl::Texture> dummyTexture(new ghoul::opengl::Texture(glm::uvec3(dimensions, dimensions, 1)));
    unsigned int dataLen = dummyTexture->numberOfChannels() * dimensions * dimensions;
    unsigned char* data = (unsigned char*)malloc(dataLen);
    for (unsigned int i = 0; i < dataLen; ++i)
    {
        data[i] = 128;
    }
    dummyTexture->setPixelData(data);

    return dummyTexture;
}
}

bool RenderableModelNew::loadModel(const std::string& file)
{
    // Create an instance of the Importer class
    Assimp::Importer importer;

    // Rendering only triangle meshes.
    const aiScene* scene = importer.ReadFile(file,
        aiProcess_GenNormals |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices);

    if (!scene) {
        LERRORC("RenderableModelNew", fmt::format("Failed loading model file {}!", file.c_str()));
        return false; // TODO Throw exeption is possibly the OpenSpace way here
    }

    // Get the basepath as string for reading textures
    ghoul::filesystem::File modelFile(file);

    // Read all textures
    _textures.reserve(scene->mNumMeshes);
    for (unsigned int meshIt = 0; meshIt < scene->mNumMeshes; ++meshIt) {
        const struct aiMesh* meshPtr = scene->mMeshes[meshIt];

        // Load diffuse texture
        aiString aiTextPath;

        if (scene->mMaterials[meshPtr->mMaterialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &aiTextPath) != AI_SUCCESS)
        {
            LWARNINGC("RenderableModelNew", fmt::format("Unable to find texture for mesh no {} in model file {}!", meshIt, file.c_str()));
            _textures.push_back(createDummyTexture());
            continue;
        }
        std::string textPath(aiTextPath.C_Str());
        textPath = modelFile.directoryName() + ghoul::filesystem::FileSystem::PathSeparator + textPath;

        std::unique_ptr<ghoul::opengl::Texture> texture = ghoul::io::TextureReader::ref().loadTexture(absPath(std::string(textPath.c_str())));

        if (!texture)
        {
            LWARNINGC("RenderableModelNew", fmt::format("Unable to load texture {} in model {}!", textPath.c_str(), file.c_str()));
            _textures.push_back(createDummyTexture());
            continue;
        }
        _textures.push_back(std::move(texture));
    }

    // Concatinate geometries and textures to reduce draw calls
    // 1. First count the number of textures of given dimension and store indexes to them. This allows us to map textures->meshes.
    std::map<TextureDimensions, std::vector<std::vector<std::size_t>>> oldTextures;
    std::size_t nNewTextures = 0;

    for (std::size_t i = 0; i < _textures.size(); ++i)
    {
        ghoul::opengl::Texture* texture = _textures[i].get();
        TextureDimensions dims(texture->width(), texture->height(), texture->depth(), texture->format());

        if (oldTextures.find(dims) == oldTextures.end())
        {
            oldTextures[dims] = std::vector<std::vector<std::size_t>>();
            oldTextures[dims].emplace_back();
            nNewTextures += 1;
        }

        // Do not create unreasonably large texures
        std::vector<std::size_t>& currentIndexVector = oldTextures[dims].back();
        std::size_t maxDim = std::max(texture->width(), texture->height());

        if (currentIndexVector.size() > 0 && (currentIndexVector.size() + 1) * maxDim > 4096)
        {
            nNewTextures += 1;
            oldTextures[dims].emplace_back();
        }

        oldTextures[dims].back().push_back(i);
    }

    // 2. Create new, concatinated textures
    std::vector<std::unique_ptr<ghoul::opengl::Texture>> newTextures;
    newTextures.reserve(nNewTextures);

    for ( auto const &it : oldTextures ) {
        const TextureDimensions& dims = it.first;
        const std::vector<std::vector<std::size_t>>& indexVectors = it.second;

        for (auto &textIndexes : indexVectors) {

            std::size_t imageDataSize = dims._width * dims._height * dims._dimensions * ghoul::opengl::Texture::numberOfChannels(dims._format);

            if (textIndexes.size() > 1) {
                unsigned char* data = (unsigned char*)malloc(imageDataSize * textIndexes.size());

                // Copy texture data
                for (std::size_t textNum = 0; textNum < textIndexes.size(); ++textNum)
                {
                    std::size_t index = textIndexes[textNum];
                    const ghoul::opengl::Texture* texture = _textures[index].get();

                    memcpy(&data[imageDataSize * textNum], texture->pixelData(), imageDataSize);
                }

                GLenum internalFormat = GL_RGB;
                switch (dims._format) {
                    case ghoul::opengl::Texture::Format::Red:
                        internalFormat = GL_RED;
                        break;
                    case ghoul::opengl::Texture::Format::RG:
                        internalFormat = GL_RG;
                        break;
                    case ghoul::opengl::Texture::Format::RGB:
                        internalFormat = GL_RGB;
                        break;
                    case ghoul::opengl::Texture::Format::RGBA:
                        internalFormat = GL_RGBA;
                        break;
                    case ghoul::opengl::Texture::Format::BGR:
                        internalFormat = GL_BGR;
                        break;
                    case ghoul::opengl::Texture::Format::BGRA:
                        internalFormat = GL_BGRA;
                        break;
                    case ghoul::opengl::Texture::Format::DepthComponent: // Should never happen since we are using diffuse textures
                        internalFormat = GL_DEPTH_COMPONENT;
                        break;
                }

                glm::uvec3 newDimensions;
                newDimensions.x = dims._width;
                newDimensions.y = dims._height  * (unsigned int)textIndexes.size();
                newDimensions.z = dims._dimensions;

                newTextures.emplace_back(new ghoul::opengl::Texture(data, newDimensions, dims._format, internalFormat));
            }
            else { // Only one texture of these dimensions, no need to copy it
                std::size_t index = textIndexes[0];
                newTextures.push_back(std::move(_textures[index]));
            }
        }
    }

    LDEBUGC(
            "RenderableModelNew",
            fmt::format("{} draw calls reduced from {} to {}", file.c_str(), _textures.size(), newTextures.size())
           );
    // Set new textures
    _textures = std::move(newTextures);

    // 3. Concatinate geometries
    _geometries.reserve(_textures.size());

    for ( auto const &it : oldTextures ) {
        for (auto &meshPosVector : it.second) {

            std::vector<modelgeometry::ModelGeometryNew::Vertex> vertices;
            std::vector<int> indices;
            std::size_t nVertices = 0;
            std::size_t nIndices = 0;

            std::size_t numMeshes = meshPosVector.size();

            // Pre-compute n indexes and n vertices
            for (std::size_t meshIt = 0; meshIt < numMeshes; ++meshIt) {

                std::size_t meshIndex = meshPosVector[meshIt];
                const struct aiMesh* meshPtr = scene->mMeshes[meshIndex];

                nVertices += meshPtr->mNumVertices;
                nIndices += meshPtr->mNumFaces * 3;
            }
            vertices.reserve(nVertices);
            indices.reserve(nIndices);

            for (std::size_t meshIt = 0; meshIt < numMeshes; ++meshIt) {

                std::size_t meshIndex = meshPosVector[meshIt];
                const struct aiMesh* meshPtr = scene->mMeshes[meshIndex];

                // Walk through each of the mesh's vertices
                std::size_t indexOffset = vertices.size();

                for (unsigned int vertIt = 0; vertIt < meshPtr->mNumVertices; vertIt++) {
                    modelgeometry::ModelGeometryNew::Vertex vTmp{};

                    // Positions
                    vTmp.location[0] = meshPtr->mVertices[vertIt].x;
                    vTmp.location[1] = meshPtr->mVertices[vertIt].y;
                    vTmp.location[2] = meshPtr->mVertices[vertIt].z;
                    vTmp.location[3] = 1.f;

                    if (meshPtr->mNormals) {
                        // Normals
                        vTmp.normal[0] = meshPtr->mNormals[vertIt].x;
                        vTmp.normal[1] = meshPtr->mNormals[vertIt].y;
                        vTmp.normal[2] = meshPtr->mNormals[vertIt].z;
                    }
                    else {
                        vTmp.normal[0] = 0.f;
                        vTmp.normal[1] = 0.f;
                        vTmp.normal[2] = 0.f;
                    }

                    // Texture Coordinates
                    // Each vertex can have at most 8 different texture coordinates.
                    // We are using only the first one provided.
                    if (meshPtr->mTextureCoords[0]) {
                        // Map texture coordinates on the Y-axis into the new larger texture
                        vTmp.tex[0] = meshPtr->mTextureCoords[0][vertIt].x;
                        vTmp.tex[1] = ((float)meshIt + meshPtr->mTextureCoords[0][vertIt].y) * (1.0f / (float)numMeshes);
                    }
                    else {
                        vTmp.tex[0] = 0.f;
                        vTmp.tex[1] = 0.f;
                    }

                    vertices.push_back(vTmp);
                }

                // Walking through the mesh faces and get the vertexes indices
                for (unsigned int nf = 0; nf < meshPtr->mNumFaces; ++nf) {
                    const struct aiFace* face = &meshPtr->mFaces[nf];

                    if (face->mNumIndices == 3) {
                        for (unsigned int ii = 0; ii < face->mNumIndices; ii++) {
                            indices.push_back(
                                static_cast<GLint>(indexOffset + face->mIndices[ii])
                            );
                        }
                    }
                }
            }

            // Create OpenSpace geometry from our vertices
            _geometries.emplace_back(new modelgeometry::ModelGeometryNew("ModelGeometryNew" + std::to_string(_geometries.size())));
            _geometries.back()->setModelData(std::move(vertices), std::move(indices));
        }
    }

    return true;
}

bool RenderableModelNew::isReady() const {
    return _program && _geometries.size() > 0 && _textures.size() == _geometries.size();
}

void RenderableModelNew::initialize() {
    for (const std::unique_ptr<LightSource>& ls : _lightSources) {
        ls->initialize();
    }
}

void RenderableModelNew::initializeGL() {
    _program = BaseModule::ProgramObjectManager.request(
            ProgramName,
            []() -> std::unique_ptr<ghoul::opengl::ProgramObject> {
            return global::renderEngine.buildRenderProgram(
                    ProgramName,
                    absPath("${MODULE_BASE}/shaders/model_vs.glsl"),
                    absPath("${MODULE_BASE}/shaders/model_fs.glsl")
                    );
            }
            );

    ghoul::opengl::updateUniformLocations(*_program, _uniformCache, UniformNames);

    for (auto &geometry : _geometries)
    {
        geometry->initialize(this);
    }

    // Upload textures to GPU
    for (auto &texture : _textures)
    {
        LDEBUGC(
                "RenderableModelNew",
                fmt::format("Uploading texture to GPU")
               );
        texture->uploadTexture();
        texture->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);
    }
}

void RenderableModelNew::deinitializeGL() {
    for (auto &geometry : _geometries)
    {
        geometry->deinitialize();
    }
    _geometries.clear();

    _textures.clear();

    BaseModule::ProgramObjectManager.release(
            ProgramName,
            [](ghoul::opengl::ProgramObject* p) {
            global::renderEngine.removeRenderProgram(p);
            }
            );
    _program = nullptr;
}

void RenderableModelNew::render(const RenderData& data, RendererTasks&) {
    _program->activate();

    _program->setUniform(_uniformCache.opacity, _opacity);

    // Model transform and view transform needs to be in double precision
    const glm::dmat4 modelTransform =
        glm::translate(glm::dmat4(1.0), data.modelTransform.translation) * // Translation
        glm::dmat4(data.modelTransform.rotation) *  // Spice rotation
        glm::scale(
                glm::dmat4(_modelTransform.value()), glm::dvec3(data.modelTransform.scale)
                );
    const glm::dmat4 modelViewTransform = data.camera.combinedViewMatrix() *
        modelTransform;

    int nLightSources = 0;
    _lightIntensitiesBuffer.resize(_lightSources.size());
    _lightDirectionsViewSpaceBuffer.resize(_lightSources.size());
    for (const std::unique_ptr<LightSource>& lightSource : _lightSources) {
        if (!lightSource->isEnabled()) {
            continue;
        }
        _lightIntensitiesBuffer[nLightSources] = lightSource->intensity();
        _lightDirectionsViewSpaceBuffer[nLightSources] =
            lightSource->directionViewSpace(data);

        ++nLightSources;
    }

    _program->setUniform(
            _uniformCache.nLightSources,
            nLightSources
            );
    _program->setUniform(
            _uniformCache.lightIntensities,
            _lightIntensitiesBuffer.data(),
            nLightSources
            );
    _program->setUniform(
            _uniformCache.lightDirectionsViewSpace,
            _lightDirectionsViewSpaceBuffer.data(),
            nLightSources
            );
    _program->setUniform(
            _uniformCache.modelViewTransform,
            glm::mat4(modelViewTransform)
            );
    _program->setUniform(
            _uniformCache.projectionTransform,
            data.camera.projectionMatrix()
            );
    _program->setUniform(
            _uniformCache.ambientIntensity,
            _ambientIntensity
            );
    _program->setUniform(
            _uniformCache.diffuseIntensity,
            _diffuseIntensity
            );
    _program->setUniform(
            _uniformCache.specularIntensity,
            _specularIntensity
            );
    _program->setUniform(
            _uniformCache.performShading,
            _performShading
            );

    if (_disableFaceCulling) {
        glDisable(GL_CULL_FACE);
    }


    // Bind texture and render
    for (unsigned int i = 0; i < _geometries.size(); ++i)
    {
        _geometries[i]->setUniforms(*_program);
        ghoul::opengl::TextureUnit unit;
        unit.activate();
        _textures[i]->bind();
        _program->setUniform(_uniformCache.texture, unit);
        _geometries[i]->render();
    }

    if (_disableFaceCulling) {
        glEnable(GL_CULL_FACE);
    }

    _program->deactivate();
}

void RenderableModelNew::update(const UpdateData&) {
    if (_program->isDirty()) {
        _program->rebuildFromFile();
        ghoul::opengl::updateUniformLocations(*_program, _uniformCache, UniformNames);
    }
}

}  // namespace openspace
