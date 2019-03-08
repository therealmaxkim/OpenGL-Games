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
#include "FlareMap.h"

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
//Global variables begin here
//************************************
SDL_Window* displayWindow;
ShaderProgram textured_program;
ShaderProgram untextured_program;
const float SCREEN_WIDTH = 1280.0;
const float SCREEN_HEIGHT = 720.0;
const float TILE_SIZE = 0.1f;
const float DISPLACEMENT = 0.00001f;
const int MAX_TIMESTEPS = 60;
const float FIXED_TIMESTEP = 1.0/MAX_TIMESTEPS;
glm::vec3 gravity = glm::vec3(0.0f, -1.2f, 0.0f);
glm::vec3 friction = glm::vec3(1.0f, 0.0f, 0.0f);
GLuint SPRITE_SHEET;
//************************************
//Global variables end here
//************************************



//************************************
//Game class definitions begin here
//************************************
class SheetSprite {
    public:
    SheetSprite() {}
    SheetSprite(unsigned int textureID, int index, int spriteCountX, int spriteCountY) {
        this->textureID = textureID;
        this->u = (float)(((int)index) % spriteCountX) / (float) spriteCountX;
        this->v = (float)(((int)index) / spriteCountX) / (float) spriteCountY;
        this->width = 1.0/(float)spriteCountX;
        this->height = 1.0/(float)spriteCountY;
    }
    void Draw(ShaderProgram&p) {
        glBindTexture(GL_TEXTURE_2D, textureID);
        GLfloat texCoords[] = {
            u, v+height,
            u+width, v,
            u, v,
            u+width, v,
            u, v+height,
            u+width, v+height
        };
        GLfloat vertices[] = {-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f};
        glVertexAttribPointer(p.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
        glVertexAttribPointer(p.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
        glEnableVertexAttribArray(p.positionAttribute);
        glEnableVertexAttribArray(p.texCoordAttribute);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(p.positionAttribute);
        glDisableVertexAttribArray(p.texCoordAttribute);
    }
    float u, v, width, height;
    unsigned int textureID;
};

enum EntityType {ENTITY_PLAYER, ENTITY_ENEMY, ENTITY_COIN};

class Entity {
    public:
    glm::vec3 position;
    glm::vec3 size;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    bool isStatic, collideTop, collideBottom, collideLeft, collideRight;
    EntityType entity_type;
    SheetSprite sprite;
    
    bool collidesWith(Entity& entity) { //Box-Box collision detection.
        if ((abs(this->position.x - entity.position.x) - ((this->sprite.width + entity.sprite.width)/2)) < 0) {     //check that x direction distance < 0
            if ((abs(this->position.y - entity.position.y) - ((this->sprite.height + entity.sprite.height)/2)) < 0) {   //check that y direction distance < 0
                return true;
            }
        }
        return false;
    }
    
    void Draw(ShaderProgram &p) {
        glm::mat4 newMatrix = glm::mat4(1.0f);
        newMatrix = glm::translate(newMatrix, position);
        newMatrix = glm::scale(newMatrix, size);
        p.SetModelMatrix(newMatrix);
        sprite.Draw(p);
    }
};

class GameState {
public:
    Entity player;
    std::vector<Entity> enemies;
    std::vector<Entity> coins;
    FlareMap map;
};

//************************************
//Game class definitions end here
//************************************



//************************************
//Custom Draw methods begin here
//************************************
void DrawTilemap(ShaderProgram& p, int textureID, int spriteCountX, int spriteCountY, GameState& state) {
    std::vector<float> vertexData;
    std::vector<float> texCoordData;
    glm::mat4 newMatrix = glm::mat4(1.0f);
    p.SetModelMatrix(newMatrix);
    for(int x = 0; x < state.map.mapWidth; x++) {
        for(int y = 0; y < state.map.mapHeight; y++) {
            if(state.map.mapData[y][x] != 0) {
                float u = (float)(((int)state.map.mapData[y][x]) % spriteCountX) / (float) spriteCountX;
                float v = (float)(((int)state.map.mapData[y][x]) / spriteCountX) / (float) spriteCountY;
                float spriteWidth = 1.0f/(float)spriteCountX;
                float spriteHeight = 1.0f/(float)spriteCountY;
                vertexData.insert(vertexData.end(), {
                    TILE_SIZE * x, -TILE_SIZE * y,
                    TILE_SIZE * x, (-TILE_SIZE * y)-TILE_SIZE,
                    (TILE_SIZE * x)+TILE_SIZE, (-TILE_SIZE * y)-TILE_SIZE,
                    TILE_SIZE * x, -TILE_SIZE * y,
                    (TILE_SIZE * x)+TILE_SIZE, (-TILE_SIZE * y)-TILE_SIZE,
                    (TILE_SIZE * x)+TILE_SIZE, -TILE_SIZE * y
                });
                texCoordData.insert(texCoordData.end(), {
                    u, v,
                    u, v+(spriteHeight),
                    u+spriteWidth, v+(spriteHeight),
                    
                    u, v,
                    u+spriteWidth, v+(spriteHeight),
                    u+spriteWidth, v
                });
            }
        }
    }
    
    glBindTexture(GL_TEXTURE_2D, textureID);
    glEnableVertexAttribArray(p.positionAttribute);
    glEnableVertexAttribArray(p.texCoordAttribute);
    glVertexAttribPointer(p.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
    glVertexAttribPointer(p.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
    glDrawArrays(GL_TRIANGLES, 0, vertexData.size()/2);
    glDisableVertexAttribArray(p.positionAttribute);
    glDisableVertexAttribArray(p.texCoordAttribute);
}
//************************************
//Custom Draw methods end here
//************************************



//************************************
//Custom game methods begin here
//************************************

//converts entity position to grid coordinates
void worldToTileCoordinates(float worldX, float worldY, int* gridX, int* gridY) {
    *gridX = (int)(worldX / TILE_SIZE);
    *gridY = (int)(worldY / -TILE_SIZE);
}

//Input: two velocities and time
float lerp(float v0, float v1, float t) {
    return (1.0-t)*v0 + t*v1;
}

//Input: two entities
//find the penetration distance between two colliding entities. Displace entity1 by that penetration value plus an additional
//displacement value. Use coolide boolean to determine which direction to offset.
void penetration_y(Entity& entity, int gridY) {
    if (entity.collideTop) {
        float penetration = fabs((-TILE_SIZE * gridY - TILE_SIZE) - (entity.position.y + entity.size.y/2));
        entity.position.y -= (penetration + DISPLACEMENT);
        entity.velocity.y = 0;
    } else if (entity.collideBottom) {
        float penetration = fabs((-TILE_SIZE * gridY) - (entity.position.y - entity.size.y/2));
        entity.position.y += (penetration + DISPLACEMENT);
        entity.velocity.y = 0;
    }
}

//Input: two entities
//find the penetration distance between two colliding entities. Displace entity1 by that penetration value plus an additional
//displacement value. Use collide boolean to determine which direction to offset.
void penetration_x(Entity& entity, int gridX) {
    if (entity.collideRight) {
        float penetration = fabs((TILE_SIZE * gridX) - (entity.position.x + entity.size.x/2));
        entity.position.x -= (penetration + DISPLACEMENT);
        entity.velocity.x = 0;
    } else if (entity.collideLeft) {
        float penetration = fabs((TILE_SIZE * gridX + TILE_SIZE) - (entity.position.x - entity.size.x/2));
        entity.position.x += (penetration + DISPLACEMENT);
        entity.velocity.x = 0;
    }
}

//Input: entity and gamestate
//determines if there is a collision with entity and tilemap in top/bottom of entity
void check_collision_y(Entity& entity, GameState& state) {
    int gridX;
    int gridY;
    
    worldToTileCoordinates(entity.position.x, (entity.position.y + entity.size.y/2), &gridX, &gridY);    //check entity top
    if (gridY < state.map.mapHeight && gridX < state.map.mapWidth && gridY >= 0 && gridX >= 0) {
        if (state.map.mapData[gridY][gridX] != 0) {
            entity.collideTop = true;
            penetration_y(entity, gridY);
        }
    }
    
    worldToTileCoordinates(entity.position.x, (entity.position.y - entity.size.y/2), &gridX, &gridY);  //check entity bottom
    if (gridY < state.map.mapHeight && gridX < state.map.mapWidth && gridY >= 0 && gridX >= 0) {
        if (state.map.mapData[gridY][gridX] != 0) {
            entity.collideBottom = true;
            penetration_y(entity, gridY);
        }
    }
}

//Input: entity and gamestate
//determines if there is a collision with entity and tilemap in left/right of entity
void check_collision_x(Entity& entity, GameState& state) {
    int gridX;
    int gridY;
    
    worldToTileCoordinates((entity.position.x - entity.size.x/2), entity.position.y, &gridX, &gridY);  //check entity left
    if (gridY < state.map.mapHeight && gridX < state.map.mapWidth && gridY >= 0 && gridX >= 0) {
        if (state.map.mapData[gridY][gridX] != 0) {
            entity.collideLeft = true;
            penetration_x(entity, gridX);
        }
    }
    
    worldToTileCoordinates((entity.position.x + entity.size.x/2), entity.position.y, &gridX, &gridY);  //check entity right
    if (gridY < state.map.mapHeight && gridX < state.map.mapWidth && gridY >= 0 && gridX >= 0) {
        if (state.map.mapData[gridY][gridX] != 0) {
            entity.collideRight = true;
            penetration_x(entity, gridX);
        }
    }
}

//Input: entity player and time
//code copy and pasted from slides. Used to move the player smoothly.
void move_entity(Entity& entity, GameState& state, const float& elapsed) {
    //apply friction
    entity.velocity.x = lerp(entity.velocity.x, 0.0f, elapsed * friction.x);
    entity.velocity.y = lerp(entity.velocity.y, 0.0f, elapsed * friction.y);
    //apply acceleration
    entity.velocity.x += entity.acceleration.x * elapsed;
    entity.velocity.y += entity.acceleration.y * elapsed;
    //apply gravity
    entity.velocity.x += gravity.x * elapsed;
    entity.velocity.y += gravity.y * elapsed;
    //check y axis
    entity.position.y += entity.velocity.y * elapsed;
    check_collision_y(entity, state);
    //check x axis
    entity.position.x += entity.velocity.x * elapsed;
    check_collision_x(entity, state);
}

//Input: entity
//Check if the collideBottom flag is true and allow jumps only when standing on platform. Set y velocity directly to jump.
void jump(Entity& entity) {
    if (entity.collideBottom) {
        entity.velocity.y = 1.2f;
    }
}
//Input: entity and vector of entities
//Check if this single entity is colliding with any of the vector of entities. If so, remove the entity that is colliding from the vector.
void check_collision_dynamic(Entity& entity, std::vector<Entity>& entities) {
    for (int i =0; i < entities.size(); i++) {
        if (entity.collidesWith(entities[i])) {
            entities.erase(entities.begin() + i);
        }
    }
}
//************************************
//Custom game methods end here
//************************************




//************************************
//Overall gamemode Game_Level methods begin here
//************************************
bool Process_Game_Level_Events(GameState& state) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        } else if(event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {  //Move player up using spacebar
                jump(state.player);
            }
        }
    }
    return false;
}

void Update_Game_Level(GameState& state, const float elapsed) {
    //Reset all of the player's collision boolean values
    state.player.collideTop = false;
    state.player.collideBottom = false;
    state.player.collideLeft = false;
    state.player.collideRight = false;
    
    //Reset the player's acceleration
    state.player.acceleration = glm::vec3(0.0f, 0.0f, 0.0f);
    
    //Move player using left/right arrow keys
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if(keys[SDL_SCANCODE_LEFT]) {
        state.player.acceleration.x = -0.5f;
    } else if(keys[SDL_SCANCODE_RIGHT]) {
        state.player.acceleration.x = 0.5f;
    }
    
    //move the players and enemies
    move_entity(state.player, state, elapsed);
    for (Entity& entity: state.enemies) {
        move_entity(entity, state, elapsed);
    }
    
    //check collision with player and coins.
    check_collision_dynamic(state.player, state.coins);
    
    //Move the viewmatrix to follow the player
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    viewMatrix = glm::translate(viewMatrix, glm::vec3(-(state.player.position.x), -(state.player.position.y), 0.0f));
    textured_program.SetViewMatrix(viewMatrix);
}

void Render_Game_Level(GameState& state) {
    state.player.Draw(textured_program);
    for (Entity& entity: state.enemies) {
        entity.Draw(textured_program);
    }
    for (Entity& entity: state.coins) {
        entity.Draw(textured_program);
    }
}
//************************************
//Overall gamemode Game_Level methods end here
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
    
    SPRITE_SHEET = LoadTexture(RESOURCE_FOLDER"spritesheet_rgba.png");
    state.map.Load(RESOURCE_FOLDER"mymap.txt");
    
    for(FlareMapEntity &entity : state.map.entities) {
        Entity newEntity;
        newEntity.position = glm::vec3(entity.x*TILE_SIZE+TILE_SIZE, entity.y*-TILE_SIZE+TILE_SIZE/2, 1.0f);
        newEntity.size = glm::vec3(TILE_SIZE, TILE_SIZE, 1.0f);
        if (entity.type == "player") {
            newEntity.sprite = SheetSprite(SPRITE_SHEET, 19, 30, 30);
            newEntity.entity_type = ENTITY_PLAYER;
            state.player = newEntity;
        } else if (entity.type == "enemy") {
            newEntity.sprite = SheetSprite(SPRITE_SHEET, 343, 30, 30);
            newEntity.entity_type = ENTITY_ENEMY;
            state.enemies.push_back(newEntity);
        } else if (entity.type == "coin") {
            newEntity.sprite = SheetSprite(SPRITE_SHEET, 78, 30, 30);
            newEntity.entity_type = ENTITY_COIN;
            state.coins.push_back(newEntity);
        }
    }
    
}

//************************************
//Overall game methods end here
//************************************

int main(int argc, char *argv[])
{
    GameState state;
    bool done = false;
    float accumulator = 0.0f;
    float lastFrameTicks = 0.0f;
    
    Setup(state);

    while (!done) {
        float ticks = (float)SDL_GetTicks()/1000.0f;
        float elapsed = ticks - lastFrameTicks;
        lastFrameTicks = ticks;
        
        done = Process_Game_Level_Events(state);
        glClear(GL_COLOR_BUFFER_BIT);

        elapsed += accumulator;
        if (elapsed < FIXED_TIMESTEP) {
            accumulator = elapsed;
            continue;
        }
        while(elapsed >= FIXED_TIMESTEP) {
            Update_Game_Level(state, FIXED_TIMESTEP);
            elapsed -= FIXED_TIMESTEP;
        }
        accumulator = elapsed;
        //keep redrawing the tilemap each frame
        DrawTilemap(textured_program, SPRITE_SHEET, 30, 30, state);
        Render_Game_Level(state);
        SDL_GL_SwapWindow(displayWindow);
    }
    SDL_Quit();
    return 0;
}


