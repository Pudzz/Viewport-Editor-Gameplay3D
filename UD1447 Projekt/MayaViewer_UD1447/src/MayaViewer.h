#ifndef MayaViewer_H_
#define MayaViewer_H_

#include "gameplay.h"
#include "../CircularBuffers.h"

using namespace gameplay;

/**
 * Main game class.
 */
class MayaViewer: public Game
{
public:

    /**
     * Constructor.
     */
    MayaViewer();

    ~MayaViewer();

    /**
     * @see Game::keyEvent
     */
	void keyEvent(Keyboard::KeyEvent evt, int key);
	
    /**
     * @see Game::touchEvent
     */
    void touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);

	// mouse events
	bool mouseEvent(Mouse::MouseEvent evt, int x, int y, int wheelDelta);


protected:

    /**
     * @see Game::initialize
     */
    void initialize();

    /**
     * @see Game::finalize
     */
    void finalize();

    /**
     * @see Game::update
     */
    void update(float elapsedTime);

    /**
     * @see Game::render
     */
    void render(float elapsedTime);

private:

    /**
     * Draws the scene each frame.
     */
    bool drawScene(Node* node);

    Mesh* CreateModel(std::vector<Vertex> vertices);
    void SetMaterial(MeshHeader* meshheader, std::vector<TextureHeader> textures, Model* model, Material* material, Texture::Sampler* sampler);

	Mesh* createCubeMesh(float size = 1.0f);
	Material* createMaterial();

    Scene* _scene;
    bool _wireframe;

    CircularBuffer* sharedBuffer;

    char* msg;
    SectionHeader* mainHeader;    

    std::unordered_map< char*, Model*> models;
    std::unordered_map< Mesh*, float*> saveVertexbuffer;
    std::unordered_map< std::string, std::vector<Vertex>> vertexBuffers;
    int nrOfModels = 0;
};

#endif
