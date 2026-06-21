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

#include "Cloth.h"

// window setting
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 1200;

// camera
Camera camera(glm::vec3(0.0f, -4.5f, 20.0f));

// mouse callback
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

// time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// shader
glm::vec3 lightColor = glm::vec3(0.5f, 0.5f, 1.0f);
Shader* lightingShader;

// cloth
Shader* clothShader;
Cloth* cloth;
glm::vec3 lightDir = glm::vec3(-0.4f, -0.7f, -0.6f); // directional light

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
void processMovement(GLFWwindow* window);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

void DrawSphere(glm::mat4 model);
void DrawCylinder(glm::mat4 model);
void DrawUnitSphere();
void RenderImGui();

void myDisplay()
{
	glClearColor(0.08f, 0.10f, 0.16f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	cloth->simulate();

	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
		(float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = camera.GetViewMatrix();

	clothShader->use();
	clothShader->setMat4("projection", projection);
	clothShader->setMat4("view", view);
	clothShader->setMat4("model", glm::mat4(1.0f));
	clothShader->setVec3("viewPos", camera.Position);
	clothShader->setVec3("lightDir", lightDir);
	clothShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
	clothShader->setVec3("objectColorFront", glm::vec3(0.15f, 0.45f, 0.85f));
	clothShader->setVec3("objectColorBack", glm::vec3(0.85f, 0.55f, 0.20f));
	clothShader->setFloat("shininess", 32.0f);
	cloth->draw();

	// sphere collider
	if (cloth->colliderEnabled)
	{
		glm::mat4 m = glm::translate(glm::mat4(1.0f), cloth->colliderCenter);
		m = glm::scale(m, glm::vec3(cloth->colliderRadius));
		clothShader->setMat4("model", m);
		clothShader->setVec3("objectColorFront", glm::vec3(0.7f, 0.7f, 0.7f));
		clothShader->setVec3("objectColorBack", glm::vec3(0.7f, 0.7f, 0.7f));
		DrawUnitSphere();
	}

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
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		processMovement(window);
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

	* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "spring damper mesh", NULL, NULL);
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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(*window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void setupShader()
{
	lightingShader = new Shader("light_casters.vs", "light_casters.fs");
	lightingShader->use();
	lightingShader->setVec3("lightColor", lightColor);

	clothShader = new Shader("cloth.vs", "cloth.fs");
}

void destroyShader()
{
	delete lightingShader;
	delete clothShader;
}

void processInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_C && action == GLFW_PRESS) {
		useCursor = !useCursor;
		if (useCursor == true)
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		else
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

void processMovement(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
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
		RightButtonDown = true;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
		RightButtonDown = false;
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
	virtual ~Primitive() {
		if (ebo) glDeleteBuffers(1, &ebo);
		if (vbo) glDeleteBuffers(1, &vbo);
		if (VAO) glDeleteVertexArrays(1, &VAO);
	}
	virtual void Draw() {
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
	void Draw() override {
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
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
	cloth = new Cloth(60, 6.0f);
}

void destroyGLPrimitives()
{
	delete unitSphere;
	delete unitCylinder;
	delete cloth;
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

void DrawUnitSphere()
{
	unitSphere->Draw();
}

void RenderImGui() {
	ImGui::Begin("Cloth");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Separator();

	ImGui::Checkbox("wind", &cloth->windEnabled);
	ImGui::SliderFloat("wind strength", &cloth->windStrength, 0.0f, 40.0f);
	ImGui::SliderFloat("stiffness k", &cloth->k, 10.0f, 300.0f);
	ImGui::SliderFloat("gravity", &cloth->gravity, 0.0f, 30.0f);

	const char* pins[] = { "top corners", "top row", "none (free fall)" };
	if (ImGui::Combo("pin mode", &cloth->pinMode, pins, 3))
		cloth->reset();
	if (ImGui::Button("reset"))
		cloth->reset();

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
			float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

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
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
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

	const float PI = (float)3.14159265359;
	float sectorStep = 2 * PI / NumSegs;
	float sectorAngle;

	for (int i = 0; i <= NumSegs; ++i)
	{
		sectorAngle = i * sectorStep;
		float xPos = std::sin(sectorAngle);
		float yPos = 0;
		float zPos = std::cos(sectorAngle);

		base.push_back(glm::vec3(xPos, yPos, zPos));
	}

	// side
	for (int i = 0; i < 2; ++i)
	{
		float h = -height / 2.0f + i * height;

		for (int j = 0; j <= NumSegs; ++j)
		{
			positions.push_back(glm::vec3(base[j].x * radius[i], h, base[j].z * radius[i]));
			normals.push_back(glm::vec3(base[j].x, 0, base[j].z));
		}
	}

	int baseCenterIndex = (int)positions.size();
	int topCenterIndex = baseCenterIndex + NumSegs + 1;

	// base and top caps
	for (int i = 0; i < 2; ++i)
	{
		float h = -height / 2.0f + i * height;
		float ny = (float)-1 + i * 2;

		positions.push_back(glm::vec3(0, h, 0));
		normals.push_back(glm::vec3(0, ny, 0));

		for (int j = 0; j < NumSegs; ++j)
		{
			positions.push_back(glm::vec3(base[j].x * radius[i], h, base[j].z * radius[i]));
			normals.push_back(glm::vec3(0, ny, 0));
		}
	}

	int k1 = 0;
	int k2 = NumSegs + 1;

	// side indices
	for (int i = 0; i < NumSegs; ++i, ++k1, ++k2)
	{
		indices.push_back(k1);
		indices.push_back(k1 + 1);
		indices.push_back(k2);

		indices.push_back(k2);
		indices.push_back(k1 + 1);
		indices.push_back(k2 + 1);
	}

	// base cap indices
	for (int i = 0, k = baseCenterIndex + 1; i < NumSegs; ++i, ++k)
	{
		if (i < NumSegs - 1)
		{
			indices.push_back(baseCenterIndex);
			indices.push_back(k + 1);
			indices.push_back(k);
		}
		else
		{
			indices.push_back(baseCenterIndex);
			indices.push_back(baseCenterIndex + 1);
			indices.push_back(k);
		}
	}

	// top cap indices
	for (int i = 0, k = topCenterIndex + 1; i < NumSegs; ++i, ++k)
	{
		if (i < NumSegs - 1)
		{
			indices.push_back(topCenterIndex);
			indices.push_back(k);
			indices.push_back(k + 1);
		}
		else
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
