/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2015                                                               *
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

#ifndef __TEXTUREPLANE_H__
#define __TEXTUREPLANE_H__

// #include <openspace/rendering/renderable.h>
#include <modules/datasurface/rendering/datasurface.h>
#include <openspace/properties/stringproperty.h>
#include <openspace/properties/vectorproperty.h>
#include <ghoul/opengl/texture.h>
#include <openspace/util/powerscaledcoordinate.h>
#include <openspace/engine/downloadmanager.h>
#include <modules/kameleon/include/kameleonwrapper.h>

 namespace openspace{
 
 class TexturePlane : public DataSurface {
 public:
 	TexturePlane(std::string path);
 	~TexturePlane();

 	virtual bool initialize();
    virtual bool deinitialize();

	bool isReady() const;

	virtual void render();
	virtual void update();
 
 private:
 	virtual void setParent() override;

 	void loadTexture();
    void createPlane();
    void updateTexture();

    static int id();
    int _id;
	std::unique_ptr<ghoul::opengl::Texture> _texture;

	glm::size3_t _dimensions;
	DownloadManager::FileFuture* _futureTexture;

	GLuint _quad;
	GLuint _vertexPositionBuffer;

	glm::dmat3 _stateMatrix;
	// bool _planeIsDirty;
 };
 
 } // namespace openspace

#endif