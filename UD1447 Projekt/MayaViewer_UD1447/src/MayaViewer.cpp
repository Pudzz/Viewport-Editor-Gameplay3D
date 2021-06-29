#include "MayaViewer.h"

// Declare our game instance
MayaViewer game;

constexpr int gModelCount = 10;
static bool gKeys[256] = {};
int gDeltaX;
int gDeltaY;
bool gMousePressed;

MayaViewer::MayaViewer()
    : _scene(NULL), _wireframe(false)
{
}

MayaViewer::~MayaViewer()
{
	
	if (sharedBuffer)
		delete sharedBuffer;
	
}

void MayaViewer::initialize()
{
	sharedBuffer = new CircularBuffer(L"Filemap", 150, Consumer);

	// Load game scene from file
	_scene = Scene::create();

	/* Light to the scene */
	Node* ourLightNode = _scene->addNode("ourLight");
	Light* ourLight = Light::createPoint(Vector3(0.5f, 0.5f, 0.5f), 20);
	ourLightNode->setLight(ourLight);
	SAFE_RELEASE(ourLight);
	ourLightNode->translate(Vector3(0, 1, 5));

	char* test = nullptr;
	SectionHeader* mainHeader = new SectionHeader;

	while (sharedBuffer->Recieve(test, mainHeader))
	{
		if (mainHeader->header == NEW_MESH)
		{
			MeshHeader* mesh = new MeshHeader;
			memcpy(mesh, test, sizeof(MeshHeader));

			if (_scene->findNode(mesh->name))
			{
				Node* temp = _scene->findNode(mesh->name);
				if (!temp->getDrawable())
				{
					std::vector<Vertex> vertices;

					Vertex* vert = new Vertex;
					int offset = sizeof(MeshHeader);
					for (int i = 0; i < mesh->nrOfVertices; i++)
					{
						memcpy((char*)vert, test + offset, sizeof(Vertex));

						offset += sizeof(Vertex);

						vertices.push_back(*vert);
					}

					std::vector<TextureHeader> text;
					for (int i = 0; i < mesh->material.nrOfTextures; i++)
					{
						TextureHeader texData;
						memcpy(&texData, test + offset, sizeof(TextureHeader));
						text.push_back(texData);
						offset += sizeof(TextureHeader);
					}

					/* Create our mesh */
					Mesh* ourMesh = CreateModel(vertices);
					vertexBuffers.insert({ std::string(mesh->name), vertices });

					Material* ourMat;
					Texture::Sampler* ourSampler;
					Model* ourModel = Model::create(ourMesh);

					/* Set material */
					SetMaterial(mesh, text, ourModel, ourMat, ourSampler);
					
					/* Set position on init */
					Matrix finalMat = mesh->transformation;
					Vector3* translate = new Vector3;
					Vector3* scale = new Vector3;
					Quaternion* rotation = new Quaternion;
					finalMat.decompose(scale, rotation, translate);
					Matrix* rotationMatrix = new Matrix;
					Matrix::createRotation(*rotation, rotationMatrix);

					temp->setTranslation(*translate);
					temp->setScale(*scale);
					temp->setRotation(*rotationMatrix);
					
					temp->setDrawable(ourModel);
					SAFE_RELEASE(ourModel);
				}

			}
			else
			{
				std::vector<Vertex> vertices;

				Vertex* vert = new Vertex;
				int offset = sizeof(MeshHeader);
				for (int i = 0; i < mesh->nrOfVertices; i++)
				{
					memcpy((char*)vert, test + offset, sizeof(Vertex));

					offset += sizeof(Vertex);

					vertices.push_back(*vert);
				}

				std::vector<TextureHeader> text;
				for (int i = 0; i < mesh->material.nrOfTextures; i++)
				{
					TextureHeader texData;
					memcpy(&texData, test + offset, sizeof(TextureHeader));
					text.push_back(texData);
					offset += sizeof(TextureHeader);
				}

				/* Create our mesh */
				Mesh* ourMesh = CreateModel(vertices);
				vertexBuffers.insert({ std::string(mesh->name), vertices });

				Material* ourMat = nullptr;
				Texture::Sampler* ourSampler = nullptr;
				Model* ourModel = Model::create(ourMesh);

				/* Set material */
				SetMaterial(mesh, text, ourModel, ourMat, ourSampler);
				
				Node* ourNode = _scene->addNode(mesh->name);

				/* Set position on init */
				Matrix finalMat = mesh->transformation;
				Vector3* translate = new Vector3;
				Vector3* scale = new Vector3;
				Quaternion* rotation = new Quaternion;
				finalMat.decompose(scale, rotation, translate);
				Matrix* rotationMatrix = new Matrix;
				Matrix::createRotation(*rotation, rotationMatrix);

				ourNode->setTranslation(*translate);
				ourNode->setScale(*scale);
				ourNode->setRotation(*rotationMatrix);
				
				ourNode->setDrawable(ourModel);
				SAFE_RELEASE(ourModel);
			}
		}
		else if (mainHeader->header == NEW_CAMERA)
		{
			int camOffset = 0;
			Camera* ourCamera;

			CameraHeader* header = new CameraHeader;
			memcpy(header, test + camOffset, sizeof(CameraHeader));
			camOffset += sizeof(CameraHeader);

			if (header->active)
			{
				if (header->camType == PERSPECTIVE)
				{
					ourCamera = Camera::createPerspective(header->fieldOfView, header->aspectRatio, header->nearPlane, header->farPlane);
				}
				else
				{
					ourCamera = Camera::createOrthographic(header->camWidth, header->camHeight, header->aspectRatio, header->nearPlane, header->farPlane);
				}

				Node* ourCameraNode = _scene->addNode(header->name);

				Matrix finalMat = header->transformation;
				Vector3* translate = new Vector3;
				Vector3* scale = new Vector3;
				Quaternion* rotation = new Quaternion;
				finalMat.decompose(scale, rotation, translate);

				Matrix* rotationMatrix = new Matrix;
				Matrix::createRotation(*rotation, rotationMatrix);
				ourCameraNode->setTranslation(*translate);
				ourCameraNode->setScale(*scale);
				ourCameraNode->setRotation(*rotationMatrix);

				ourCameraNode->setCamera(ourCamera);
				_scene->setActiveCamera(ourCamera);

				SAFE_RELEASE(ourCamera);
			}
			else
			{
				if (header->camType == PERSPECTIVE)
				{
					ourCamera = Camera::createPerspective(header->fieldOfView, header->aspectRatio, header->nearPlane, header->farPlane);
				}
				else
				{
					ourCamera = Camera::createOrthographic(header->camWidth, header->camHeight, header->aspectRatio, header->nearPlane, header->farPlane);
				}
				Node* ourCameraNode = _scene->addNode(header->name);

				Matrix finalMat = header->transformation;
				Vector3* translate = new Vector3;
				Vector3* scale = new Vector3;
				Quaternion* rotation = new Quaternion;
				finalMat.decompose(scale, rotation, translate);

				Matrix* rotationMatrix = new Matrix;
				Matrix::createRotation(*rotation, rotationMatrix);
				ourCameraNode->setTranslation(*translate);
				ourCameraNode->setScale(*scale);
				ourCameraNode->setRotation(*rotationMatrix);

				ourCameraNode->setCamera(ourCamera);

				SAFE_RELEASE(ourCamera);
			}
		}
	}

}

void MayaViewer::finalize()
{
    SAFE_RELEASE(_scene);
}

void MayaViewer::update(float elapsedTime)
{
	static float totalTime = 0;
	totalTime += elapsedTime;
	float step = 360.0 / float(gModelCount);
	char name[10] = {};
	

	while (sharedBuffer->Recieve(msg, mainHeader))
	{
		if (mainHeader->header == NEW_MESH)
		{
			Node* ourLightNode = _scene->findNode("ourLight");
			MeshHeader* mesh = new MeshHeader;
			memcpy(mesh, msg, sizeof(MeshHeader));

			if (_scene->findNode(mesh->name))
			{
				Node* temp = _scene->findNode(mesh->name);
				if (!temp->getDrawable())
				{
					std::vector<Vertex> vertices;

					Vertex* vert = new Vertex;
					int offset = sizeof(MeshHeader);
					for (int i = 0; i < mesh->nrOfVertices; i++)
					{
						memcpy((char*)vert, msg + offset, sizeof(Vertex));

						offset += sizeof(Vertex);

						vertices.push_back(*vert);
					}

					std::vector<TextureHeader> text;
					for (int i = 0; i < mesh->material.nrOfTextures; i++)
					{
						TextureHeader texData;
						memcpy(&texData, msg + offset, sizeof(TextureHeader));
						text.push_back(texData);
						offset += sizeof(TextureHeader);
					}

					/* Create our mesh */
					Mesh* ourMesh = CreateModel(vertices);
					vertexBuffers.insert({ std::string(mesh->name), vertices });
					
					Material* ourMat = nullptr;
					Texture::Sampler* ourSampler = nullptr;
					Model* ourModel = Model::create(ourMesh);

					/* Set material */
					SetMaterial(mesh, text, ourModel, ourMat, ourSampler);
					
					/* Set position on init */
					Matrix finalMat = mesh->transformation;
					Vector3* translate = new Vector3;
					Vector3* scale = new Vector3;
					Quaternion* rotation = new Quaternion;
					finalMat.decompose(scale, rotation, translate);
					Matrix* rotationMatrix = new Matrix;
					Matrix::createRotation(*rotation, rotationMatrix);

					temp->setTranslation(*translate);
					temp->setScale(*scale);
					temp->setRotation(*rotationMatrix);
					
					temp->setDrawable(ourModel);
					SAFE_RELEASE(ourModel);
				}
			}
			else
			{
				std::vector<Vertex> vertices;

				Vertex* vert = new Vertex;
				int offset = sizeof(MeshHeader);
				for (int i = 0; i < mesh->nrOfVertices; i++)
				{
					memcpy((char*)vert, msg + offset, sizeof(Vertex));

					offset += sizeof(Vertex);

					vertices.push_back(*vert);
				}

				std::vector<TextureHeader> text;
				for (int i = 0; i < mesh->material.nrOfTextures; i++)
				{
					TextureHeader texData;
					memcpy(&texData, msg + offset, sizeof(TextureHeader));
					text.push_back(texData);
					offset += sizeof(TextureHeader);
				}

				/* Create our mesh */
				Mesh* ourMesh = CreateModel(vertices);
				vertexBuffers.insert({ std::string(mesh->name), vertices });

				Material* ourMat = nullptr;
				Texture::Sampler* ourSampler = nullptr;
				Model* ourModel = Model::create(ourMesh);

				/* Set material */
				SetMaterial(mesh, text, ourModel, ourMat, ourSampler);
				
				Node* ourNode = _scene->addNode(mesh->name);

				/* Set position on init */
				Matrix finalMat = mesh->transformation;
				Vector3* translate = new Vector3;
				Vector3* scale = new Vector3;
				Quaternion* rotation = new Quaternion;
				finalMat.decompose(scale, rotation, translate);
				Matrix* rotationMatrix = new Matrix;
				Matrix::createRotation(*rotation, rotationMatrix);

				ourNode->setTranslation(*translate);
				ourNode->setScale(*scale);
				ourNode->setRotation(*rotationMatrix);
												
				ourNode->setDrawable(ourModel);
				SAFE_RELEASE(ourModel);
			}
		}
		else if (mainHeader->header == CHANGED_CAMERA)
		{			
			CameraTransformation* camera = new CameraTransformation;
			int offset = 0;
			memcpy(camera, msg + offset, sizeof(CameraTransformation));

			Node* cameraNode = _scene->findNode(camera->camName);
			Camera* cam = cameraNode->getCamera();

			if (cam == _scene->getActiveCamera())
			{
				if (cam->getCameraType() == 2)
				{
					cam->setZoomX(camera->camWidth);
					cam->setZoomY(camera->camHeight);
					cam->setAspectRatio(camera->aspectRatio);

				}
				else
				{
					cam->setAspectRatio(camera->aspectRatio);
					cam->setFieldOfView(camera->fieldOfView);
				}

				Matrix finalMat = camera->transformation;
				Vector3* translate = new Vector3;
				Vector3* scale = new Vector3;
				Quaternion* rotation = new Quaternion;
				finalMat.decompose(scale, rotation, translate);

				Matrix* rotationMatrix = new Matrix;
				Matrix::createRotation(*rotation, rotationMatrix);
				cameraNode->setTranslation(*translate);
				cameraNode->setScale(*scale);
				cameraNode->setRotation(*rotationMatrix);
			}
			else
			{
				_scene->setActiveCamera(cam);

				if (cam->getCameraType() == 2)
				{
					cam->setZoomX(camera->camWidth);
					cam->setZoomY(camera->camHeight);
					cam->setAspectRatio(camera->aspectRatio);
				}
				else
				{
					cam->setAspectRatio(camera->aspectRatio);
					cam->setFieldOfView(camera->fieldOfView);
				}

				Matrix finalMat = camera->transformation;
				Vector3* translate = new Vector3;
				Vector3* scale = new Vector3;
				Quaternion* rotation = new Quaternion;
				finalMat.decompose(scale, rotation, translate);

				Matrix* rotationMatrix = new Matrix;
				Matrix::createRotation(*rotation, rotationMatrix);
				cameraNode->setTranslation(*translate);
				cameraNode->setScale(*scale);
				cameraNode->setRotation(*rotationMatrix);
			}
		}
		else if (mainHeader->header == CHANGED_TRANSFORMATION)
		{
			MeshTransformation* mesh = new MeshTransformation;
			int offset = 0;
			memcpy(mesh, msg + offset, sizeof(MeshTransformation));

			Node* meshnode = _scene->findNode(mesh->meshName);

			if (meshnode)
			{
				Matrix finalMat = mesh->transformation;
				Vector3* translate = new Vector3;
				Vector3* scale = new Vector3;
				Quaternion* rotation = new Quaternion;
				finalMat.decompose(scale, rotation, translate);

				Matrix* rotationMatrix = new Matrix;
				Matrix::createRotation(*rotation, rotationMatrix);
				meshnode->setTranslation(*translate);
				meshnode->setScale(*scale);
				meshnode->setRotation(*rotationMatrix);
			}
		}
		else if (mainHeader->header == CHANGED_NAME)
		{
			NameChanged* newName = new NameChanged;
			int offset = 0;
			memcpy(newName, msg + offset, sizeof(NameChanged));

			_scene->findNode(newName->oldName)->setId(newName->newName);

			auto iterator = vertexBuffers.find(std::string(newName->oldName));

			std::vector<Vertex> vertices;
			vertices.resize(iterator->second.size());
			vertices = iterator->second;
			vertexBuffers.erase(iterator);

			vertexBuffers.insert({ newName->newName, vertices });
		}
		else if (mainHeader->header == DELETE_MESH)
		{
			DeleteMesh* deleted = new DeleteMesh;
			memcpy(deleted, msg, sizeof(DeleteMesh));

			_scene->findNode(deleted->meshName)->setDrawable(nullptr);

			auto iterator = vertexBuffers.find(std::string(deleted->meshName));
			if (iterator != vertexBuffers.end())
				vertexBuffers.erase(iterator);
		}
		else if (mainHeader->header == CHANGED_MESH)
		{
			std::vector<Vertex> vertices;

			TopologyChanged* topChanged = new TopologyChanged;
			int offset = 0;
			memcpy(topChanged, msg + offset, sizeof(TopologyChanged));
			offset += sizeof(TopologyChanged);

			for (int i = 0; i < topChanged->vertexCount; i++)
			{
				Vertex* temp = new Vertex;
				memcpy(temp, msg + offset, sizeof(Vertex));
				offset += sizeof(Vertex);
				vertices.push_back(*temp);
			}

			Node* tempNode = _scene->findNode(topChanged->name);

			if (tempNode)
			{
				Model* tempModel = dynamic_cast<Model*>(tempNode->getDrawable());

				if (topChanged->wholeMesh)
				{
					Mesh* newMesh = CreateModel(vertices);

					auto iterator = vertexBuffers.find(std::string(topChanged->name));
					if (iterator != vertexBuffers.end())
					{
						vertexBuffers.erase(iterator);
						vertexBuffers.insert({ std::string(topChanged->name), vertices });
					}


					Model* ourModel = Model::create(newMesh);

					ourModel->setMaterial(tempModel->getMaterial());
					tempNode->setDrawable(nullptr);
					tempNode->setDrawable(ourModel);
				}
				else
				{
					auto iterator = vertexBuffers.find(std::string(topChanged->name));
					if (iterator != vertexBuffers.end())
					{
						vertexBuffers.erase(iterator);
						vertexBuffers.insert({ std::string(topChanged->name), vertices });
						
						Mesh* tempMesh = tempModel->getMesh();

						unsigned int vertexCount = vertices.size();
						int amount = vertexCount * 8;
						float* vertexBuffer = new float[amount];

						std::vector<Vertex> buffer;
						auto it = vertexBuffers.find(std::string(topChanged->name));
						buffer.resize(it->second.size());
						buffer = it->second;

						int index = 0;
						for (int i = 0; i < vertexCount; i++)
						{
							vertexBuffer[index] = buffer[i].pos.x;
							vertexBuffer[index + 1] = buffer[i].pos.y;
							vertexBuffer[index + 2] = buffer[i].pos.z;

							vertexBuffer[index + 3] = buffer[i].uv.x;
							vertexBuffer[index + 4] = buffer[i].uv.y;

							vertexBuffer[index + 5] = buffer[i].normal.x;
							vertexBuffer[index + 6] = buffer[i].normal.y;
							vertexBuffer[index + 7] = buffer[i].normal.z;
							index += 8;
						}

						
						glBindBuffer(GL_ARRAY_BUFFER, tempMesh->getVertexBuffer());
						glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * tempMesh->getVertexFormat().getVertexSize(), (void*)vertexBuffer);
						
					}
				}
			}
		}
		else if (mainHeader->header == CHANGED_MATERIAL)
		{
			ChangedMaterial* topChanged = new ChangedMaterial;
			int offset = 0;
			memcpy(topChanged, msg + offset, sizeof(ChangedMaterial));
			offset += sizeof(ChangedMaterial);
						
			Node* getNode = _scene->findNode(topChanged->meshName);
			if (getNode)
			{
				Model* currentModel = dynamic_cast<Model*>(getNode->getDrawable());
				Material* modelMaterial = currentModel->getMaterial();

				std::vector<TextureHeader> text;
				for (int i = 0; i < topChanged->newmat.nrOfTextures; i++)
				{
					TextureHeader texData;
					memcpy(&texData, msg + offset, sizeof(TextureHeader));
					text.push_back(texData);
					offset += sizeof(TextureHeader);
				}

				if (topChanged->newmat.nrOfTextures > 0)
				{
					modelMaterial = currentModel->setMaterial("res/shaders/textured.vert", "res/shaders/textured.frag", "POINT_LIGHT_COUNT 1");

					modelMaterial->getParameter("u_diffuseColor")->setValue(Vector4(topChanged->newmat.diffuse.x, topChanged->newmat.diffuse.y, topChanged->newmat.diffuse.z, topChanged->newmat.diffuse.w));
					modelMaterial->getParameter("u_ambientColor")->setValue(Vector3(topChanged->newmat.ambient.x, topChanged->newmat.ambient.y, topChanged->newmat.ambient.z));

				}
				else
				{
					modelMaterial = currentModel->setMaterial("res/shaders/colored.vert", "res/shaders/colored.frag", "POINT_LIGHT_COUNT 1");

					modelMaterial->getParameter("u_diffuseColor")->setValue(Vector4(topChanged->newmat.diffuse.x, topChanged->newmat.diffuse.y, topChanged->newmat.diffuse.z, topChanged->newmat.diffuse.w));
					modelMaterial->getParameter("u_ambientColor")->setValue(Vector3(topChanged->newmat.ambient.x, topChanged->newmat.ambient.y, topChanged->newmat.ambient.z));
				}

				modelMaterial->setParameterAutoBinding("u_worldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX");
				modelMaterial->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX");

				Node* ourLightNode = _scene->findNode("ourLight");				

				modelMaterial->getParameter("u_pointLightColor[0]")->setValue(ourLightNode->getLight()->getColor());
				modelMaterial->getParameter("u_pointLightPosition[0]")->bindValue(ourLightNode, &Node::getTranslationWorld);
				modelMaterial->getParameter("u_pointLightRangeInverse[0]")->bindValue(ourLightNode->getLight(), &Light::getRangeInverse);

				Texture::Sampler* newSampler;
				if (topChanged->newmat.hasTexture && !topChanged->newmat.hasNormal)
				{
					newSampler = modelMaterial->getParameter("u_diffuseTexture")->setValue(text[0].filePath, true);
					newSampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
				}
				if (topChanged->newmat.hasNormal && !topChanged->newmat.hasTexture)
				{
					newSampler = modelMaterial->getParameter("u_normalmapTexture")->setValue(text[0].filePath, true);
					newSampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
				}
				if (topChanged->newmat.hasTexture && topChanged->newmat.hasNormal)
				{
					newSampler = modelMaterial->getParameter("u_diffuseTexture")->setValue(text[0].filePath, true);
					newSampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);

					newSampler = modelMaterial->getParameter("u_normalmapTexture")->setValue(text[1].filePath, true);
					newSampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
				}

				modelMaterial->getStateBlock()->setCullFace(true);
				modelMaterial->getStateBlock()->setDepthTest(true);
				modelMaterial->getStateBlock()->setDepthWrite(true);
			}			
		}
		else if (mainHeader->header == CHANGED_TEXTURE)
		{
			ChangedTexture* changedTexture = new ChangedTexture;
			int offset = 0;
			memcpy(changedTexture, msg + offset, sizeof(ChangedTexture));
			offset += sizeof(ChangedTexture);

			Node* getNode = _scene->findNode(changedTexture->meshName);

			if (getNode)
			{
				Model* currentModel = dynamic_cast<Model*>(getNode->getDrawable());
				Material* modelMaterial = currentModel->getMaterial();

				Texture::Sampler* newSampler;
				
				newSampler = modelMaterial->getParameter("u_diffuseTexture")->setValue(changedTexture->filePath, true);
				newSampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);		
			}
		}
	}

	Node* camnode = _scene->getActiveCamera()->getNode();

	if (camnode)
	{
		if (gKeys[Keyboard::KEY_W])
			camnode->translateForward(0.5);
		if (gKeys[Keyboard::KEY_S])
			camnode->translateForward(-0.5);
		if (gKeys[Keyboard::KEY_A])
			camnode->translateLeft(0.5);
		if (gKeys[Keyboard::KEY_D])
			camnode->translateLeft(-0.5);
	}

	if (gMousePressed) {
		camnode->rotate(camnode->getRightVectorWorld(), MATH_DEG_TO_RAD(gDeltaY / 10.0));
		camnode->rotate(camnode->getUpVectorWorld(), MATH_DEG_TO_RAD(gDeltaX / 5.0));
	}	
}

void MayaViewer::render(float elapsedTime)
{
    // Clear the color and depth buffers
    clear(CLEAR_COLOR_DEPTH, Vector4(0.1f,0.0f,0.0f,1.0f), 1.0f, 0);

    // Visit all the nodes in the scene for drawing
    _scene->visit(this, &MayaViewer::drawScene);
}

bool MayaViewer::drawScene(Node* node)
{
    // If the node visited contains a drawable object, draw it
    Drawable* drawable = node->getDrawable(); 
    if (drawable)
        drawable->draw(_wireframe);

    return true;
}

void MayaViewer::keyEvent(Keyboard::KeyEvent evt, int key)
{
    if (evt == Keyboard::KEY_PRESS)
    {
		gKeys[key] = true;
        switch (key)
        {
        case Keyboard::KEY_ESCAPE:
            exit();
            break;
		};
    }
	else if (evt == Keyboard::KEY_RELEASE)
	{
		gKeys[key] = false;
	}
}

bool MayaViewer::mouseEvent(Mouse::MouseEvent evt, int x, int y, int wheelDelta)
{
	static int lastX = 0;
	static int lastY = 0;
	gDeltaX = lastX - x;
	gDeltaY = lastY - y;
	lastX = x;
	lastY = y;
	gMousePressed =
		(evt == Mouse::MouseEvent::MOUSE_PRESS_LEFT_BUTTON) ? true :
		(evt == Mouse::MouseEvent::MOUSE_RELEASE_LEFT_BUTTON) ? false : gMousePressed;

	return true;
}

void MayaViewer::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex)
{
    switch (evt)
    {
    case Touch::TOUCH_PRESS:
        _wireframe = !_wireframe;
        break;
    case Touch::TOUCH_RELEASE:
        break;
    case Touch::TOUCH_MOVE:
        break;
    };
}

Mesh* MayaViewer::CreateModel(std::vector<Vertex> vertices)
{
	unsigned int vertexCount =  vertices.size();
	//unsigned int indexCount =  indices.size();
	int amount = vertexCount * 8;
	float* vertexBuffer = new float[amount];

	int index = 0;
	for (int i = 0; i < vertexCount; i++)
	{		
		vertexBuffer[index] = vertices[i].pos.x;
		vertexBuffer[index + 1] = vertices[i].pos.y;
		vertexBuffer[index + 2] = vertices[i].pos.z;

		vertexBuffer[index + 3] = vertices[i].uv.x;
		vertexBuffer[index + 4] = vertices[i].uv.y;

		vertexBuffer[index + 5] = vertices[i].normal.x;
		vertexBuffer[index + 6] = vertices[i].normal.y;
		vertexBuffer[index + 7] = vertices[i].normal.z;
		index += 8;			
	}
	
	VertexFormat::Element elements[] =
	{
		VertexFormat::Element(VertexFormat::POSITION, 3),
		VertexFormat::Element(VertexFormat::TEXCOORD0, 2),
		VertexFormat::Element(VertexFormat::NORMAL, 3)
	};

	Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 3), vertexCount, true);
	if (mesh == NULL) {
		GP_ERROR("Failed to create stupid mesh.");
		return NULL;
	}

	mesh->setVertexData(vertexBuffer, 0, vertexCount);
	
	/*MeshPart* meshPart = mesh->addPart(Mesh::TRIANGLE_STRIP, Mesh::INDEX16, indexCount, false);
	meshPart->setIndexData(arr, 0, indexCount);*/

	return mesh;
}

void MayaViewer::SetMaterial(MeshHeader* meshheader, std::vector<TextureHeader> textures, Model* model, Material* material, Texture::Sampler* sampler)
{
	if (meshheader->material.nrOfTextures > 0)
	{
		material = model->setMaterial("res/shaders/textured.vert", "res/shaders/textured.frag", "POINT_LIGHT_COUNT 1");

		material->getParameter("u_diffuseColor")->setValue(Vector4(meshheader->material.diffuse.x, meshheader->material.diffuse.y, meshheader->material.diffuse.z, meshheader->material.diffuse.w));
		material->getParameter("u_ambientColor")->setValue(Vector3(meshheader->material.ambient.x, meshheader->material.ambient.y, meshheader->material.ambient.z));
	}
	else
	{
		material = model->setMaterial("res/shaders/colored.vert", "res/shaders/colored.frag", "POINT_LIGHT_COUNT 1");

		material->getParameter("u_diffuseColor")->setValue(Vector4(meshheader->material.diffuse.x, meshheader->material.diffuse.y, meshheader->material.diffuse.z, meshheader->material.diffuse.w));
		material->getParameter("u_ambientColor")->setValue(Vector3(meshheader->material.ambient.x, meshheader->material.ambient.y, meshheader->material.ambient.z));
	}

	material->setParameterAutoBinding("u_worldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX");
	material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX");

	Node* ourLightNode = _scene->findNode("ourLight");
	material->getParameter("u_pointLightColor[0]")->setValue(/* lightNode */ourLightNode->getLight()->getColor());
	material->getParameter("u_pointLightPosition[0]")->bindValue(/* lightNode */ ourLightNode, &Node::getTranslationWorld);
	material->getParameter("u_pointLightRangeInverse[0]")->bindValue(/* lightNode */ ourLightNode->getLight(), &Light::getRangeInverse);

	if (meshheader->material.hasTexture && !meshheader->material.hasNormal)
	{
		sampler = material->getParameter("u_diffuseTexture")->setValue(textures[0].filePath, true);
		sampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
	}
	if (meshheader->material.hasNormal && !meshheader->material.hasTexture)
	{
		sampler = material->getParameter("u_normalmapTexture")->setValue(textures[0].filePath, true);
		sampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
	}
	if (meshheader->material.hasTexture && meshheader->material.hasNormal)
	{
		sampler = material->getParameter("u_diffuseTexture")->setValue(textures[0].filePath, true);
		sampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);

		sampler = material->getParameter("u_normalmapTexture")->setValue(textures[1].filePath, true);
		sampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
	}

	material->getStateBlock()->setCullFace(true);
	material->getStateBlock()->setDepthTest(true);
	material->getStateBlock()->setDepthWrite(true);
}

Mesh* MayaViewer::createCubeMesh(float size)
{
	float a = size * 0.5f;
	float vertices[] =
	{
		-a, -a,  a,    0.0,  0.0,  1.0,   0.0, 0.0,
		a, -a,  a,    0.0,  0.0,  1.0,   1.0, 0.0,
		-a,  a,  a,    0.0,  0.0,  1.0,   0.0, 1.0,
		a,  a,  a,    0.0,  0.0,  1.0,   1.0, 1.0,
		-a,  a,  a,    0.0,  1.0,  0.0,   0.0, 0.0,
		a,  a,  a,    0.0,  1.0,  0.0,   1.0, 0.0,
		-a,  a, -a,    0.0,  1.0,  0.0,   0.0, 1.0,
		a,  a, -a,    0.0,  1.0,  0.0,   1.0, 1.0,
		-a,  a, -a,    0.0,  0.0, -1.0,   0.0, 0.0,
		a,  a, -a,    0.0,  0.0, -1.0,   1.0, 0.0,
		-a, -a, -a,    0.0,  0.0, -1.0,   0.0, 1.0,
		a, -a, -a,    0.0,  0.0, -1.0,   1.0, 1.0,
		-a, -a, -a,    0.0, -1.0,  0.0,   0.0, 0.0,
		a, -a, -a,    0.0, -1.0,  0.0,   1.0, 0.0,
		-a, -a,  a,    0.0, -1.0,  0.0,   0.0, 1.0,
		a, -a,  a,    0.0, -1.0,  0.0,   1.0, 1.0,
		a, -a,  a,    1.0,  0.0,  0.0,   0.0, 0.0,
		a, -a, -a,    1.0,  0.0,  0.0,   1.0, 0.0,
		a,  a,  a,    1.0,  0.0,  0.0,   0.0, 1.0,
		a,  a, -a,    1.0,  0.0,  0.0,   1.0, 1.0,
		-a, -a, -a,   -1.0,  0.0,  0.0,   0.0, 0.0,
		-a, -a,  a,   -1.0,  0.0,  0.0,   1.0, 0.0,
		-a,  a, -a,   -1.0,  0.0,  0.0,   0.0, 1.0,
		-a,  a,  a,   -1.0,  0.0,  0.0,   1.0, 1.0
	};
	short indices[] =
	{
		0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9, 10, 10, 9, 11, 12, 13, 14, 14, 13, 15, 16, 17, 18, 18, 17, 19, 20, 21, 22, 22, 21, 23
	};
	unsigned int vertexCount = 24;
	unsigned int indexCount = 36;
	VertexFormat::Element elements[] =
	{
		VertexFormat::Element(VertexFormat::POSITION, 3),
		VertexFormat::Element(VertexFormat::NORMAL, 3),
		VertexFormat::Element(VertexFormat::TEXCOORD0, 2)
	};
	Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 3), vertexCount, false);
	if (mesh == NULL)
	{
		GP_ERROR("Failed to create mesh.");
		return NULL;
	}
	mesh->setVertexData(vertices, 0, vertexCount);
	MeshPart* meshPart = mesh->addPart(Mesh::TRIANGLES, Mesh::INDEX16, indexCount, false);
	meshPart->setIndexData(indices, 0, indexCount);
	return mesh;
}
