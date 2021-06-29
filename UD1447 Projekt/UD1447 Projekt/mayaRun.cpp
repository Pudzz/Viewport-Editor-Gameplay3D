
#include "maya_includes.h"
#include <maya/MTimer.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <queue>

#include "../Circularbuffers.h"
#include <unordered_map>
#include <map>

using namespace std;
//MCallbackIdArray callbackIdArray;

std::unordered_map<std::string, MCallbackId> callbacks;

MObject m_node;
MStatus status = MS::kSuccess;
bool initBool = false;

enum NODE_TYPE { TRANSFORM, MESH };
MTimer gTimer;

// keep track of created meshes to maintain them
queue<MObject> newMeshes;

CircularBuffer* buffer;// = new CircularBuffer(L"name", 1024, Producer);

float degrees = 57.2957795;

void attributeEval(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData)
{	
	if (msg & MNodeMessage::kAttributeEval)
	{
		MObject node = plug.node(&status);
		if (status == MS::kSuccess)
		{
			MFnMesh mesh(node, &status);
			if (status == MS::kSuccess)
			{				
				if (mesh.name() + ".outMesh" == plug.name())
				{					
					std::vector<Vertex*> vertices;

					MFnMesh mesh(plug.node());
					MFnTransform transDag(mesh.parent(0));

					TopologyChanged* topChanged = new TopologyChanged;
					strcpy(topChanged->name, transDag.name().asChar());

					topChanged->wholeMesh = true;

					MPointArray vertexArray;
					MFloatVectorArray normalArray;
					MFloatArray u, v;

					// Get vertices information
					mesh.getPoints(vertexArray, MSpace::kObject);
					mesh.getNormals(normalArray);
					mesh.getUVs(u, v);

					// For indexing triangles
					MIntArray count, id;
					MIntArray triangleCount, triangleIndex;
					MIntArray vertexCounts, vertexID;
					MIntArray normalCount, normalList;

					// Get all the indices
					mesh.getAssignedUVs(count, id);
					mesh.getTriangleOffsets(triangleCount, triangleIndex);
					mesh.getVertices(vertexCounts, vertexID);
					mesh.getNormalIds(normalCount, normalList);

					for (int i = 0; i < triangleIndex.length(); i++)
					{
						Vertex* temp = new Vertex;

						temp->pos.x = vertexArray[vertexID[triangleIndex[i]]].x;
						temp->pos.y = vertexArray[vertexID[triangleIndex[i]]].y;
						temp->pos.z = vertexArray[vertexID[triangleIndex[i]]].z;

						temp->normal.x = normalArray[normalList[triangleIndex[i]]].x;
						temp->normal.y = normalArray[normalList[triangleIndex[i]]].y;
						temp->normal.z = normalArray[normalList[triangleIndex[i]]].z;

						temp->uv.x = u[id[triangleIndex[i]]];
						temp->uv.y = v[id[triangleIndex[i]]];

						vertices.push_back(temp);
					}

					topChanged->vertexCount = vertices.size();

					int msgSize = sizeof(TopologyChanged) + (sizeof(Vertex) * vertices.size()) + 1;

					char* msg = new char[msgSize];
					int offset = 0;

					memcpy(msg + offset, (char*)topChanged, sizeof(TopologyChanged));
					offset += sizeof(TopologyChanged);

					for (int i = 0; i < vertices.size(); i++)
					{
						memcpy(msg + offset, (char*)vertices[i], sizeof(Vertex));
						offset += sizeof(Vertex);
					}

					// Create the section header, which will tell the consumer that a new mesh is coming
					SectionHeader* mainHeader = new SectionHeader;
					mainHeader->header = CHANGED_MESH;
					mainHeader->messageSize = msgSize;

					// Send first the section header
					buffer->Send(msg, mainHeader);

					/* Memoryleaks */
					for (int i = 0; i < vertices.size(); i++)
						delete vertices[i];

					if (topChanged)
						delete topChanged;

					if (mainHeader)
						delete mainHeader;
				}
			}
		}
	}
}

void topologyChanged(MObject& node, void* clientData)
{
	if (node.hasFn(MFn::kMesh))
	{		
		/* This is for shape, when i change vertices or something else in shapenode */
		MCallbackId topologyID = MNodeMessage::addAttributeChangedCallback(node, attributeEval, NULL, &status);
		if (status == MS::kSuccess)
		{
			MFnDagNode dag(node);

			std::string name = dag.name().asChar();
			std::string extra = "attributeEvaluate";

			auto iterator = callbacks.find(name + extra);
			if (iterator != callbacks.end())
			{
				std::cout << "Callback erased: " << iterator->first << std::endl;
				MMessage::removeCallback(iterator->second);
				callbacks.erase(iterator);
			}
			callbacks.insert({ name + extra, topologyID });

		}
	}
}

void GetHierarchy(MObject node)
{
	MeshTransformation* meshTransform = new MeshTransformation;
			
	MFnDagNode dag(node);
	strcpy(meshTransform->meshName, dag.name().asChar());

	MMatrix worldMatrix;
	MDagPath daggyPath = MDagPath::getAPathTo(node);
	worldMatrix = daggyPath.inclusiveMatrix();
		
	// Send new transform matrix
	int index = 0;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			meshTransform->transformation[index] = worldMatrix[i][j];
			index++;
		}
	}

	int msgSize = sizeof(MeshTransformation) + 1;

	// Local "buffer" to store the message
	char* msg = new char[msgSize];
	int offset = 0;

	memcpy(msg + offset, (char*)meshTransform, sizeof(MeshTransformation));
	offset += sizeof(MeshTransformation);

	// Create the section header, which will tell the consumer that a new mesh is coming
	SectionHeader* mainHeader = new SectionHeader;
	mainHeader->header = CHANGED_TRANSFORMATION;
	mainHeader->messageSize = msgSize;

	// Send first the section header
	buffer->Send(msg, mainHeader);

	if (dag.childCount() > 1)
	{
		for (int i = 0; i < dag.childCount(); i++)
		{
			MObject child = dag.child(i);

			if (child.hasFn(MFn::kDagNode))
				GetHierarchy(child);
		}
	}
}

void AttributeTransformChanged(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData)
{	
	/* Displays fullpath name | Displays individual attribute, translate, rotate, scale | Displays both local and world matrices */
	MStatus status;
	if (msg & MNodeMessage::AttributeMessage::kAttributeSet)
	{
		if (plug.node().hasFn(MFn::kTransform))
		{
			GetHierarchy(plug.node());	
		}
	}
}

void MaterialParametersChanged(MObject object, MPlug& plug)
{
	ChangedMaterial* changedInfo;
	std::vector <TextureHeader*> textures;

	changedInfo = new ChangedMaterial;
	if (object.hasFn(MFn::kDagNode))
	{
		MFnDagNode dag(object);
		strcpy(changedInfo->meshName, dag.name().asChar());
	}

	MObject tempobj(plug.node());
	MFnLambertShader lambert(tempobj);

	MaterialHeader* changedMat = new MaterialHeader;
	changedMat->diffuse.x = lambert.color().r;
	changedMat->diffuse.y = lambert.color().g;
	changedMat->diffuse.z = lambert.color().b;
	changedMat->diffuse.w = lambert.color().a;

	changedMat->ambient.x = lambert.ambientColor().r;
	changedMat->ambient.y = lambert.ambientColor().g;
	changedMat->ambient.z = lambert.ambientColor().b;
	changedMat->ambient.w = lambert.ambientColor().a;
	
	/* Check textures */
	MPlugArray lambertConnection;
	MPlug lambertPlugs = lambert.findPlug("color");
	lambertPlugs.connectedTo(lambertConnection, true, true);

	for (int k = 0; k < lambertConnection.length(); k++)
	{
		std::cout << lambertConnection[k].name() << std::endl;
		MObject textureCheck(lambertConnection[k].node());
		if (textureCheck.hasFn(MFn::kFileTexture))
		{
			// Create new texture
			TextureHeader* newTexture = new TextureHeader;

			MPlug filenamePlug = MFnDependencyNode(textureCheck).findPlug("fileTextureName");
			MString textureName;
			filenamePlug.getValue(textureName);
			if (textureName != "")
			{
				strcpy(newTexture->filePath, textureName.asChar());
				textures.push_back(newTexture);

				changedMat->hasTexture = true;
				changedMat->nrOfTextures += 1;
			}
		}
	}

	// For normal mapping
	MPlugArray lambertMapConnections;
	MPlug lambertMapPlugs = lambert.findPlug("normalCamera");
	lambertMapPlugs.connectedTo(lambertMapConnections, true, true);

	for (int j = 0; j < lambertMapConnections.length(); j++)
	{
		MObject normalCheck(lambertMapConnections[j].node());
		if (normalCheck.hasFn(MFn::kBump))
		{
			MPlug bumpValuePlug = MFnDependencyNode(normalCheck).findPlug("bumpValue");
			MPlugArray bumpValuePlugConnections;
			bumpValuePlug.connectedTo(bumpValuePlugConnections, true, false);

			for (int h = 0; h < bumpValuePlugConnections.length(); h++)
			{
				MObject bumpValueCheck(bumpValuePlugConnections[h].node());

				if (bumpValueCheck.hasFn(MFn::kFileTexture))
				{
					TextureHeader* newBumpTexture = new TextureHeader;

					MPlug bumpfilenamePlug = MFnDependencyNode(bumpValueCheck).findPlug("fileTextureName");
					MString bumptextureName;
					bumpfilenamePlug.getValue(bumptextureName);
					if (bumptextureName != "")
					{

						strcpy(newBumpTexture->filePath, bumptextureName.asChar());
						textures.push_back(newBumpTexture);

						changedMat->hasNormal = true;
						changedMat->nrOfTextures += 1;
					}
				}
			}
		}
	}

	/* SEND TO BUFFER */
	changedInfo->newmat = *changedMat;

	int msgSize = sizeof(ChangedMaterial) + ((sizeof(TextureHeader) * changedInfo->newmat.nrOfTextures)) + 1;

	// Local "buffer" to store the message
	char* msg = new char[msgSize];
	int offset = 0;

	// Copy the data from the header to the local buffer
	memcpy(msg + offset, (char*)changedInfo, sizeof(ChangedMaterial));
	offset += sizeof(ChangedMaterial);


	for (int i = 0; i < textures.size(); i++)
	{
		memcpy(msg + offset, (char*)textures[i], sizeof(TextureHeader));
		offset += sizeof(TextureHeader);
	}

	// Create the section header, which will tell the consumer that a new mesh is coming
	SectionHeader* mainHeader = new SectionHeader;
	mainHeader->header = CHANGED_MATERIAL;
	mainHeader->messageSize = msgSize;


	// Send first the section header
	buffer->Send(msg, mainHeader);

	// Fix memory leaks here
	if (changedMat)
		delete changedMat;

	if(changedInfo)
		delete changedInfo;

	if(mainHeader)
		delete mainHeader;

	for (int i = 0; i < textures.size(); i++)
	{
		delete textures[i];
	}
	textures.clear();
}

void AttributeFileChangeCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData)
{
	if (msg & MNodeMessage::AttributeMessage::kAttributeSet)
	{
		if (plug.name() == (MFnDependencyNode(plug.node()).name() + ".fileTextureName"))
		{			
			ChangedTexture* changedTexture = new ChangedTexture;

			MFnDependencyNode dep(plug.node());
			MPlugArray outConnections;
			MPlug plug = dep.findPlug("outColor");
			plug.connectedTo(outConnections, true, true);

			for (int i = 0; i < outConnections.length(); i++)
			{
				MFnLambertShader templamb(outConnections[i].node());
				MPlugArray outFromlambertConnections;
				MPlug lambertPlugs = templamb.findPlug("outColor");
				lambertPlugs.connectedTo(outFromlambertConnections, true, true);

				for (int out = 0; out < outFromlambertConnections.length(); out++)
				{
					if (outFromlambertConnections[out].node().hasFn(MFn::kShadingEngine))
					{
						MFnDependencyNode shadingNode(outFromlambertConnections[out].node());
						if (strcmp(shadingNode.name().asChar(), "initialParticleSE") != 0)
						{
							MPlug dagSetMembers = shadingNode.findPlug("dagSetMembers");
							for (int child = 0; child < dagSetMembers.numElements(); child++)
							{
								MPlugArray dagSetmembersConnections;
								dagSetMembers[child].connectedTo(dagSetmembersConnections, true, false);
								for (int d = 0; d < dagSetmembersConnections.length(); d++)
								{
									if (dagSetmembersConnections[d].node().hasFn(MFn::kMesh) && MFnMesh(dagSetmembersConnections[d].node()).name() != "shaderBallGeomShape1")
									{
										MFnMesh mesh(dagSetmembersConnections[d].node());
										MObject dagobject(mesh.parent(0));
										if (dagobject.hasFn(MFn::kDagNode))
										{
											MFnDagNode dag(dagobject);
											strcpy(changedTexture->meshName, dag.name().asChar());
										}
									}
								}
							}
						}
					}
				}

				MPlugArray lambertConnection;
				MPlug lambertPlugsTexture = templamb.findPlug("color");
				lambertPlugsTexture.connectedTo(lambertConnection, true, false);

				for (int k = 0; k < lambertConnection.length(); k++)
				{
					MObject textureCheck(lambertConnection[k].node());
					if (textureCheck.hasFn(MFn::kFileTexture))
					{
						MPlug filenamePlug = MFnDependencyNode(textureCheck).findPlug("fileTextureName", status);
						MString textureName;
						filenamePlug.getValue(textureName);
						if (textureName != "")
						{
							strcpy(changedTexture->filePath, textureName.asChar());							
						}
					}					
				}


				int msgSize = sizeof(ChangedTexture) + 1;

				// Local "buffer" to store the message
				char* msg = new char[msgSize];
				int offset = 0;

				// Copy the data from the header to the local buffer
				memcpy(msg + offset, (char*)changedTexture, sizeof(ChangedTexture));
				offset += sizeof(ChangedTexture);
				
				// Create the section header, which will tell the consumer that a new mesh is coming
				SectionHeader* mainHeader = new SectionHeader;
				mainHeader->header = CHANGED_TEXTURE;
				mainHeader->messageSize = msgSize;

				// Send first the section header
				buffer->Send(msg, mainHeader);
							
				if(changedTexture)
					delete changedTexture;

				if (mainHeader)
					delete mainHeader;
			}
		}		
	}
}

void AttributeMaterialChangeCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData)
{		
	if (msg & MNodeMessage::AttributeMessage::kConnectionMade)
	{
		if (plug.node().apiType() == MFn::Type::kLambert)		
		{			
			MObject tempobj(plug.node());
			MFnLambertShader templamb(tempobj);

			/* Search for plugs on right side */
			MPlugArray outConnections;
			MPlug shaderPlug = templamb.findPlug("outColor");
			shaderPlug.connectedTo(outConnections, true, true);
			for (int out = 0; out < outConnections.length(); out++)
			{
				if (outConnections[out].node().hasFn(MFn::kShadingEngine))
				{
					MFnDependencyNode shadingNode(outConnections[out].node());					
					if (strcmp(shadingNode.name().asChar(), "initialParticleSE") != 0)
					{
						MPlug dagSetMembers = shadingNode.findPlug("dagSetMembers");
						for (int child = 0; child < dagSetMembers.numElements(); child++)
						{
							MPlugArray dagSetmembersConnections;
							dagSetMembers[child].connectedTo(dagSetmembersConnections, true, false);
							for (int d = 0; d < dagSetmembersConnections.length(); d++)
							{								
								if (dagSetmembersConnections[d].node().hasFn(MFn::kMesh) && MFnMesh(dagSetmembersConnections[d].node()).name() != "shaderBallGeomShape1")
								{
									MFnMesh mesh(dagSetmembersConnections[d].node());
									MObject dagobject(mesh.parent(0));

									MaterialParametersChanged(dagobject, plug);

									MCallbackId materialCallback = MNodeMessage::addAttributeChangedCallback(plug.node(), AttributeMaterialChangeCallback, NULL, &status);
									if (status == MS::kSuccess)
									{
										MFnDependencyNode ob(plug.node());
										std::string name = ob.name().asChar();
										std::string extra = "materialChanged";

										auto iterator = callbacks.find(name + extra);
										if (iterator != callbacks.end())
										{
											std::cout << "Callback erased: " << iterator->first << std::endl;
											MMessage::removeCallback(iterator->second);
											callbacks.erase(iterator);
										}
										callbacks.insert({ name + extra, materialCallback });
									}

									if (otherPlug.node().apiType() == MFn::Type::kFileTexture)
									{
										MObject dep(otherPlug.node());
										MCallbackId fileCallback = MNodeMessage::addAttributeChangedCallback(dep, AttributeFileChangeCallback, NULL, &status);
										if (status == MS::kSuccess)
										{
											MFnDependencyNode ob(plug.node());
											std::string name = ob.name().asChar();
											std::string extra = "fileChanged";

											auto iterator = callbacks.find(name + extra);
											if (iterator != callbacks.end())
											{
												std::cout << "Callback erased: " << iterator->first << std::endl;
												MMessage::removeCallback(iterator->second);
												callbacks.erase(iterator);
											}
											callbacks.insert({ name + extra, fileCallback });
										}
									}
								}
							}
						}
					}
				}
			}			
		}
	}

	if (msg & MNodeMessage::kConnectionBroken)
	{
		if (plug.node().apiType() == MFn::Type::kLambert)		// 
		{			
			MObject tempobj(plug.node());

			MFnLambertShader templamb(tempobj);

			/* Search for plugs on right side */
			MPlugArray outConnections;
			MPlug shaderPlug = templamb.findPlug("outColor");
			shaderPlug.connectedTo(outConnections, true, true);
			for (int out = 0; out < outConnections.length(); out++)
			{
				if (outConnections[out].node().hasFn(MFn::kShadingEngine))
				{
					MFnDependencyNode shadingNode(outConnections[out].node());
					if (strcmp(shadingNode.name().asChar(), "initialParticleSE") != 0)
					{
						MPlug dagSetMembers = shadingNode.findPlug("dagSetMembers");
						for (int child = 0; child < dagSetMembers.numElements(); child++)
						{
							MPlugArray dagSetmembersConnections;
							dagSetMembers[child].connectedTo(dagSetmembersConnections, true, false);

							for (int d = 0; d < dagSetmembersConnections.length(); d++)
							{
								//std::cout << "This node: " << dagSetmembersConnections[d].name() << std::endl;
								if (dagSetmembersConnections[d].node().hasFn(MFn::kMesh) && MFnMesh(dagSetmembersConnections[d].node()).name() != "shaderBallGeomShape1")
								{
									MFnMesh mesh(dagSetmembersConnections[d].node());
									MObject dagobject(mesh.parent(0));

									MaterialParametersChanged(dagobject, plug);	

									MCallbackId materialCallback = MNodeMessage::addAttributeChangedCallback(plug.node(), AttributeMaterialChangeCallback, NULL, &status);
									if (status == MS::kSuccess)
									{
										MFnDependencyNode ob(plug.node());
										std::string name = ob.name().asChar();
										std::string extra = "materialChanged";

										auto iterator = callbacks.find(name + extra);
										if (iterator != callbacks.end())
										{
											std::cout << "Callback erased: " << iterator->first << std::endl;
											MMessage::removeCallback(iterator->second);
											callbacks.erase(iterator);
										}
										callbacks.insert({ name + extra, materialCallback });
									}

									if (otherPlug.node().apiType() == MFn::Type::kFileTexture)
									{
										MObject dep(otherPlug.node());
										//std::cout << std::endl << "CHECK OTHER: " << dep.name() << std::endl;
										std::cout << "OTHER PLUG " << otherPlug.name() << std::endl;

										MCallbackId fileCallback = MNodeMessage::addAttributeChangedCallback(dep, AttributeFileChangeCallback, NULL, &status);
										if (status == MS::kSuccess)
										{
											MFnDependencyNode ob(plug.node());
											std::string name = ob.name().asChar();
											std::string extra = "fileChanged";

											auto iterator = callbacks.find(name + extra);
											if (iterator != callbacks.end())
											{
												std::cout << "Callback erased: " << iterator->first << std::endl;
												MMessage::removeCallback(iterator->second);
												callbacks.erase(iterator);
											}
											callbacks.insert({ name + extra, fileCallback });
										}

									}


								}
							}
						}
					}
				}
			}
		}		
	}

	if (msg & MNodeMessage::AttributeMessage::kAttributeSet)
	{		
		MObject tempobj(plug.node());
		if (tempobj.hasFn(MFn::kLambert))
		{
			/* Get color here */
			MFnLambertShader templamb(tempobj);

			/* Search for plugs on right side */
			MPlugArray outConnections;
			MPlug shaderPlug = templamb.findPlug("outColor");
			shaderPlug.connectedTo(outConnections, true, true);

			for (int out = 0; out < outConnections.length(); out++)
			{
				if (outConnections[out].node().hasFn(MFn::kShadingEngine))
				{
					MFnDependencyNode shadingNode(outConnections[out].node());

					if (strcmp(shadingNode.name().asChar(), "initialParticleSE") != 0)
					{
						MPlug dagSetMembers = shadingNode.findPlug("dagSetMembers");

						for (int child = 0; child < dagSetMembers.numElements(); child++)
						{
							MPlugArray dagSetmembersConnections;
							dagSetMembers[child].connectedTo(dagSetmembersConnections, true, false);

							for (int d = 0; d < dagSetmembersConnections.length(); d++)
							{
								//std::cout << "Dags: " << dagSetmembersConnections[d].name() << std::endl;
								if (dagSetmembersConnections[d].node().hasFn(MFn::kMesh) && MFnMesh(dagSetmembersConnections[d].node()).name() != "shaderBallGeomShape1")
								{									
									MFnMesh mesh(dagSetmembersConnections[d].node());
									MObject dagobject(mesh.parent(0));

									MaterialParametersChanged(dagobject, plug);		

									MCallbackId materialCallback = MNodeMessage::addAttributeChangedCallback(plug.node(), AttributeMaterialChangeCallback, NULL, &status);
									if (status == MS::kSuccess)
									{
										MFnDependencyNode ob(plug.node());
										std::string name = ob.name().asChar();
										std::string extra = "materialChanged";

										auto iterator = callbacks.find(name + extra);
										if (iterator != callbacks.end())
										{
											std::cout << "Callback erased: " << iterator->first << std::endl;
											MMessage::removeCallback(iterator->second);
											callbacks.erase(iterator);
										}
										callbacks.insert({ name + extra, materialCallback });
									}


									if (otherPlug.node().apiType() == MFn::Type::kFileTexture)
									{
										MObject dep(otherPlug.node());
										//std::cout << std::endl << "CHECK OTHER: " << dep.name() << std::endl;
										std::cout << "OTHER PLUG " << otherPlug.name() << std::endl;

										MCallbackId fileCallback = MNodeMessage::addAttributeChangedCallback(dep, AttributeFileChangeCallback, NULL, &status);
										if (status == MS::kSuccess)
										{
											MFnDependencyNode ob(plug.node());
											std::string name = ob.name().asChar();
											std::string extra = "fileChanged";

											auto iterator = callbacks.find(name + extra);
											if (iterator != callbacks.end())
											{
												std::cout << "Callback erased: " << iterator->first << std::endl;
												MMessage::removeCallback(iterator->second);
												callbacks.erase(iterator);
											}
											callbacks.insert({ name + extra, fileCallback });
										}

									}


								}
							}
						}
					}
				}
			}
		}
	}
}

void NodeRemoved(MObject& node, void* clientData)
{
	MStatus status;
	MFnTransform transform(node, &status);
	if (status == MS::kSuccess)
	{
		MFnDagNode* dag = new MFnDagNode(node);
		clientData = (void*)dag;

		SectionHeader* mainHeader = new SectionHeader;
		mainHeader->header = DELETE_MESH;
		mainHeader->messageSize = sizeof(DeleteMesh);

		DeleteMesh header;
		strcpy(header.meshName, transform.name().asChar());

		char* msg = new char[sizeof(DeleteMesh)];
		memcpy(msg, &header, sizeof(DeleteMesh));

		buffer->Send(msg, mainHeader);
	}
}

// This function will serve as a way to collect info regarding materials and textures
void GetMaterials(MObject meshNode, MaterialHeader*& materials, std::vector<TextureHeader*>& textures)
{
	int matIndex = 0;

	MFnMesh mesh(meshNode);

	MObjectArray shaders;
	MIntArray materialindices;
	mesh.getConnectedShaders(0, shaders, materialindices);
		
	for (int i = 0; i < shaders.length(); i++)
	{		
		MaterialHeader* newMat = new MaterialHeader;

		MFnDependencyNode depobj(shaders[i]);
		MPlugArray connections;

		MPlug shaderPlug = depobj.findPlug("surfaceShader");
		shaderPlug.connectedTo(connections, true, false);

		for (int u = 0; u < connections.length(); u++)
		{
			MObject objcheck(connections[u].node());

			if (objcheck.hasFn(MFn::kLambert))
			{
				MFnDependencyNode tempLambert = (objcheck);
				MFnLambertShader lambert(objcheck);
				
				newMat->diffuse.x = lambert.color().r;
				newMat->diffuse.y = lambert.color().g;
				newMat->diffuse.z = lambert.color().b;
				newMat->diffuse.w = lambert.color().a;

				newMat->ambient.x = lambert.ambientColor().r;
				newMat->ambient.y = lambert.ambientColor().g;
				newMat->ambient.z = lambert.ambientColor().b;
				newMat->ambient.w = lambert.ambientColor().a;

				MPlugArray lambertConnection;
				MPlug lambertPlugs = tempLambert.findPlug("color");
				lambertPlugs.connectedTo(lambertConnection, true, false);

				for (int k = 0; k < lambertConnection.length(); k++)
				{
					MObject textureCheck(lambertConnection[k].node());
					if (textureCheck.hasFn(MFn::kFileTexture))
					{
						// Create new texture
						TextureHeader* newTexture = new TextureHeader;
						MPlug filenamePlug = MFnDependencyNode(textureCheck).findPlug("fileTextureName", status);
						
						MString textureName;
						filenamePlug.getValue(textureName);

						if (textureName != "")
						{
							strcpy(newTexture->filePath, textureName.asChar());
							textures.push_back(newTexture);

							newMat->hasTexture = true;
							newMat->nrOfTextures += 1;
						}						
					}

					if (textureCheck.apiType() == MFn::Type::kFileTexture)
					{
						MObject dep(textureCheck);
						MCallbackId fileCallback = MNodeMessage::addAttributeChangedCallback(dep, AttributeFileChangeCallback, NULL, &status);
						if (status == MS::kSuccess)
						{
							MFnDependencyNode ob(dep);
							std::string name = ob.name().asChar();
							std::string extra = "fileChanged";

							auto iterator = callbacks.find(name + extra);
							if (iterator != callbacks.end())
							{
								std::cout << "Callback erased: " << iterator->first << std::endl;
								MMessage::removeCallback(iterator->second);
								callbacks.erase(iterator);
							}
							callbacks.insert({ name + extra, fileCallback });
						}
					}
				}

				// For normal mapping
				MPlugArray lambertMapConnections;
				MPlug lambertMapPlugs = tempLambert.findPlug("normalCamera");
				lambertMapPlugs.connectedTo(lambertMapConnections, true, false);
								
				for (int j = 0; j < lambertMapConnections.length(); j++)
				{
					MObject normalCheck(lambertMapConnections[j].node());
					if (normalCheck.hasFn(MFn::kBump))
					{	
						MPlug bumpValuePlug = MFnDependencyNode(normalCheck).findPlug("bumpValue");
						MPlugArray bumpValuePlugConnections;
						bumpValuePlug.connectedTo(bumpValuePlugConnections, true, false);

						for (int h = 0; h < bumpValuePlugConnections.length(); h++)
						{
							MObject bumpValueCheck(bumpValuePlugConnections[h].node());
							if (bumpValueCheck.hasFn(MFn::kFileTexture))
							{
								TextureHeader* newBumpTexture = new TextureHeader;

								MPlug bumpfilenamePlug = MFnDependencyNode(bumpValueCheck).findPlug("fileTextureName");
								MString bumptextureName;
								bumpfilenamePlug.getValue(bumptextureName);
								if (bumptextureName != "")
								{
									strcpy(newBumpTexture->filePath, bumptextureName.asChar());
									textures.push_back(newBumpTexture);

									newMat->hasNormal = true;
									newMat->nrOfTextures += 1;
								}
							}
						}
					}
				}


				MCallbackId materialCallback = MNodeMessage::addAttributeChangedCallback(objcheck, AttributeMaterialChangeCallback, NULL, &status);
				if (status == MS::kSuccess)
				{
					MFnDependencyNode ob(objcheck);
					std::string name = ob.name().asChar();
					std::string extra = "materialChanged";

					auto iterator = callbacks.find(name + extra);
					if (iterator != callbacks.end())
					{
						std::cout << "Callback erased: " << iterator->first << std::endl;
						MMessage::removeCallback(iterator->second);
						callbacks.erase(iterator);
					}
					callbacks.insert({ name + extra, materialCallback });
				}				
			}					
		}

		materials = newMat;
	}
}

void attributeShapeChanged(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData)
{		
	std::cout << "Plug 1" << plug.name() << std::endl;
	std::cout << "Plug 2" << otherPlug.name() << std::endl;

	if (msg & MNodeMessage::kConnectionMade)
	{		
		if (otherPlug.node().hasFn(MFn::kShadingEngine))
		{
			MFnDependencyNode lamb(otherPlug.node());

			MPlugArray connections;
			MPlug shaderPlug = lamb.findPlug("surfaceShader");
			shaderPlug.connectedTo(connections, true, false);

			std::vector<TextureHeader*> textures;
			for (int i = 0; i < connections.length(); i++)
			{
				MObject connection(connections[i].node());
				if (connection.hasFn(MFn::kLambert))
				{
					ChangedMaterial* changedInfo = new ChangedMaterial;

					MFnLambertShader templamb(connection);									
					
					MObject dagobject(plug.node());
					if (dagobject.hasFn(MFn::kDagNode))
					{
						MFnMesh shape(dagobject);
						MFnDagNode dag(shape.parent(0));

						// SEND DAG NAME HERE
						strcpy(changedInfo->meshName, dag.name().asChar());
					}		

					// GET MATERIAL INFO TO SEND
					MaterialHeader* changedMat = new MaterialHeader;
					changedMat->diffuse.x = templamb.color().r;
					changedMat->diffuse.y = templamb.color().g;
					changedMat->diffuse.z = templamb.color().b;
					changedMat->diffuse.w = templamb.color().a;

					changedMat->ambient.x = templamb.ambientColor().r;
					changedMat->ambient.y = templamb.ambientColor().g;
					changedMat->ambient.z = templamb.ambientColor().b;
					changedMat->ambient.w = templamb.ambientColor().a;
										
					/* Check new textures maybe??  */
					MPlugArray lambertConnection;
					MPlug lambertPlugs = templamb.findPlug("color");
					lambertPlugs.connectedTo(lambertConnection, true, true);

					for (int k = 0; k < lambertConnection.length(); k++)
					{
						std::cout << lambertConnection[k].name() << std::endl;
						MObject textureCheck(lambertConnection[k].node());
						if (textureCheck.hasFn(MFn::kFileTexture))
						{
							// Create new texture
							TextureHeader* newTexture = new TextureHeader;

							MPlug filenamePlug = MFnDependencyNode(textureCheck).findPlug("fileTextureName");
							MString textureName;
							filenamePlug.getValue(textureName);

							if (textureName != "")
							{
								strcpy(newTexture->filePath, textureName.asChar());
								textures.push_back(newTexture);

								changedMat->hasTexture = true;
								changedMat->nrOfTextures += 1;								
							}
						}
					}

					// For normal mapping
					MPlugArray lambertMapConnections;
					MPlug lambertMapPlugs = templamb.findPlug("normalCamera");
					lambertMapPlugs.connectedTo(lambertMapConnections, true, true);

					for (int j = 0; j < lambertMapConnections.length(); j++)
					{
						MObject normalCheck(lambertMapConnections[j].node());
						if (normalCheck.hasFn(MFn::kBump))
						{
							MPlug bumpValuePlug = MFnDependencyNode(normalCheck).findPlug("bumpValue");
							MPlugArray bumpValuePlugConnections;
							bumpValuePlug.connectedTo(bumpValuePlugConnections, true, true);

							for (int h = 0; h < bumpValuePlugConnections.length(); h++)
							{
								MObject bumpValueCheck(bumpValuePlugConnections[h].node());

								if (bumpValueCheck.hasFn(MFn::kFileTexture))
								{
									TextureHeader* newBumpTexture = new TextureHeader;

									MPlug bumpfilenamePlug = MFnDependencyNode(bumpValueCheck).findPlug("fileTextureName");
									MString bumptextureName;
									bumpfilenamePlug.getValue(bumptextureName);

									if (bumptextureName != "")
									{
										strcpy(newBumpTexture->filePath, bumptextureName.asChar());
										textures.push_back(newBumpTexture);

										changedMat->hasNormal = true;
										changedMat->nrOfTextures += 1;
									}
								}
							}
						}
					}

					/* SEND TO BUFFER */
					changedInfo->newmat = *changedMat;
										
					int msgSize = sizeof(ChangedMaterial) + ((sizeof(TextureHeader) * changedInfo->newmat.nrOfTextures)) + 1;

					// Local "buffer" to store the message
					char* msg = new char[msgSize];
					int offset = 0;

					// Copy the data from the header to the local buffer
					memcpy(msg + offset, (char*)changedInfo, sizeof(ChangedMaterial));
					offset += sizeof(ChangedMaterial);
										
					for (int i = 0; i < textures.size(); i++)
					{
						memcpy(msg + offset, (char*)textures[i], sizeof(TextureHeader));
						offset += sizeof(TextureHeader);
					}

					// Create the section header, which will tell the consumer that a new mesh is coming
					SectionHeader* mainHeader = new SectionHeader;
					mainHeader->header = CHANGED_MATERIAL;
					mainHeader->messageSize = msgSize;

					// Send the section header
					buffer->Send(msg, mainHeader);

					// Fix memory leaks here
					if(mainHeader)
						delete mainHeader;
					
					if (changedMat)
						delete changedMat;

					if(changedInfo)
						delete changedInfo;

					for (int del = 0; del < textures.size(); del++)
					{
						delete textures[del];
					}
					textures.clear();


					MCallbackId materialCallback = MNodeMessage::addAttributeChangedCallback(connection, AttributeMaterialChangeCallback, NULL, &status);
					if (status == MS::kSuccess)
					{
						MFnDependencyNode ob(connection);
						std::string name = ob.name().asChar();
						std::string extra = "materialChanged";

						auto iterator = callbacks.find(name + extra);
						if (iterator != callbacks.end())
						{
							std::cout << "Callback erased: " << iterator->first << std::endl;
							MMessage::removeCallback(iterator->second);
							callbacks.erase(iterator);
						}
						callbacks.insert({ name + extra, materialCallback });
					}									
				}
			}		
		}
	}
	
	if (msg & MNodeMessage::kConnectionBroken)
	{		
		if (otherPlug.node().hasFn(MFn::kShadingEngine))
		{
			MFnDependencyNode lamb(otherPlug.node());

			MPlugArray connections;
			MPlug shaderPlug = lamb.findPlug("surfaceShader");
			shaderPlug.connectedTo(connections, true, false);

			std::vector<TextureHeader*> textures;
			for (int i = 0; i < connections.length(); i++)
			{
				MObject connection(connections[i].node());
				if (connection.hasFn(MFn::kLambert))
				{
					ChangedMaterial* changedInfo = new ChangedMaterial;

					MFnLambertShader templamb(connection);								
					
					MObject dagobject(plug.node());
					if (dagobject.hasFn(MFn::kDagNode))
					{
						MFnMesh shape(dagobject);
						MFnDagNode dag(shape.parent(0));

						// SEND DAG NAME HERE
						strcpy(changedInfo->meshName, dag.name().asChar());
					}		

					// GET MATERIAL INFO TO SEND
					MaterialHeader* changedMat = new MaterialHeader;
					changedMat->diffuse.x = templamb.color().r;
					changedMat->diffuse.y = templamb.color().g;
					changedMat->diffuse.z = templamb.color().b;
					changedMat->diffuse.w = templamb.color().a;

					changedMat->ambient.x = templamb.ambientColor().r;
					changedMat->ambient.y = templamb.ambientColor().g;
					changedMat->ambient.z = templamb.ambientColor().b;
					changedMat->ambient.w = templamb.ambientColor().a;
					

					/* Check new textures maybe??  */
					MPlugArray lambertConnection;
					MPlug lambertPlugs = templamb.findPlug("color");
					lambertPlugs.connectedTo(lambertConnection, true, false);

					for (int k = 0; k < lambertConnection.length(); k++)
					{
						std::cout << lambertConnection[k].name() << std::endl;
						MObject textureCheck(lambertConnection[k].node());
						if (textureCheck.hasFn(MFn::kFileTexture))
						{

							// Create new texture
							TextureHeader* newTexture = new TextureHeader;

							MPlug filenamePlug = MFnDependencyNode(textureCheck).findPlug("fileTextureName");
							MString textureName;
							filenamePlug.getValue(textureName);

							if (textureName != "")
							{
								strcpy(newTexture->filePath, textureName.asChar());
								textures.push_back(newTexture);

								changedMat->hasTexture = true;
								changedMat->nrOfTextures += 1;
							}
						}
					}

					// For normal mapping
					MPlugArray lambertMapConnections;
					MPlug lambertMapPlugs = templamb.findPlug("normalCamera");
					lambertMapPlugs.connectedTo(lambertMapConnections, true, false);

					for (int j = 0; j < lambertMapConnections.length(); j++)
					{
						MObject normalCheck(lambertMapConnections[j].node());
						if (normalCheck.hasFn(MFn::kBump))
						{

							MPlug bumpValuePlug = MFnDependencyNode(normalCheck).findPlug("bumpValue");
							MPlugArray bumpValuePlugConnections;
							bumpValuePlug.connectedTo(bumpValuePlugConnections, true, false);

							for (int h = 0; h < bumpValuePlugConnections.length(); h++)
							{
								MObject bumpValueCheck(bumpValuePlugConnections[h].node());

								if (bumpValueCheck.hasFn(MFn::kFileTexture))
								{
									TextureHeader* newBumpTexture = new TextureHeader;

									MPlug bumpfilenamePlug = MFnDependencyNode(bumpValueCheck).findPlug("fileTextureName");
									MString bumptextureName;
									bumpfilenamePlug.getValue(bumptextureName);

									if (bumptextureName != "")
									{
										strcpy(newBumpTexture->filePath, bumptextureName.asChar());
										textures.push_back(newBumpTexture);

										changedMat->hasNormal = true;
										changedMat->nrOfTextures += 1;

									}
								}
							}
						}
					}

					/* SEND TO BUFFER */
					changedInfo->newmat = *changedMat;

					int msgSize = sizeof(ChangedMaterial) + ((sizeof(TextureHeader) * changedInfo->newmat.nrOfTextures)) + 1;


					// Local "buffer" to store the message
					char* msg = new char[msgSize];
					int offset = 0;

					// Copy the data from the header to the local buffer
					memcpy(msg + offset, (char*)changedInfo, sizeof(ChangedMaterial));
					offset += sizeof(ChangedMaterial);


					for (int i = 0; i < textures.size(); i++)
					{
						memcpy(msg + offset, (char*)textures[i], sizeof(TextureHeader));
						offset += sizeof(TextureHeader);
					}

					// Create the section header, which will tell the consumer that a new mesh is coming
					SectionHeader* mainHeader = new SectionHeader;
					mainHeader->header = CHANGED_MATERIAL;
					mainHeader->messageSize = msgSize;


					// Send first the section header
					buffer->Send(msg, mainHeader);

					// Fix memory leaks here
					if (changedInfo)
						delete changedInfo;

					if (changedMat)
						delete changedMat;

					if(mainHeader)
						delete mainHeader;
					
					for (int del = 0; del < textures.size(); del++)
					{
						delete textures[del];
					}
					textures.clear();

					MCallbackId materialCallback = MNodeMessage::addAttributeChangedCallback(connection, AttributeMaterialChangeCallback, NULL, &status);
					if (status == MS::kSuccess)
					{
						MFnDependencyNode ob(connection);
						std::string name = ob.name().asChar();
						std::string extra = "materialChanged";

						auto iterator = callbacks.find(name + extra);
						if (iterator != callbacks.end())
						{
							std::cout << "Callback erased: " << iterator->first << std::endl;
							MMessage::removeCallback(iterator->second);
							callbacks.erase(iterator);
						}
						callbacks.insert({ name + extra, materialCallback });
					}
				}
			}		
		}
	}

	/* Displaying everythime I change a vertex. Shows the vertex id, and the shape its connected to */
	if (msg & MNodeMessage::kAttributeSet)
	{
		if (plug.isElement() && (plug.partialName().asChar()[0] == 'p') && (plug.partialName().asChar()[1] == 't') && (plug.partialName().asChar()[2] == '['))
		{			
			std::vector<Vertex*> vertices;

			MFnMesh mesh(plug.node());
			MFnTransform transDag(mesh.parent(0));

			TopologyChanged* topChanged = new TopologyChanged;
			strcpy(topChanged->name, transDag.name().asChar());

			topChanged->wholeMesh = false;

			MPointArray vertexArray;
			MFloatVectorArray normalArray;
			MFloatArray u, v;

			// Get vertices information
			mesh.getPoints(vertexArray, MSpace::kObject);
			mesh.getNormals(normalArray);
			mesh.getUVs(u, v);

			// For indexing triangles
			MIntArray count, id;
			MIntArray triangleCount, triangleIndex;
			MIntArray vertexCounts, vertexID;
			MIntArray normalCount, normalList;

			// Get all the indices
			mesh.getAssignedUVs(count, id);
			mesh.getTriangleOffsets(triangleCount, triangleIndex);
			mesh.getVertices(vertexCounts, vertexID);
			mesh.getNormalIds(normalCount, normalList);

			for (int i = 0; i < triangleIndex.length(); i++)
			{
				Vertex* temp = new Vertex;

				temp->pos.x = vertexArray[vertexID[triangleIndex[i]]].x;
				temp->pos.y = vertexArray[vertexID[triangleIndex[i]]].y;
				temp->pos.z = vertexArray[vertexID[triangleIndex[i]]].z;

				temp->normal.x = normalArray[normalList[triangleIndex[i]]].x;
				temp->normal.y = normalArray[normalList[triangleIndex[i]]].y;
				temp->normal.z = normalArray[normalList[triangleIndex[i]]].z;

				temp->uv.x = u[id[triangleIndex[i]]];
				temp->uv.y = v[id[triangleIndex[i]]];

				vertices.push_back(temp);
			}

			topChanged->vertexCount = vertices.size();

			int msgSize = sizeof(TopologyChanged) + (sizeof(Vertex) * vertices.size()) + 1;

			char* msg = new char[msgSize];
			int offset = 0;

			memcpy(msg + offset, (char*)topChanged, sizeof(TopologyChanged));
			offset += sizeof(TopologyChanged);

			for (int i = 0; i < vertices.size(); i++)
			{
				memcpy(msg + offset, (char*)vertices[i], sizeof(Vertex));
				offset += sizeof(Vertex);
			}

			// Create the section header, which will tell the consumer that a new mesh is coming
			SectionHeader* mainHeader = new SectionHeader;
			mainHeader->header = CHANGED_MESH;
			mainHeader->messageSize = msgSize;

			// Send first the section header
			buffer->Send(msg, mainHeader);


			if (topChanged)
				delete topChanged;

			if (mainHeader)
				delete mainHeader;

			for (int i = 0; i < vertices.size(); i++)
				delete vertices[i];
		}
	}	
}

 /* Iterate through stuff on the scene */
void nameChangedFunction(MObject& node, const MString& str, void* clientData)
{	
	if (node.hasFn(MFn::kTransform))
	{
		MFnDagNode dag(node, &status);
		if (status == MStatus::kSuccess)
		{
			NameChanged* newName = new NameChanged;
			strcpy(newName->oldName, str.asChar());
			strcpy(newName->newName, dag.name().asChar());
						
			int msgSize = sizeof(NameChanged) + 1;

			// Local "buffer" to store the message
			char* msg = new char[msgSize];
			int offset = 0;

			memcpy(msg + offset, (char*)newName, sizeof(NameChanged));
			offset += sizeof(NameChanged);

			// Create the section header, which will tell the consumer that a new mesh is coming
			SectionHeader* mainHeader = new SectionHeader;
			mainHeader->header = CHANGED_NAME;
			mainHeader->messageSize = msgSize;

			// Send first the section header
			buffer->Send(msg, mainHeader);
		}
	}
}

// This function will be the main function for sending mesh info
// It will do so in a smooth and nice manner
// Without a bunch of prints
void SendMesh(MObject node)
{
	MStatus status;
	MFnMesh mesh(node, &status);

	if (status == MStatus::kSuccess)
	{
		MeshHeader* meshHeader = new MeshHeader;
		MaterialHeader* materials = nullptr;
		std::vector<TextureHeader*> textures;
		std::vector<Vertex*> vertices;

		MTransformationMatrix localMatrix;
		MMatrix worldMatrix;

		MFnDagNode transform(mesh.parent(0, &status));
		if (status == MStatus::kSuccess)
		{
			strcpy(meshHeader->name, transform.name().asChar());
			MDagPath path = MDagPath::getAPathTo(node);

			localMatrix = transform.transformationMatrix();
			worldMatrix = path.inclusiveMatrix();
		}

		MPointArray vertexArray;
		MFloatVectorArray normalArray;
		MFloatArray u, v;

		// Get vertices information
		mesh.getPoints(vertexArray, MSpace::kObject);
		mesh.getNormals(normalArray);
		mesh.getUVs(u, v);

		// For indexing triangles
		MIntArray count, id;
		MIntArray triangleCount, triangleIndex;
		MIntArray vertexCounts, vertexID;
		MIntArray normalCount, normalList;

		// Get all the indices
		mesh.getAssignedUVs(count, id);
		mesh.getTriangleOffsets(triangleCount, triangleIndex);
		mesh.getVertices(vertexCounts, vertexID);
		mesh.getNormalIds(normalCount, normalList);

		for (int i = 0; i < triangleIndex.length(); i++)
		{
			Vertex* temp = new Vertex;

			temp->pos.x = vertexArray[vertexID[triangleIndex[i]]].x;
			temp->pos.y = vertexArray[vertexID[triangleIndex[i]]].y;
			temp->pos.z = vertexArray[vertexID[triangleIndex[i]]].z;

			temp->normal.x = normalArray[normalList[triangleIndex[i]]].x;
			temp->normal.y = normalArray[normalList[triangleIndex[i]]].y;
			temp->normal.z = normalArray[normalList[triangleIndex[i]]].z;

			temp->uv.x = u[id[triangleIndex[i]]];
			temp->uv.y = v[id[triangleIndex[i]]];

			vertices.push_back(temp);
		}

		GetMaterials(node, materials, textures);

		if (materials)
		{
			meshHeader->nrOfVertices = vertices.size();
			meshHeader->material = *materials;

			// Send new matrix
			int index = 0;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					meshHeader->transformation[index] = worldMatrix[i][j];
					index++;
				}
			}			


			int msgSize = sizeof(MeshHeader) + ((sizeof(Vertex) * meshHeader->nrOfVertices))
				+ (sizeof(MaterialHeader)) + ((sizeof(TextureHeader) * materials->nrOfTextures)) + 1;


			// Local "buffer" to store the message
			char* msg = new char[msgSize];
			int offset = 0;

			// Copy the data from the header to the local buffer
			memcpy(msg + offset, (char*)meshHeader, sizeof(MeshHeader));
			offset += sizeof(MeshHeader);

			// Copy the vertices to the local buffer
			for (int i = 0; i < vertices.size(); i++)
			{
				memcpy(msg + offset, (char*)vertices[i], sizeof(Vertex));
				offset += sizeof(Vertex);
			}

			for (int i = 0; i < textures.size(); i++)
			{
				memcpy(msg + offset, (char*)textures[i], sizeof(TextureHeader));
				offset += sizeof(TextureHeader);
			}

			// Create the section header, which will tell the consumer that a new mesh is coming
			SectionHeader* mainHeader = new SectionHeader;
			mainHeader->header = NEW_MESH;
			mainHeader->messageSize = msgSize;

			// Send first the section header
			buffer->Send(msg, mainHeader);


			/* Clear vertices vector -> makes it ready for a new bastard */
			for (int i = 0; i < vertices.size(); i++)
			{
				delete vertices[i];
			}

			for (int k = 0; k < textures.size(); k++)
			{
				delete textures[k];
			}

			// Fix memory leaks here
			if(mainHeader)
				delete mainHeader;

			if(materials)
				delete materials;

			if(meshHeader)
				delete meshHeader;
		}
	}
}

// Dirty plug function, will activate once the plug isnt dirty anymore
void DirtyPlugFunction(MObject& node, MPlug& plug, void* clientData)
{
	MStatus statuc;
	MFnMesh mesh(node, &status);
	if (status == MS::kSuccess)
	{
		MFnDependencyNode depNode(node);

		if (plug == depNode.findPlug("inMesh"))
		{
			// Send mesh information
			SendMesh(node);

			MFnDagNode dag(node);
			MObject parent = dag.parent(0);
			MFnDagNode parentNode(parent);

			std::string name = parentNode.name().asChar();

			for (auto i : callbacks)
			{
				if (i.first == name + "dirty")
				{
					std::cout << "Callback erased: " << i.first << std::endl;
					//std::cout << "deleted callback " << name + "dirty" << std::endl;
					MMessage::removeCallback(i.second);
				}
			}
		}
		else
		{
			//std::cout << "Not the same" << std::endl;
		}
	}
}

void cameraMoved(const MString& str, void* clientData)
{
	MString cmd = "getPanel -wf";
	MString activePanel = MGlobal::executeCommandStringResult(cmd);

	if (strcmp(str.asChar(), activePanel.asChar()) == 0)
	{
		CameraTransformation* camTrans = new CameraTransformation;

		MDagPath daggyPath;
		M3dView::active3dView().getCamera(daggyPath);

		
		MMatrix worldMatrix;
		worldMatrix = daggyPath.inclusiveMatrix();

		MObject obj = daggyPath.node();
		MFnDagNode node(obj);

		MFnCamera cam(obj);
		if (cam.isOrtho())
		{
			float camWidth = cam.orthoWidth();
			float camHeight = 2 / cam.projectionMatrix()[1][1];

			camTrans->camHeight = camHeight;
			camTrans->camWidth = camWidth;
		}
		else
			camTrans->camWidth = cam.horizontalFieldOfView() * degrees;

	/*	double focalLength = cam.focalLength();
		double angle = cam.shutterAngle();

		float d = camTrans->camWidth;
		float a = 2 * atan(d / (2 * focalLength));
		float fieldOfView = a * degrees;*/

		camTrans->aspectRatio = cam.aspectRatio();
		camTrans->fieldOfView = cam.horizontalFieldOfView() * degrees;

		MObject parent = node.parent(0);
		MFnDagNode parentNode(parent);

		strcpy(camTrans->camName, parentNode.name().asChar());

		// SEND NEW MATRIX
		int index = 0;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				camTrans->transformation[index] = worldMatrix[i][j];
				index++;
			}
		}

		int msgSize = sizeof(CameraTransformation) + 1;

		char* msg = new char[msgSize];
		int offset = 0;

		memcpy(msg + offset, (char*)camTrans, sizeof(CameraTransformation));
		offset += sizeof(CameraTransformation);

		// Create the section header, which will tell the consumer that a new mesh is coming
		SectionHeader* mainHeader = new SectionHeader;
		mainHeader->header = CHANGED_CAMERA;
		mainHeader->messageSize = msgSize;

		// Send first the section header
		buffer->Send(msg, mainHeader);
	}
}

void NodeAdded(MObject &node, void * clientData) 
{
	/* Check if node is a dag */
	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node, &status);

		/* If it is meshshape or a transform */
		if (node.hasFn(MFn::kMesh))
		{
			/* This is for shape, when i change vertices or something else in shapenode */
			/* Also add topology changed callback here*/
			MCallbackId attributeID = MNodeMessage::addAttributeChangedCallback(node, attributeShapeChanged, NULL, &status);
			if (status == MS::kSuccess)
			{
				MObject parent = dagNode.parent(0);
				MFnDagNode parentNode(parent);
								
				std::string name = parentNode.name().asChar();
				std::string extra = "shapeChanged";

				auto iterator = callbacks.find(name + extra);
				if (iterator != callbacks.end())
				{
					std::cout << "Callback erased: " << iterator->first << std::endl;
					MMessage::removeCallback(iterator->second);
					callbacks.erase(iterator);
				}
				callbacks.insert({ name + extra, attributeID });
			}

			MFnMesh mesh(node, &status);

			// This will fail if the mesh hasnt been created yet
			if (status == MS::kInvalidParameter)
			{
				// If it fails, we want to add a callback for when the mesh is created
				// Which we can do by checking the mesh plug
				MCallbackId plugDirty = MNodeMessage::addNodeDirtyPlugCallback(node, DirtyPlugFunction, NULL, &status);
				if (status == MS::kSuccess)
				{
					MObject parent = dagNode.parent(0);
					MFnDagNode parentNode(parent);

					std::string name = parentNode.name().asChar();
					std::string extra = "dirty";

					auto iterator = callbacks.find(name + extra);
					if (iterator != callbacks.end())
					{
						std::cout << "Callback erased: " << iterator->first << std::endl;
						MMessage::removeCallback(iterator->second);
						callbacks.erase(iterator);
					}
					callbacks.insert({ name + extra, plugDirty });
				}
			}
			// If the mesh didnt fail, for some reason, -> never gonne go in here
			else
			{
				/* For example duplication */
				//SendMesh(node);
			}

			/* Task 3: Displaying new vertices position */
			MCallbackId topoID = MPolyMessage::addPolyTopologyChangedCallback(node, topologyChanged, NULL, &status);
			if (status == MS::kSuccess)
			{
				MObject parent = dagNode.parent(0);
				MFnDagNode parentNode(parent);
								
				std::string name = parentNode.name().asChar();
				std::string extra = "topologychanged";

				auto iterator = callbacks.find(name + extra);
				if (iterator != callbacks.end())
				{
					std::cout << "Callback erased: " << iterator->first << std::endl;
					MMessage::removeCallback(iterator->second);
					callbacks.erase(iterator);
				}
				callbacks.insert({ name + extra, topoID });
			}
			
		}
		else if (node.hasFn(MFn::kTransform))
		{	
			/* Can change name on every new node */
			MCallbackId idName = MNodeMessage::addNameChangedCallback(node, nameChangedFunction, NULL, &status);
			if (status == MS::kSuccess)
			{				
				MFnDagNode* dag = new MFnDagNode(node);
				std::string name = dag->name().asChar();
				std::string extra = "nameChanged";

				auto iterator = callbacks.find(name + extra);
				if (iterator != callbacks.end())
				{
					std::cout << "Callback erased: " << iterator->first << std::endl;
					MMessage::removeCallback(iterator->second);
					callbacks.erase(iterator);
				}

				callbacks.insert({ name + extra, idName });

				if(dag)
					delete dag;
			}

			MCallbackId sceneRemovedID = MNodeMessage::addNodePreRemovalCallback(node, NodeRemoved, NULL, &status);
			if (status == MS::kSuccess)
			{
				std::string name = dagNode.name().asChar();
				std::string extra = "removed";

				auto iterator = callbacks.find(name + extra);
				if (iterator != callbacks.end())
				{
					std::cout << "Callback erased: " << iterator->first << std::endl;
					MMessage::removeCallback(iterator->second);
					callbacks.erase(iterator);
				}
				callbacks.insert({ name + extra, sceneRemovedID });
			}
						
			/* This is for transform node, when i change position/rotation/scale etc etc and print world and local matrices */
			MCallbackId transFormID = MNodeMessage::addAttributeChangedCallback(node, AttributeTransformChanged, NULL, &status);
			if (status == MS::kSuccess)
			{
				MFnDagNode dag(node);
				std::string name = dag.name().asChar();
				std::string extra = "transformchanged";

				auto iterator = callbacks.find(name + extra);
				if (iterator != callbacks.end())
				{
					std::cout << "Callback erased: " << iterator->first << std::endl;
					MMessage::removeCallback(iterator->second);
					callbacks.erase(iterator);
				}
				callbacks.insert({ name + extra, transFormID });
			}
		}		
	}
	else
	{
		MFnDependencyNode dependency(node, &status);
		if (status == MStatus::kSuccess)
		{
			//std::cout << "*Has Been created* | Dependency: " << dependency.name() << std::endl;		 // NO NEED TO HAVE
		}
	}
	
}

void SendCamera(MObject node)
{
	CameraHeader* newCamera = new CameraHeader;

	MFnCamera camera(node, &status);
	if (status == MStatus::kSuccess)
	{
		MFnDagNode cameraDag(camera.parent(0), &status);
		if (status == MStatus::kSuccess)
		{
			strcpy(newCamera->name, cameraDag.name().asChar());

			if (cameraDag.name() == "persp")
			{
				newCamera->camType = PERSPECTIVE;
			}
			else
			{
				newCamera->camType = ORTHOGRAPHIC;
			}

			MPlugArray camPlugs;
			MPlug camTranslate = cameraDag.findPlug("translate");

			MDagPath path = MDagPath::getAPathTo(node);

			MTransformationMatrix cameraMatrix(cameraDag.transformationMatrix());
			MMatrix worldMatrix = path.inclusiveMatrix();

			// Send new matrix
			int index = 0;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					newCamera->transformation[index] = worldMatrix[i][j];
					index++;
				}
			}			

			newCamera->nearPlane = camera.nearClippingPlane();;
			newCamera->farPlane = camera.farClippingPlane();

			// Sneaky testy stuff for field of view
			// Seems legit
			/*double focalLength = camera.focalLength();
			double angle = camera.shutterAngle();*/

			float camWidth = 0;
			float camHeight = 0;

			if (camera.isOrtho())
			{
				camWidth = camera.orthoWidth();
				camHeight = 2 / camera.projectionMatrix()[1][1];
			}
			else
			{
				camWidth = camera.horizontalFieldOfView() * degrees;
				camHeight = camera.verticalFieldOfView() * degrees;
			}

			// d is supposed to be aperture width, couldnt find it
			/*float d = camWidth;
			float a = 2 * atan(d / (2 * focalLength));
			float fieldOfView = a * degrees;*/

			newCamera->fieldOfView = camera.horizontalFieldOfView() * degrees;
			newCamera->aspectRatio = camera.aspectRatio();

			// Find out if this camera is the active one
			MDagPath activeCamera;
			M3dView::active3dView().getCamera(activeCamera);

			MObject tempactiveCamShape(activeCamera.node());
			MFnDagNode acticeCamShape(tempactiveCamShape);
			MFnDependencyNode activeCamObj(acticeCamShape.parent(0));

			newCamera->camWidth = camWidth;
			newCamera->camHeight = camHeight;

			if (activeCamObj.name() == cameraDag.name())
			{
				newCamera->active = true;
			}
			else
			{
				newCamera->active = false;
			}

			int msgSize = sizeof(CameraHeader);
			char* camMsg = new char[msgSize];

			int camOffset = 0;

			memcpy(camMsg + camOffset, (char*)newCamera, sizeof(CameraHeader));

			SectionHeader* mainHeader = new SectionHeader;
			mainHeader->header = NEW_CAMERA;
			mainHeader->messageSize = msgSize;

			buffer->Send(camMsg, mainHeader);

			if (newCamera)
				delete newCamera;

			if(mainHeader)
				delete mainHeader;
		}
	}
}

void iterateThroughNameChanging()
{
	MItDag dagIterator(MItDag::kBreadthFirst, MFn::kDagNode, &status);
	for (; !dagIterator.isDone(); dagIterator.next())
	{
		MObject object(dagIterator.currentItem());
		MCallbackId id = MNodeMessage::addNameChangedCallback(object, nameChangedFunction, NULL, &status);
		if (status == MS::kSuccess)
		{
			MFnDagNode dag(object);
			std::string name = dag.name().asChar();
			std::string extra = "namechange";
			callbacks.insert({ name + extra, id });
		}
	}
}

void iterateThroughWholeScene()
{	
	/* All mesh nodes */
	std::cout << "- - All mesh-nodes - - \n";
	MItDag meshIterator(MItDag::kBreadthFirst, MFn::kMesh, &status);
	for (; !meshIterator.isDone(); meshIterator.next())
	{
		MObject object(meshIterator.currentItem());
				
		if (object.hasFn(MFn::kMesh))
		{
			MFnMesh tempmesh(object);
			MObject dag(tempmesh.parent(0));
						
			if (dag.hasFn(MFn::kTransform))
			{					
				/* This is for transform node, when i change position/rotation/scale etc etc and print world and local matrices */
				MCallbackId transformID = MNodeMessage::addAttributeChangedCallback(dag, AttributeTransformChanged, NULL, &status);
				if (status == MS::kSuccess)
				{
					MFnDagNode ob(dag);
					std::string name = ob.name().asChar();
					std::string extra = "transformchanged";

					auto iterator = callbacks.find(name + extra);
					if (iterator != callbacks.end())
					{
						std::cout << "Callback erased: " << iterator->first << std::endl;
						MMessage::removeCallback(iterator->second);
						callbacks.erase(iterator);
					}
					callbacks.insert({ name + extra, transformID });
				}				

				MCallbackId sceneRemovedID = MNodeMessage::addNodePreRemovalCallback(dag, NodeRemoved, NULL, &status);
				if (status == MS::kSuccess)
				{
					MFnDagNode ob(dag);
					std::string name = ob.name().asChar();
					std::string extra = "preRemoval";

					auto iterator = callbacks.find(name + extra);
					if (iterator != callbacks.end())
					{
						std::cout << "Callback erased: " << iterator->first << std::endl;
						MMessage::removeCallback(iterator->second);
						callbacks.erase(iterator);
					}
					callbacks.insert({ name + extra, sceneRemovedID });
				}
			}

			MCallbackId topoID = MPolyMessage::addPolyTopologyChangedCallback(object, topologyChanged, NULL, &status);
			if (status == MS::kSuccess)
			{
				//callbackIdArray.append(topoID);
				MFnDagNode ob(dag);
				std::string name = ob.name().asChar();
				std::string extra = "topologychanged";

				auto iterator = callbacks.find(name + extra);
				if (iterator != callbacks.end())
				{
					std::cout << "Callback erased: " << iterator->first << std::endl;
					MMessage::removeCallback(iterator->second);
					callbacks.erase(iterator);
				}
				callbacks.insert({ name + extra, topoID });
			}

			/* This is for shape, when i change vertices or something else in shapenode */
			/* Also add topology changed callback here*/
			MCallbackId attributeID = MNodeMessage::addAttributeChangedCallback(object, attributeShapeChanged, NULL, &status);
			if (status == MS::kSuccess)
			{
				MFnDagNode dag(object);
				std::string name = dag.name().asChar();
				std::string extra = "shapesChanged";

				auto iterator = callbacks.find(name + extra);
				if (iterator != callbacks.end())
				{
					std::cout << "Callback erased: " << iterator->first << std::endl;
					MMessage::removeCallback(iterator->second);
					callbacks.erase(iterator);

				}
				callbacks.insert({ name + extra, attributeID });
			}

			SendMesh(object);
		}
	}

	std::cout << std::endl;	

	MItDag cameraIterator(MItDag::kBreadthFirst, MFn::kCamera, &status);
	for (; !cameraIterator.isDone(); cameraIterator.next())
	{
		MObject object(cameraIterator.currentItem());		
		if (object.hasFn(MFn::kCamera))
		{			
			SendCamera(object);
		}
	}
}

EXPORT MStatus initializePlugin(MObject obj) 
{		
	MFnPlugin myPlugin(obj, "FGPS Producer", "1.0", "Any", &status);
	if (MFAIL(status)) 
	{
		CHECK_MSTATUS(status);
		return status;
	}  	

	/* Redirects outputs to mayas output window instead of scripting output */
	std::cout.set_rdbuf(MStreamUtils::stdOutStream().rdbuf());
	std::cerr.set_rdbuf(MStreamUtils::stdErrorStream().rdbuf());
	cout << endl << "* * * * * Plugin for maya is loaded * * * * *" << endl;

	buffer = new CircularBuffer(L"Filemap", 150, Producer);
	
	/* iterate through whole maya scene and check name changing */
	iterateThroughWholeScene();
	iterateThroughNameChanging();

	MCallbackId panel1ID = MUiMessage::add3dViewPreRenderMsgCallback("modelPanel1", cameraMoved, NULL, &status);
	if (status == MS::kSuccess)
		callbacks.insert({ "cam1", panel1ID });
	
	MCallbackId panel2ID = MUiMessage::add3dViewPreRenderMsgCallback("modelPanel2", cameraMoved, NULL, &status);
	if (status == MS::kSuccess)
		callbacks.insert({ "cam2", panel2ID });
	
	MCallbackId panel3ID = MUiMessage::add3dViewPreRenderMsgCallback("modelPanel3", cameraMoved, NULL, &status);
	if (status == MS::kSuccess)
		callbacks.insert({ "cam3", panel3ID });
	
	MCallbackId panel4ID = MUiMessage::add3dViewPreRenderMsgCallback("modelPanel4", cameraMoved, NULL, &status);
	if (status == MS::kSuccess)
		callbacks.insert({ "cam4", panel4ID });

	/* Node added callback */
	MCallbackId nodeAddedId = MDGMessage::addNodeAddedCallback(NodeAdded, "dependNode", NULL, &status);	// dagNode, meshNode, dependNode finns också
	if (status == MS::kSuccess)
	{
		callbacks.insert({ "nodeAdded", nodeAddedId });
	}
	else
	{
		cout << "Something went wrong with node added callback" << endl;
	}	
	
	cout << "* * * * * New stuff happening under * * * * *" << endl;

	return status;
}
	
EXPORT MStatus uninitializePlugin(MObject obj) {
	MFnPlugin plugin(obj);

	cout << "Plugin unloaded =========================" << endl;
		
	std::cout << "REMOVING CALLBACKS: " << std::endl;
	for (auto i : callbacks)
	{
		std::cout << i.first << std::endl;
		MMessage::removeCallback(i.second);
	}

	delete buffer;

	gTimer.endTimer();
	gTimer.clear();

	return MS::kSuccess;
}