#pragma once

#include "pch.h"

#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/Graphics1/Image.h>
#include <Kore/Graphics4/Graphics.h>
#include "ObjLoader.h"

using namespace Kore;

class MeshObject {
public:
	MeshObject(const char* meshFile, const char* textureFile, const Graphics4::VertexStructure& structure, float scale = 1.0f) {
		mesh = loadObj(meshFile);
		image = new Graphics4::Texture(textureFile, true);
		
		minx = miny = minz = 9999999;
		maxx = maxy = maxz = -9999999;
		
		vertexBuffer = new Graphics4::VertexBuffer(mesh->numVertices, structure, 0);
		float* vertices = vertexBuffer->lock();
		for (int i = 0; i < mesh->numVertices; ++i) {
			vertices[i * 8 + 0] = mesh->vertices[i * 8 + 0] * scale;
			minx = min(vertices[i * 8 + 0], minx);
			maxx = max(vertices[i * 8 + 0], maxx);
			vertices[i * 8 + 1] = mesh->vertices[i * 8 + 1] * scale;
			miny = min(vertices[i * 8 + 1], miny);
			maxy = max(vertices[i * 8 + 1], maxy);
			vertices[i * 8 + 2] = mesh->vertices[i * 8 + 2] * scale;
			minz = min(vertices[i * 8 + 2], minz);
			maxz = max(vertices[i * 8 + 2], maxz);
			vertices[i * 8 + 3] = mesh->vertices[i * 8 + 3];
			vertices[i * 8 + 4] = 1.0f - mesh->vertices[i * 8 + 4];
			vertices[i * 8 + 5] = mesh->vertices[i * 8 + 5];
			vertices[i * 8 + 6] = mesh->vertices[i * 8 + 6];
			vertices[i * 8 + 7] = mesh->vertices[i * 8 + 7];
		}
		vertexBuffer->unlock();
		
		indexBuffer = new Graphics4::IndexBuffer(mesh->numFaces * 3);
		int* indices = indexBuffer->lock();
		for (int i = 0; i < mesh->numFaces * 3; i++) {
			indices[i] = mesh->indices[i];
		}
		indexBuffer->unlock();
		
		M = mat4::Identity();
	}
	
	void render(Graphics4::TextureUnit tex) {
		Graphics4::setTexture(tex, image);
		Graphics4::setVertexBuffer(*vertexBuffer);
		Graphics4::setIndexBuffer(*indexBuffer);
		Graphics4::drawIndexedVertices();
	}
	
	virtual void update(float tdif) {
		(void)tdif;	// Do nothing
	}
	
	mat4 M;
	
private:
	Graphics4::VertexBuffer* vertexBuffer;
	Graphics4::IndexBuffer* indexBuffer;
	Mesh* mesh;
	Graphics4::Texture* image;
	
protected:
	float minx, miny, minz;
	float maxx, maxy, maxz;
};
