#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <vector>

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#include "ShaderProgram.h"  //import the shader program
#include "glm/mat4x4.hpp"   //import the matrix class
#include "glm/gtc/matrix_transform.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"      //load an image using STB_image


//************************************
//Global variables begin here
//************************************
SDL_Window* displayWindow;
ShaderProgram textured_program;
ShaderProgram untextured_program;
//float vertices[] = {-0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5};
//float texCoords[] = {0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0};
float lastFrameTicks = 0.0f;
const float SCREEN_WIDTH = 1280.0;
const float SCREEN_HEIGHT = 720.0;
//************************************
//Global variables end here
//************************************


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


//************************************
//Game class definitions begin here
//************************************
class SheetSprite {
public:
    SheetSprite() {}
    SheetSprite(unsigned int textureID, float u, float v, float width, float height, float x, float y, float x_scale, float y_scale) {
        this->textureID = textureID;
        this->u = u;
        this->v = v;
        this->width = width;
        this->height = height;
        this->x = x;
        this->y = y;
        this->x_scale = x_scale;
        this->y_scale = y_scale;
    }
    void Draw(ShaderProgram &p) {
        glm::mat4 newMatrix = glm::mat4(1.0f);
        newMatrix = glm::translate(newMatrix, glm::vec3(x, y, 1.0f));
        newMatrix = glm::scale(newMatrix, glm::vec3(x_scale, y_scale, 1.0f));
        p.SetModelMatrix(newMatrix);
        glBindTexture(GL_TEXTURE_2D, textureID);
        GLfloat texCoords[] = {
            u, v+height,
            u+width, v,
            u, v,
            u+width, v,
            u, v+height,
            u+width, v+height
        };
        float aspect = width / height;
        float vertices[] = {
            -0.5f * aspect, -0.5f,
            0.5f * aspect, 0.5f,
            -0.5f * aspect, 0.5f,
            0.5f * aspect, 0.5f,
            -0.5f * aspect, -0.5f,
            0.5f * aspect, -0.5f
        };
        glVertexAttribPointer(p.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
        glVertexAttribPointer(p.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
        glEnableVertexAttribArray(p.positionAttribute);
        glEnableVertexAttribArray(p.texCoordAttribute);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(p.positionAttribute);
        glDisableVertexAttribArray(p.texCoordAttribute);
    }
    float x_scale;
    float y_scale;
    unsigned int textureID;
    float u;
    float v;
    float width;
    float height;
    float x;
    float y;
};

class Entity {
public:
    float x_velocity;
    float y_velocity;
    SheetSprite sprite;
};

class GameState {
public:
    std::vector<Entity> playerShip;
    std::vector<Entity> enemyShips;
    std::vector<Entity> lasers;
    int score;
    bool moved_down = false;
};

enum GameMode {TITLE_SCREEN, GAME_LEVEL};
//************************************
//Game class definitions end here
//************************************


//************************************
//Custom game methods begin here
//************************************
void shoot_laser(GameState& state) {
    Entity laser;
    float u = 843.0f/1024.0f;
    float v = 602.0f/1024.0f;
    float width = 13.0f/1024.0f;
    float height = 37.0f/1024.0f;
    float x_scale = 0.05f;
    float y_scale = 0.20f;
    //X coordinate will be the player's ship
    float x = state.playerShip[0].sprite.x;
    //Y coordinate will be at the tip of the player's ship
    float y = state.playerShip[0].sprite.y + state.playerShip[0].sprite.height*2;
    laser.sprite = SheetSprite(LoadTexture(RESOURCE_FOLDER"sheet.png"), u, v, width, height, x, y, x_scale, y_scale);
    laser.y_velocity = 2.0f;
    state.lasers.push_back(laser);
}
void move_enemy_ships(std::vector<Entity>& ships, const float elapsed) {
    for (Entity& ship: ships) {
        ship.sprite.x += elapsed * ship.x_velocity;
    }
}
void move_enemy_ship_down(std::vector<Entity>& ships, const float orthoY, const float lastFrameTicks, bool& status, const int seconds) {
    if (((int)lastFrameTicks+1) % seconds == 0) {     //move down every (seconds) variable
        if (status == false) {    //use a boolean variable to prevent multiple down moves per second
            for (Entity& ship: ships) {
                ship.sprite.y += ship.y_velocity;
            }
            status = true;
        }
    } else {
        status = false;
    }
}
bool x_boundary(std::vector<Entity>& ships, const float orthoX) {
    for (Entity& ship: ships) {
        float shipHalfX = ship.sprite.width;
        //Ship collides with +x boundary. Reverse direction for all ships
        if (ship.sprite.x + shipHalfX > orthoX) {
            ship.sprite.x = orthoX - shipHalfX;
            for (Entity& ship: ships) {
                ship.x_velocity = -1 * ship.x_velocity;
            }
            return true;
        //Ship collides with -x boundary. Reverse direction for all ships.
        } else if (ship.sprite.x - shipHalfX < -orthoX) {
            ship.sprite.x = -orthoX + shipHalfX;
            for (Entity& ship: ships) {
                ship.x_velocity = -1 * ship.x_velocity;
            }
            return true;
        }
    }
    return false;
}
bool y_boundary(std::vector<Entity>& ships, const float orthoY) {
    for (Entity& ship: ships) {
        float shipHalfY = ship.sprite.width;
        //enemy ship collides with -Y boundary. Enemy wins
        if (ship.sprite.y - shipHalfY < -orthoY) {
            return true;
        }
    }
    return false;
}
bool laser_y_boundary(Entity& laser) {
    float top = laser.sprite.y + laser.sprite.height;
    float bottom = laser.sprite.y - laser.sprite.height;
    return (top >= 1.0 | bottom <= -1.0) ? true : false;
}
void ship_laser_collision(std::vector<Entity>& ships, std::vector<Entity>& lasers) {
    //Box-Box collision detection.
    for (int i = 0; i < ships.size(); i++) {
        for (int j = 0; j < lasers.size(); j++) {
            //check that x direction distance < 0
            if ((abs(ships[i].sprite.x - lasers[j].sprite.x) - ((ships[i].sprite.width*2 + lasers[j].sprite.width*2)/2)) < 0) {
                //check that y direction distance < 0
                if ((abs(ships[i].sprite.y - lasers[j].sprite.y) - ((ships[i].sprite.height*2 + lasers[j].sprite.height*2)/2)) < 0) {
                    ships.erase(ships.begin() + i);
                    lasers.erase(lasers.begin() + j);
                }
            }
        }
    }
}
void ship_ship_collision(std::vector<Entity>& ships, std::vector<Entity>& ships2) {
    //Box-Box collision detection.
    for (int i = 0; i < ships.size(); i++) {
        for (int j = 0; j < ships2.size(); j++) {
            //check that x direction distance < 0
            if ((abs(ships[i].sprite.x-ships2[j].sprite.x) - ((ships[i].sprite.width + ships2[j].sprite.width)/2)) < 0) {
                //check that y direction distance < 0
                if ((abs(ships[i].sprite.y-ships2[j].sprite.y) - ((ships[i].sprite.height + ships2[j].sprite.height)/2)) < 0) {
                    ships.erase(ships.begin() + i);
                    ships2.erase(ships2.begin() + j);
                }
            }
        }
    }
}
//************************************
//Custom game methods end here
//************************************


//************************************
//Overall gamemode Game_Level methods begin here
//************************************
//Our SDL event loop. check input events
bool Process_Game_Level_Events(GameState& state) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        //Player pressed spacebar to shoot
        } else if(event.type == SDL_KEYDOWN) {
            if(event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                //Only shoot if the player is still alive
                if (!state.playerShip.empty()) {
                    shoot_laser(state);
                }
            }
        }
    }
    return false;
}

//move stuff and check for collisions
void Update_Game_Level(GameState& state) {
    
    float ticks = (float)SDL_GetTicks()/1000.0f;
    float elapsed = ticks - lastFrameTicks;
    lastFrameTicks = ticks;
    
    //move player ship with Left/Right keyboard arrows. Player speed is hardcoded to 1.5f distance per second;
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if(keys[SDL_SCANCODE_LEFT]) {
        state.playerShip[0].sprite.x -= elapsed * 1.5f;
    } else if(keys[SDL_SCANCODE_RIGHT]) {
        state.playerShip[0].sprite.x += elapsed * 1.5f;
    }
    
    //move the enemy ships across x direction
    move_enemy_ships(state.enemyShips, elapsed);
    //move the enemy ships down every 4 seconds. Pass 4 as a variable to determine frequency.
    move_enemy_ship_down(state.enemyShips, 1.0, lastFrameTicks, state.moved_down, 4);
    
    //check if player/enemy ship collide with -X/+X boundary, using the default 1.777f orthoY value.
    x_boundary(state.playerShip, 1.777);
    x_boundary(state.enemyShips, 1.777);
    
    //check if laser collides with +Y/-Y boundary, using the default 1.0f orthoY value.
    state.lasers.erase(std::remove_if(state.lasers.begin(), state.lasers.end(), laser_y_boundary), state.lasers.end());
    
    //check if enemy ships collide with bottom boundary
    //y_boundary(state.enemyShips, 1.0);
    
    //move the lasers
    for (Entity& laser: state.lasers) {
        laser.sprite.y += elapsed * laser.y_velocity;
    }
    
    //check if lasers hit player/enemies
    ship_laser_collision(state.enemyShips, state.lasers);
    //ship_laser_collision(state.playerShip, state.lasers);
    
    //check if enemies collide with player
    ship_ship_collision(state.playerShip, state.enemyShips);
    
    //check if player/enemy wins. Enemy wins if collide with player ship or reach below screen.
    if (state.enemyShips.empty()) {
        std::cout << "Player wins!" << std::endl;
    } else if (state.playerShip.empty()) {
        std::cout << "Enemy wins!" << std::endl;
    } else if (y_boundary(state.enemyShips, 1.0f)) {
        std::cout << "Enemy wins!" << std::endl;
    }
}

void Render_Game_Level(GameState& state) {
    // for all game elements
    // setup transforms, render sprites
    
    //draw the player ship
    for (Entity& ship: state.playerShip) {
        ship.sprite.Draw(textured_program);
    }

    //draw the enemy ship
    for (Entity& ship: state.enemyShips) {
        ship.sprite.Draw(textured_program);
    }
    
    //draw the lasers
    for (Entity& laser: state.lasers) {
        laser.sprite.Draw(textured_program);
    }

}
//************************************
//Overall gamemode Game_Level methods end here
//************************************


//************************************
//Overall GameMode Title_Screen methods begin here
//************************************
void DrawText(ShaderProgram &p, int fontTexture, std::string text, float size, float spacing, float x, float y) {
    glm::mat4 newMatrix = glm::mat4(1.0f);
    newMatrix = glm::translate(newMatrix, glm::vec3(x, y, 1.0f));
    p.SetModelMatrix(newMatrix);
    
    float character_size = 1.0/16.0f;
    std::vector<float> vertexData;
    std::vector<float> texCoordData;
    for(int i=0; i < text.size(); i++) {
        int spriteIndex = (int)text[i];
        //std::cout << "This is the letter " << text[i] << std::endl;
        //std::cout << "This is the sprite index " << spriteIndex << std::endl;
        float texture_x = (float)(spriteIndex % 16) / 16.0f;
        float texture_y = (float)(spriteIndex / 16) / 16.0f;
        //std::cout << "This texture_x " << texture_x << std::endl;
        //std::cout << "This texture_y " << texture_y << std::endl;
        vertexData.insert(vertexData.end(), {
            ((size+spacing) * i) + (-0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (-0.5f * size), -0.5f * size,
            ((size+spacing) * i) + (0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (0.5f * size), -0.5f * size,
            ((size+spacing) * i) + (0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (-0.5f * size), -0.5f * size,
        });
        texCoordData.insert(texCoordData.end(), {
            texture_x, texture_y,
            texture_x, texture_y + character_size,
            texture_x + character_size, texture_y,
            texture_x + character_size, texture_y + character_size,
            texture_x + character_size, texture_y,
            texture_x, texture_y + character_size,
        }); }
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    // draw this data (use the .data() method of std::vector to get pointer to data)
    glEnableVertexAttribArray(p.positionAttribute);
    glEnableVertexAttribArray(p.texCoordAttribute);
    glVertexAttribPointer(p.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
    glVertexAttribPointer(p.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
    glDrawArrays(GL_TRIANGLES, 0, 6*text.size() );
    //std::cout << "This is the array vertices number " << (6*text.size()) << std::endl;
    // draw this yourself, use text.size() * 6 or vertexData.size()/2 to get number of vertices
    glDisableVertexAttribArray(p.positionAttribute);
    glDisableVertexAttribArray(p.texCoordAttribute);
}
void Render_Title_Screen() {
    DrawText(textured_program, LoadTexture(RESOURCE_FOLDER"font1.png"), "Welcome to Space Invaders", 0.14f, -0.05f, -1.1f, 0.5f);
    DrawText(textured_program, LoadTexture(RESOURCE_FOLDER"font1.png"), "Press enter to play", 0.1f, -0.05f, -0.5f, 0.0f);
    DrawText(textured_program, LoadTexture(RESOURCE_FOLDER"font1.png"), "Left/right arrow keys to move", 0.1f, -0.05f, -0.75f, -0.4f);
    DrawText(textured_program, LoadTexture(RESOURCE_FOLDER"font1.png"), "Spacebar to shoot", 0.1f, -0.05f, -0.45, -0.6f);
}
void Update_Title_Screen() {
    
}
bool Process_Title_Screen_Events(GameMode& mode) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        } else if(event.type == SDL_KEYDOWN) {
            if(event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
                mode = GAME_LEVEL;
            } else if (event.key.keysym.scancode == SDL_SCANCODE_KP_ENTER) {
                mode = GAME_LEVEL;
            }
        }
    }
    return false;
}

//************************************
//Overall GameMode Title_Screen methods end here
//************************************


//************************************
//Overall game methods begin here
//************************************
void Setup(GameState& state) {
    // setup SDL, setup OpenGL, Set our projection matrix
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
    glEnable(GL_BLEND); //enable blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  //set alpha blend function
    
    //setup player ship
    for (int i = 0; i < 1; i++) {
        Entity player;
        float u = 346.0f/1024.0f;
        float v = 75.0f/1024.0f;
        float width = 98.0f/1024.0f;
        float height = 75.0f/1024.0f;
        float x_scale = 0.2f;
        float y_scale = 0.2f;
        float x = 0.0f;
        //the Y coordinate will be at the bottom of the screen
        float y = -1.0 + height*2;
        player.sprite = SheetSprite(LoadTexture(RESOURCE_FOLDER"sheet.png"), u, v, width, height, x, y, x_scale, y_scale);
        //player.x_velocity = 1.5f;
        state.playerShip.push_back(player);
    }
    
    //setup enemy ships
    const int NUMBER_OF_ENEMY_SHIPS = 28;
    const int ENEMIES_PER_LINE = 7;
    for (int i = 0; i < NUMBER_OF_ENEMY_SHIPS; i++) {
        Entity enemyShip;
        float u = 120.0f/1024.0f;
        float v = 520.0f/1024.0f;
        float width = 104.0f/1024.0f;
        float height = 84.0f/1024.0f;
        float x_scale = 0.15f;
        float y_scale = 0.15f;
        //the X coordinate will be 2 times the width of each ship spaced apart
        float x = 1.777 - (width * 2) * (1 + (i % ENEMIES_PER_LINE));
        //the Y coordinate will be two times the height of each ship spaced apart
        float y = 1.0 - (i % (NUMBER_OF_ENEMY_SHIPS/ENEMIES_PER_LINE) * height * 2) - height;
        enemyShip.sprite = SheetSprite(LoadTexture(RESOURCE_FOLDER"sheet.png"),u ,v , width, height, x, y, x_scale, y_scale);
        //move -X and -Y direction at the start
        enemyShip.x_velocity = -0.5f;
        enemyShip.y_velocity = -height * 3.0;
        state.enemyShips.push_back(enemyShip);
    }
}
void Render(GameState& state, GameMode& mode) {
    switch(mode) {
        case TITLE_SCREEN:
            Render_Title_Screen();
            break;
        case GAME_LEVEL:
            Render_Game_Level(state);
            break;
    }
}
void Update(GameState& state, GameMode& mode) {
    switch(mode) {
        case TITLE_SCREEN:
            Update_Title_Screen();
            break;
        case GAME_LEVEL:
            Update_Game_Level(state);
            break;
    }
}
bool ProcessInput(GameState& state, GameMode& mode) {
    switch(mode) {
        case TITLE_SCREEN:
            return Process_Title_Screen_Events(mode);
            break;
        case GAME_LEVEL:
            return Process_Game_Level_Events(state);
            break;
    }
}
//************************************
//Overall game methods end here
//************************************

int main(int argc, char *argv[])
{
    GameMode mode = TITLE_SCREEN;
    GameState state;
    
    Setup(state);
    bool done = false;
    while (!done) {
        done = ProcessInput(state, mode);
        glClear(GL_COLOR_BUFFER_BIT);
        Update(state, mode);
        Render(state, mode);
        SDL_GL_SwapWindow(displayWindow);
    }
    SDL_Quit();
    return 0;
}


