#pragma once
#include <DirectXMath.h>
// MAKE HEADERS HERE

enum Headers {
	NEW_MESH = 0,
	NEW_CAMERA = 1,
	CHANGED_MESH = 2,
	CHANGED_CAMERA = 3,
	CHANGED_TRANSFORMATION = 4,
	CHANGED_MATERIAL = 5,
	CHANGED_NAME = 6,
	DELETE_MESH = 7,
	CHANGED_TEXTURE = 8,
};

struct SectionHeader
{
	Headers header;
	size_t messageSize;
	size_t ID;
};

enum CameraType { PERSPECTIVE, ORTHOGRAPHIC };


struct CameraHeader
{
	CameraHeader() {}
	CameraHeader(CameraType ct, DirectX::XMMATRIX transform, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projMatrix, bool state)
		: camType(ct), active(state) {}

	CameraType camType;
	float transformation[16];
	char name[20];
		
	double farPlane = 0;
	double nearPlane = 0;
	double fieldOfView = 0;
	double aspectRatio = 0;

	float camWidth;
	float camHeight;

	bool active = true;
};

struct CameraTransformation
{
	float transformation[16];
	float camWidth;
	float camHeight;
	float aspectRatio;
	float fieldOfView;
	char camName[20];
};

struct ChangedTexture
{
	char filePath[100];
	char meshName[100];
};

struct TextureHeader
{
	char filePath[100];
};

struct MaterialHeader
{
	MaterialHeader() {}
	MaterialHeader(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular, bool hasTex, bool hasNorm)
		: diffuse(diffuse), ambient(ambient), hasTexture(hasTex), hasNormal(hasNorm) {}

	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT4 ambient;

	bool hasTexture = false;
	bool hasNormal = false;
	int nrOfTextures = 0;
};

struct ChangedMaterial
{
	char meshName[100];
	MaterialHeader newmat;
};

struct Vertex
{
	Vertex() : pos(), uv(), normal() {}
	Vertex(DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 texCoord, DirectX::XMFLOAT3 normals)
		: pos(position), uv(texCoord), normal(normals) {}

	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT2 uv;
	DirectX::XMFLOAT3 normal;
};

struct NameChanged
{
	char oldName[100];
	char newName[100];
};

struct MeshHeader
{
	MeshHeader() {}

	int nrOfVertices;
	char name[100];
	MaterialHeader material;

	float transformation[16];	
};

struct TopologyChanged
{
	char name[100];
	int vertexIndex;
	int vertexCount;
	bool wholeMesh;
};

struct MeshTransformation
{
	float transformation[16];
	char meshName[100];
};

struct DeleteMesh
{
	char meshName[50];
};