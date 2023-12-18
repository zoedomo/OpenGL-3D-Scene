#include <iostream>
#include <cstdlib>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
    const char* const WINDOW_TITLE = "Zoe Domagalski";
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    struct GLMesh {
        GLuint vao;
        GLuint vbos[2];
        GLuint nIndices;
    };

    struct LightSource
    {
        glm::vec3 position;
        glm::vec3 color;
        GLMesh mesh;
        glm::mat4 model;
    };

    GLFWwindow* gWindow = nullptr;
    GLMesh gMeshPyramid;
    GLMesh gMeshSphere;
    GLMesh gMeshPlane;
    GLMesh gMeshTorus;
    GLMesh gMeshCube;
    GLMesh gMeshCylinder;
    GLuint gProgramId;
    GLuint spongeTexture;
    GLuint woodTexture;
    GLuint bluecontainerTexture;
    LightSource keyLight;
    LightSource fillLight;


    // Camera variables
    // could not figure out how to use camera.h from OpenGLsample code
    glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    float cameraSpeed = 2.5f; // Adjust camera movement speed here
    float cameraYaw = -90.0f;
    float cameraPitch = 0.0f;
    float sensitivity = 0.05f; // Adjust mouse sensitivity here
    bool firstMouse = true;
    float lastMouseX = WINDOW_WIDTH / 2.0f;
    float lastMouseY = WINDOW_HEIGHT / 2.0f;

    //lights and thier color
    const glm::vec3 KEY_LIGHT_COLOR = glm::vec3(1.0f, 0.6f, 0.2f); //sunset yellow
    const glm::vec3 FILL_LIGHT_COLOR = glm::vec3(0.9f, 0.9f, 0.9f);


}

void UMouseScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    cameraSpeed += yOffset * 0.1f; // Adjust the factor 0.1f to control the zoom speed.
    if (cameraSpeed < 0.1f) // Set a minimum speed.
        cameraSpeed = 0.1f;
    if (cameraSpeed > 5.0f) // Set a maximum speed.
        cameraSpeed = 5.0f;
}


bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UCreateMesh(GLMesh& mesh, const vector<GLfloat>& vertices, const vector<GLushort>& indices);
void UDestroyMesh(GLMesh& mesh);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);
void UMouseScrollCallback(GLFWwindow* window, double xOffset, double yOffset);
void UpdateCameraPosition(GLFWwindow* window, float deltaTime);
bool isPerspective = true;  // Start with the perspective view
bool pKeyLastState = false; // by default, key not pressed
void UCreateLightSource(LightSource& light, const glm::vec3& position, const glm::vec3& color);
void UDestroyLightSource(LightSource& light);


// Vertex Shader Source Code
const GLchar* vertexShaderSource = GLSL(440,
    layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 texCoord; // Added texture coordinate

out vec4 vertexColor;
out vec2 fragTexCoord; // Added fragment texture coordinate
out vec3 fragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0f);
    vertexColor = color;
    fragTexCoord = texCoord; // Pass texture coordinate to fragment shader
    fragPos = vec3(model * vec4(position, 1.0));
}
);

// Fragment Shader Source Code
const GLchar* fragmentShaderSource = GLSL(440,
in vec4 vertexColor;
in vec2 fragTexCoord;
in vec3 fragPos;

out vec4 fragmentColor;

uniform sampler2D textureSampler; // Added texture

uniform vec3 keyLightPosition;
uniform vec3 keyLightColor;
uniform vec3 fillLightPosition;
uniform vec3 fillLightColor;
uniform vec3 ambientLightColor;
uniform bool useUniformColor;
uniform vec3 uniformColor;
uniform vec3 viewPosition;
uniform float shininess;

void main()
{
    if (useUniformColor) {
        fragmentColor = vec4(uniformColor, 1.0);
        return;
    }

    vec3 normal = normalize(-vec3(0.0, 1.0, 0.0));
    vec3 viewDir = normalize(viewPosition - fragPos);

    // Key Light
    vec3 lightDir1 = normalize(keyLightPosition - fragPos);
    float diff1 = max(dot(lightDir1, normal), 0.0);
    vec3 diffuse1 = keyLightColor * diff1 * vertexColor.rgb;

    vec3 reflectDir1 = reflect(-lightDir1, normal);
    float spec1 = pow(max(dot(viewDir, reflectDir1), 0.0), shininess);
    vec3 specular1 = keyLightColor * spec1 * vertexColor.rgb;

    // Fill Light
    vec3 lightDir2 = normalize(fillLightPosition - fragPos);
    float diff2 = max(dot(lightDir2, normal), 0.0);
    vec3 diffuse2 = fillLightColor * diff2 * vertexColor.rgb;

    vec3 reflectDir2 = reflect(-lightDir2, normal);
    float spec2 = pow(max(dot(viewDir, reflectDir2), 0.0), shininess);
    vec3 specular2 = fillLightColor * spec2 * vertexColor.rgb;

    vec3 ambient = ambientLightColor * vertexColor.rgb;

    vec3 finalColor = ambient + diffuse1 + specular1 + diffuse2 + specular2;

    fragmentColor = texture(textureSampler, fragTexCoord) * vec4(finalColor, 1.0);
}
);
int main(int argc, char* argv[])
{
    if (!UInitialize(argc, argv, &gWindow))
        return EXIT_FAILURE;

    // Create the key and fill light sources
    UCreateLightSource(keyLight, glm::vec3(-5.0f, 1.5f, 1.0f), KEY_LIGHT_COLOR);
    UCreateLightSource(fillLight, glm::vec3(5.5f, -1.0f, 0.0f), FILL_LIGHT_COLOR);

    vector<GLfloat> pyramidVertices = {
        // Vertex Positions    // Colors (r,g,b,a)
       0.0f,  0.5f, 0.0f,    1.0f, 0.5f, 0.0f, 1.0f, // Top Vertex 0 (apex)
      -0.5f, -1.2f, 0.5f,    1.0f, 0.5f, 0.0f, 1.0f, // Bottom-left Vertex 1
       0.5f, -1.2f, 0.5f,    1.0f, 0.5f, 0.0f, 1.0f, // Bottom-right Vertex 2

       0.0f,  0.5f, 0.0f,    1.0f, 0.5f, 0.0f, 1.0f, // Top Vertex 3 (apex)
       0.5f, -1.2f, 0.5f,    1.0f, 0.5f, 0.0f, 1.0f, // Bottom-right Vertex 4
       0.5f, -1.2f, -0.5f,   1.0f, 0.5f, 0.0f, 1.0f, // Bottom-left Vertex 5

       0.0f,  0.5f, 0.0f,    1.0f, 0.5f, 0.0f, 1.0f, // Top Vertex 6 (apex)
       0.5f, -1.2f, -0.5f,   1.0f, 0.5f, 0.0f, 1.0f, // Bottom-right Vertex 7
      -0.5f, -1.2f, -0.5f,   1.0f, 0.5f, 0.0f, 1.0f, // Bottom-left Vertex 8

       0.0f,  0.5f, 0.0f,    1.0f, 0.5f, 0.0f, 1.0f, // Top Vertex 9 (apex)
      -0.5f, -1.2f, -0.5f,   1.0f, 0.5f, 0.0f, 1.0f, // Bottom-left Vertex 10
      -0.5f, -1.2f, 0.5f,    1.0f, 0.5f, 0.0f, 1.0f  // Bottom-right Vertex 11
    };

    vector<GLushort> pyramidIndices = {
        0, 1, 2, // Right face
        3, 4, 5, // Back face
        6, 7, 8, // Left face
        9, 10, 11, // Front face

        // Base of the pyramid (two triangles)
        1, 2, 5,
        2, 5, 8
    };

    vector<GLfloat> sphereVertices;
    vector<GLushort> sphereIndices;
    const int numStacks = 20;
    const int numSlices = 20;
    const float radius = 0.5f;

    // Create sphere vertices and indices
    for (int i = 0; i <= numStacks; ++i) {
        float phi = glm::pi<float>() * static_cast<float>(i) / numStacks;
        for (int j = 0; j <= numSlices; ++j) {
            float theta = 2 * glm::pi<float>() * static_cast<float>(j) / numSlices;

            float x = radius * sin(phi) * cos(theta);
            float y = radius * cos(phi);
            float z = radius * sin(phi) * sin(theta);

            float r = 1.0f; // Orange color
            float g = 0.5f; // Orange color
            float b = 0.0f; // Orange color

            sphereVertices.push_back(x);
            sphereVertices.push_back(y);
            sphereVertices.push_back(z);
            sphereVertices.push_back(r);
            sphereVertices.push_back(g);
            sphereVertices.push_back(b);
            sphereVertices.push_back(1.0f);
        }
    }

    //NOTE: ALL MY TEXTURE MAPPING IS INCORRECT. I CANNOT FIGURE OUT HOW TO MAP IT WITHOUT STRETCHING. 

    // Create sphere indices
    for (int i = 0; i < numStacks; ++i) {
        for (int j = 0; j < numSlices; ++j) {
            int first = i * (numSlices + 1) + j;
            int second = first + numSlices + 1;

            sphereIndices.push_back(static_cast<GLushort>(first));
            sphereIndices.push_back(static_cast<GLushort>(second));
            sphereIndices.push_back(static_cast<GLushort>(first + 1));

            sphereIndices.push_back(static_cast<GLushort>(second));
            sphereIndices.push_back(static_cast<GLushort>(second + 1));
            sphereIndices.push_back(static_cast<GLushort>(first + 1));
        }
    }

    // Plane vertices
    vector<GLfloat> planeVertices = {
        // Positions          // Colors
         5.0f,  0.0f,  5.0f,   1.0f, 1.0f, 1.0f, 1.0f,  // Top Right
         5.0f,  0.0f, -5.0f,   1.0f, 1.0f, 1.0f, 1.0f,  // Bottom Right
        -5.0f,  0.0f, -5.0f,   1.0f, 1.0f, 1.0f, 1.0f,  // Bottom Left
        -5.0f,  0.0f,  5.0f,   1.0f, 1.0f, 1.0f, 1.0f   // Top Left 
    };


    vector<GLushort> planeIndices = {
        0, 1, 3,  // First Triangle
        1, 2, 3   // Second Triangle

    };

    // Torus vertices and indices
    vector<GLfloat> torusVertices;
    vector<GLushort> torusIndices;

    const int numCirclePoints = 10;  // Number of points for the small circle
    const int numCircles = 10;       // Number of circles
    const GLfloat R = 1.0f;          // Big circle radius
    const GLfloat r = 0.25f;          // Small circle radius

    // Generate the torus
    for (int i = 0; i < numCircles; ++i) {
        GLfloat phi = 2.0f * M_PI * i / numCircles;
        for (int j = 0; j < numCirclePoints; ++j) {
            GLfloat theta = 2.0f * M_PI * j / numCirclePoints;

            GLfloat x = (R + r * cos(theta)) * cos(phi);
            GLfloat y = r * sin(theta);
            GLfloat z = (R + r * cos(theta)) * sin(phi);

            torusVertices.push_back(x);
            torusVertices.push_back(y);
            torusVertices.push_back(z);
            torusVertices.push_back(0.3f);  // Color
            torusVertices.push_back(0.3f);
            torusVertices.push_back(0.3f);
            torusVertices.push_back(1.0f);
        }
    }

    // Generate the indices for the torus
    for (int i = 0; i < numCircles; ++i) {
        for (int j = 0; j < numCirclePoints; ++j) {
            int nextCircle = (i + 1) % numCircles;
            int nextPoint = (j + 1) % numCirclePoints;

            int currentPoint = i * numCirclePoints + j;
            int adjacentPoint = i * numCirclePoints + nextPoint;
            int belowPoint = nextCircle * numCirclePoints + j;
            int diagonalPoint = nextCircle * numCirclePoints + nextPoint;

            // First triangle
            torusIndices.push_back(currentPoint);
            torusIndices.push_back(belowPoint);
            torusIndices.push_back(adjacentPoint);

            // Second triangle
            torusIndices.push_back(adjacentPoint);
            torusIndices.push_back(belowPoint);
            torusIndices.push_back(diagonalPoint);
        }
    }


    // add the cube
    GLfloat l = 2.0f; // length
    GLfloat w = 2.0f; // width
    GLfloat h = 1.0f; // height (smaller than length and width)

    vector<GLfloat> cubeVertices = {
        // Positions          // Colors
        l / 2, -h / 2,  w / 2,   0.1f, 0.1f, 0.3f, 1.0f,  // Top Right Front
        l / 2, -h / 2, -w / 2,   0.1f, 0.1f, 0.3f, 1.0f,  // Top Right Back
       -l / 2, -h / 2, -w / 2,   0.1f, 0.1f, 0.3f, 1.0f,  // Top Left Back
       -l / 2, -h / 2,  w / 2,   0.1f, 0.1f, 0.f, 1.0f,  // Top Left Front
        l / 2,  h / 2,  w / 2,   0.1f, 0.1f, 0.3f, 1.0f,  // Bottom Right Front
        l / 2,  h / 2, -w / 2,   0.1f, 0.1f, 0.3f, 1.0f,  // Bottom Right Back
       -l / 2,  h / 2, -w / 2,   0.1f, 0.1f, 0.3f, 1.0f,  // Bottom Left Back
       -l / 2,  h / 2,  w / 2,   0.1f, 0.1f, 0.3f, 1.0f   // Bottom Left Front 
    };

    vector<GLushort> cubeIndices = {
        0, 1, 5,  0, 5, 4,  // Front face
        1, 2, 6,  1, 6, 5,  // Right face
        2, 3, 7,  2, 7, 6,  // Back face
        3, 0, 4,  3, 4, 7,  // Left face
        4, 5, 6,  4, 6, 7,  // Bottom face
        3, 2, 1,  3, 1, 0   // Top face
    };

    const int cylinderSegments = 32;
    const float cylinderHeight = 1.0f;
    const float cylinderRadius = 0.5f;

    // Cylinder Vertices
    vector<GLfloat> cylinderVertices;

    // vertices for the top and bottom circles and the side
    for (int i = 0; i < cylinderSegments; i++)
    {
        float theta = (float)i / cylinderSegments * 2.0f * M_PI;
        float x = cylinderRadius * cos(theta);
        float z = cylinderRadius * sin(theta);

        // Top circle vertex
        cylinderVertices.push_back(x);                          // x
        cylinderVertices.push_back(cylinderHeight / 2);         // y
        cylinderVertices.push_back(z);                          // z
        // Color (Royal Blue)
        cylinderVertices.push_back(0.254f);
        cylinderVertices.push_back(0.412f);
        cylinderVertices.push_back(0.882f);
        cylinderVertices.push_back(1.0f);

        // Bottom circle vertex
        cylinderVertices.push_back(x);                          // x
        cylinderVertices.push_back(-cylinderHeight / 2);        // y
        cylinderVertices.push_back(z);                          // z
        // Color (Royal Blue)
        cylinderVertices.push_back(0.254f); 
        cylinderVertices.push_back(0.412f); 
        cylinderVertices.push_back(0.882f); 
        cylinderVertices.push_back(1.0f); 
    }

    // Center vertex for top circle
    cylinderVertices.push_back(0.0f);                  // x
    cylinderVertices.push_back(cylinderHeight / 2);    // y
    cylinderVertices.push_back(0.0f);                  // z
    // Color
    cylinderVertices.push_back(0.254f); 
    cylinderVertices.push_back(0.412f); 
    cylinderVertices.push_back(0.882f); 
    cylinderVertices.push_back(1.0f); 

    // Center vertex for bottom circle
    cylinderVertices.push_back(0.0f);                  // x
    cylinderVertices.push_back(-cylinderHeight / 2);   // y
    cylinderVertices.push_back(0.0f);                  // z
    // Color
    cylinderVertices.push_back(0.254f); 
    cylinderVertices.push_back(0.412f); 
    cylinderVertices.push_back(0.882f); 
    cylinderVertices.push_back(1.0f); 

    // Cylinder Indices
    std::vector<GLushort> cylinderIndices;

    // Indices for the side of the cylinder
    for (int i = 0; i < cylinderSegments; i++)
    {
        cylinderIndices.push_back(i * 2);
        cylinderIndices.push_back((i * 2) + 1);
        cylinderIndices.push_back(((i + 1) * 2) % (cylinderSegments * 2));

        cylinderIndices.push_back(((i + 1) * 2) % (cylinderSegments * 2));
        cylinderIndices.push_back((i * 2) + 1);
        cylinderIndices.push_back(((i + 1) * 2 + 1) % (cylinderSegments * 2));
    }

    // Indices for the top of the cylinder
    GLushort topCenterIndex = cylinderSegments * 2;  // Index of the top center vertex
    for (int i = 0; i < cylinderSegments; i++)
    {
        cylinderIndices.push_back(topCenterIndex);
        cylinderIndices.push_back(i * 2);
        cylinderIndices.push_back((i * 2 + 2) % (cylinderSegments * 2));
    }

    // Indices for the bottom of the cylinder
    GLushort bottomCenterIndex = topCenterIndex + 1; // Index of the bottom center vertex
    for (int i = 0; i < cylinderSegments; i++)
    {
        cylinderIndices.push_back(bottomCenterIndex);
        cylinderIndices.push_back(i * 2 + 1);
        cylinderIndices.push_back((i * 2 + 3) % (cylinderSegments * 2));
    }

    // Load the texture
    int width, height, numComponents;
    unsigned char* textureData = stbi_load("wood_texture.jpg", &width, &height, &numComponents, 0);

    // Create and bind texture object
    glGenTextures(1, &woodTexture);
    glBindTexture(GL_TEXTURE_2D, woodTexture);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create the texture
    if (textureData)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        cout << "Failed to load wood texture" << endl;
    }
    stbi_image_free(textureData);

    // Load the sponge texture
    unsigned char* spongeData = stbi_load("sponge_texture.jpg", &width, &height, &numComponents, 0);

    // Create and bind sponge texture object
    glGenTextures(1, &spongeTexture); 
    glBindTexture(GL_TEXTURE_2D, spongeTexture); 

    // Set sponge texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 

    // Create the sponge texture
    if (spongeData)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, spongeData); 
        glGenerateMipmap(GL_TEXTURE_2D); 
    }
    else
    {
        cout << "Failed to load sponge texture" << endl;
    }
    stbi_image_free(spongeData);

    // load blue container
    unsigned char* blueContainerData = stbi_load("bluecontainer_texture.jpg", &width, &height, &numComponents, 0);

    glGenTextures(1, &bluecontainerTexture);  
    glBindTexture(GL_TEXTURE_2D, bluecontainerTexture);  

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 

    if (blueContainerData)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, blueContainerData); 
        glGenerateMipmap(GL_TEXTURE_2D); 
    } 
    else
    {
        std::cout << "Failed to load blue container texture" << std::endl; 
    }
    stbi_image_free(blueContainerData); 

    // Create the pyramid mesh
    UCreateMesh(gMeshPyramid, pyramidVertices, pyramidIndices);

    // Create the sphere mesh
    UCreateMesh(gMeshSphere, sphereVertices, sphereIndices);

    // Create the plane mesh
    UCreateMesh(gMeshPlane, planeVertices, planeIndices);

    // Create the torus mesh
    UCreateMesh(gMeshTorus, torusVertices, torusIndices);

    // Create the cube mesh
    UCreateMesh(gMeshCube, cubeVertices, cubeIndices);

    // Create the cylinder mesh
    UCreateMesh(gMeshCylinder, cylinderVertices, cylinderIndices);

    if (!UCreateShaderProgram(vertexShaderSource, fragmentShaderSource, gProgramId))
        return EXIT_FAILURE;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    while (!glfwWindowShouldClose(gWindow))
    {
        UProcessInput(gWindow);
        URender();
        glfwPollEvents();
    }

    UDestroyMesh(gMeshPyramid);
    UDestroyMesh(gMeshSphere);
    UDestroyMesh(gMeshPlane);
    UDestroyMesh(gMeshTorus);
    UDestroyShaderProgram(gProgramId);

    exit(EXIT_SUCCESS);
}

bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    * window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (*window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);

    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult)
    {
        std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
        return false;
    }

    cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(*window, [](GLFWwindow* window, double xPos, double yPos)
        {
            if (firstMouse)
            {
                lastMouseX = xPos;
                lastMouseY = yPos;
                firstMouse = false;
            }

            float xOffset = xPos - lastMouseX;
            float yOffset = lastMouseY - yPos; // Reversed since y-coordinates go from bottom to top

            lastMouseX = xPos;
            lastMouseY = yPos;

            xOffset *= sensitivity;
            yOffset *= sensitivity;

            cameraYaw += xOffset;
            cameraPitch += yOffset;

            if (cameraPitch > 89.0f)
                cameraPitch = 89.0f;
            if (cameraPitch < -89.0f)
                cameraPitch = -89.0f;

            glm::vec3 front;
            front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
            front.y = sin(glm::radians(cameraPitch));
            front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
            cameraFront = glm::normalize(front);
        });

    glfwSetScrollCallback(*window, UMouseScrollCallback);

    return true;
}

void UProcessInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    bool pKeyPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS; // adding this because it was picking up "P" being pressed multiple times
    if (pKeyPressed && !pKeyLastState)  // If P is currently pressed and was not pressed last frame 
        isPerspective = !isPerspective;  // Toggle the projection mode 
    pKeyLastState = pKeyPressed; // Update the last state 
}

void UCreateLightSource(LightSource& light, const glm::vec3& position, const glm::vec3& color)
{
    light.position = position;
    light.color = color;

    float size = 1.0f;
    GLfloat verts[] = {
        // front
        -size, -size, size,
        size, -size, size,
        size, size, size,
        -size, size, size,

        // back
        size, -size, -size,
        -size, -size, -size,
        -size, size, -size,
         size, size, -size,

         // right
         size, -size, size,
         size, -size, -size,
         size, size, -size,
         size, size, size,

         // left
         -size, -size, -size,
         -size, -size, size,
         -size, size, size,
         -size, size, -size,

         // top
         -size, size, size,
         size, size, size,
         size, size, -size,
         -size, size, -size,

         // bottom
         -size, -size, -size,
         size, -size, -size,
         size, -size, size,
         -size, -size, size
    };

    GLushort indices[] = {
        0, 1, 2, 2, 3, 0, // Front
        4, 5, 6, 6, 7, 4, // Back
        8, 9, 10, 10, 11, 8, // Right
        12, 13, 14, 14, 15, 12, // Left
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20 // Bottom
    };

    glGenVertexArrays(1, &light.mesh.vao);
    glBindVertexArray(light.mesh.vao);

    glGenBuffers(2, light.mesh.vbos);
    glBindBuffer(GL_ARRAY_BUFFER, light.mesh.vbos[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    light.mesh.nIndices = sizeof(indices) / sizeof(indices[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, light.mesh.vbos[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    GLint stride = sizeof(float) * 3;

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(0);

    light.model = glm::translate(position) * glm::scale(glm::vec3(0.2f));
}

void UDestroyLightSource(LightSource& light)
{
    glDeleteVertexArrays(1, &light.mesh.vao);
    glDeleteBuffers(2, light.mesh.vbos);
}

void UResizeWindow(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void UpdateCameraPosition(GLFWwindow* window, float deltaTime)
{
    const float cameraSpeed = 2.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPosition += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPosition -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPosition -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPosition += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPosition += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPosition -= cameraSpeed * cameraUp;
}


void UCreateMesh(GLMesh& mesh, const vector<GLfloat>& vertices, const vector<GLushort>& indices)
{
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(2, mesh.vbos);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbos[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

    mesh.nIndices = static_cast<GLuint>(indices.size());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.vbos[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLushort), indices.data(), GL_STATIC_DRAW);

    GLint stride = sizeof(float) * 7;

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (char*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (char*)(sizeof(float) * 6)); // Set up texture coordinate attribute
    glEnableVertexAttribArray(2);
}

void UDestroyMesh(GLMesh& mesh)
{
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(2, mesh.vbos);
}

void URender()
{
    static double lastTime = glfwGetTime();
    double currentTime = glfwGetTime();
    float deltaTime = float(currentTime - lastTime);
    lastTime = currentTime;

    glEnable(GL_DEPTH_TEST);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    UpdateCameraPosition(gWindow, deltaTime);

    glm::mat4 scale = glm::scale(glm::vec3(2.0f, 2.0f, 2.0f));
    glm::mat4 rotation = glm::rotate(90.0f, glm::vec3(0.0, 1.0f, 0.0f));
    glm::mat4 translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
    glm::mat4 model = translation * rotation * scale;

    glm::mat4 view = glm::lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);
    //glm::mat4 projection = glm::perspective(45.0f, (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);

    glm::mat4 projection;
    if (isPerspective)
    {
        projection = glm::perspective(45.0f, (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);
    }
    else
    {
        float orthoScale = 5.0f;  // This controls how "zoomed out" your orthographic view is
        projection = glm::ortho(-orthoScale * ((GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT),
            orthoScale * ((GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT),
            -orthoScale,
            orthoScale,
            0.1f,
            100.0f);

    }

    // Update the position of the key and fill lights based on their current model matrix
    keyLight.position = glm::vec3(keyLight.model[3][0], keyLight.model[3][1], keyLight.model[3][2]);
    fillLight.position = glm::vec3(fillLight.model[3][0], fillLight.model[3][1], fillLight.model[3][2]);

    glUseProgram(gProgramId);

    GLint viewPosLoc = glGetUniformLocation(gProgramId, "viewPosition"); 
    glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPosition)); 

    GLint shininessLoc = glGetUniformLocation(gProgramId, "shininess"); 
    glUniform1f(shininessLoc, 32.0f);  // Adjust this value as desired, higher values mean smaller, sharper highlights

    // Set uniforms for using a uniform color
    GLint useUniformColorLoc = glGetUniformLocation(gProgramId, "useUniformColor"); 
    GLint uniformColorLoc = glGetUniformLocation(gProgramId, "uniformColor"); 
    glUniform1i(useUniformColorLoc, GL_FALSE);  // Perform lighting calculations  

    glm::vec3 ambientLight = glm::vec3(0.3, 0.3, 0.3);  // Soft general light
    GLint ambientLightColorLoc = glGetUniformLocation(gProgramId, "ambientLightColor");
    glUniform3fv(ambientLightColorLoc, 1, glm::value_ptr(ambientLight));


    GLint modelLoc = glGetUniformLocation(gProgramId, "model");
    GLint viewLoc = glGetUniformLocation(gProgramId, "view");
    GLint projLoc = glGetUniformLocation(gProgramId, "projection");


    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    GLint keyLightPosLoc = glGetUniformLocation(gProgramId, "keyLightPosition");
    GLint keyLightColorLoc = glGetUniformLocation(gProgramId, "keyLightColor");
    GLint fillLightPosLoc = glGetUniformLocation(gProgramId, "fillLightPosition");
    GLint fillLightColorLoc = glGetUniformLocation(gProgramId, "fillLightColor");

    // Set the uniform values for the key and fill lights' positions and colors
    glUniform3fv(glGetUniformLocation(gProgramId, "keyLightPosition"), 1, glm::value_ptr(keyLight.position));
    glUniform3fv(glGetUniformLocation(gProgramId, "keyLightColor"), 1, glm::value_ptr(keyLight.color));
    glUniform3fv(glGetUniformLocation(gProgramId, "fillLightPosition"), 1, glm::value_ptr(fillLight.position));
    glUniform3fv(glGetUniformLocation(gProgramId, "fillLightColor"), 1, glm::value_ptr(fillLight.color));

    glUniform1i(useUniformColorLoc, GL_FALSE);

    // Bind Wood texture
    glActiveTexture(GL_TEXTURE0); 
    glBindTexture(GL_TEXTURE_2D, woodTexture);

    // Render Plane
    glm::mat4 planeModel = glm::translate(glm::vec3(0.0f, -2.1f, 0.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(planeModel));
    glBindVertexArray(gMeshPlane.vao);
    glDrawElements(GL_TRIANGLES, gMeshPlane.nIndices, GL_UNSIGNED_SHORT, NULL);
    //glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
    glBindVertexArray(0);

    // unbind wood
    glBindTexture(GL_TEXTURE_2D, 0); 

    // Bind Sponge texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, spongeTexture);

    // Render Pyramid
    glm::mat4 pyramidModel = glm::translate(glm::vec3(-2.5f, 0.1f, 0.3f)) *
        glm::rotate(180.0f, glm::vec3(0.5, 1.0f, 0.0f)) *
        glm::scale(glm::vec3(1.2f, 1.2f, 1.2f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pyramidModel));
    glBindVertexArray(gMeshPyramid.vao);
    glDrawElements(GL_TRIANGLES, gMeshPyramid.nIndices, GL_UNSIGNED_SHORT, NULL);
    glBindVertexArray(0);

    // Render Sphere
    glm::mat4 sphereModel = glm::translate(glm::vec3(-3.6f, -1.1f, 1.0f)) * 
        glm::rotate(90.0f, glm::vec3(0.0, -1.2f, 1.0f)) *
        glm::scale(glm::vec3(2.0f, 2.0f, 2.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sphereModel)); 
    glBindVertexArray(gMeshSphere.vao); 
    glDrawElements(GL_TRIANGLES, gMeshSphere.nIndices, GL_UNSIGNED_SHORT, NULL); 
    glBindVertexArray(0); 

    glBindTexture(GL_TEXTURE_2D, 0); 
    // Bind Wood texture
    glActiveTexture(GL_TEXTURE0); 
    glBindTexture(GL_TEXTURE_2D, woodTexture); 

    // Render Torus
    glm::mat4 torusModel = glm::translate(glm::vec3(-0.5f, -1.8f, 4.0f)) *  
    glm::scale(glm::vec3(0.7f, 0.7f, 0.7f)); 
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(torusModel)); 
    glBindVertexArray(gMeshTorus.vao); 
    glDrawElements(GL_TRIANGLES, gMeshTorus.nIndices, GL_UNSIGNED_SHORT, NULL); 
    glBindVertexArray(0); 

    // Render Cube
    glm::mat4 prismModel = 
        glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
        glm::translate(glm::vec3(3.0f, -1.8f, 2.0f)) *
        glm::scale(glm::vec3(1.5f, 0.5f, 1.5f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(prismModel));
    glBindVertexArray(gMeshCube.vao);
    glDrawElements(GL_TRIANGLES, gMeshCube.nIndices, GL_UNSIGNED_SHORT, NULL);
    glBindVertexArray(0); 

    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bluecontainerTexture);

    // Render the cylinder 
    glm::mat4 cylinderModel = glm::translate(glm::vec3(-1.0f, -0.8f, -1.5f)) *
    glm::scale(glm::vec3(3.5f, 2.5f, 3.5f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(cylinderModel)); 
    glBindVertexArray(gMeshCylinder.vao);   
    glDrawElements(GL_TRIANGLES, gMeshCylinder.nIndices, GL_UNSIGNED_SHORT, 0);  
    glBindVertexArray(0);  

    glUniform1i(useUniformColorLoc, GL_TRUE);  // Use the uniform color FOR LIGHTS
    glm::vec3 whiteColor(1.0f, 1.0f, 1.0f);
    glUniform3fv(uniformColorLoc, 1, glm::value_ptr(whiteColor));

    // Draw the key light and fill light sources
    glBindVertexArray(keyLight.mesh.vao);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(keyLight.model));
    glDrawElements(GL_TRIANGLES, keyLight.mesh.nIndices, GL_UNSIGNED_SHORT, 0);

    glBindVertexArray(fillLight.mesh.vao);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(fillLight.model));
    glDrawElements(GL_TRIANGLES, fillLight.mesh.nIndices, GL_UNSIGNED_SHORT, 0);

    glUniform1i(useUniformColorLoc, GL_FALSE);

    glfwSwapBuffers(gWindow);
}

bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
    int success = 0;
    char infoLog[512];

    programId = glCreateProgram();
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

    glCompileShader(vertexShaderId);
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }

    glCompileShader(fragmentShaderId);
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }

    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return false;
    }

    glUseProgram(programId);
    return true;
}

void UDestroyShaderProgram(GLuint programId)
{
    glDeleteProgram(programId);
}