# Alien-Invaders
A recreation of the classic Space Invaders game using C++ and OpenGL for graphics.

## Learning Outcomes
1. Become more proficient in C++ and solidify OOP skills learned in CS courses.
2. Learn how to write shaders and interact with the GPU as opposed to using a game engine or library.
3. Learn how to write a cross-platform execuatable game.

## Program Structure
Include statements

Key Callback for handling keyboard input (switch)

Functions and structs for handling:
  - Shader and program validation
  - Setting the buffer and drawing sprites to the buffer
  - Structs for game data, sprite data, player data, alien data
  
Main Function
  Create window and handle OpenGL context including error handling
    - set background color
  
  Initialize the buffer
    - define buffer size
    - define a background color in uint32
    - dynamically allocate buffer data in an array
    - color the background the defined color (clear_buffer)
    
  Create buffer texture - OpenGL texture that transfers image data to the GPU
  
  Create vertex and fragment shaders
    - create shader
    - define shader source
    - compile shader
    - validate shader
    - attach shader
    - delete shader
  
  Create sprites
  
  Initialize game and player information, array of aliens, and array to keep track of alien deaths
  
  Create array to keep track of alien animations, frame duration, and timing
  
  Initialize alien location and type for each position
  
  Game Loop:
   
    Clear buffer
    
    Draw aliens - handle death cases and check death counter
    Draw player
    Draw rockets
    
    Update animation timing
    
    Draw to screen (update texture buffers and swap buffers)
    
    Handle alien deaths - each death animation lasts 10 frames
    
    Handle rocket impacts
      - remove rockets out of the play area
      - check for hit on an alien, if hit change alien type to dead and remove the rocket
    
    Handle player movements and rocket launches
    
  free dynamically allocated memory
  
  end program
    
    
    
  
  
  
