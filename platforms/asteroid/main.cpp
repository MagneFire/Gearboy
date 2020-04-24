/*
 * Gearboy - Nintendo Game Boy Emulator
 * Copyright (C) 2012  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <SDL2/SDL.h>
#include <libconfig.h++>
#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "GLES2/gl2.h"
#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "gearboy.h"
#include "audio/Sound_Queue.h"
#include "hwcomposer.h"

using namespace std;
using namespace libconfig;

bool running = true;
bool paused = false;


#define frame_width 160
#define frame_height 144

enum GC_Keys
{
    GC_A_Key = A_Key,
    GC_B_Key = B_Key,
    GC_Start_Key = Start_Key,
    GC_Select_Key = Select_Key,
    GC_Right_Key = Right_Key,
    GC_Left_Key = Left_Key,
    GC_Up_Key = Up_Key,
    GC_Down_Key = Down_Key,
    GC_Quit_Key = 10,
    GC_None_Key = 11,
};

static const char *output_file = "gearboy.cfg";

const float kGB_Width = 160.0f;
const float kGB_Height = 144.0f;
const float kGB_TexWidth = kGB_Width / 256.0f;
const float kGB_TexHeight = kGB_Height / 256.0f;
const GLfloat kQuadTex[8] = { 0, 0, kGB_TexWidth, 0, kGB_TexWidth, kGB_TexHeight, 0, kGB_TexHeight};
GLshort quadVerts[8];

GearboyCore* theGearboyCore;
Sound_Queue* theSoundQueue;
u16* theFrameBuffer;
GLuint theGBTexture;
int rate = 44100;
static uint16_t soundFinalWave[1600];
s16 theSampleBufffer[AUDIO_BUFFER_SIZE];

bool audioEnabled = true;

struct palette_color
{
    int red;
    int green;
    int blue;
    int alpha;
};

palette_color dmg_palette[4];

SDL_GameController* game_pad = NULL;
//SDL_Keycode kc_keypad_left, kc_keypad_right, kc_keypad_up, kc_keypad_down, kc_keypad_a, kc_keypad_b, kc_keypad_start, kc_keypad_select, kc_emulator_pause, kc_emulator_quit;
bool jg_x_axis_invert, jg_y_axis_invert;
int jg_a, jg_b, jg_start, jg_select, jg_x_axis, jg_y_axis;



static GLfloat proj[4][4];

#define	SHOW_ERROR		gles_show_error();

static void SetOrtho(GLfloat m[4][4], GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near, GLfloat far, GLfloat scale_x, GLfloat scale_y);

static const char* vertex_shader =
	"uniform mat4 u_vp_matrix;								\n"
	"attribute vec4 a_position;								\n"
	"attribute vec2 a_texcoord;								\n"
	"varying mediump vec2 v_texcoord;						\n"
	"void main()											\n"
	"{														\n"
	"	v_texcoord = a_texcoord;							\n"
	"	gl_Position = u_vp_matrix * a_position;				\n"
	"}														\n";

static const char* fragment_shader =
	"varying mediump vec2 v_texcoord;						\n"
	"uniform sampler2D u_texture;							\n"
	"void main()											\n"
	"{														\n"
	"	gl_FragColor = texture2D(u_texture, v_texcoord);	\n"
	"}														\n";

static const GLfloat vertices[] =
{
	-0.5f, -0.5f, 0.0f,
	-0.5f, +0.5f, 0.0f,
	+0.5f, +0.5f, 0.0f,
	+0.5f, -0.5f, 0.0f,
};

#define	TEX_WIDTH	1024
#define	TEX_HEIGHT	512

static GLfloat uvs[8];

static const GLushort indices[] =
{
	0, 1, 2,
	0, 2, 3,
};

static const int kVertexCount = 4;
static const int kIndexCount = 6;


void Create_uvs(GLfloat * matrix, GLfloat max_u, GLfloat max_v) {
    memset(matrix,0,sizeof(GLfloat)*8);
    matrix[3]=max_v;
    matrix[4]=max_u;
    matrix[5]=max_v;
    matrix[6]=max_u;

}

void gles_show_error()
{
	GLenum error = GL_NO_ERROR;
    error = glGetError();
    if (GL_NO_ERROR != error)
        printf("GL Error %x encountered!\n", error);
}

static GLuint CreateShader(GLenum type, const char *shader_src)
{
	GLuint shader = glCreateShader(type);
	if(!shader)
		return 0;

	// Load and compile the shader source
	glShaderSource(shader, 1, &shader_src, NULL);
	glCompileShader(shader);

	// Check the compile status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(!compiled)
	{
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(sizeof(char) * info_len);
			glGetShaderInfoLog(shader, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error compiling shader:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint CreateProgram(const char *vertex_shader_src, const char *fragment_shader_src)
{
	GLuint vertex_shader = CreateShader(GL_VERTEX_SHADER, vertex_shader_src);
	if(!vertex_shader)
		return 0;
	GLuint fragment_shader = CreateShader(GL_FRAGMENT_SHADER, fragment_shader_src);
	if(!fragment_shader)
	{
		glDeleteShader(vertex_shader);
		return 0;
	}

	GLuint program_object = glCreateProgram();
	if(!program_object)
		return 0;
	glAttachShader(program_object, vertex_shader);
	glAttachShader(program_object, fragment_shader);

	// Link the program
	glLinkProgram(program_object);

	// Check the link status
	GLint linked = 0;
	glGetProgramiv(program_object, GL_LINK_STATUS, &linked);
	if(!linked)
	{
		GLint info_len = 0;
		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(info_len);
			glGetProgramInfoLog(program_object, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error linking program:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteProgram(program_object);
		return 0;
	}
	// Delete these here because they are attached to the program object.
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return program_object;
}

typedef	struct ShaderInfo {
		GLuint program;
		GLint a_position;
		GLint a_texcoord;
		GLint u_vp_matrix;
		GLint u_texture;
} ShaderInfo;

static ShaderInfo shader;
static ShaderInfo shader_filtering;
static GLuint buffers[3];
static GLuint textures[2];

static void gles2_create()
{
	memset(&shader, 0, sizeof(ShaderInfo));
	shader.program = CreateProgram(vertex_shader, fragment_shader);
	if(shader.program)
	{
		shader.a_position	= glGetAttribLocation(shader.program,	"a_position");
		shader.a_texcoord	= glGetAttribLocation(shader.program,	"a_texcoord");
		shader.u_vp_matrix	= glGetUniformLocation(shader.program,	"u_vp_matrix");
		shader.u_texture	= glGetUniformLocation(shader.program,	"u_texture");
	}
	glGenTextures(1, textures);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);

	Create_uvs(uvs, (float)frame_width/TEX_WIDTH, (float)frame_height/TEX_HEIGHT);

	glGenBuffers(3, buffers);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 3, vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIndexCount * sizeof(GL_UNSIGNED_SHORT), indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_DITHER);
}

#define RGB15(r, g, b)  (((r) << (5+6)) | ((g) << 6) | (b))

static void gles2_Draw( uint16_t *pixels)
{
	if(!shader.program)
		return;

	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(shader.program);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GAMEBOY_WIDTH, GAMEBOY_HEIGHT, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, (GLvoid*) pixels);

	glActiveTexture(GL_TEXTURE0);
	glUniform1i(shader.u_texture, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glVertexAttribPointer(shader.a_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(shader.a_position);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glVertexAttribPointer(shader.a_texcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(shader.a_texcoord);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	glUniformMatrix4fv(shader.u_vp_matrix, 1, GL_FALSE, (const GLfloat * )&proj);
	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

u32 joy_map(u32 button)
{
  switch(button)
  {
    case SDL_CONTROLLER_BUTTON_START:
      return GC_Start_Key;
    case SDL_CONTROLLER_BUTTON_BACK:
    case SDL_CONTROLLER_BUTTON_GUIDE:
      return GC_Select_Key;
    case SDL_CONTROLLER_BUTTON_B:
      return GC_B_Key;
    case SDL_CONTROLLER_BUTTON_A:
      return GC_A_Key;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      return GC_Up_Key;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      return GC_Down_Key;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      return GC_Left_Key;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      return GC_Right_Key;
    case SDL_CONTROLLER_BUTTON_Y:
      return GC_Quit_Key;
    default:
      return GC_None_Key;
  }
}

void update(void)
{
    int sampleCount = 0;
    SDL_Event event;
    u32 button;

    while(SDL_PollEvent(&event))
    {
        switch(event.type) {
            case SDL_CONTROLLERAXISMOTION:
            if (event.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX) { //Left-Right
                if (event.caxis.value < -3200)  theGearboyCore->KeyPressed(Left_Key);
                else if (event.caxis.value > 3200)  theGearboyCore->KeyPressed(Right_Key);
                else {
                    theGearboyCore->KeyReleased(Left_Key);
                    theGearboyCore->KeyReleased(Right_Key);
                }
            }
            if (event.caxis.axis==SDL_CONTROLLER_AXIS_LEFTY) {  //Up-Down
                if (event.caxis.value < -3200) theGearboyCore->KeyPressed(Up_Key);
                else if (event.caxis.value > 3200)  theGearboyCore->KeyPressed(Down_Key);
                else {
                    theGearboyCore->KeyReleased(Up_Key);
                    theGearboyCore->KeyReleased(Down_Key);
                }
            }
            break;
            case SDL_CONTROLLERBUTTONDOWN:
            button = joy_map(event.cbutton.button);
            if (button == GC_Quit_Key) {
                running = false;
            } else if (button != GC_None_Key) {
                theGearboyCore->KeyPressed((Gameboy_Keys)button);
            }
            break;
            case SDL_CONTROLLERBUTTONUP:
            button = joy_map(event.cbutton.button);
            if (button != GC_None_Key && button != GC_Quit_Key) {
                theGearboyCore->KeyPressed((Gameboy_Keys)button);
            }
            theGearboyCore->KeyReleased((Gameboy_Keys)joy_map(event.cbutton.button));
            break;
            default:
            break;
        }
    }

    theGearboyCore->RunToVBlank(theFrameBuffer, theSampleBufffer, &sampleCount);

    if (audioEnabled && (sampleCount > 0))
    {
        //theSoundQueue->write(theSampleBufffer, sampleCount);
        int soundBufferLen = (rate / 60) * 4;

        // soundBufferLen should have a whole number of sample pairs
        assert(soundBufferLen % (2 * sizeof *soundFinalWave) == 0);

        // number of samples in output buffer
        int const out_buf_size = soundBufferLen / sizeof *soundFinalWave;

        int samplesRead = 0;
        while ((sampleCount - samplesRead) >= out_buf_size) {
            theSoundQueue->write((SoundDriver::sample_t*)&theSampleBufffer[samplesRead], soundBufferLen);
            samplesRead += out_buf_size;
        }
    }

    gles2_Draw((uint16_t*)theFrameBuffer);
	eglSwapBuffers(display, surface);
}


static void SetOrtho(GLfloat m[4][4], GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near, GLfloat far, GLfloat scale_x, GLfloat scale_y)
{
	memset(m, 0, 4*4*sizeof(GLfloat));
	m[0][0] = 2.0f/(right - left)*scale_x;
	m[1][1] = 2.0f/(top - bottom)*scale_y;
	m[2][2] = -2.0f/(far - near);
	m[3][0] = -(right + left)/(right - left);
	m[3][1] = -(top + bottom)/(top - bottom);
	m[3][2] = -(far + near)/(far - near);
	m[3][3] = 1;
}

void init_sdl(void)
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_NOPARACHUTE) < 0)
    {
        printf("SDL Error Init: %s", SDL_GetError());
    }

    /*theWindow = SDL_CreateWindow("Gearboy", 0, 0, 0, 0, 0);

    if (theWindow == NULL)
    {
        Log("SDL Error Video: %s", SDL_GetError());
    }*/

    SDL_ShowCursor(SDL_DISABLE);

    game_pad = SDL_GameControllerOpen(0);
    SDL_GameControllerEventState(SDL_ENABLE);

    if(game_pad == NULL)
    {
        printf("Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError());
    }
/*
    kc_keypad_left = SDLK_LEFT;
    kc_keypad_right = SDLK_RIGHT;
    kc_keypad_up = SDLK_UP;
    kc_keypad_down = SDLK_DOWN;
    kc_keypad_a = SDLK_a;
    kc_keypad_b = SDLK_s;
    kc_keypad_start = SDLK_RETURN;
    kc_keypad_select = SDLK_SPACE;
    kc_emulator_pause = SDLK_p;
    kc_emulator_quit = SDLK_ESCAPE;*/

    jg_x_axis_invert = false;
    jg_y_axis_invert = false;
    jg_a = 1;
    jg_b = 2;
    jg_start = 9;
    jg_select = 8;
    jg_x_axis = 0;
    jg_y_axis = 1;

    dmg_palette[0].red = 0xEF;
    dmg_palette[0].green = 0xF3;
    dmg_palette[0].blue = 0xD5;
    dmg_palette[0].alpha = 0xFF;

    dmg_palette[1].red = 0xA3;
    dmg_palette[1].green = 0xB6;
    dmg_palette[1].blue = 0x7A;
    dmg_palette[1].alpha = 0xFF;

    dmg_palette[2].red = 0x37;
    dmg_palette[2].green = 0x61;
    dmg_palette[2].blue = 0x3B;
    dmg_palette[2].alpha = 0xFF;

    dmg_palette[3].red = 0x04;
    dmg_palette[3].green = 0x1C;
    dmg_palette[3].blue = 0x16;
    dmg_palette[3].alpha = 0xFF;

}

void init_ogl(void)
{
    int32_t success = 0;

    hwcomposer_init();
    gles2_create();

	int rr=(screen_height/frame_height);
	int h = (frame_height*rr);
	int w = (frame_width*rr);
	if (w>screen_width) {
	    rr = (screen_width/frame_width);
	    h = (frame_height*rr);
	    w = (frame_width*rr);
	}
	glViewport((screen_width-w)/2, (screen_height-h)/2, w, h);
	SetOrtho(proj, -0.5f, +0.5f, +0.5f, -0.5f, -1.0f, 1.0f, 1.0f ,1.0f );

    glClear(GL_COLOR_BUFFER_BIT);
}

void init(void)
{
    init_ogl();
    init_sdl();

    theGearboyCore = new GearboyCore();
    theGearboyCore->Init();
    theGearboyCore->SetSoundSampleRate(rate);

    theSoundQueue = new Sound_Queue();
    theSoundQueue->start(rate, 2);

    theFrameBuffer = new u16[GAMEBOY_WIDTH * GAMEBOY_HEIGHT];

    for (int i = 0; i < (GAMEBOY_WIDTH * GAMEBOY_HEIGHT); ++i)
    {
        theFrameBuffer[i] = 0;
    }
}

void end(void)
{
    Config cfg;

    Setting &root = cfg.getRoot();
    Setting &address = root.add(GEARBOY_TITLE, Setting::TypeGroup);
/*
    address.add("keypad_left", Setting::TypeString) = SDL_GetKeyName(kc_keypad_left);
    address.add("keypad_right", Setting::TypeString) = SDL_GetKeyName(kc_keypad_right);
    address.add("keypad_up", Setting::TypeString) = SDL_GetKeyName(kc_keypad_up);
    address.add("keypad_down", Setting::TypeString) = SDL_GetKeyName(kc_keypad_down);
    address.add("keypad_a", Setting::TypeString) = SDL_GetKeyName(kc_keypad_a);
    address.add("keypad_b", Setting::TypeString) = SDL_GetKeyName(kc_keypad_b);
    address.add("keypad_start", Setting::TypeString) = SDL_GetKeyName(kc_keypad_start);
    address.add("keypad_select", Setting::TypeString) = SDL_GetKeyName(kc_keypad_select);

    address.add("joystick_gamepad_a", Setting::TypeInt) = jg_a;
    address.add("joystick_gamepad_b", Setting::TypeInt) = jg_b;
    address.add("joystick_gamepad_start", Setting::TypeInt) = jg_start;
    address.add("joystick_gamepad_select", Setting::TypeInt) = jg_select;
    address.add("joystick_gamepad_x_axis", Setting::TypeInt) = jg_x_axis;
    address.add("joystick_gamepad_y_axis", Setting::TypeInt) = jg_y_axis;
    address.add("joystick_gamepad_x_axis_invert", Setting::TypeBoolean) = jg_x_axis_invert;
    address.add("joystick_gamepad_y_axis_invert", Setting::TypeBoolean) = jg_y_axis_invert;

    address.add("emulator_pause", Setting::TypeString) = SDL_GetKeyName(kc_emulator_pause);
    address.add("emulator_quit", Setting::TypeString) = SDL_GetKeyName(kc_emulator_quit);
*/
    address.add("emulator_pal0_red", Setting::TypeInt) = dmg_palette[0].red;
    address.add("emulator_pal0_green", Setting::TypeInt) = dmg_palette[0].green;
    address.add("emulator_pal0_blue", Setting::TypeInt) = dmg_palette[0].blue;
    address.add("emulator_pal1_red", Setting::TypeInt) = dmg_palette[1].red;
    address.add("emulator_pal1_green", Setting::TypeInt) = dmg_palette[1].green;
    address.add("emulator_pal1_blue", Setting::TypeInt) = dmg_palette[1].blue;
    address.add("emulator_pal2_red", Setting::TypeInt) = dmg_palette[2].red;
    address.add("emulator_pal2_green", Setting::TypeInt) = dmg_palette[2].green;
    address.add("emulator_pal2_blue", Setting::TypeInt) = dmg_palette[2].blue;
    address.add("emulator_pal3_red", Setting::TypeInt) = dmg_palette[3].red;
    address.add("emulator_pal3_green", Setting::TypeInt) = dmg_palette[3].green;
    address.add("emulator_pal3_blue", Setting::TypeInt) = dmg_palette[3].blue;

    try
    {
        cfg.writeFile(output_file);
    }
    catch(const FileIOException &fioex)
    {
        Log("I/O error while writing file: %s", output_file);
    }

    SDL_GameControllerClose(game_pad);

    SafeDeleteArray(theFrameBuffer);
    SafeDelete(theSoundQueue);
    SafeDelete(theGearboyCore);
    SDL_Quit();
}

int main(int argc, char** argv)
{
    char load_filename[512];
    bool forcedmg = false;
    init();

    sprintf(load_filename, getenv("GAME_PATH"));
    if (load_filename[0] == '\0')
    {
        if (argc < 2 || argc > 4)
        {
            end();
            printf("usage: %s rom_path [options]\n", argv[0]);
            printf("options:\n-nosound\n-forcedmg\n");
            return -1;
        }


        if (argc > 2)
        {
            for (int i = 2; i < argc; i++)
            {
                if (strcmp("-nosound", argv[i]) == 0)
                    audioEnabled = false;
                else if (strcmp("-forcedmg", argv[i]) == 0)
                    forcedmg = true;
                else
                {
                    end();
                    printf("invalid option: %s\n", argv[i]);
                    return -1;
                }
            }
        }
        sprintf(load_filename, argv[1]);
    }

    if (theGearboyCore->LoadROM(load_filename, forcedmg))
    {
        GB_Color color1;
        GB_Color color2;
        GB_Color color3;
        GB_Color color4;

        color1.red = dmg_palette[0].red;
        color1.green = dmg_palette[0].green;
        color1.blue = dmg_palette[0].blue;
        color2.red = dmg_palette[1].red;
        color2.green = dmg_palette[1].green;
        color2.blue = dmg_palette[1].blue;
        color3.red = dmg_palette[2].red;
        color3.green = dmg_palette[2].green;
        color3.blue = dmg_palette[2].blue;
        color4.red = dmg_palette[3].red;
        color4.green = dmg_palette[3].green;
        color4.blue = dmg_palette[3].blue;

        theGearboyCore->SetDMGPalette(color1, color2, color3, color4);
        theGearboyCore->LoadRam();

        while (running)
        {
            update();
        }

        theGearboyCore->SaveRam();
    }

    end();

    return 0;
}
