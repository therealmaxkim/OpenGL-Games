#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <random>       //used to generate random number
#include <math.h>       //used to find PI value

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

//import the shader program
#include "ShaderProgram.h"
//import the matrix class
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
//load an image using STB_image
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//global variables
SDL_Window* displayWindow;
ShaderProgram textured_program;
ShaderProgram untextured_program;
GLuint playerTexture;
GLuint enemyTexture;
GLuint ballTexture;
//GLuint lineTexture;


float vertices[] = {-0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5};
float texCoords[] = {0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0};
float lastFrameTicks = 0.0f;

const float SCREEN_WIDTH = 1280.0;
const float SCREEN_HEIGHT = 720.0;

//end of global variables

//random number generator
std::random_device rd; // obtain a random number from hardware
std::mt19937 eng(rd()); // seed the generator
std::uniform_int_distribution<> distr(0, 360); // define the range

//PI value
const float PI = atan(1.0)*4;

class Entity {
public:
    void Draw(ShaderProgram &p) {
        //order matters. translate then scale entities
        glm::mat4 newMatrix = glm::mat4(1.0f);
        newMatrix = glm::translate(newMatrix, glm::vec3(x, y, 1.0f));
        newMatrix = glm::scale(newMatrix, glm::vec3(x_scale, y_scale, 1.0f));
        p.SetModelMatrix(newMatrix);
    }
    float x;
    float y;
    float x_scale;
    float y_scale;
    float rotation;
    
    int textureID;
    
    float width;
    float height;
    
    float velocity;
    float direction_x;
    float direction_y;
};

//global entity objects for player paddle, enemy paddle, and ball
Entity playerPaddle;
Entity enemyPaddle;
Entity ball;

//function to load textures
GLuint LoadTexture(const char *filePath) {
    int w,h,comp;
    unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
    if(image == NULL) {
        std::cout << "Unable to load image. Make sure the path is correct\n";
        assert(false);
    }
    GLuint retTexture;
    glGenTextures(1, &retTexture);
    glBindTexture(GL_TEXTURE_2D, retTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(image);
    
    return retTexture;
}

void Setup() {
    // setup SDL
    // setup OpenGL
    // Set our projection matrix
    SDL_Init(SDL_INIT_VIDEO);
    displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    SDL_GL_MakeCurrent(displayWindow, context);
    
#ifdef _WINDOWS
    glewInit();
#endif
    
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    //load texture and untexture shaders
    textured_program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
    untextured_program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
    
    //setup projection matrix (based on aspect ratio of screen)
    glm::mat4 projectionMatrix = glm::mat4(1.0f);
    float aspectRatio = SCREEN_WIDTH/SCREEN_HEIGHT;
    float projectionHeight = 1.0f;
    float projectionWidth = 1.0f * aspectRatio;
    float projectionDepth = 1.0f;
    projectionMatrix = glm::ortho(-projectionWidth, projectionWidth, -projectionHeight, projectionHeight, -projectionDepth, projectionDepth);
    
    //setup view matrix
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    
    //use set the view and projection matrix to shader
    glUseProgram(textured_program.programID);
    textured_program.SetProjectionMatrix(projectionMatrix);
    textured_program.SetViewMatrix(viewMatrix);
    
    //enable blending
    glEnable(GL_BLEND);
    //set alpha blend function
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    //set attributes for shaders
    glVertexAttribPointer(textured_program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glVertexAttribPointer(textured_program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
    
    //setup Matrix for player's paddle (Right side)
    //set width and height of player's paddle. Initial height can be hardcoded to 1
    playerPaddle.height = 1;
    playerPaddle.width = 1;
    playerPaddle.x = 1.5;
    playerPaddle.y = 0.0;
    playerPaddle.x_scale = 0.1;
    playerPaddle.y_scale = 0.4;
    
    //setup Matrix for enemy's paddle (Left side)
    //set width and height of enemy's paddle. Initial height can be hardcoded to 1
    enemyPaddle.height = 1;
    enemyPaddle.width = 1;
    enemyPaddle.x = -1.5;
    enemyPaddle.y = 0.0;
    enemyPaddle.x_scale = 0.1;
    enemyPaddle.y_scale = 0.4;
    
    //setup Matrix for ball (middle)
    //set initial width and height of ball. Initial height can be hardcoded to 1
    ball.height = 1;
    ball.width = 1;
    ball.x = 0.0;
    ball.y = 0.0;
    ball.x_scale = 0.1;
    ball.y_scale = 0.1;
    //setup initial random x and y angles and velocity for the ball
    int randomAngle = distr(eng);
    ball.direction_x = cos(randomAngle*PI/180);
    ball.direction_y = sin(randomAngle*PI/180);
    ball.velocity = 1.5f;
    
    //setup Matrix for middle separating line
    //modelMatrixLine = glm::scale(modelMatrixLine, glm::vec3(1.0f, 2.0f, 1.0f));
    
    //load the textures for all entities
    playerTexture = LoadTexture(RESOURCE_FOLDER"player.png");
    enemyTexture = LoadTexture(RESOURCE_FOLDER"enemy.png");
    ballTexture = LoadTexture(RESOURCE_FOLDER"ball.png");
    //lineTexture = LoadTexture(RESOURCE_FOLDER"line.png");
    
}

bool ProcessEvents() {
    // our SDL event loop
    // check input events
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

void checkBallPaddleCollision(Entity& paddle, Entity& ball) {
    //Use box box collision method. check that x direction distance < 0
    if ((abs(paddle.x-ball.x) - ((paddle.width * paddle.x_scale + ball.width * ball.x_scale)/2)) < 0) {
        //check that y direction distance < 0
        if ((abs(paddle.y-ball.y) - ((paddle.height * paddle.y_scale + ball.height * ball.y_scale)/2)) < 0) {
            //reverse both directions
            ball.direction_x = -ball.direction_x;
        }
    }
}

void checkBallBoundaryCollision(Entity& ball, float orthoHeight) {
    //Multiply height by scale to equalize scales
    float ballHalfY = (ball.height*ball.y_scale)/2;
    //Higher boundary
    if (ball.y + ballHalfY > orthoHeight) {
        ball.direction_y = -ball.direction_y;
    //Lower boundary
    } else if (ball.y - ballHalfY < -orthoHeight) {
        ball.direction_y = -ball.direction_y;
    }
}

void checkPaddleBoundaryCollision(Entity& paddle, float orthoHeight) {
    //Multiply height by scale to equalize scales
    float paddleHalfY = (paddle.height*paddle.y_scale)/2;
    //Higher boundary.
    if (paddle.y + paddleHalfY > orthoHeight) {
        paddle.y = 1.0 - paddleHalfY;
    //Lower boundary.
    } else if (paddle.y - paddleHalfY < -orthoHeight) {
        paddle.y = -1.0 + paddleHalfY;
    }
}

void checkWinCondition(Entity ball, float orthoWidth) {
    //player is on right side, enemy is on left side
    //simply check the ball's x position with the orthographic matrix width
    if (ball.x < -orthoWidth) {
        std::cout << "Player wins!" << std::endl;
    } else if (ball.x > orthoWidth) {
        std::cout << "Enemy wins!" << std::endl;
    }
}

void Update() {
    // move stuff and check for collisions
    
    float ticks = (float)SDL_GetTicks()/1000.0f;
    float elapsed = ticks - lastFrameTicks;
    lastFrameTicks = ticks;
    float distance_to_travel_in_one_second = 1.0f;
    
    //move player paddle with keyboard arrows up/down
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if(keys[SDL_SCANCODE_UP]) {
        playerPaddle.y += elapsed * distance_to_travel_in_one_second;
    } else if(keys[SDL_SCANCODE_DOWN]) {
        playerPaddle.y -= elapsed * distance_to_travel_in_one_second;
    }
    
    //move the enemy paddle with w and s key
    if(keys[SDL_SCANCODE_W]) {
        enemyPaddle.y += elapsed * distance_to_travel_in_one_second;
    } else if (keys[SDL_SCANCODE_S]) {
        enemyPaddle.y -= elapsed * distance_to_travel_in_one_second;
    }
    
    //check if player/enemy paddle hits the top/bottom boundary
    checkPaddleBoundaryCollision(playerPaddle, 1.0);
    checkPaddleBoundaryCollision(enemyPaddle, 1.0);
    
    //check for ball collisions with top/bottom boundary
    checkBallBoundaryCollision(ball, 1.0);
    
    //move the ball.
    ball.x += elapsed * ball.velocity * ball.direction_x;
    ball.y += elapsed * ball.velocity * ball.direction_y;
    
    
    //check for ball hitting player/enemy paddle
    checkBallPaddleCollision(playerPaddle, ball);
    checkBallPaddleCollision(enemyPaddle, ball);

    //check if anyone wins
    checkWinCondition(ball, 1.777f);
    
}

void Render() {
    // for all game elements
    // setup transforms, render sprites
    
    glEnableVertexAttribArray(textured_program.positionAttribute);
    glEnableVertexAttribArray(textured_program.texCoordAttribute);
    
    //draw the player paddle
    playerPaddle.Draw(textured_program);
    glBindTexture(GL_TEXTURE_2D, playerTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    //draw the enemy paddle
    enemyPaddle.Draw(textured_program);
    glBindTexture(GL_TEXTURE_2D, enemyTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    //draw the ball
    ball.Draw(textured_program);
    glBindTexture(GL_TEXTURE_2D, ballTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glDisableVertexAttribArray(textured_program.positionAttribute);
    glDisableVertexAttribArray(textured_program.texCoordAttribute);

}

int main(int argc, char *argv[])
{
    Setup();
    bool done = false;
    while (!done) {
        done = ProcessEvents();
        glClear(GL_COLOR_BUFFER_BIT);
        Update();
        Render();
        SDL_GL_SwapWindow(displayWindow);
    }
    SDL_Quit();
    return 0;
}
