#pragma once

#include <string>
using namespace std;

struct Float3
{
	float x;
	float y;
	float z;

	Float3(float x, float y, float z)
	{
		this->x = x;
		this->y = y;
		this->z = z;
	}
	Float3() {};

	Float3(double* pos)
	{
		this->x = float(pos[0]);
		this->y = float(pos[1]);
		this->z = float(pos[2]);

	}

	Float3& operator=(double* pos)
	{
		this->x = float(pos[0]);
		this->y = float(pos[1]);
		this->z = float(pos[2]);

		return *this;
	}
};

struct Float2
{
	float x;
	float y;

	Float2(float x, float y)
	{
		this->x = x;
		this->y = y;
	}
	Float2() {};

	Float2& operator=(double* pos)
	{
		this->x = float(pos[0]);
		this->y = float(pos[1]);

		return *this;
	}
};

struct Vertex
{

	Float3 position;
	Float3 normal;
	Float3 binormal;
	Float2 uv;


	Vertex(Float3 position, Float3 normal, Float3 binormal, Float2 uv)
	{
		this->position.x = position.x;
		this->position.y = position.y;
		this->position.z = position.z;

		this->normal.x = normal.x;
		this->normal.y = normal.y;
		this->normal.z = normal.z;

		this->binormal.x = binormal.x;
		this->binormal.y = binormal.y;
		this->binormal.z = binormal.z;

		this->uv.x = uv.x;
		this->uv.y = uv.y;

	}
	Vertex() {};


};



struct Matrix
{
	float matrix[16];
	//Matrix( float m00, float m01, float m02, float m03,
	//		  float m10, float m11, float m12, float m13,
	//		  float m20, float m21, float m22, float m23,
	//		  float m30, float m31, float m32, float m33);

};
enum MessageType {
	MESH,
	VERTSEGMENT,
	VERTEX,
	CAMERA,
	TRANSFORM,
	MATERIAL


};

struct MainMessageHeader
{
	MessageType messageType; 
		//messageiD?
	unsigned int msgSize;
};

struct MeshMessage
{
	char nodeName[256];
	unsigned int nameLength;
	
	float worldMatrix[16];
	unsigned int vertexCount;
	unsigned int indexCount;
	
};

struct VertSegmentMessage //for n verts, but not a whole mesh,
{
	char nodeName[256];
	unsigned int nameLength;
	unsigned int numVertices;

};

struct VertexMessage // for single verts.
{
	unsigned int indexId; // the id in the array. so we know which one to update
	Vertex vert;

};

struct TransformMessage
{
	char nodeName[256];
	unsigned int nameLength;
	float matrix[16];

};

struct CameraMessage
{
	char nodeName[256];
	unsigned int nameLength;
	float viewMatrix[16];
	float projMatrix[16];

};

