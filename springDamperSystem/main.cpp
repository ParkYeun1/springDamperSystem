#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// window setting
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 1200;

// camera
Camera camera(glm::vec3(0.0f, 1.0f, 10.0f));

// mouse callback
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

// time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// shader
glm::vec3 lightColor = glm::vec3(0.5f, 0.5f, 1.0f);
Shader* lightingShader;

// flag
bool useCursor = true;
bool firstMouse = false;
bool LeftButtonDown = false;
bool RightButtonDown = false;
bool hasTextures = false;

// house keeping
void initGL(GLFWwindow** window);
void setupShader();
void destroyShader();
void createGLPrimitives();
void destroyGLPrimitives();
void myDisplay();

// callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window, int key, int scancode, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

void DrawSphere(glm::mat4 model);
void DrawCylinder(glm::mat4 model);

//imgui
void RenderImGui();

float gravity = 10;
float mass = 30;
glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);
float timestep = 0.1f;
float anchorX = 20;
float anchorY = 20;
float k = 7;
float springForceY;
float forceY;
float accelerationY;

void myDisplay()
{
	glClearColor(0.1f, 0.1f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, -2.0f, 0.0f));
	springForceY = -k * (position[1] - anchorY);
	forceY = springForceY + mass * gravity;
	accelerationY = forceY / mass;
	velocity[1] = velocity[1] + accelerationY * timestep;
	position[1] = position[1] + velocity[1] * timestep;
	model = glm::translate(model, position*0.05f);
	DrawCylinder(model);

	//imgui
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	RenderImGui();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main() {
	
	GLFWwindow* window = NULL;
	initGL(&window);
	setupShader();
	createGLPrimitives();  
	while (!glfwWindowShouldClose(window))
	{
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		lightingShader->use();
		lightingShader->setVec3("light.position", camera.Position);
		lightingShader->setVec3("light.direction", camera.Front);
		lightingShader->setVec3("viewPos", camera.Position);
		lightingShader->setVec3("light.ambient", 0.2f, 0.2f, 0.2f);
		lightingShader->setVec3("light.diffuse", 10.0f, 10.0f, 10.0f);
		lightingShader->setVec3("light.specular", 1.0f, 1.0f, 1.0f);
		lightingShader->setFloat("light.constant", 0.1f);
		lightingShader->setFloat("light.linear", 1.0f);
		lightingShader->setFloat("light.quadratic", 0.0009f);
		lightingShader->setFloat("material.shininess", 16.0f);
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = camera.GetViewMatrix();
		lightingShader->setMat4("projection", projection);
		lightingShader->setMat4("view", view);
		myDisplay();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glfwSwapBuffers(window);
	glfwPollEvents();
	destroyGLPrimitives();
	destroyShader();
	glfwDestroyWindow(window);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
	return 0;
}

void initGL(GLFWwindow** window)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif // __APPLE__

	* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "boid", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(*window);
	glfwSetFramebufferSizeCallback(*window, framebuffer_size_callback);
	glfwSetCursorPosCallback(*window, mouse_callback);
	glfwSetMouseButtonCallback(*window, mouse_button_callback);
	glfwSetScrollCallback(*window, scroll_callback);
	glfwSetKeyCallback(*window, processInput);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		exit(-1);
	}
	glEnable(GL_DEPTH_TEST);

	//imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(*window, true);
	ImGui_ImplOpenGL3_Init((char*)glGetString(GL_NUM_SHADING_LANGUAGE_VERSIONS));
}

void setupShader()
{
	lightingShader = new Shader("light_casters.vs", "light_casters.fs");
	lightingShader->use();
	lightingShader->setVec3("lightColor", lightColor);
}

void destroyShader()
{
	delete lightingShader;
}

void processInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	float cameraSpeed = 2.5f * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S)== GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			camera.ProcessKeyboard(RIGHT, deltaTime);
	if ((glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)) {
		useCursor = !useCursor;
		if (useCursor == true)
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		else
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = (float)xpos;
		lastY = (float)ypos;
		firstMouse = false;
	}
	float xoffset = (float)(xpos - lastX) / SCR_WIDTH * 10.0f;
	float yoffset = (float)(lastY - ypos) / SCR_HEIGHT * 10.0f;

	lastX = (float)xpos;
	lastY = (float)ypos;
	if (RightButtonDown)
		camera.ProcessMouseMovement(xoffset * 200, yoffset * 200);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		LeftButtonDown = true;
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
		LeftButtonDown = false;
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		LeftButtonDown = true;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
		LeftButtonDown = false;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}

class Primitive {
public:
	Primitive() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);
	}
	~Primitive() {
		if (!ebo) glDeleteBuffers(1, &ebo);
		if (!vbo) glDeleteBuffers(1, &vbo);
		if (!VAO) glDeleteVertexArrays(1, &VAO);
	}
	void Draw() {
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLE_STRIP, IndexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
protected:
	unsigned int VAO = 0, vbo = 0, ebo = 0;
	unsigned int IndexCount = 0;
	float height = 1.0f;
	float radius[2] = { 1.0f, 1.0f };
};

class Cylinder : public Primitive {
public:
	Cylinder(float bottomRadius = 0.5f, float topRadius = 0.5f, int NumSegs = 16);
};

class Sphere : public Primitive {
public:
	Sphere(int NumSegs = 16);
};

Sphere* unitSphere;
Cylinder* unitCylinder;

void createGLPrimitives()
{
	unitSphere = new Sphere();
	unitCylinder = new Cylinder();
}

void destroyGLPrimitives()
{
	delete unitSphere;
	delete unitCylinder;
}

void DrawCylinder(glm::mat4 model)
{
	lightingShader->use();
	lightingShader->setMat4("model", model);
	lightingShader->setVec3("ObjColor", glm::vec3(1.0f, 1.0f, 0.0f));
	lightingShader->setInt("hasTextures", false);
	unitCylinder->Draw();
}

void DrawSphere(glm::mat4 model)
{
	lightingShader->use();
	lightingShader->setMat4("model", model);
	lightingShader->setVec3("ObjColor", glm::vec3(1.0f, 1.0f, 0.0f));
	lightingShader->setInt("hasTextures", false);
	unitSphere->Draw();
}

void RenderImGui() {
	ImGui::Begin("spring damper");
	ImGui::End();
}

Sphere::Sphere(int NumSegs)
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	const unsigned int X_SEGMENTS = NumSegs;
	const unsigned int Y_SEGMENTS = NumSegs;
	const float PI = (float)3.14159265359;

	for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
	{
		for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
		{
			float xSegment = (float)x / (float)X_SEGMENTS;
			float ySegment = (float)y / (float)Y_SEGMENTS;
			float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
			float yPos = std::cos(ySegment * PI);
			float zPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

			positions.push_back(glm::vec3(xPos, yPos, zPos));
			normals.push_back(glm::vec3(xPos, yPos, zPos));
		}
	}
	bool oddRow = false;
	for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
	{
		if (!oddRow)
		{
			for (unsigned int x = 0; x < X_SEGMENTS; ++x)
			{
				indices.push_back(y * (X_SEGMENTS + 1) + x);
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
			}
		}
		else
		{
			for (int x = X_SEGMENTS; x >= 0; --x)
			{
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
				indices.push_back(y * (X_SEGMENTS + 1) + x);
			}
		}
		oddRow = !oddRow;
	}
	IndexCount = (unsigned int)indices.size();
	std::vector<float> data;
	for (int i = 0; i < positions.size(); ++i)
	{
		data.push_back(positions[i].x);
		data.push_back(positions[i].y);
		data.push_back(positions[i].z);
		if (normals.size() > 0)
		{
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
		}
	}
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
	GLsizei stride = (3 + 3) * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
}

Cylinder::Cylinder(float bottomRadius, float topRadius, int NumSegs)
{
	radius[0] = bottomRadius; radius[1] = topRadius;

	std::vector<glm::vec3> base;
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	//a circle
	const float PI = (float)3.14159265359;
	float sectorStep = 2 * PI / NumSegs;				// Angle increasing
	float sectorAngle;									// radian

	for (int i = 0; i <= NumSegs; ++i)
	{
		sectorAngle = i * sectorStep;
		float xPos = std::sin(sectorAngle);
		float yPos = 0;
		float zPos = std::cos(sectorAngle);

		base.push_back(glm::vec3(xPos, yPos, zPos));
	}

	//put side of cylinder
	for (int i = 0; i < 2; ++i)
	{
		float h = -height / 2.0f + i * height;			// height from -h/2 to h/2   

		for (int j = 0; j <= NumSegs; ++j)
		{
			positions.push_back(glm::vec3(base[j].x * radius[i], h, base[j].z * radius[i]));
			normals.push_back(glm::vec3(base[j].x, h, base[j].z));
		}
	}

	//the starting index for the base/top surface
	//NOTE: it is used for generating indices later
	int baseCenterIndex = (int)positions.size();
	int topCenterIndex = baseCenterIndex + NumSegs + 1; // include center vertex

	//put base and top circles
	for (int i = 0; i < 2; ++i)
	{
		float h = -height / 2.0f + i * height;
		float ny = (float)-1 + i * 2;

		// center point
		positions.push_back(glm::vec3(0, h, 0));		// height from -h/2 to h/2
		normals.push_back(glm::vec3(0, ny, 0));			// z value of normal; -1 to 1

		for (int j = 0; j < NumSegs; ++j)
		{
			positions.push_back(glm::vec3(base[j].x * radius[i], h, base[j].z * radius[i]));
			normals.push_back(glm::vec3(0, ny, 0));
		}
	}

	//Indexing
	int k1 = 0;											// 1st vertex index at base
	int k2 = NumSegs + 1;								// 1st vertex index at top

	// indices for the side surface
	for (int i = 0; i < NumSegs; ++i, ++k1, ++k2)
	{
		// 2 triangles per sector
		// k1 => k1+1 => k2
		indices.push_back(k1);
		indices.push_back(k1 + 1);
		indices.push_back(k2);

		// k2 => k1+1 => k2+1
		indices.push_back(k2);
		indices.push_back(k1 + 1);
		indices.push_back(k2 + 1);
	}

	//indices for the base surface
	//NOTE: baseCenterIndex and topCenterIndices are pre-computed during vertex generation
	//      please see the previous code snippet
	for (int i = 0, k = baseCenterIndex + 1; i < NumSegs; ++i, ++k)
	{
		if (i < NumSegs - 1)
		{
			indices.push_back(baseCenterIndex);
			indices.push_back(k + 1);
			indices.push_back(k);
		}
		else // last triangle
		{
			indices.push_back(baseCenterIndex);
			indices.push_back(baseCenterIndex + 1);
			indices.push_back(k);
		}
	}

	// indices for the top surface
	for (int i = 0, k = topCenterIndex + 1; i < NumSegs; ++i, ++k)
	{
		if (i < NumSegs - 1)
		{
			indices.push_back(topCenterIndex);
			indices.push_back(k);
			indices.push_back(k + 1);
		}
		else // last triangle
		{
			indices.push_back(topCenterIndex);
			indices.push_back(k);
			indices.push_back(topCenterIndex + 1);
		}
	}
	IndexCount = (unsigned int)indices.size();

	std::vector<float> data;
	for (int i = 0; i < positions.size(); ++i)
	{
		data.push_back(positions[i].x);
		data.push_back(positions[i].y);
		data.push_back(positions[i].z);

		if (normals.size() > 0)
		{
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
		}
	}
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
	GLsizei stride = (3 + 3) * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
}
