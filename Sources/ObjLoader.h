#pragma once

struct Mesh {
	int numFaces;
	int numVertices;
	int numUVs;
	int numNormals;
	int numIndices;
	
	float* vertices;
	int* indices;
	float* uvs;
	float * normals;
	
	// very private
	float* curVertex;
	int* curIndex;
	float* curUV;
	float* curNormal;
};

Mesh* loadObj(const char* filename);
