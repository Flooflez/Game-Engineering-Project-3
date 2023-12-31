/**
* Author: Maximilian Ta
* Assignment: Lunar Lander
* Date due: 2023-11-08, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#define NUMBER_OF_ENEMIES 3
#define FIXED_TIMESTEP 0.0166666f
#define ACC_OF_GRAVITY -0.25f
#define PLATFORM_COUNT 6
#define DEATH_BOX_COUNT 20

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"

//  STRUCTS AND ENUMS //
struct GameState
{
    Entity* player;
    Entity* start_platform;
    Entity* platforms;
    Entity* deathboxes;
};

//  CONSTANTS  //
const int WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

const float BG_RED = 0.1922f,
BG_BLUE = 0.549f,
BG_GREEN = 0.9059f,
BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;
const char  PLAYER_SPRITE_FILEPATH[] = "assets/tardis.png",
BG_FILEPATH[] = "assets/bg.png",
PARTICLES_FILEPATH[] = "assets/particles.png",
GROUND_FILEPATH[] = "assets/ground.png",
BOOM_FILEPATH[] = "assets/boom.png",
FONT_FILEPATH[] = "assets/font1.png";


const int NUMBER_OF_TEXTURES = 1;  // to be generated, that is
const GLint LEVEL_OF_DETAIL = 0;  // base image level; Level n is the nth mipmap reduction image
const GLint TEXTURE_BORDER = 0;  // this value MUST be zero

const std::string DEATH_MESSAGE = "KABOOM! YOU CRASHED!! GAME OVER!!!",
                  WIN_MESSAGE   = "CONGRATS! YOU SAFELY TIME TRAVELLED!!";

// ----- BACKGROUND ----- //
struct GameAmbience
{
    Entity* background;
    Entity* particle1;
    Entity* particle2;
    Entity* ground;
};

//  VARIABLES  //
GameState g_game_state;
GameAmbience g_game_ambience;

SDL_Window* g_display_window;
bool g_game_is_running = true;
bool g_game_over = false;
bool g_player_is_moving = false;
GLuint g_text_texture_id;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;

int g_fuel = 750; //not the gamer drink

std::string g_display_message;

//  GENERAL FUNCTIONS  //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}

const int FONTBANK_SIZE = 16;

void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text, float screen_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairsone for each character
    // Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their position
        //    relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (screen_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    program->set_model_matrix(model_matrix);
    glUseProgram(program->get_program_id());

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0, vertices.data());
    glEnableVertexAttribArray(program->get_position_attribute());
    glVertexAttribPointer(program->get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(program->get_position_attribute());
    glDisableVertexAttribArray(program->get_tex_coordinate_attribute());
}

void lose_game() {
    g_game_over = true;
    g_game_state.player->m_texture_id = load_texture(BOOM_FILEPATH);
    g_game_state.player->update_model_matrix();
    g_display_message = DEATH_MESSAGE;
}

void win_game() {
    g_game_over = true;
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Definetly NOT the Tardis (For Legal Reasons)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    //  PLAYER  //
    // Existing
    g_game_state.player = new Entity();
    g_game_state.player->set_position(glm::vec3(-3.5f, -1.7f, 0.0f));
    g_game_state.player->set_scale(glm::vec3(0.5f));
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1, 0.0f));
    g_game_state.player->set_speed(0.2f);
    g_game_state.player->m_texture_id = load_texture(PLAYER_SPRITE_FILEPATH);
    g_game_state.player->set_height(0.5f);
    g_game_state.player->set_width(0.26f);


    //  PLATFORM  //
    g_game_state.platforms = new Entity[PLATFORM_COUNT];

    g_game_state.platforms[0].set_position(glm::vec3(-0.1f, -2.25f, 0.0f));
    g_game_state.platforms[1].set_position(glm::vec3(0.77f, -1.7f, 0.0f));
    g_game_state.platforms[2].set_position(glm::vec3(2.1f, -2.58f, 0.0f));
    g_game_state.platforms[3].set_position(glm::vec3(1.6f, -2.58f, 0.0f));
    g_game_state.platforms[4].set_position(glm::vec3(3.6f, -3.07f, 0.0f));
    g_game_state.platforms[5].set_position(glm::vec3(4.7f, -1.92f, 0.0f));

    for (int i = 0; i < PLATFORM_COUNT; i++)
    {
        g_game_state.platforms[i].set_scale(glm::vec3(0.6f));
        g_game_state.platforms[i].set_height(0.6f);
        g_game_state.platforms[i].set_width(0.6f);
        g_game_state.platforms[i].update(0.0f, NULL, 0);
    }


    // ----- DEATHBOXES ----- //
    g_game_state.deathboxes = new Entity[DEATH_BOX_COUNT];

    g_game_state.deathboxes[0].set_position(glm::vec3(-2.9f, -2.9f, 0.0f));
    g_game_state.deathboxes[0].set_scale(glm::vec3(1.0f, 0.5f, 0.0f));
    g_game_state.deathboxes[0].set_width(0.5f);

    g_game_state.deathboxes[1].set_position(glm::vec3(-2.4f, -2.4f, 0.0f));
    g_game_state.deathboxes[1].set_scale(glm::vec3(0.5f, 1.0f, 0.0f));
    g_game_state.deathboxes[1].set_width(0.5f);

    g_game_state.deathboxes[2].set_position(glm::vec3(-1.87f, -1.3f, 0.0f));
    g_game_state.deathboxes[2].set_scale(glm::vec3(1.0f, 4.0f, 0.0f));
    g_game_state.deathboxes[2].set_height(4.0f);

    g_game_state.deathboxes[3].set_position(glm::vec3(-1.87f, 0.48f, 0.0f));
    g_game_state.deathboxes[3].set_scale(glm::vec3(0.5f, 1.0f, 0.0f));
    g_game_state.deathboxes[3].set_width(0.5f);

    g_game_state.deathboxes[4].set_position(glm::vec3(-1.5f, -0.2f, 0.0f));

    g_game_state.deathboxes[5].set_position(glm::vec3(-1.3f, -0.95f, 0.0f));

    g_game_state.deathboxes[6].set_position(glm::vec3(-1.0f, -1.5f, 0.0f));

    g_game_state.deathboxes[7].set_position(glm::vec3(-0.87f, -1.9f, 0.0f));

    g_game_state.deathboxes[8].set_position(glm::vec3(0.55f, -2.15f, 0.0f));
    g_game_state.deathboxes[8].set_scale(glm::vec3(0.5f, 1.0f, 0.0f));
    g_game_state.deathboxes[8].set_width(0.5f);

    g_game_state.deathboxes[9].set_position(glm::vec3(1.0f, -2.0f, 0.0f));
    g_game_state.deathboxes[9].set_scale(glm::vec3(0.5f, 1.0f, 0.0f));
    g_game_state.deathboxes[9].set_width(0.5f);


    g_game_state.deathboxes[10].set_position(glm::vec3(1.17f, -2.3f, 0.0f));
    g_game_state.deathboxes[10].set_scale(glm::vec3(0.5f, 1.0f, 0.0f));
    g_game_state.deathboxes[10].set_width(0.5f);

    g_game_state.deathboxes[11].set_position(glm::vec3(2.6f, -2.6f, 0.0f));
    g_game_state.deathboxes[11].set_scale(glm::vec3(0.8f, 1.0f, 0.0f));
    g_game_state.deathboxes[11].set_width(0.8f);

    g_game_state.deathboxes[12].set_position(glm::vec3(2.65f, -2.4f, 0.0f));
    g_game_state.deathboxes[12].set_scale(glm::vec3(0.4f, 1.0f, 0.0f));
    g_game_state.deathboxes[12].set_width(0.4f);

    g_game_state.deathboxes[13].set_position(glm::vec3(3.0f, -2.75f, 0.0f));
    g_game_state.deathboxes[13].set_scale(glm::vec3(0.4f, 1.0f, 0.0f));
    g_game_state.deathboxes[13].set_width(0.4f);

    g_game_state.deathboxes[14].set_position(glm::vec3(3.22f, -3.08f, 0.0f));
    g_game_state.deathboxes[14].set_scale(glm::vec3(0.4f, 1.0f, 0.0f));
    g_game_state.deathboxes[14].set_width(0.4f);

    g_game_state.deathboxes[15].set_position(glm::vec3(4.3f, -2.8f, 0.0f));

    g_game_state.deathboxes[16].set_position(glm::vec3(4.6f, -2.3f, 0.0f));

    g_game_state.deathboxes[17].set_position(glm::vec3(4.4f, -2.6f, 0.0f));

    g_game_state.deathboxes[18].set_position(glm::vec3(-4.0f, -3.5f, 0.0f));
    g_game_state.deathboxes[19].set_position(glm::vec3(-3.5f, -2.8f, 0.0f));


    for (int i = 0; i < DEATH_BOX_COUNT; i++)
    {
        g_game_state.deathboxes[i].update_model_matrix();
    }

    //  START PLATFORM  //
    g_game_state.start_platform = new Entity();
    g_game_state.start_platform->set_position(glm::vec3(-3.5f, -2.2f, 0.0f));
    g_game_state.start_platform->set_scale(glm::vec3(0.5f));
    g_game_state.start_platform->set_height(0.5f);
    g_game_state.start_platform->set_width(0.5f);
    g_game_state.start_platform->update_model_matrix();

    //  GENERAL  //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ----- BG ----- //
    g_game_ambience.background = new Entity();
    g_game_ambience.background->set_position(glm::vec3(0.0f));
    g_game_ambience.background->set_scale(glm::vec3(10.0f));
    g_game_ambience.background->m_texture_id = load_texture(BG_FILEPATH);
    g_game_ambience.background->update_model_matrix();

    g_game_ambience.particle1 = new Entity();
    g_game_ambience.particle1->set_position(glm::vec3(0.0f));
    g_game_ambience.particle1->set_scale(glm::vec3(10.0f));
    g_game_ambience.particle1->set_velocity(glm::vec3(1.0f, 0.0f, 0.0f));
    g_game_ambience.particle1->m_texture_id = load_texture(PARTICLES_FILEPATH);

    g_game_ambience.particle2 = new Entity();
    g_game_ambience.particle2->set_position(glm::vec3(-10.0f,0.0f, 0.0f));
    g_game_ambience.particle2->set_scale(glm::vec3(10.0f));
    g_game_ambience.particle2->set_velocity(glm::vec3(1.0f, 0.0f, 0.0f));
    g_game_ambience.particle2->m_texture_id = load_texture(PARTICLES_FILEPATH);

    g_game_ambience.ground = new Entity();
    g_game_ambience.ground->set_position(glm::vec3(0.0f));
    g_game_ambience.ground->set_scale(glm::vec3(10.0f));
    g_game_ambience.ground->m_texture_id = load_texture(GROUND_FILEPATH);
    g_game_ambience.ground->update_model_matrix();

    // ----- TEXT ----- //
    g_text_texture_id = load_texture(FONT_FILEPATH);
    g_display_message = WIN_MESSAGE;

}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_player_is_moving = false;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (g_fuel > 0) {
        if (key_state[SDL_SCANCODE_LEFT])
        {
            g_player_is_moving = true;
            g_game_state.player->set_acceleration_x(-0.15f);
        }
        else if (key_state[SDL_SCANCODE_RIGHT])
        {
            g_player_is_moving = true;
            g_game_state.player->set_acceleration_x(0.15f);
        }
        else {
            g_game_state.player->set_acceleration_x(0.0f);
        }

        if (key_state[SDL_SCANCODE_SPACE] || key_state[SDL_SCANCODE_UP]) {
            g_player_is_moving = true;
            g_game_state.player->set_acceleration_y(0.2f);
        }
        else {
            g_game_state.player->set_acceleration_y(ACC_OF_GRAVITY);
        }
    }
    else {
        g_game_state.player->set_acceleration_x(0.0f);
        g_game_state.player->set_acceleration_y(ACC_OF_GRAVITY);
    }
    
    //let them move faster diagonally since it isn't easier
}

void update()
{
    //  DELTA TIME  //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND; // get the current number of ticks
    float delta_time = ticks - g_previous_ticks; // the delta time is the difference from the last frame
    g_previous_ticks = ticks;

    //  FIXED TIMESTEP  //
    // STEP 1: Keep track of how much time has passed since last step
    delta_time += g_time_accumulator;

    // STEP 2: Accumulate the ammount of time passed while we're under our fixed timestep
    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    // STEP 3: Once we exceed our fixed timestep, apply that elapsed time into the objects' update function invocation
    while (delta_time >= FIXED_TIMESTEP)
    {
        // Notice that we're using FIXED_TIMESTEP as our delta time
        if (!g_game_over) {
            g_game_state.player->update(FIXED_TIMESTEP, g_game_state.start_platform, 1);

            if (g_player_is_moving && g_fuel > 0) {
                g_fuel -= 1;
            }

            glm::vec3 player_pos = g_game_state.player->get_position();
            if (player_pos.x < -5.0f ||
                player_pos.x > 5.0f ||
                player_pos.y < -3.6f ||
                player_pos.y > 3.6f) { //out of bounds check
                lose_game();
            }
            else { //bad encapsulation but is ok ?
                for (int i = 0; i < DEATH_BOX_COUNT; i++) {
                    if (g_game_state.player->check_collision(&g_game_state.deathboxes[i])) {
                        lose_game();
                    }
                }
                for (int i = 0; i < PLATFORM_COUNT; i++) {
                    if (g_game_state.player->check_collision(&g_game_state.platforms[i])) {
                        win_game();
                    }
                }
            }
        }
        
        g_game_ambience.particle1->update(FIXED_TIMESTEP, nullptr, 0);
        g_game_ambience.particle2->update(FIXED_TIMESTEP, nullptr, 0);

        if (g_game_ambience.particle1->get_position().x > 10.0f) {
            g_game_ambience.particle1->set_position(glm::vec3(-10.0f, 0.0f, 0.0f));
        }
        if (g_game_ambience.particle2->get_position().x > 10.0f) {
            g_game_ambience.particle2->set_position(glm::vec3(-10.0f, 0.0f, 0.0f));
        }


        delta_time -= FIXED_TIMESTEP;
    }

    g_time_accumulator = delta_time;
}

void render()
{
    //  GENERAL  //
    glClear(GL_COLOR_BUFFER_BIT);
    g_game_ambience.background->render(&g_shader_program);
   
    g_game_ambience.particle1->render(&g_shader_program);
    g_game_ambience.particle2->render(&g_shader_program);
    g_game_ambience.ground->render(&g_shader_program);


    //  PLAYER  //
    g_game_state.player->render(&g_shader_program);

    //  PLATFORM  //
    //for (int i = 0; i < PLATFORM_COUNT; i++) g_game_state.platforms[i].render(&g_shader_program);
    //for (int i = 0; i < DEATH_BOX_COUNT; i++) g_game_state.deathboxes[i].render(&g_shader_program);
    //g_game_state.start_platform->render(&g_shader_program);
    
    // ----- TEXT ----- //
    if (g_game_over) {
        draw_text(&g_shader_program, g_text_texture_id, g_display_message, 0.25f, 0.0f, glm::vec3(-4.5f, 2.0f, 0.0f));
    }

    std::string fuel_label = std::string("Fuel: ");
    fuel_label.append(std::to_string(g_fuel));

    draw_text(&g_shader_program, g_text_texture_id, fuel_label, 0.25f, 0.0f, glm::vec3(-4.5f, 3.5f, 0.0f));

    //  GENERAL  //
    SDL_GL_SwapWindow(g_display_window);
}

void shutdown() { SDL_Quit(); }

//  DRIVER GAME LOOP  /
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}