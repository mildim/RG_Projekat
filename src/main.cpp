#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);
void processInput1(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadCubemap(vector<std::string> faces);

unsigned int loadTexture(char const *path);

bool lineColision(glm::vec3 p1,glm::vec3 p2,glm::vec3 px);

bool CircleColision(glm::vec3 c,float r);

void renderQuad();


// settings
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;
bool godMode = false;
bool spotlightOn = true;

bool bloom = true;
bool bloomKeyPressed = false;
float exposure = 1.0f;
// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = false;

glm::vec3 lastCamPosition;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    glm::vec3 dirLightDir = glm::vec3(-0.2f, -1.0f, -0.3f);
    glm::vec3 dirLightAmbDiffSpec = glm::vec3(0.3f, 0.3f,0.2f);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 backpackPosition = glm::vec3(0.0f);
    float backpackScale = 1.0f;
    float objectRotateAngle = 0.0f;
    float fineCalibrate = 0.0f;
    PointLight pointLight;
    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z;
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }


    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui (ovo je komandni prozor)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state (ovo je bitno da bi program znao sta je ispred a sta iza)
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // -------------------------
    Shader ourShader("resources/shaders/directionLight.vs", "resources/shaders/directionLight.fs");
    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
    Shader lightSourceShader("resources/shaders/directionLight.vs", "resources/shaders/lightSourceShader.fs");

    Shader shaderBlur("resources/shaders/blur.vs", "resources/shaders/blur.fs");
    Shader shaderBloomFinal("resources/shaders/bloom_final.vs", "resources/shaders/bloom_final.fs");
    // load models
    // --------------------------------------------------------------------------
    //rome
    Model ourModel("resources/objects/Rome/Rome.obj",true);
    ourModel.SetShaderTextureNamePrefix("material.");

    //wall
    Model ourModelWall("resources/objects/wall/concrete_wall.obj",true);
    ourModelWall.SetShaderTextureNamePrefix("material.");

    //bird
    Model ourModelBird("resources/objects/bird/rapid.obj",true);
    ourModelWall.SetShaderTextureNamePrefix("material.");

    //diamond light portal
    Model ourModelCrystal("resources/objects/diamond/diamond.obj",true);
    ourModelCrystal.SetShaderTextureNamePrefix("material.");

    //stonehenge
    Model ourModelStonehenge("resources/objects/Stonehenge/Stonehenge.obj",true);
    ourModelStonehenge.SetShaderTextureNamePrefix("material.");

    //Bloom-------------------------------------------------------------------------------------------------------------
    // configure (floating point) framebuffers
    // ---------------------------------------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffers[0], 0); //<=
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);//<=
    // check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    //Seting skybox vertices
    //---------------------------------------------------------------------------
    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    // skybox VAO
    //------------------------------------------------------------------------------------------
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // load textures for skybox
    // -------------------------------------------------------------------------------------------
    vector<std::string> faces
            {
                    FileSystem::getPath("resources/textures/skybox1/right.jpg"),
                    FileSystem::getPath("resources/textures/skybox1/left.jpg"),
                    FileSystem::getPath("resources/textures/skybox1/top.jpg"),
                    FileSystem::getPath("resources/textures/skybox1/bottom.jpg"),
                    FileSystem::getPath("resources/textures/skybox1/front.jpg"),
                    FileSystem::getPath("resources/textures/skybox1/back.jpg"),
            };
//    vector<std::string> faces
//            {
//                    FileSystem::getPath("resources/textures/skybox/right.jpg"),
//                    FileSystem::getPath("resources/textures/skybox/left.jpg"),
//                    FileSystem::getPath("resources/textures/skybox/top.jpg"),
//                    FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
//                    FileSystem::getPath("resources/textures/skybox/front.jpg"),
//                    FileSystem::getPath("resources/textures/skybox/back.jpg"),
//            };
    unsigned int cubemapTexture = loadCubemap(faces);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);


    //Point light
    //--------------------------------------------------------------------
    PointLight& pointLight = programState->pointLight;
    pointLight.position = glm::vec3(-5.6f, 5.0f, 1.7f);
    pointLight.ambient = glm::vec3(0.1, 0.1, 0.1); //==> Svetlost iz okruzenja, ravnomerna svuda
    pointLight.diffuse = glm::vec3(0.6, 0.6, 0.6); //==> Svetlost iz nekog izvora koji je blizu
    pointLight.specular = glm::vec3(1.0, 1.0, 1.0); //==> Simulira deo koji se presijava

    pointLight.constant = 0.05f;
    pointLight.linear = 0.25f;
    pointLight.quadratic = 0.032f;



    // shader configuration
    // --------------------
    ourShader.use();
    ourShader.setInt("diffuseTexture", 0);
    shaderBlur.use();
    shaderBlur.setInt("image", 0);
    shaderBloomFinal.use();
    shaderBloomFinal.setInt("scene", 0);
    shaderBloomFinal.setInt("bloomBlur", 1);
    // draw in wireframe <==Jako zanimljivo izgleda
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        float time = currentFrame;

        // input
        // -----
        if(godMode) {
            processInput(window);
        }else{
            processInput1(window);
            if(!(lineColision(glm::vec3(-4.0f,0.0f,-8.9f),glm::vec3(1.2f,0.0f,-10.4f),programState->camera.Position))
               && programState->camera.Position.y <100.0f){
                programState->camera.Position.y = 0.7f;
            }else if(programState->camera.Position.y <100.0f){
                programState->camera.Position.y = 0.2f;
            }else if(programState->camera.Position.y >100.0f){
                programState->camera.Position.y = 120.7f;
            }
        }

        // render
        // ------
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. render scene into floating point framebuffer
        // -----------------------------------------------BLOOM
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // don't forget to enable shader before setting uniforms
        ourShader.use();
        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        ourShader.setVec3("viewPosition", programState->camera.Position);
        ourShader.setFloat("material.shininess", 32.0f);
        // directional light
        ourShader.setVec3("dirLight.direction", programState->dirLightDir);
        ourShader.setVec3("dirLight.ambient", glm::vec3(programState->dirLightAmbDiffSpec.x));
        ourShader.setVec3("dirLight.diffuse", glm::vec3(programState->dirLightAmbDiffSpec.y));
        ourShader.setVec3("dirLight.specular", glm::vec3(programState->dirLightAmbDiffSpec.z));

        //pointLight.position = glm::vec3(4.0 * cos(currentFrame), 4.0f, 4.0 * sin(currentFrame));
        ourShader.setVec3("pointLights[0].position", glm::vec3(-6.3f ,0.8f, -22.4f));
        ourShader.setVec3("pointLights[0].ambient", pointLight.ambient);
        ourShader.setVec3("pointLights[0].diffuse", pointLight.diffuse);
        ourShader.setVec3("pointLights[0].specular", pointLight.specular);
        ourShader.setFloat("pointLights[0].constant", pointLight.constant);
        ourShader.setFloat("pointLights[0].linear", pointLight.linear);
        ourShader.setFloat("pointLights[0].quadratic", pointLight.quadratic);

        ourShader.setVec3("pointLights[1].position", glm::vec3(-3.0f ,120.8f, 0.35f));
        ourShader.setVec3("pointLights[1].ambient", pointLight.ambient);
        ourShader.setVec3("pointLights[1].diffuse", pointLight.diffuse);
        ourShader.setVec3("pointLights[1].specular", pointLight.specular);
        ourShader.setFloat("pointLights[1].constant", pointLight.constant);
        ourShader.setFloat("pointLights[1].linear", pointLight.linear);
        ourShader.setFloat("pointLights[1].quadratic", pointLight.quadratic);


        // spotLight
        if (spotlightOn) {
            ourShader.setVec3("spotLight.position", programState->camera.Position);
            ourShader.setVec3("spotLight.direction", programState->camera.Front);
            ourShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
            ourShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
            ourShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
            ourShader.setFloat("spotLight.constant", 1.0f);
            ourShader.setFloat("spotLight.linear", 0.09);
            ourShader.setFloat("spotLight.quadratic", 0.032);
            ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
            ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));
        }else{
            ourShader.setVec3("spotLight.diffuse", 0.0f, 0.0f, 0.0f);
            ourShader.setVec3("spotLight.specular", 0.0f, 0.0f, 0.0f);
        }




        // render the loaded models
        //---------------------------------------------------------------------------------
        //raw Rome
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model,glm::vec3 (0.0f));
        model = glm::scale(model, glm::vec3(0.06f));
        model = glm::rotate(model,glm::radians(195.0f), glm::vec3(0.0f ,1.0f, 0.0f));
        //model = glm::rotate(model,glm::radians(programState->objectRotateAngle), glm::vec3(0.0f ,1.0f, 0.0f));
        ourShader.setMat4("model", model);
        ourModel.Draw(ourShader);

        //draw walls back
        glm::mat4 modelWall = model;
        modelWall = glm::scale(modelWall, glm::vec3(3.4f));
        modelWall = glm::translate(modelWall,glm::vec3(44.0f,-3.0f,4.0f));
        for(int i =0; i<5;i++){
            modelWall = glm::translate(modelWall,glm::vec3(-20.0f,0.0f,0.0f));
            ourShader.setMat4("model", modelWall);
            ourModelWall.Draw(ourShader);
        }
        //draw walls side
        modelWall = glm::translate(modelWall,glm::vec3(2.0f,0.0f,74.01f));
        modelWall = glm::rotate(modelWall,glm::radians(-90.0f), glm::vec3(0.0f ,1.0f, 0.0f));
        for(int i =0; i<3;i++){
            modelWall = glm::translate(modelWall,glm::vec3(-20.0f,0.0f,0.0f));
            ourShader.setMat4("model", modelWall);
            ourModelWall.Draw(ourShader);
            modelWall = glm::translate(modelWall,glm::vec3(0.0f,0.0f,-91.9f));
            ourShader.setMat4("model", modelWall);
            ourModelWall.Draw(ourShader);
            modelWall = glm::translate(modelWall,glm::vec3(0.0f,0.0f,91.9f));
        }
        //draw walls side front
        modelWall = glm::rotate(modelWall,glm::radians(90.0f), glm::vec3(0.0f ,1.0f, 0.0f));
        modelWall = glm::translate(modelWall,glm::vec3(-15.0f ,0.0f, 45.0f));
        ourShader.setMat4("model", modelWall);
        ourModelWall.Draw(ourShader);


        //draw diamonds
        glm::mat4 modelDiamond = glm::mat4(1.0f);
        modelDiamond = glm::translate(modelDiamond,
                                      glm::vec3(-6.3f ,0.8f, -22.4f) +0.6f*glm::vec3(sin(-3.0f*time), sin(time)/2.0f+0.1f, cos(-3.0f*time)));
        modelDiamond = glm::scale(modelDiamond, glm::vec3(0.15f));
        ourShader.setMat4("model", modelDiamond);
        ourModelCrystal.Draw(ourShader);

        glm::mat4 modelDiamond1 = glm::mat4(1.0f);
        modelDiamond1 = glm::translate(modelDiamond1,
                                       glm::vec3(-6.3f ,0.8f, -22.4f) +0.6f*glm::vec3(sin(2.0f*time), sin(2.0f*time)/2.0f+0.1f, cos(2.0f*time)));
        modelDiamond1 = glm::scale(modelDiamond1, glm::vec3(0.15f));
        ourShader.setMat4("model", modelDiamond1);
        ourModelCrystal.Draw(ourShader);

        //diamond stonehange
        modelDiamond = glm::mat4(1.0f);
        modelDiamond = glm::translate(modelDiamond,
                                      glm::vec3(-3.0f,120.6f,0.35f) +0.6f*glm::vec3(sin(-3.0f*time), sin(time)/2.0f+0.1f, cos(-3.0f*time)));
        modelDiamond = glm::scale(modelDiamond, glm::vec3(0.15f));
        ourShader.setMat4("model", modelDiamond);
        ourModelCrystal.Draw(ourShader);

        modelDiamond1 = glm::mat4(1.0f);
        modelDiamond1 = glm::translate(modelDiamond1,
                                       glm::vec3(-3.0f,120.6f,0.35f) +0.6f*glm::vec3(sin(2.0f*time), sin(2.0f*time)/2.0f+0.1f, cos(2.0f*time)));
        modelDiamond1 = glm::scale(modelDiamond1, glm::vec3(0.15f));
        ourShader.setMat4("model", modelDiamond1);
        ourModelCrystal.Draw(ourShader);

        //draw big orbiting diamonds
        modelDiamond = glm::mat4(1.0f);
        modelDiamond = glm::translate(modelDiamond,
                                      glm::vec3(-1.5f,120.6f,1.0f) +6.6f*glm::vec3(sin(1.5f*time), sin(time)/10.0f+0.2f, cos(1.5f*time)));
        modelDiamond = glm::scale(modelDiamond, glm::vec3(1.5f));
        ourShader.setMat4("model", modelDiamond);
        ourModelCrystal.Draw(ourShader);

        modelDiamond1 = glm::mat4(1.0f);
        modelDiamond1 = glm::translate(modelDiamond1,
                                       glm::vec3(-1.5f,120.6f,1.0f) +6.6f*glm::vec3(sin(1.5f*time), sin(time)/10.0f+0.2f, cos(1.5f*time)));
        modelDiamond1 = glm::scale(modelDiamond1, glm::vec3(1.5f));
        ourShader.setMat4("model", modelDiamond1);
        ourModelCrystal.Draw(ourShader);

        //draw bird
        glm::mat4 modelBird = glm::mat4(1.0f);
        modelBird = glm::scale(modelBird, glm::vec3(0.001));
        modelBird = glm::translate(modelBird,glm::vec3(1900.0f ,1000.0f, -25900.0f));
        modelBird = glm::rotate(modelBird,glm::radians(-70.0f), glm::vec3(0.0f ,1.0f, 0.0f));
        ourShader.setMat4("model", modelBird);
        ourModelBird.Draw(ourShader);
        //draw bird2
        modelBird = glm::translate(modelBird,glm::vec3(300.0f ,0.0f, -1900.0f));
        ourShader.setMat4("model", modelBird);
        ourModelBird.Draw(ourShader);

        //draw Stonehenge
        glm::mat4 modelStonehenge = glm::mat4(1.0f);
        modelStonehenge = glm::translate(modelStonehenge,glm::vec3(0.0f ,120.0f, 0.0f));
        model = glm::rotate(model, glm::radians(programState->objectRotateAngle), glm::vec3(0.0f ,1.0f, 0.0f));
        modelStonehenge = glm::scale(modelStonehenge, glm::vec3 (0.01f));
        ourShader.setMat4("model", modelStonehenge);
        ourModelStonehenge.Draw(ourShader);

        //--------------------------LIGHT-----------------------------------

        //draw Crystal
        lightSourceShader.use();
        lightSourceShader.setMat4("projection", projection);
        lightSourceShader.setMat4("view", view);
        glm::mat4 modelCrystal = glm::mat4(1.0f);
        modelCrystal = glm::translate(modelCrystal,glm::vec3(-6.3f ,0.8f, -22.4f));
        modelCrystal = glm::scale(modelCrystal, glm::vec3(1.0f));
        modelCrystal = glm::rotate(modelCrystal, time*5.0f, glm::vec3(0.1f ,1.0f, 0.0f)); //Rotiramo oko y ose
        lightSourceShader.setMat4("model", modelCrystal);
        lightSourceShader.setVec3("lightColor", glm::vec3(1.0f,1.0f,1.0f));
        ourModelCrystal.Draw(lightSourceShader);

        modelCrystal = glm::mat4(1.0f);
        modelCrystal = glm::translate(modelCrystal,glm::vec3(-3.0f ,120.8f, 0.35f));
        modelCrystal = glm::scale(modelCrystal, glm::vec3(1.0f));
        modelCrystal = glm::rotate(modelCrystal, time*5.0f, glm::vec3(0.1f ,1.0f, 0.0f)); //Rotiramo oko y ose
        lightSourceShader.setMat4("model", modelCrystal);
        lightSourceShader.setVec3("lightColor", glm::vec3(1.0f,1.0f,1.0f));
        ourModelCrystal.Draw(lightSourceShader);



        // draw skybox as last
        //-------------------------------------------------------------------------------------------------------------------
        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default

        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        // 2. blur bright fragments with two-pass Gaussian Blur
        // --------------------------------------------------
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        shaderBlur.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            shaderBlur.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 3. now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
        // --------------------------------------------------------------------------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderBloomFinal.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        shaderBloomFinal.setInt("bloom", bloom);
        shaderBloomFinal.setFloat("exposure", exposure);
        renderQuad();

        //Phisics
        //---------------------------------------
        //teleport
        if((programState->camera.Position.x <= -6.2f && programState->camera.Position.x >= -6.4f) &&
                (programState->camera.Position.z <= -22.3f && programState->camera.Position.z >= -22.5f)){
            programState->camera.Position = glm::vec3(1.0f,121.0f,1.0f);
        }

        if((programState->camera.Position.x <= -2.9f && programState->camera.Position.x >= -3.1f) &&
           (programState->camera.Position.z <= 0.45f && programState->camera.Position.z >= 0.25f)){
            programState->camera.Position = glm::vec3(1.0f,1.0f,-4.0f);
        }



        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVAO);

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}
// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

bool lineColision(glm::vec3 p1,glm::vec3 p2,glm::vec3 px){
    float res;
    res = ((p2.z-p1.z)/(p2.x-p1.x))*(px.x - p1.x) + p1.z - px.z;
    if(res <= 0)
        return true;
    else return false;
}
bool circleColision(glm::vec3 c,glm::vec3 x,float r){
    float res;
    res = pow(x.x-c.x,2.0f)+pow(x.z-c.z,2.0f) - pow(r,2.0f);
    if(res >= 0)
        return true;
    else return false;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
}
void processInput1(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if(programState->camera.Position.y < 100.0f) {
        if (!(lineColision(glm::vec3(-6.8f, 0.0f, -0.2f), glm::vec3(10.8f, 0.0f, -5.0f), programState->camera.Position))
            && !(lineColision(glm::vec3(-6.8f, 0.0f, -0.2f), glm::vec3(-10.0f, 0.0f, -20.5f),
                              programState->camera.Position))
            && (lineColision(glm::vec3(10.8f, 0.0f, -5.0f), glm::vec3(7.8f, 0.0f, -16.5f),
                             programState->camera.Position))
            && (lineColision(glm::vec3(-3.2f, 0.0f, -23.6f), glm::vec3(7.2f, 0.0f, -26.7f),
                             programState->camera.Position))) {
            lastCamPosition = programState->camera.Position;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                programState->camera.ProcessKeyboard(FORWARD, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                programState->camera.ProcessKeyboard(LEFT, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                programState->camera.ProcessKeyboard(RIGHT, deltaTime);
        }else {
            programState->camera.Position = lastCamPosition;
        }
    } else {
            if (!circleColision(glm::vec3(-1.4f, 0.0f, 0.6f), programState->camera.Position, 5.0f)) {
                lastCamPosition = programState->camera.Position;
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                    programState->camera.ProcessKeyboard(FORWARD, deltaTime);
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                    programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                    programState->camera.ProcessKeyboard(LEFT, deltaTime);
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                    programState->camera.ProcessKeyboard(RIGHT, deltaTime);
            } else {
                programState->camera.Position = lastCamPosition;
            }

    }

}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Hello window");
        ImGui::Text("Hello text");
        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
        ImGui::DragFloat3("Object position", (float*)&programState->backpackPosition, 0.1);
        ImGui::DragFloat("Object scale", &programState->backpackScale, 0.005, 0.1, 5.0);
        ImGui::DragFloat("Object rotate", &programState->objectRotateAngle, 0.5, 0.0, 360.0);

        ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.05, 0.0, 1.0);

        ImGui::Text("DirLight settings");
        ImGui::DragFloat3("Direction light direction", (float*)&programState->dirLightDir, 0.05, -1.0, 1.0);

        ImGui::Text("Ambient    Diffuse    Specular");
        ImGui::DragFloat3("Direction light settings", (float*)&programState->dirLightAmbDiffSpec, 0.05, 0.001, 1.0);

        ImGui::Text("Fine Calibrate");
        ImGui::DragFloat("FC", &programState->fineCalibrate, 0.05);

        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            programState->CameraMouseMovementUpdateEnabled = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (key == GLFW_KEY_F && action == GLFW_PRESS){
        if(spotlightOn){
            spotlightOn = false;
        }else{
            spotlightOn = true;
        }
    }
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS){
        if(godMode){
            godMode = false;
        }else{
            godMode = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !bloomKeyPressed)
    {
        bloom = !bloom;
        bloomKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        bloomKeyPressed = false;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        if (exposure > 0.0f)
            exposure -= 0.001f;
        else
            exposure = 0.0f;
    }
    else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        exposure += 0.001f;
    }

}
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrComponents;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

unsigned int loadTexture(char const *path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}