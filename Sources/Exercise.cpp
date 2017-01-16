#include <Kore/pch.h>

#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Audio/Mixer.h>
#include <Kore/Graphics/Image.h>
#include <Kore/Graphics/Graphics.h>
#include <Kore/Math/Quaternion.h>
#include <Kore/Threads/Thread.h>
#include <Kore/Threads/Mutex.h>
#include <Kore/Network/Socket.h>
#include <Kore/Log.h>
#include "ObjLoader.h"

#include <sstream>

// uncomment to be in control of the game
//#define MASTER

#ifdef MASTER
	#define SRC_PORT 9898
	#define DEST_PORT 9897
	#define CLIENT_NAME "Master"
#else
	#define SRC_PORT 9897
	#define DEST_PORT 9898
	#define CLIENT_NAME "Slave"
#endif

#define DEST_IP1 127
#define DEST_IP2 0
#define DEST_IP3 0
#define DEST_IP4 1

using namespace Kore;

class MeshObject {
public:
	MeshObject(const char* meshFile, const char* textureFile, const VertexStructure& structure, float scale = 1.0f) {
		mesh = loadObj(meshFile);
		image = new Texture(textureFile, true);
		
		minx = miny = minz = 9999999;
		maxx = maxy = maxz = -9999999;
		
		vertexBuffer = new VertexBuffer(mesh->numVertices, structure, 0);
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
		
		indexBuffer = new IndexBuffer(mesh->numFaces * 3);
		int* indices = indexBuffer->lock();
		for (int i = 0; i < mesh->numFaces * 3; i++) {
			indices[i] = mesh->indices[i];
		}
		indexBuffer->unlock();
		
		M = mat4::Identity();
	}

	void render(TextureUnit tex) {
		Graphics::setTexture(tex, image);
		Graphics::setVertexBuffer(*vertexBuffer);
		Graphics::setIndexBuffer(*indexBuffer);
		Graphics::drawIndexedVertices();
	}

	virtual void update(float tdif) {
		(void)tdif;	// Do nothing
	}

	mat4 M;
protected:
	float minx, miny, minz;
	float maxx, maxy, maxz;
private:
	VertexBuffer* vertexBuffer;
	IndexBuffer* indexBuffer;
	Mesh* mesh;
	Texture* image;
};

class Ball : public MeshObject {
public:
	Ball(float x, float y, float z, const VertexStructure& structure, float scale = 1.0f) : MeshObject("ball.obj", "unshaded.png", structure, scale), x(x), y(y), z(z), dir(0, 0, 0) {
		rotation = Quaternion(vec3(0, 0, 1), 0);
	}

	void update(float tdif) override {
		vec3 dir = this->dir;
		if (dir.getLength() != 0) dir.setLength(dir.getLength() * tdif * 60.0f);
		x += dir.x();
		if (x > 1) {
			x = 1;
		}
		if (x < -1) {
			x = -1;
		}
		y += dir.y();
		if (y < 0) {
			y = 4;
		}
		if (y > 4) {
			y = 0;
		}
		z += dir.z();
		if (dir.getLength() != 0) {
			float Horizontal = dir.dot(vec3(1, 0, 0));
			float Vertical = dir.dot(vec3(0, 1, 0));

			rotation = rotation.rotated(Quaternion(vec3(-1, 0, 0), Vertical * 3.0f));
			rotation = rotation.rotated(Quaternion(vec3(0, 1, 0), Horizontal * 3.0f));
		}
		M = mat4::Translation(x, y, z) * rotation.matrix();
	}
	
	vec3 dir;
	Quaternion rotation;
	float x, y, z;
};

namespace {
	void updateBall();

	const int width = 1024;
	const int height = 768;

	double startTime;
	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;
	MeshObject* objects[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	Ball* balls[] = { nullptr, nullptr, nullptr };
	TextureUnit tex;
	ConstantLocation pvLocation;
	ConstantLocation mLocation;
	mat4 PV;
	float lastTime = 0;

	Socket socket;
	vec3 position(0, 2, -3);
	
	const int port = SRC_PORT;
	const int destPort = DEST_PORT;
	const char* destination = "localhost";
	
	
	// Send a packet to the other client
	// If you are sending strings, make sure to null-terminate them, e.g. "hello\0"
	// length is the length of the packet in bytes
	void sendPacket(const unsigned char data[], int length) {
		socket.send(destination, destPort, data, length);
	}
	
	// Movement data for clients
	bool left = false, right = false, up = false, down = false;
	bool left2 = false, right2 = false, up2 = false, down2 = false;
	
	void update() {
		// receive packets
		while (true) {
			// Read buffer
			unsigned char buffer[256];
			float *floatBuffer = (float*) buffer;
			unsigned fromAddress;
			unsigned fromPort;
			int read = socket.receive(buffer, sizeof(buffer), fromAddress, fromPort);
			std::ostringstream ss;
			ss << buffer;
			
			// break if there was no new packet
			if (read <= 0) {
				break;
			}
			
			/************************************************************************/
			/* Practical Task: Read the packets with the movement data you sent  and 
			/* apply them by setting the boolean values for movement control. 
			/************************************************************************/
			#ifdef MASTER
				// Set the values for left2, right2, up2, down2 here
			#else
			// Set the values for left, right, up, down here

			// receive position updates of the npc ball
			if ((ss.str() == "x")) {
				read = socket.receive(buffer, sizeof(buffer), fromAddress, fromPort);
				balls[2]->x = floatBuffer[0];
			}
			if ((ss.str() == "y")) {
				read = socket.receive(buffer, sizeof(buffer), fromAddress, fromPort);
				balls[2]->y = floatBuffer[0];
			}
			if ((ss.str() == "z")) {
				read = socket.receive(buffer, sizeof(buffer), fromAddress, fromPort);
				balls[2]->z = floatBuffer[0];
			}
			#endif // MASTER
			
			updateBall();
		}
		
		float t = (float)(System::time() - startTime);
		float tdif = t - lastTime;
		lastTime = t;
		
		updateBall();
		
		Kore::Audio::update();
		
		Graphics::begin();
		Graphics::clear(Graphics::ClearColorFlag | Graphics::ClearDepthFlag, 0xff9999FF, 1.0f);
		
		program->set();
		
		PV = mat4::Perspective(60, (float)width / (float)height, 0.1f, 100) * mat4::lookAt(position, vec3(position.x(), position.y(), position.z() + 10.0f), vec3(0, 1, 0));
		Graphics::setMatrix(pvLocation, PV);
		
		MeshObject** current = &objects[0];
		while (*current != nullptr) {
			(*current)->update(tdif);
			Graphics::setMatrix(mLocation, (*current)->M);
			(*current)->render(tex);
			++current;
		}
		
		#ifdef MASTER
			// send position of the npc ball
			unsigned char floatData[255];
			float *f_buf = (float*)floatData;
			
			const unsigned char data1[] = "x\0";
			const unsigned char data2[] = "y\0";
			const unsigned char data3[] = "z\0";
			
			sendPacket(data1, sizeof(unsigned char) * 2);
			f_buf[0] = balls[2]->x;
			sendPacket(floatData, sizeof(float));
			sendPacket(data2, sizeof(unsigned char)* 2);
			f_buf[0] = balls[2]->y;
			sendPacket(floatData, sizeof(float));
			sendPacket(data3, sizeof(unsigned char)* 2);
			f_buf[0] = balls[2]->z;
			sendPacket(floatData, sizeof(float));
		#endif
		
		Graphics::end();
		Graphics::swapBuffers();
	}

	void updateBall() {
		// user controlled balls
		float speed = 0.05f;
		if (left) {
			balls[0]->dir.x() = -speed;
		}
		else if (right) {
			balls[0]->dir.x() = speed;
		}
		else {
			balls[0]->dir.x() = 0;
		}
		if (up) {
			balls[0]->dir.y() = speed;
		}
		else if (down) {
			balls[0]->dir.y() = -speed;
		}
		else {
			balls[0]->dir.y() = 0;
		}
		if (left2) {
			balls[1]->dir.x() = -speed;
		}
		else if (right2) {
			balls[1]->dir.x() = speed;
		}
		else {
			balls[1]->dir.x() = 0;
		}
		if (up2) {
			balls[1]->dir.y() = speed;
		}
		else if (down2) {
			balls[1]->dir.y() = -speed;
		}
		else {
			balls[1]->dir.y() = 0;
		}
		// NPC ball
		balls[2]->dir.y() = -0.02f;
		#ifdef MASTER
			if (balls[2]->y == 4) {
				balls[2]->x = ((float)rand() / RAND_MAX)*2-1;
			}
		#endif
	}

	/************************************************************************/
	/* Practical Task: Send packets with information about the input controls
	/* of the local player - keyDown
	/************************************************************************/
	void keyDown(KeyCode code, wchar_t character) {
		#ifdef MASTER
		if (code == Key_Left) {
			left = true;
		}
		else if (code == Key_Right) {
			right = true;
		}
		else if (code == Key_Up) {
			up = true;
		}
		else if (code == Key_Down) {
			down = true;
		}
		#else
		if (code == Key_A) {
			left2 = true;
		}
		else if (code == Key_D) {
			right2 = true;
		}
		if (code == Key_W) {
			up2 = true;
		}
		else if (code == Key_S) {
			down2 = true;
		}
		#endif // MASTER
	}
	
	/************************************************************************/
	/* Practical Task: Send packets with information about the input controls
	/* of the local player - keyUp
	/************************************************************************/
	void keyUp(KeyCode code, wchar_t character) {
		#ifdef MASTER
		if (code == Key_Left) {
			left = false;
		}
		else if (code == Key_Right) {
			right = false;
		}
		else if (code == Key_Up) {
			up = false;
		}
		else if (code == Key_Down) {
			down = false;
		}
		#else
		if (code == Key_A) {
			left2 = false;
		}
		else if (code == Key_D) {
			right2 = false;
		}
		else if (code == Key_W) {
			up2 = false;
		}
		else if (code == Key_S) {
			down2 = false;
		}
		#endif // MASTER
	}
	
	void init() {
		srand(42);
		socket.init();
		socket.open(port);
		
		// send "hello" when joining to tell other player you are there
		const unsigned char data[] = "hello\0";
		sendPacket(data, sizeof(unsigned char)* 6);
		
		#ifdef MASTER
			log(Info, "Waiting for another player (the SLAVE) to join my game...");
		#else
			log(Info, "Waiting for another player (the MASTER) in control of the game...");
		#endif // MASTER
		
		// wait for other player
		while (true) {
			// read buffer
			unsigned char buffer[256];
			unsigned fromAddress;
			unsigned fromPort;
			int read = socket.receive(buffer, sizeof(buffer), fromAddress, fromPort);
			std::ostringstream ss;
			ss << buffer;
			// break if player is there
			if (ss.str() == "hello\0") {
				break;
			}
		}
		
		#ifdef MASTER
			log(Info, "Another player (the SLAVE) has joined my game!");
		#else
			log(Info, "I have joined another players (the MASTER) game!");
		#endif // MASTER
		
		// resend hello for newly connected player
		sendPacket(data, sizeof(unsigned char)* 6);
		
		FileReader vs("shader.vert");
		FileReader fs("shader.frag");
		vertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
		fragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);
		
		// This defines the structure of your Vertex Buffer
		VertexStructure structure;
		structure.add("pos", Float3VertexData);
		structure.add("tex", Float2VertexData);
		structure.add("nor", Float3VertexData);
		
		program = new Program;
		program->setVertexShader(vertexShader);
		program->setFragmentShader(fragmentShader);
		program->link(structure);
		
		tex = program->getTextureUnit("tex");
		pvLocation = program->getConstantLocation("PV");
		mLocation = program->getConstantLocation("M");
		
		objects[0] = balls[0] = new Ball(0.5f, 1.0f, 0.0f, structure, 3.0f);
		objects[1] = balls[1] = new Ball(-0.5f, 1.0f, 0.0f, structure, 3.0f);
		
		objects[2] = balls[2] = new Ball(((float)rand() / RAND_MAX)*2-1, 4.0f, 0.0f, structure, 3.0f);
		objects[3] = new MeshObject("base.obj", "floor.png", structure);
		objects[3]->M = mat4::RotationX(3.1415f / 2.0f)*mat4::Scale(0.15f, 1, 1);
		objects[4] = new MeshObject("base.obj", "StarMap.png", structure);
		objects[4]->M = mat4::RotationX(3.1415f / 2.0f)*mat4::Scale(1, 1, 1)*mat4::Translation(0, 0, 0.5f);
		
		Graphics::setRenderState(DepthTest, true);
		Graphics::setRenderState(DepthTestCompare, ZCompareLess);
		
		Graphics::setTextureAddressing(tex, U, Repeat);
		Graphics::setTextureAddressing(tex, V, Repeat);
	}
}

int kore(int argc, char** argv) {
	#ifdef MASTER
		log(Info, "I am the MASTER, I am in control of the game.");
	#else
		log(Info, "I am the SLAVE, I want to join another game.");
	#endif // MASTER

	log(Info, "I am listening on port %i", port);
	log(Info, "and want to connect to %s:%i\n", destination, destPort);
	
	Kore::System::init("Exercise 12 - "  CLIENT_NAME, width, height);

	init();
	
	Kore::System::setCallback(update);
	
	startTime = System::time();
	Kore::Mixer::init();
	Kore::Audio::init();
	
	Keyboard::the()->KeyDown = keyDown;
	Keyboard::the()->KeyUp = keyUp;
	
	Kore::System::start();
	
	return 0;
}
