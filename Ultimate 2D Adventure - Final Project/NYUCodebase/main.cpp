#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <vector>
#include <string>
#include <map>
#include <set>
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
#include <SDL_mixer.h>

//************************************
//Global variables begin here
//************************************
SDL_Window* displayWindow;
ShaderProgram textured_program;
ShaderProgram untextured_program;
const float SCREEN_WIDTH = 1280.0, SCREEN_HEIGHT = 720.0;
const int SPRITE_COUNT_X = 30, SPRITE_COUNT_Y = 30;
const int MAX_TIMESTEPS = 60;
const float TILE_SIZE = 0.13f;
const float DISPLACEMENT = 0.0f;
const float FIXED_TIMESTEP = 1.0/MAX_TIMESTEPS;
glm::vec3 gravity = glm::vec3(0.0f, -1.2f, 0.0f), friction = glm::vec3(1.0f, 0.0f, 0.0f);
GLuint SPRITE_SHEET, FONTS;
Mix_Chunk *jumpSound, *coinSound, *deathSound;
Mix_Music *music;
//hold the indices of lethal tiles, such as water, lava, etc.
std::set<int> LETHAL_TILE_INDEX = {577, 578, 579, 580, 42};
//hold the entity names and their corresponding tile index.
std::map<std::string, int> ENTITY_INDEX = {
    {"player", 79}, {"red", 378}, {"green", 377}, {"blue", 379}, {"yellow", 376}, {"snowman", 139}, {"bee", 354}, {"spider", 472}, {"door", 732}, {"ghost", 446}, {"bird", 442}
};
//hold the entity names that are enemies
std::set<std::string> ENEMIES = {"snowman", "bee", "spider", "bird", "ghost"};

//************************************
//Global variables end here
//************************************

//plays music with fadein/outs
void Play_Music(const std::string& title) {
    if (!Mix_PlayingMusic()) {  //if not already playing music, play music.
        music = Mix_LoadMUS((std::string(RESOURCE_FOLDER) + title).c_str());
        Mix_FadeInMusic(music, 2, 2000);
    } else {        //if playing music, fade out and then play music.
        while(!Mix_FadeOutMusic(2000) && Mix_PlayingMusic()) {
            // wait for any fades to complete
            SDL_Delay(100);
        }
        music = Mix_LoadMUS((std::string(RESOURCE_FOLDER) + title).c_str());
        Mix_FadeInMusic(music, 2, 2000);
    }
}


//************************************
//Game class definitions begin here
//************************************
class SheetSprite {
public:
    SheetSprite() {}
    SheetSprite(unsigned int textureID, int index) {
        this->textureID = textureID;
        this->u = (float)(((int)index) % SPRITE_COUNT_X) / (float) SPRITE_COUNT_X;
        this->v = (float)(((int)index) / SPRITE_COUNT_X) / (float) SPRITE_COUNT_Y;
        this->width = 1.0/(float)SPRITE_COUNT_X;
        this->height = 1.0/(float)SPRITE_COUNT_Y;
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

enum EntityType {ENTITY_PLAYER, ENTITY_ENEMY, ENTITY_COIN, ENTITY_DOOR};

class Entity {
public:
    bool collidesWith(Entity& entity) {     //Box-Box collision detection.
        if ((abs(this->position.x - entity.position.x) - ((TILE_SIZE + TILE_SIZE)/3)) < 0) {     //check that x direction distance < 0
            if ((abs(this->position.y - entity.position.y) - ((TILE_SIZE + TILE_SIZE)/3)) < 0) {   //check that y direction distance < 0
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
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), size = glm::vec3(0.0f, 0.0f, 0.0f),
        velocity = glm::vec3(0.0f, 0.0f, 0.0f), acceleration = glm::vec3(0.0f, 0.0f, 0.0f);
    bool isStatic, collideTop, collideBottom, collideLeft, collideRight;
    EntityType entity_type;
    SheetSprite sprite;
};

class GameState {
public:
    std::vector<Entity> player = std::vector<Entity>();
    std::vector<Entity> enemies = std::vector<Entity>();
    std::vector<Entity> coins = std::vector<Entity>();
    std::vector<Entity> doors = std::vector<Entity>();
    FlareMap map = FlareMap();
};

enum GameMode {TITLE_SCREEN, GAME_LEVEL1, GAME_LEVEL2, GAME_LEVEL3, GAME_OVER, GAME_MENU, GAME_PAUSE};
GameMode save;  //global variable to save last level mode
//************************************
//Game class definitions end here
//************************************

//plays death sound
void Die(GameMode& mode) {
    Mix_HaltMusic();                    //stop the music
    Mix_PlayChannel(-1, deathSound, 0);  //play death sound
    mode = GAME_OVER;                   //change game mode
}

//************************************
//Custom Draw methods begin here
//************************************
void DrawTilemap(ShaderProgram& p, int textureID, GameState& state) {
    std::vector<float> vertexData;
    std::vector<float> texCoordData;
    glm::mat4 newMatrix = glm::mat4(1.0f);
    p.SetModelMatrix(newMatrix);
    for(int x = 0; x < state.map.mapWidth; x++) {
        for(int y = 0; y < state.map.mapHeight; y++) {
            if(state.map.mapData[y][x] != 0) {
                float u = (float)(((int)state.map.mapData[y][x]) % SPRITE_COUNT_X) / (float) SPRITE_COUNT_X;
                float v = (float)(((int)state.map.mapData[y][x]) / SPRITE_COUNT_X) / (float) SPRITE_COUNT_Y;
                float spriteWidth = 1.0f/(float)SPRITE_COUNT_X;
                float spriteHeight = 1.0f/(float)SPRITE_COUNT_Y;
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
void DrawText(ShaderProgram &p, int fontTexture, std::string text, float size, float spacing, float x, float y) {
    glm::mat4 newMatrix = glm::mat4(1.0f);
    newMatrix = glm::translate(newMatrix, glm::vec3(x, y, 1.0f));
    p.SetModelMatrix(newMatrix);
    
    float character_size = 1.0/16.0f;
    std::vector<float> vertexData;
    std::vector<float> texCoordData;
    for(int i=0; i < text.size(); i++) {
        int spriteIndex = (int)text[i];
        float texture_x = (float)(spriteIndex % 16) / 16.0f;
        float texture_y = (float)(spriteIndex / 16) / 16.0f;
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
//draw different tile maps once each time, depending on which game level is selected
void Draw_Game_Level(GameState& state, GameMode& mode) {
    //reset player, enemies, coins, doors, and map from previous levels
    state.player.clear();
    state.enemies.clear();
    state.coins.clear();
    state.doors.clear();
    state.map = FlareMap();
    
    switch(mode) {
        case GAME_LEVEL1:
            state.map.Load(RESOURCE_FOLDER"Level_1.txt");
            Play_Music("Level_1.mp3");
            break;
        case GAME_LEVEL2:
            state.map.Load(RESOURCE_FOLDER"Level_2.txt");
            Play_Music("Level_2.mp3");
            break;
        case GAME_LEVEL3:
            state.map.Load(RESOURCE_FOLDER"Level_3.txt");
            Play_Music("Level_3.mp3");
            break;
        default:
            break;
    }
    for (FlareMapEntity &entity : state.map.entities) {
        Entity newEntity;
        newEntity.position = glm::vec3(entity.x*TILE_SIZE+TILE_SIZE, entity.y*-TILE_SIZE+TILE_SIZE/2, 1.0f);
        newEntity.sprite = SheetSprite(SPRITE_SHEET, ENTITY_INDEX[entity.type]);
        newEntity.size = glm::vec3(TILE_SIZE, TILE_SIZE, 1.0f);
        
        if (entity.type == "player") {                                  //moving player
            newEntity.entity_type = ENTITY_PLAYER;
            newEntity.isStatic = false;
            state.player.push_back(newEntity);
        } else if (entity.type == "door") {                             //static door
            newEntity.entity_type = ENTITY_DOOR;
            newEntity.isStatic = true;
            state.doors.push_back(newEntity);
        } else if (ENEMIES.find(entity.type) != ENEMIES.end()) {        //enemy
            newEntity.entity_type = ENTITY_ENEMY;
            if (entity.type == "spider") {                              //make spiders move vertically
                newEntity.acceleration = glm::vec3(0.0f, -0.25f, 0.0f);
            } else {                                                    //make all other enemies move horizontally
                newEntity.acceleration = glm::vec3(0.25f, 0.0f, 0.0f);
            }
            newEntity.isStatic = false;
            state.enemies.push_back(newEntity);
        } else {                                                        //static coins
            newEntity.entity_type = ENTITY_COIN;
            newEntity.isStatic = true;
            state.coins.push_back(newEntity);
        }
    }
}

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
//Custom Draw methods end here
//************************************



//************************************
//Custom game methods begin here
//************************************

float mapValue(float value, float srcMin, float srcMax, float dstMin, float dstMax) {
    float retVal = dstMin + ((value - srcMin)/(srcMax-srcMin) * (dstMax-dstMin));
    if(retVal < dstMin) {
        retVal = dstMin;
    }
    if(retVal > dstMax) {
        retVal = dstMax;
    }
    return retVal;
}

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
bool check_collision_y(Entity& entity, GameState& state, GameMode& mode) {
    int gridX, gridY;
    //check entity top
    worldToTileCoordinates(entity.position.x, (entity.position.y + entity.size.y/2), &gridX, &gridY);
    if (gridY < state.map.mapHeight && gridX < state.map.mapWidth && gridY >= 0 && gridX >= 0) {
        int index = state.map.mapData[gridY][gridX];
        if (index != 0) {    //check if tile is an empty space
            if (LETHAL_TILE_INDEX.find(index) != LETHAL_TILE_INDEX.end() && entity.entity_type == ENTITY_PLAYER) {   //check if tile is a lethal tile
                Die(mode);
            }
            entity.collideTop = true;
            penetration_y(entity, gridY);
            return true;
        }
    }
    //check entity bottom
    worldToTileCoordinates(entity.position.x, (entity.position.y - entity.size.y/2), &gridX, &gridY);
    if (gridY < state.map.mapHeight && gridX < state.map.mapWidth && gridY >= 0 && gridX >= 0) {
        int index = state.map.mapData[gridY][gridX];
        if (index != 0) {    //check if tile is an empty space
            if (LETHAL_TILE_INDEX.find(index) != LETHAL_TILE_INDEX.end() && entity.entity_type == ENTITY_PLAYER) {   //check if tile is a lethal tile
                Die(mode);
            }
            entity.collideBottom = true;
            penetration_y(entity, gridY);
            return true;
        }
    }
    return false;
}

//Input: entity and gamestate
//determines if there is a collision with entity and tilemap in left/right of entity
bool check_collision_x(Entity& entity, GameState& state, GameMode& mode) {
    int gridX, gridY;
    //check entity left
    worldToTileCoordinates((entity.position.x - entity.size.x/2), entity.position.y, &gridX, &gridY);
    if (gridY < state.map.mapHeight && gridX < state.map.mapWidth && gridY >= 0 && gridX >= 0) {
        int index = state.map.mapData[gridY][gridX];
        if (index != 0) {    //check if tile is an empty space
            if (LETHAL_TILE_INDEX.find(index) != LETHAL_TILE_INDEX.end() && entity.entity_type == ENTITY_PLAYER) {   //check if tile is a lethal tile
                Die(mode);
            }
            entity.collideLeft = true;
            penetration_x(entity, gridX);
            return true;
        }
    }
    //check entity right
    worldToTileCoordinates((entity.position.x + entity.size.x/2), entity.position.y, &gridX, &gridY);
    if (gridY < state.map.mapHeight && gridX < state.map.mapWidth && gridY >= 0 && gridX >= 0) {
        int index = state.map.mapData[gridY][gridX];
        if (index != 0) {    //check if tile is an empty space
            if (LETHAL_TILE_INDEX.find(index) != LETHAL_TILE_INDEX.end() && entity.entity_type == ENTITY_PLAYER) {   //check if tile is a lethal tile
                Die(mode);
            }
            entity.collideRight = true;
            penetration_x(entity, gridX);
            return true;
        }
    }
    return false;
}

//Input: entity player and time
//code copy and pasted from slides. Used to move the player smoothly.
void move_entity(Entity& entity, GameState& state, GameMode& mode, const float& elapsed) {
    //apply friction
    entity.velocity.x = lerp(entity.velocity.x, 0.0f, elapsed * friction.x);
    entity.velocity.y = lerp(entity.velocity.y, 0.0f, elapsed * friction.y);
    //apply acceleration
    entity.velocity.x += entity.acceleration.x * elapsed;
    entity.velocity.y += entity.acceleration.y * elapsed;
    //apply gravity only to player
    if (entity.entity_type == ENTITY_PLAYER) {
        entity.velocity.x += gravity.x * elapsed;
        entity.velocity.y += gravity.y * elapsed;
    }
    //check y axis and reverse direction if entity is an enemy
    entity.position.y += entity.velocity.y * elapsed;
    if (check_collision_y(entity, state, mode) && entity.entity_type == ENTITY_ENEMY) {
        entity.acceleration.y = -entity.acceleration.y;
    }
    //check x axis and reverse direction if entity is an enemy
    entity.position.x += entity.velocity.x * elapsed;
    if (check_collision_x(entity, state, mode) && entity.entity_type == ENTITY_ENEMY) {
        entity.acceleration.x = -entity.acceleration.x;
    }
}


//Input: entity
//Check if the collideBottom flag is true and allow jumps only when standing on platform. Set y velocity directly to jump.
void jump(Entity& entity) {
    if (entity.collideBottom) {
        entity.velocity.y = 0.95f;
        Mix_PlayChannel(-1, jumpSound, 0);
    }
}

//************************************
//Custom game methods end here
//************************************


//************************************
//Overall Game_Level update/render/process_input methods begin here
//************************************
bool Process_Game_Level_Events(GameState& state, GameMode& mode) {
    SDL_Event event;
    save = mode;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        } else if(event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_SPACE:    //Move player up when spacebar pressed
                    jump(state.player[0]);
                    break;
                case SDL_SCANCODE_ESCAPE:   //Pause the game when escape key pressed
                    mode = GAME_PAUSE;
                    break;
                default:
                    break;
            }
        }
    }
    return false;
}

void Update_Game_Level(GameState& state, GameMode& mode, const float elapsed) {
    //Reset all of the player's collision boolean values
    state.player[0].collideTop = false;
    state.player[0].collideBottom = false;
    state.player[0].collideLeft = false;
    state.player[0].collideRight = false;
    
    //Reset all moving enemies's collision boolean values
    for (Entity& entity: state.enemies) {
        if (!entity.isStatic) {
            entity.collideTop = false;
            entity.collideBottom = false;
            entity.collideLeft = false;
            entity.collideRight = false;
        }
    }
    
    //Reset the player's acceleration
    state.player[0].acceleration = glm::vec3(0.0f, 0.0f, 0.0f);
    
    //Move player using left/right arrow keys
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if(keys[SDL_SCANCODE_LEFT]) {
        state.player[0].acceleration.x = -0.75f;
    } else if(keys[SDL_SCANCODE_RIGHT]) {
        state.player[0].acceleration.x = 0.75f;
    }
    
    //move player
    move_entity(state.player[0], state, mode, elapsed);
    
    //move enemies that are not static
    for (Entity& entity: state.enemies) {
        if (!entity.isStatic) {
            move_entity(entity, state, mode, elapsed);
        }
    }
    
    //check collision between player and enemies
    for (Entity& entity: state.enemies) {
        if (state.player[0].collidesWith(entity)) {
            Die(mode);
            return;
        }
    }
    
    //check collision with player and coins.
    for (int i =0; i < state.coins.size(); i++) {
        if (state.player[0].collidesWith(state.coins[i])) {
            Mix_PlayChannel(-1, coinSound, 0);          //play coin sound
            state.coins.erase(state.coins.begin() + i); //erase the coin
        }
    }
    //check collision with player and doors
    for (Entity& entity: state.doors) {
        if (state.player[0].collidesWith(entity)) {
            //return to main menu
            Play_Music("Title_Screen.mp3");
            mode = GAME_MENU;
        }
    }
    
    //Move the viewmatrix to follow the player
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    viewMatrix = glm::translate(viewMatrix, glm::vec3(-(state.player[0].position.x), -(state.player[0].position.y), 0.0f));
    textured_program.SetViewMatrix(viewMatrix);
    
    //make the player's x width change as you increase/decrease x velocity.
    // map Y velocity 0.0 - 5.0 to 1.0 - 1.6 Y scale and 1.0 - 0.8 X scale
    state.player[0].size = glm::vec3(mapValue(fabs(state.player[0].velocity.x), 0.4, 0.0, TILE_SIZE*1.0, TILE_SIZE*1.7),
                                     TILE_SIZE,
                                     0.0f);
    

}

void Render_Game_Level(GameState& state) {
    //draw the player
    state.player[0].Draw(textured_program);
    //draw the enemies
    for (Entity& entity: state.enemies) {
        entity.Draw(textured_program);
    }
    //draw the coins
    for (Entity& entity: state.coins) {
        entity.Draw(textured_program);
    }
    //draw the doors
    for (Entity& entity: state.doors) {
        entity.Draw(textured_program);
    }
}
//************************************
//Overall Game_Level update/render/process_input methods end here
//************************************

//************************************
//Overall Title_Screen update/render/process_input methods begin here
//************************************
bool Process_Title_Screen_Events(GameState& state, GameMode& mode) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        } else if(event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_RETURN:       //Go to game menu when enter is pressed
                    mode = GAME_MENU;
                    break;
                case SDL_SCANCODE_ESCAPE:       //end game when escape is pressed
                    return true;
                    break;
                default:
                    break;
            }
        }
    }
    return false;
}
void Render_Title_Screen() {
    //Reset the view matrix to identity before drawing UI
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    textured_program.SetViewMatrix(viewMatrix);
    DrawText(textured_program, FONTS, "Welcome to Jump", 0.14f, -0.05f, -1.1f, 0.5f);
    DrawText(textured_program, FONTS, "Press enter to play", 0.1f, -0.05f, -0.5f, 0.0f);
    DrawText(textured_program, FONTS, "Left/right arrow keys to move", 0.1f, -0.05f, -0.75f, -0.4f);
    DrawText(textured_program, FONTS, "Spacebar to jump", 0.1f, -0.05f, -0.45, -0.6f);
    DrawText(textured_program, FONTS, "Escape to end", 0.1f, -0.05f, -0.45, -0.8f);
}


//************************************
//Overall Title_Screen update/render/process_input methods end here
//************************************



//************************************
//Overall Game_Menu update/render/process_input methods begin here
//************************************
bool Process_Game_Menu_Events(GameState& state, GameMode& mode) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        } else if(event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_1:        //press 1 to play level 1
                    mode = GAME_LEVEL1;
                    Draw_Game_Level(state, mode);
                    break;
                case SDL_SCANCODE_2:        //press 2 to play level 2
                    mode = GAME_LEVEL2;
                    Draw_Game_Level(state, mode);
                    break;
                case SDL_SCANCODE_3:        //press 3 to play level 3
                    mode = GAME_LEVEL3;
                    Draw_Game_Level(state, mode);
                    break;
                case SDL_SCANCODE_4:        //press 4 to return to title screen
                    mode = TITLE_SCREEN;
                    Play_Music("Title_Screen.mp3");
                    break;
                default:
                    break;
            }
        }
    }
    return false;
}
void Render_Game_Menu_Screen() {
    //Reset the view matrix to identity before drawing UI
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    textured_program.SetViewMatrix(viewMatrix);
    DrawText(textured_program, FONTS, "Press 1 to play level 1", 0.1f, -0.05f, -1.1f, 0.5f);
    DrawText(textured_program, FONTS, "Press 2 to play level 2", 0.1f, -0.05f, -0.5f, 0.0f);
    DrawText(textured_program, FONTS, "Press 3 to play level 3", 0.1f, -0.05f, -0.75f, -0.4f);
    DrawText(textured_program, FONTS, "Press 4 to go back", 0.1f, -0.05f, -0.45, -0.6f);
}

//************************************
//Overall Game_Menu update/render/process_input methods begin here
//************************************


//************************************
//Overall Game_Over update/render/process_input methods begin here
//************************************
bool Process_Game_Over_Events(GameState& state, GameMode& mode) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        } else if(event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_RETURN:       //press enter to return to game menu
                    Play_Music("Title_Screen.mp3");
                    mode = GAME_MENU;
                    break;
                default:
                    break;
            }
        }
    }
    return false;
}
void Render_Game_Over_Screen() {
    //Reset the view matrix to identity before drawing UI
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    textured_program.SetViewMatrix(viewMatrix);
    DrawText(textured_program, FONTS, "Game Over", 0.1f, -0.05f, -1.1f, 0.5f);
    DrawText(textured_program, FONTS, "Press enter to return to menu", 0.1f, -0.05f, -0.5f, 0.0f);
}
//************************************
//Overall Game_Over update/render/process_input methods end here
//************************************


//************************************
//Overall Game_Pause update/render/process_input methods start here
//************************************
bool Process_Game_Pause_Events(GameState& state, GameMode& mode) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            return true;
        } else if(event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_RETURN:   //return to previous game
                    mode = save;
                    break;
                case SDL_SCANCODE_1:        //return to game menu
                    mode = GAME_MENU;
                    break;
                case SDL_SCANCODE_2:        //quit the game
                    return true;
                    break;
                default:
                    break;
            }
        }
    }
    return false;
}
void Render_Game_Pause_Screen() {
    //Reset the view matrix to identity before drawing UI
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    textured_program.SetViewMatrix(viewMatrix);
    DrawText(textured_program, FONTS, "Press enter to unpause", 0.1f, -0.05f, -1.1f, 0.5f);
    DrawText(textured_program, FONTS, "Press 1 to return to main menu", 0.1f, -0.05f, -0.5f, 0.0f);
    DrawText(textured_program, FONTS, "Press 2 to quit game", 0.1f, -0.05f, -0.45, -0.6f);
}
//************************************
//Overall Game_Pause update/render/process_input methods end here
//************************************


//************************************
//Overall Game methods begin here
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
    FONTS = LoadTexture(RESOURCE_FOLDER"font1.png");

    Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 );
    jumpSound = Mix_LoadWAV(RESOURCE_FOLDER"jumpSound.wav");
    Mix_VolumeChunk(jumpSound, 16);
    coinSound = Mix_LoadWAV(RESOURCE_FOLDER"coinSound.wav");
    Mix_VolumeChunk(coinSound, 16);
    deathSound = Mix_LoadWAV(RESOURCE_FOLDER"deathSound.wav");
    Mix_VolumeChunk(deathSound, 16);
}

void Render(GameState& state, GameMode& mode) {
    switch(mode) {
        case TITLE_SCREEN:
            Render_Title_Screen();
            break;
        case GAME_OVER:
            Render_Game_Over_Screen();
            break;
        case GAME_MENU:
            Render_Game_Menu_Screen();
            break;
        case GAME_PAUSE:
            Render_Game_Pause_Screen();
            break;
        case GAME_LEVEL1:
        case GAME_LEVEL2:
        case GAME_LEVEL3:
            DrawTilemap(textured_program, SPRITE_SHEET, state);
            Render_Game_Level(state);
            break;
    }
}
void Update(GameState& state, GameMode& mode, const float elapsed) {
    switch(mode) {
        case GAME_LEVEL1:
        case GAME_LEVEL2:
        case GAME_LEVEL3:
            Update_Game_Level(state, mode, elapsed);
            break;
        default:
            break;
    }
}
bool ProcessInput(GameState& state, GameMode& mode) {
    switch(mode) {
        case TITLE_SCREEN:
            return Process_Title_Screen_Events(state, mode);
            break;
        case GAME_OVER:
            return Process_Game_Over_Events(state, mode);
            break;
        case GAME_MENU:
            return Process_Game_Menu_Events(state, mode);
            break;
        case GAME_PAUSE:
            return Process_Game_Pause_Events(state, mode);
            break;
        case GAME_LEVEL1:
        case GAME_LEVEL2:
        case GAME_LEVEL3:
            return Process_Game_Level_Events(state, mode);
            break;
    }
}

//************************************
//Overall game methods end here
//************************************

int main(int argc, char *argv[])
{
    GameState state;
    GameMode mode = TITLE_SCREEN;
    bool done = false;
    float accumulator = 0.0f;
    float lastFrameTicks = 0.0f;
    
    Setup(state);
    Play_Music("Title_screen.mp3");
    
    while (!done) {
        float ticks = (float)SDL_GetTicks()/1000.0f;
        float elapsed = ticks - lastFrameTicks;
        lastFrameTicks = ticks;
        
        done = ProcessInput(state, mode);

        elapsed += accumulator;
        if (elapsed < FIXED_TIMESTEP) {
            accumulator = elapsed;
            continue;
        }
        while(elapsed >= FIXED_TIMESTEP) {
            Update(state, mode, FIXED_TIMESTEP);
            elapsed -= FIXED_TIMESTEP;
        }
        accumulator = elapsed;
        
        glClear(GL_COLOR_BUFFER_BIT);
        Render(state, mode);
        SDL_GL_SwapWindow(displayWindow);
    }
    SDL_Quit();
    return 0;
}


