/**********
** Author:		Tyler Betley
** Description: Space Invaders type game using OpenGL, GLEW, GLFW APIs.
** Date:		August/September 2019
** References:	OpenGL Series by The Cherno (Yan Chernikov)
**********/

#include <cstdio>
#include <iostream>
#include <string>
#include <Windows.h>
#include <cstdint>
#include <stdlib.h>
#include <glew.h>
#include <glfw3.h>
#define GAME_MAX_ROCKETS 128

// events in GLFW are reported through a callback. 
// a callback is a function that is passed to another API - in GLFW it expects: typedef void(*GLFWerrorFun)(int, const char*)
void error_callback(int error, const char* description)
{
	// prints error to an output file
	fprintf(stderr, "Error: %s\n", description);
}

// variable for game_running condition, true if running
bool game_running = false;

// initial player movement value
int move_dir = 0;

// add rocket fired value
bool rocket_fired = false;

/**********
** SWITCH: handles player key input using a callback
**********/
// callback to exit game if esc key is pressed
// key: key pressed; scancode: system specific code for key, not needed; action: bool for pressed state; mods: modifier like ctr-alt 
void key_callback(GLFWwindow*, int key, int scancode, int action, int mods)
{
	switch (key)
	{
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS)
			game_running = false;
		break;
	case GLFW_KEY_RIGHT:
		if (action == GLFW_PRESS)
			move_dir += 1;
		else if (action == GLFW_RELEASE)
			move_dir -= 1;
		break;
	case GLFW_KEY_LEFT:
		if (action == GLFW_PRESS)
			move_dir -= 1;
		else if (action == GLFW_RELEASE)
			move_dir += 1;
		break;
	case GLFW_KEY_SPACE:
		if (action == GLFW_PRESS)
			rocket_fired = true;
		break;
	default:
		break;
	}
}

/**********
** Two following functions validate and catch errors during compilation of shaders to the GPU
***********/
// validate shader function
void validate_shader(GLuint shader, const char* file = 0)
{
	static const unsigned int BUFFER_SIZE = 512;
	char buffer[BUFFER_SIZE];
	GLsizei length = 0;

	glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

	if (length > 0)
	{
		printf("Shader %d(%s) compile error: %s\n", shader, (file ? file : ""), buffer);
	}
}
// validate program function
bool validate_program(GLuint program)
{
	static const GLsizei BUFFER_SIZE = 512;
	GLchar buffer[BUFFER_SIZE];
	GLsizei length = 0;

	glGetProgramInfoLog(program, BUFFER_SIZE, &length, buffer);

	if (length > 0)
	{
		printf("Program %d link error: %s\n", program, buffer);
		return false;
	}

	return true;
}


// buffer structure that can be passed to the gpu as a texture
struct Buffer
{
	unsigned int width, height;

	// pointer to an array of data of type uint32 - unsigned integer type with width of 32 bits
	uint32_t* data;
};

// function to convert rgb color input into uinu_32 values, or RGBA value
uint32_t rgb_to_uint32(uint8_t r, uint8_t g, uint8_t b)
{
	// convert using bitwise left shift operator, creating 4 "channels" each with 8 bits for color. last bit is not used - the alpha channel
	return (r << 24) | (g << 16) | (b << 8) | 225;
}

// function to clear the buffer to a specific color
void clear_buffer(Buffer* buffer, uint32_t color)
{
	// iterate through the entire data set of the buffer (width*height)
	for (unsigned int i = 0; i < buffer->width * buffer->height; i++)
	{
		// assign each data point in the buffer as a specific color
		buffer->data[i] = color;
	}
}

// sprite struct to hold sprite data
struct Sprite
{
	unsigned int width, height;
	uint8_t* data;
};

// function to check if sprites are overlapped or register a hit: hit if true
bool sprite_overlap_check(
	const Sprite& sp_a, unsigned int x_a, unsigned int y_a,
	const Sprite& sp_b, unsigned int x_b, unsigned int y_b)
{
	// Just check overlap of the sprite rectangles. Instead, if the rectangles overlap, we should
	// further check if any pixel of sprite A overlap with any of sprite B.
	if (x_a < x_b + sp_b.width && x_a + sp_a.width > x_b &&
		y_a < y_b + sp_b.height && y_a + sp_a.height > y_b)
	{
		return true;
	}

	return false;
}

// function to draw sprite, draw pixels that are on (have a value of 1)
void DrawSprite(Buffer* buffer, const Sprite& sprite, unsigned int x, unsigned int y, uint32_t color)
{
	for(unsigned int i = 0; i < sprite.width; ++i)
	{
		for (unsigned int j = 0; j < sprite.height; ++j)
		{
			unsigned int yPos = sprite.height - 1 + y - j;
			unsigned int xPos = x + i;
			if (sprite.data[j * sprite.width + i] && yPos < buffer->height && xPos < buffer->width)
			{
				buffer->data[yPos * buffer->width + xPos] = color;
			}
		}
	}
}

// function to draw text on the screen
void DrawBufferText(Buffer* buffer, const Sprite& text_spritesheet, const char* text, unsigned int x, unsigned int y, uint32_t color)
{
	// stride is the size of the character sprite
	unsigned int stride = text_spritesheet.height * text_spritesheet.width;
	unsigned int xp = x;
	Sprite sprite = text_spritesheet;

	// iterate until you reach the null terminator
	for (const char* char_pos = text; *char_pos != '\0'; char_pos++)
	{
		char character = *char_pos - 32; // assists in indexing the sprite sheet
		if (character < 0 || character >= 65)
			continue;

		// index on the correct location of the sprite sheet and enter data into the sprite
		sprite.data = text_spritesheet.data + character * stride;

		// draw to buffer
		DrawSprite(buffer, sprite, xp, y, color);
		xp += sprite.width + 1;
	}
}

// type
enum AlienType : uint8_t
{
	ALIEN_DEAD = 0,
	ALIEN_TYPE_A = 1,
	ALIEN_TYPE_B = 2
};

// struct to define alien types and states
struct Alien
{
	unsigned int x, y;
	uint8_t type;
};

// struct to define player interactions
struct Player
{
	unsigned int x, y;
	unsigned int lives;
};

// struct to define rocket interactions
struct Rocket
{
	// position and direction of fire/speed
	unsigned int x, y;
	int dir;
};

// struct to define game parameters and states
struct Game
{
	unsigned int height, width;
	unsigned int alien_hoard_size;
	unsigned int num_rockets;
	unsigned int frame_counter;
	Alien* alien_array; // this will be a dynamically allocated array to store alien location and type data
	Alien* first_line_array; // array holding the front facing aliens
	Player player;
	Rocket rockets[GAME_MAX_ROCKETS];
};

// animation struct
struct SpriteAnimation
{
	// flag: indicated whether to loop over animation or play it once
	bool loop;
	unsigned int num_frames; 
	unsigned int frame_duration; // time spent in each animation
	unsigned int time; // time between frames
	// pointer to pointer for sprite storage so that sprites can be shared
	Sprite** frames;
};


/********
Main function
*********/
int main()
{
	// callback functions must be set, and can be done before initialization
	glfwSetErrorCallback(error_callback);

	// now initialize glfw library, glfwInit returns false if it fails
	if (!glfwInit())
		return -1;

	// give GLFW hints before creating the window
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	// initialize pointer to window object
	GLFWwindow* window;
	window = glfwCreateWindow(640, 640, "Alien Invaders", NULL, NULL);

	// handle window creation error
	if (!window)
	{
		// terminate glfw instance
		glfwTerminate();
		// return error code
		return -1;
	}

	// set key callback
	glfwSetKeyCallback(window, key_callback);

	// before I can use the OpenGL API, we must have a current OpenGL context
	glfwMakeContextCurrent(window);

	// initialize GLEW - a loading library, needed because openGL requires funcitons to be declared and loaded explicitly at runtime
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		//print error, terminate glfw instance
		fprintf(stderr, "Error initializing GLEW.\n");
		glfwTerminate();
		return -1;
	}

	// now we can make calls to openGL functions
	// check openGL version
	int glVersion[2] = { -1, 1 };
	glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
	glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);

	printf("Using OpenGL Version: %d.%d\n", glVersion[0], glVersion[1]);

	// turn on V-Sync 60Hz
	glfwSwapInterval(1);

	// color the backgroud red
	glClearColor(1.0, 0.0, 0.0, 1.0);



	/**********
	** Initialize the buffer data
	**********/
	// initialize buffer size
	const unsigned int buffer_width = 300; // 224
	const unsigned int buffer_height = 400; // 256

	// create clear color set to green
	uint32_t backgroundColor = rgb_to_uint32(255, 255, 255);
	
	// initialize buffer
	Buffer buffer;
	buffer.width = buffer_width;
	buffer.height = buffer_height;
	buffer.data = new uint32_t[buffer.height * buffer.width]; //****** clear this memory later *******// 
	
	// set buffer background color
	clear_buffer(&buffer, backgroundColor);  // or clear color



	/**********
	** BUFFER TEXTURE - OpenGL texture that transfers image data to the GPU
	**********/
	// generate a texture
	GLuint buffer_texture;
	// generate one texture of the specified array in which the texture names are stored
	glGenTextures(1, &buffer_texture);

	// image format and parameters
	// binding selects the current thing you are working on 
	glBindTexture(GL_TEXTURE_2D, buffer_texture);
	// texture attributes/ send data to GPU
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGB8,
		buffer.width, buffer.height, 0,
		GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data
	);

	// texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);



	// create vertex array object
	GLuint fullscreen_triangle_vao;
	glGenVertexArrays(1, &fullscreen_triangle_vao);
	


	/*********
	** SHADERS - Vertex and Fragment Shaders
	*********/
	// create the vertex shader
	static const char* vertex_shader =
		"\n"
		"#version 330\n" // using version 3.3
		"\n"
		"noperspective out vec2 TexCoord;\n" 
		"\n"
		"void main(void){\n"
		"\n"
		"	TexCoord.x = (gl_VertexID == 2)? 2.0: 0.0;\n"
		"	TexCoord.y = (gl_VertexID == 1)? 2.0: 0.0;\n"
		"	\n"
		"	gl_Position = vec4(2.0 * TexCoord - 1.0, 0.0, 1.0);\n"
		"}\n";

	// create fragment/pixel shader - colors each pixel
	static const char* fragment_shader =
		"\n"
		"#version 330\n"
		"\n"
		"uniform sampler2D buffer;\n"
		"noperspective in vec2 TexCoord;\n"
		"\n"
		"out vec3 outColor;\n"
		"\n"
		"void main(void){\n"
		"    outColor = texture(buffer, TexCoord).rgb;\n"
		"}\n";

	// create shaders - compiled into GPU code and linked into shader program
	GLuint shader_id = glCreateProgram();

	// vertex shader
	{
		GLuint shader_vp = glCreateShader(GL_VERTEX_SHADER);

		glShaderSource(shader_vp, 1, &vertex_shader, 0);
		glCompileShader(shader_vp);
		validate_shader(shader_vp, vertex_shader);
		glAttachShader(shader_id, shader_vp);

		glDeleteShader(shader_vp);
	}

	// fragment/pixel shader
	{
		GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(shader_fp, 1, &fragment_shader, 0);
		glCompileShader(shader_fp);
		validate_shader(shader_fp, fragment_shader);
		glAttachShader(shader_id, shader_fp);

		glDeleteShader(shader_fp);
	}

	// link program
	glLinkProgram(shader_id);

	// test linked program
	if (!validate_program(shader_id))
	{
		fprintf(stderr, "Error while validating shader.\n");
		glfwTerminate();
		glDeleteVertexArrays(1, &fullscreen_triangle_vao);
		delete[] buffer.data;
		return -1;
	}

	// use the program
	glUseProgram(shader_id);

	GLint location = glGetUniformLocation(shader_id, "buffer");
	glUniform1i(location, 0);


	// openGL setup
	glDisable(GL_DEPTH_TEST);
	glBindVertexArray(fullscreen_triangle_vao);

	/********
	Game and Sprite data
	********/
	// create alien sprites
	Sprite alien_sprites[4];

	// alien type 1 animation 1
	alien_sprites[0].width = 11;
	alien_sprites[0].height = 8;
	alien_sprites[0].data = new uint8_t[11 * 8]
	{
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		0,0,0,1,0,0,0,1,0,0,0, // ...@...@...
		0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
		0,1,1,0,1,1,1,0,1,1,0, // .@@.@@@.@@.
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
		1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
		1,0,1,0,0,0,0,0,1,0,1, // @.@.....@.@
		0,0,0,1,1,0,1,1,0,0,0  // ...@@.@@...
	};

	// alien type 1 animation 2
	alien_sprites[1].width = 11;
	alien_sprites[1].height = 8;
	alien_sprites[1].data = new uint8_t[11 * 8]
	{
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		1,0,0,1,0,0,0,1,0,0,1, // @..@...@..@
		1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
		1,1,1,0,1,1,1,0,1,1,1, // @@@.@@@.@@@
		1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
		0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
		0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
		0,1,0,0,0,0,0,0,0,1,0  // .@.......@.
	};

	// alien type 2 animation 1
	alien_sprites[2].width = 8;
	alien_sprites[2].height = 8;
	alien_sprites[2].data = new uint8_t[8 * 8]
	{
		0,0,0,1,1,0,0,0, // ...@@...
		0,0,1,1,1,1,0,0, // ..@@@@..
		0,1,1,1,1,1,1,0, // .@@@@@@.
		1,1,0,1,1,0,1,1, // @@.@@.@@
		1,1,1,1,1,1,1,1, // @@@@@@@@
		0,1,0,1,1,0,1,0, // .@.@@.@.
		1,0,0,0,0,0,0,1, // @......@
		0,1,0,0,0,0,1,0  // .@....@.
	};

	//alien type 2 animation 2
	alien_sprites[3].width = 8;
	alien_sprites[3].height = 8;
	alien_sprites[3].data = new uint8_t[8 * 8]
	{
		0,0,0,1,1,0,0,0, // ...@@...
		0,0,1,1,1,1,0,0, // ..@@@@..
		0,1,1,1,1,1,1,0, // .@@@@@@.
		1,1,0,1,1,0,1,1, // @@.@@.@@
		1,1,1,1,1,1,1,1, // @@@@@@@@
		0,0,1,0,0,1,0,0, // ..@..@..
		0,1,0,1,1,0,1,0, // .@.@@.@.
		1,0,1,0,0,1,0,1  // @.@..@.@
	};

	// alien death sprite
	Sprite alien_death_sprite;
	alien_death_sprite.width = 13;
	alien_death_sprite.height = 7;
	alien_death_sprite.data = new uint8_t[13 * 7]
	{
		0,1,0,0,1,0,0,0,1,0,0,1,0, // .@..@...@..@.
		0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
		0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
		1,1,0,0,0,0,0,0,0,0,0,1,1, // @@.........@@
		0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
		0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
		0,1,0,0,1,0,0,0,1,0,0,1,0  // .@..@...@..@.
	};

	// create player sprite
	Sprite playerSprite;
	playerSprite.height = 10;
	playerSprite.width = 10;
	playerSprite.data = new uint8_t[10 * 10]
	{
		0,0,1,0,0,0,0,1,0,0,
		0,0,1,0,0,0,0,1,0,0,
		0,1,1,1,0,0,1,1,1,0,
		0,1,1,1,0,0,1,1,1,0,
		0,1,1,1,0,0,1,1,1,0,
		0,1,1,1,1,1,1,1,1,0,
		0,1,1,1,1,1,1,1,1,0,
		0,1,1,1,1,1,1,1,1,0,
		0,1,1,1,1,1,1,1,1,0,
		1,1,1,1,1,1,1,1,1,1
	};

	// create text spritesheet, follows ASCII
	Sprite text_spritesheet;
	text_spritesheet.width = 5;
	text_spritesheet.height = 7;
	text_spritesheet.data = new uint8_t[65 * 35]
	{	// others
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
		0,1,0,1,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,1,0,1,0,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,0,1,0,1,0,
		0,0,1,0,0,0,1,1,1,0,1,0,1,0,0,0,1,1,1,0,0,0,1,0,1,0,1,1,1,0,0,0,1,0,0,
		1,1,0,1,0,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,0,1,0,1,1,
		0,1,1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,1,0,0,1,0,0,1,0,1,0,0,0,1,0,1,1,1,1,
		0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
		1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
		0,0,1,0,0,1,0,1,0,1,0,1,1,1,0,0,0,1,0,0,0,1,1,1,0,1,0,1,0,1,0,0,1,0,0,
		0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
		0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,
		// numbers
		0,1,1,1,0,1,0,0,0,1,1,0,0,1,1,1,0,1,0,1,1,1,0,0,1,1,0,0,0,1,0,1,1,1,0,
		0,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
		0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,
		1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		0,0,0,1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,0,1,1,1,1,1,0,0,0,1,0,0,0,0,1,0,
		1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,
		0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		//others
		0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
		0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
		0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
		1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
		0,1,1,1,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
		0,1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,1,0,1,1,1,0,1,0,0,1,0,0,0,1,0,1,1,1,0,
		// letters
		0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,
		1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
		0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,1,0,
		1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
		1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
		1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
		0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,1,1,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
		0,1,1,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
		0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,1,0,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
		1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
		1,0,0,0,1,1,1,0,1,1,1,0,1,0,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
		1,0,0,0,1,1,0,0,0,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,1,0,0,0,1,1,0,0,0,1,
		0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
		0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,0,1,1,0,1,1,1,1,
		1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
		0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,1,1,1,0,
		1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
		1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
		1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,
		1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,1,0,1,1,1,0,1,1,1,0,0,0,1,
		1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,
		1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
		1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,
		// others
		0,0,0,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,1,
		0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,
		1,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,0,
		0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
		0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	};

	// create rocket sprite
	Sprite rocket_sprite;
	rocket_sprite.height = 5;
	rocket_sprite.width = 2;
	rocket_sprite.data = new uint8_t[5 * 2]
	{
		1,1,
		1,1,
		1,1,
		1,1,
		1,1
	};

	// initialize game struct
	Game game;
	game.width = buffer_width;
	game.height = buffer_height;
	game.alien_hoard_size = 50;
	// create array of Aliens the size of the hoard
	game.alien_array = new Alien[game.alien_hoard_size]; // delete allocation later
	// initialize player information embedded in Game instance
	game.player.lives = 3;
	game.player.x = 150;
	game.player.y = 25;
	// set number of rockets
	game.num_rockets = 0;


	// create an array to keep track of alien deaths, each frame the death will be decremented by one
	uint8_t* death_counter = new uint8_t[game.alien_hoard_size];
	for (unsigned int i = 0; i < game.alien_hoard_size; i++)
	{
		death_counter[i] = 10;
	}

	// create sprite animation for alien animations
	SpriteAnimation alien_animation[3];

	// iterate over the alien animations and assign frames to each animation for each alien type
	for (unsigned int i = 0; i < 2; i++)
	{
		alien_animation[i].loop = true;
		alien_animation[i].num_frames = 2;
		alien_animation[i].frame_duration = 10;
		alien_animation[i].time = 0;

		alien_animation[i].frames = new Sprite * [2];
		alien_animation[i].frames[0] = &alien_sprites[2 * i];
		alien_animation[i].frames[1] = &alien_sprites[2 * i + 1];
	}

	// initialize alien hoard positions - assign positions to Alien array
	for (unsigned int j = 0; j < 5; j++) // iterate over y position
	{
		for (unsigned int i = 0; i < 10; i++) // iterate over x position
		{
			if (j * 10 + 1 < 20)
			{
				game.alien_array[j * 10 + i].type = 1;
			}
			else
			{
				game.alien_array[j * 10 + i].type = 2;
			}
			
			game.alien_array[j * 10 + i].x = 30 + i * (11 + 15);
			game.alien_array[j * 10 + i].y = 250 + j * (8 + 15);
		}
	}

	// initialize first line array
	game.first_line_array = new Alien[10];
	game.frame_counter = 0;

	// initialize score and start game
	unsigned int score = 0;
	game_running = true;
	int aliens_killed = 0;

	/******************** 
	GAME LOOP
	********************/
	// while the window has not been closed, execute the following...
	while (!glfwWindowShouldClose(window) && game_running)
	{
		clear_buffer(&buffer, backgroundColor); // need to clear the buffer completely before you can start drawing again

		glClear(GL_COLOR_BUFFER_BIT); // ???

		/*********
		** DRAW SPRITES
		**********/
		// draw score counter
		DrawBufferText(&buffer, text_spritesheet, "SCORE", 5, 375, rgb_to_uint32(0, 128, 0));

		// convert score int to string and draw score
		std::string score_string = std::to_string(score);
		char const *score_array = score_string.c_str();
		DrawBufferText(&buffer, text_spritesheet, score_array, 50, 375, rgb_to_uint32(0, 128, 0));

		// draw title on screen 
		DrawBufferText(&buffer, text_spritesheet, "ALIEN INVADERS", 110, 380, rgb_to_uint32(0, 128, 0));

		// draw alien sprite array to specified buffer
		for (unsigned int i = 0; i < game.alien_hoard_size; i++)
		{
			// if alien is dead move on to next one
			if (!death_counter[i])
				continue;
			// if alien is dead type, draw death animation
			if (game.alien_array[i].type == ALIEN_DEAD)
			{
				DrawSprite(&buffer, alien_death_sprite, game.alien_array[i].x, game.alien_array[i].y, rgb_to_uint32(128, 0, 0));
			}
			// else draw the alien type
			else
			{
				const SpriteAnimation& animation = alien_animation[game.alien_array[i].type - 1];
				unsigned int current_frame = animation.time / animation.frame_duration;
				const Sprite& sprite = *animation.frames[current_frame];
				DrawSprite(&buffer, sprite, game.alien_array[i].x, game.alien_array[i].y, rgb_to_uint32(128, 0, 0));
			}

		}

		// draw player sprite
		DrawSprite(&buffer, playerSprite, game.player.x, game.player.y, rgb_to_uint32(0, 0, 128));

		// draw rockets on screen
		for (unsigned int i = 0; i < game.num_rockets; i++)
		{
			DrawSprite(&buffer, rocket_sprite, game.rockets[i].x, game.rockets[i].y, rgb_to_uint32(0, 0, 128));
		}
		

		// update animation timing
		for (unsigned int i = 0; i < 2; i++)
		{
			alien_animation[i].time++;
			if (alien_animation[i].time == alien_animation[i].num_frames * alien_animation[i].frame_duration)
			{
				alien_animation[i].time = 0;  // reset time
			}
		}


		// update the texture buffer
		glTexSubImage2D(
			GL_TEXTURE_2D, 0, 0, 0,
			buffer.width, buffer.height,
			GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
			buffer.data
		);

		// works on the currently bound object
		glDrawArrays(GL_TRIANGLES, 0, 3);

		// swap buffers
		glfwSwapBuffers(window);

		/**********
		** Player inputs/projectiles
		**********/

		// handle alien deaths
		for (unsigned int i = 0; i < game.alien_hoard_size; i++)
		{
			// death counter slows down the death, making sure each death animation lasts for 10 frames
			if (game.alien_array[i].type == ALIEN_DEAD && death_counter[i])
			{
				death_counter[i]--;
			}
		}


		// rocket conditions
		for (unsigned int i = 0; i < game.num_rockets; i++)
		{
			game.rockets[i].y += game.rockets[i].dir;
			if (game.rockets[i].y >= game.height || game.rockets[i].y < rocket_sprite.height)
			{
				// overwrite the rocket with the last rocket in the array
				game.rockets[i] = game.rockets[game.num_rockets - 1];
				game.num_rockets--;
				continue;
			}

			// check for a hit on a live alien
			for (unsigned int j = 0; j < game.alien_hoard_size; j++)
			{
				// check if current alien is type dead
				if (game.alien_array[j].type == ALIEN_DEAD)
					continue;
				
				const SpriteAnimation& animation = alien_animation[game.alien_array[j].type - 1];
				unsigned int current_frame = animation.time / animation.frame_duration;
				const Sprite& alien_sprite = *animation.frames[current_frame];
				bool overlap = sprite_overlap_check(rocket_sprite, game.rockets[i].x, game.rockets[i].y, alien_sprite, game.alien_array[j].x, game.alien_array[j].y);

				if (overlap)
				{
					// update score
					score += 10 * (4 - game.alien_array[j].type);
					// update kill count
					aliens_killed++;
					// update alien type
					game.alien_array[j].type = ALIEN_DEAD;
					// recenter death sprite
					game.alien_array[j].x -= (alien_death_sprite.width - alien_sprite.width) / 2;
					// remove rocket
					game.rockets[i] = game.rockets[game.num_rockets - 1];
					game.num_rockets--;
					continue;
				}
			}

			// check for hit on player
			bool overlap = sprite_overlap_check(rocket_sprite, game.rockets[i].x, game.rockets[i].y, playerSprite, game.player.x, game.player.y);
			if (overlap)
			{
				// decrement player lives
				game.player.lives--;

				// if lose
				if (game.player.lives == 0)
				{
					// print out lose screen 
					// clear buffer
					clear_buffer(&buffer, backgroundColor); 
					glClear(GL_COLOR_BUFFER_BIT);

					// draw lose screen
					DrawBufferText(&buffer, text_spritesheet, "YOU LOSE", 120, 200, rgb_to_uint32(128, 0, 0));

					// update the texture buffer
					glTexSubImage2D(
						GL_TEXTURE_2D, 0, 0, 0,
						buffer.width, buffer.height,
						GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
						buffer.data
					);

					// works on the currently bound object
					glDrawArrays(GL_TRIANGLES, 0, 3);

					// swap buffers
					glfwSwapBuffers(window);

					Sleep(3000);
					
					return 0;
				}

				// reset player location
				game.player.x = 150;
				game.player.y = 25;

			}
		}

		// check win condition
		if (aliens_killed == 50)
		{
			// print out win screen 
			// clear buffer
			clear_buffer(&buffer, backgroundColor);
			glClear(GL_COLOR_BUFFER_BIT);

			// draw win screen
			DrawBufferText(&buffer, text_spritesheet, "YOU WIN", 120, 200, rgb_to_uint32(0, 128, 0));

			// update the texture buffer
			glTexSubImage2D(
				GL_TEXTURE_2D, 0, 0, 0,
				buffer.width, buffer.height,
				GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
				buffer.data
			);

			// works on the currently bound object
			glDrawArrays(GL_TRIANGLES, 0, 3);

			// swap buffers
			glfwSwapBuffers(window);

			Sleep(3000);

			return 0;
		}


		// player left/right movement
		// scaling movement speed
		int player_move_dir = 2 * move_dir;

		// boundary check and update location
		if (player_move_dir != 0)
		{
			// right boundary condition
			if (game.player.x + playerSprite.width + player_move_dir >= game.width)
			{
				game.player.x = game.width - playerSprite.width;
			}
			else if ((int)game.player.x + player_move_dir <= 0)
			{
				game.player.x = 0;
			}
			else
			{
				game.player.x += player_move_dir;
			}
		}

		// update first line array
		// iterate through first line array
		for (int i = 0; i < 10; i++)
		{
			// check if each index is an alive alien, if so update to first line
			if (game.alien_array[i].type != ALIEN_DEAD)
			{
				game.first_line_array[i] = game.alien_array[i];
			}
			// else if the first alien is dead, jump to the next level and check if its alive
			else
			{
				bool isAlien = false;
				// iterate through the tiers
				for (int j = 1; j < 5; j++)
				{
					// if the alien is not dead on the next level
					if (game.alien_array[i + (10 * j)].type != ALIEN_DEAD)
					{
						// update it to the first line array
						game.first_line_array[i] = game.alien_array[i + (10 * j)];
						isAlien = true;
						break;
					}
				}
				// if there was no alien updated into the array
				if (isAlien == false)
				{
					// make the last alien the one on the line, handle its death case later
					game.first_line_array[i] = game.alien_array[i + 40];
				}
			}
		}

		// use first line array to randomly fire rockets from aliens on that first line
		game.frame_counter++;
		if (game.frame_counter == 30 && game.num_rockets < GAME_MAX_ROCKETS)
		{
			// select random alien on first line
			int rand = std::rand() % 10;

			// fire rocket from that alien in opposite dir
			if (game.first_line_array[rand].type != ALIEN_DEAD)
			{
				game.rockets[game.num_rockets].x = game.first_line_array[rand].x + 4;
				game.rockets[game.num_rockets].y = game.first_line_array[rand].y - 10;
				game.rockets[game.num_rockets].dir = -3;
				game.num_rockets++;
			}
		
			game.frame_counter = 0;
		}


		// fire rocket if space is pressed
		if (rocket_fired == true && game.num_rockets < GAME_MAX_ROCKETS)
		{
			game.rockets[game.num_rockets].x = game.player.x + (playerSprite.width / 2);
			game.rockets[game.num_rockets].y = game.player.y + playerSprite.height;
			// set rocket direction and speed
			game.rockets[game.num_rockets].dir = 3;
			game.num_rockets++;
		}
		rocket_fired = false;

		//std::cout << "Num Rockets " << game.num_rockets << std::endl;

		// handle poll events
		glfwPollEvents();
	}

	glDeleteProgram(shader_id);

	// destroy window
	glfwDestroyWindow(window);

	// terminate the glfw instance
	glfwTerminate();


	// delete allocated memory
	for (unsigned int i = 0; i < 3; i++)
	{
		delete[] alien_sprites[i].data;
	}
	delete[] alien_death_sprite.data;
	delete[] playerSprite.data;
	delete[] rocket_sprite.data;
	delete[] buffer.data;
	delete[] death_counter;
	for (unsigned int i = 0; i < 2; i++)
	{
		delete[] alien_animation[i].frames;
	}
	delete[] game.alien_array;
	delete[] game.first_line_array;
	

	return 0;
}