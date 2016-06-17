#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#include <common/shader.hpp>
#include <common/texture.hpp>

using namespace glm;

GLFWwindow* window;
float maxVelocity = 0.001;
float radius = 0.0305; //size of the ball is 0.03, size of the particles 0.005

// CPU representation of a particle
struct Particle{
    glm::vec3 pos;
    glm::vec2 speed;
    unsigned char r,g,b,a; // Color
    float size, weight,life;
};

const int MaxParticles = 1000;
Particle ParticlesContainer[MaxParticles];
int LastUsedParticle = 0;

// Finds a Particle in ParticlesContainer which isn't used yet.
// (i.e. life < 0);
// the idea is that recently used particles are probably still alive, that's why we save the
// the index of last used.
int FindUnusedParticle(){
    
    for(int i=LastUsedParticle; i<MaxParticles; i++){
        if (ParticlesContainer[i].life < 0){
            LastUsedParticle = i;
            return i;
        }
    }
    // i =0 is where the ball is stored
    for(int i=1; i<LastUsedParticle; i++){
        if (ParticlesContainer[i].life < 0){
            LastUsedParticle = i;
            return i;
        }
    }
    
    return -1; // All particles are still alive
}

float RandomFloat(float a, float b) {
    float random = ((float) rand()) / (float) RAND_MAX;
    float diff = b - a;
    float r = random * diff;
    return a + r;
}

void addNewParticle(int i)
{
    ParticlesContainer[i].life = RandomFloat(0.01,1.0); // This particle will live from 100ms to 1000ms.
    ParticlesContainer[i].pos = glm::vec3(RandomFloat(0.0,1.0),RandomFloat(0.0,1.0),0); // random position in [0 1] range, occupies full screen
    ParticlesContainer[i].weight = RandomFloat(0.01,1.0); //random weight/strength
    
    glm::vec2 randomdir = glm::vec2(
    -1.0f * maxVelocity + RandomFloat(0.01,1.0) * maxVelocity * 2.0f,
    -1.0f * maxVelocity + RandomFloat(0.01,1.0) * maxVelocity * 2.0f
    );
    
    ParticlesContainer[i].speed = randomdir;
    
    // generate a random color
    ParticlesContainer[i].r = rand() % 256;
    ParticlesContainer[i].g = rand() % 256;
    ParticlesContainer[i].b = rand() % 256;
    ParticlesContainer[i].a = 255;
    
    ParticlesContainer[i].size = 0.005f;
}

void dropTheBall()
{
    
    ParticlesContainer[0].life = DBL_MAX; // This particle will live 5 seconds.
    ParticlesContainer[0].pos = glm::vec3(RandomFloat(0.0,1.0),0.2,0);
    ParticlesContainer[0].weight = 0.5;
    
    glm::vec2 randomdir = glm::vec2(
    0.0,
    maxVelocity
    );
    
    ParticlesContainer[0].speed = randomdir;
    
    ParticlesContainer[0].r = 255;
    ParticlesContainer[0].g = 255;
    ParticlesContainer[0].b = 255;
    ParticlesContainer[0].a = 255;
    
    ParticlesContainer[0].size = 0.03f;
}
int main( void )
{
    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFWn" );
        getchar();
        return -1;
    }
    
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_RESIZABLE,GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Open a window and create its OpenGL context
    window = glfwCreateWindow( 768, 768, "Programmer test: Force Field", NULL, NULL); // make it square
    if( window == NULL ){
        fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.n" );
        getchar();
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEWn");
        getchar();
        glfwTerminate();
        return -1;
    }
    
    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    // Hide the mouse and enable unlimited mouvement
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Dark blue background
    glClearColor(0.0f, 0.0f, 0.4f, 0.0f);
    
    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);
    
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    
    
    // Create and compile our GLSL program from the shaders
    GLuint programID = LoadShaders( "Particle.vertexshader", "Particle.fragmentshader" );
    
    // Vertex shader
    GLuint CameraRight_worldspace_ID  = glGetUniformLocation(programID, "CameraRight_worldspace");
    GLuint CameraUp_worldspace_ID  = glGetUniformLocation(programID, "CameraUp_worldspace");
    GLuint ViewProjMatrixID = glGetUniformLocation(programID, "VP");
    
    // fragment shader
    GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");
    
    static GLfloat* g_particule_position_size_data = new GLfloat[MaxParticles * 4];
    static GLubyte* g_particule_color_data         = new GLubyte[MaxParticles * 4];
    
    // texture to render squares as spheres/circles
    GLuint Texture = loadDDS("particle.DDS");
    
    // The VBO containing the 4 vertices of the particles.
    // Thanks to instancing, they will be shared by all particles.
    static const GLfloat g_vertex_buffer_data[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
        0.5f,  0.5f, 0.0f,
    };
    
    GLuint billboard_vertex_buffer;
    glGenBuffers(1, &billboard_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
    
    // The VBO containing the positions and sizes of the particles
    GLuint particles_position_buffer;
    glGenBuffers(1, &particles_position_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
    // Initialize with empty (NULL) buffer : it will be updated later, each frame.
    glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
    
    // The VBO containing the colors of the particles
    GLuint particles_color_buffer;
    glGenBuffers(1, &particles_color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
    // Initialize with empty (NULL) buffer : it will be updated later, each frame.
    glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW);
    
    // we use ortho, as we need only 2D graphics in our case, no need for perspective view
    glm::mat4  ProjectionMatrix = glm::ortho(0.0, 1.0,1.0,0.0, 0.1, 100.0);
    // Camera matrix
    glm::mat4 ViewMatrix       = glm::lookAt(
    vec3(0.0,0.0,1.0),           // Camera is here
    vec3(0.0,0.0,0.0), // and looks here : at the same position, plus "direction"
    vec3(0.0,1.0,0.0)                 // Head is up (set to 0,-1,0 to look upside-down)
    );
    
    // We will need the camera's position in order to sort the particles
    // w.r.t the camera's distance.
    // There should be a getCameraPosition() function in common/controls.cpp,
    // but this works too.
    
    glm::mat4 ViewProjectionMatrix =  ProjectionMatrix*ViewMatrix;
    
    double lastTime = glfwGetTime();
    // init the particles
    for(int i=1; i<MaxParticles; i++)
    addNewParticle(i);
    
    double startTime =glfwGetTime();
    
    double Timedrop = 2.0f; // as soon the program starts, the ball will be dropped in 2 sec
    
    //stores the particles which interact currently with a ball
    std::vector<int> indicesOfinter;
    
    // loop
    do
    {
        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        double currentTime = glfwGetTime();
        double delta = currentTime - lastTime;
        lastTime = currentTime;
        
        if(currentTime-startTime>Timedrop){ startTime=DBL_MAX; dropTheBall();}
        
        // Update 10 new particule each 16 millisecond (if 60hz)
        // or until all particles alive
        int newparticles = (int)(delta*10000.0);
        if (newparticles > (int)(0.016f*10000.0))
        newparticles = (int)(0.016f*10000.0);
        
        for(int i=0; i<newparticles; i++){
            
            int particleIndex = FindUnusedParticle();
            if(particleIndex==-1) break;
            addNewParticle(particleIndex);
        }
        
        // Simulate all particles
        int ParticlesCount = 0;
        indicesOfinter.clear();
        Particle& ball = ParticlesContainer[0]; //shortcut to the ball
        vec3 oldPos = ball.pos;
        for(int i=0; i<MaxParticles; i++){
            
            Particle& p = ParticlesContainer[i]; // shortcut
            
            if(p.life > 0.0f){
                
                // Decrease life
                p.life -= delta;
                if (p.life > 0.0f){
                    
                    p.pos +=vec3(p.speed,0.0); //move the particles
                    // bouncing of edges
                    if(p.pos[0]>0.97f || p.pos[0]<0.01f ) p.speed[0]*=-1.0f;
                    if(p.pos[1]>0.97f || p.pos[0]<0.01f )  p.speed[1]*=-1.0f;
                    
                    // Fill the GPU buffer
                    g_particule_position_size_data[4*ParticlesCount+0] = p.pos.x;
                    g_particule_position_size_data[4*ParticlesCount+1] = p.pos.y;
                    g_particule_position_size_data[4*ParticlesCount+2] = p.pos.z;
                    
                    g_particule_position_size_data[4*ParticlesCount+3] = p.size;
                    
                    g_particule_color_data[4*ParticlesCount+0] = p.r;
                    g_particule_color_data[4*ParticlesCount+1] = p.g;
                    g_particule_color_data[4*ParticlesCount+2] = p.b;
                    g_particule_color_data[4*ParticlesCount+3] = p.a;
                    
                    if(glm::distance(p.pos,ball.pos)<radius) indicesOfinter.push_back(i);
                    
                }
                ParticlesCount++;
            }
        }
        // if collision occurs, we dont count the ball itself thats why >1
        if(indicesOfinter.size()>1)
        {
            //iterate through all particles which are in radius of the ball
            for(int i = 1; i<indicesOfinter.size(); i++)
            {
                Particle& p2=ParticlesContainer[indicesOfinter.at(i)];
                
                // Find normal between particle and a ball (basically two balls)
                
                float normX = ball.pos.x - p2.pos.x;
                float normY = ball.pos.y - p2.pos.y;
                
                //normalize
                
                float normLength = sqrt( normX * normX + normY * normY);
                
                normX = normX / normLength;
                normY = normY / normLength;
                
                // Get projection of movement vectors onto normal
                
                float myProj    = (ball.speed.x * normX) + (ball.speed.y * normY);
                
                float otherProj = (p2.speed.x * normX) + (p2.speed.y * normY);
                // Now, factor in impulse, derived from
                // Conservation of Energy / Conservation of Momentum
                float impulse = ( 2.0f * (myProj - otherProj) );
                
                impulse /=  (ball.weight + p2.weight); //velocity change
                ball.speed.x -=  impulse * p2.weight * normX; // change the speed of only the ball, particles are not changed
                ball.speed.y -=  impulse * p2.weight * normY;
                
            }
            oldPos +=vec3(ball.speed,0.0); // bounce the ball
            ball.pos=oldPos;
            // update GPU buffer with new pos of the ball
            g_particule_position_size_data[0] = ball.pos.x;
            g_particule_position_size_data[1] = ball.pos.y;
            g_particule_position_size_data[2] = ball.pos.z;
        }
        
        // Update the buffers that OpenGL uses for rendering.
        // There are much more sophisticated means to stream data from the CPU to the GPU,
        // but this is outside the scope of this tutorial.
        // http://www.opengl.org/wiki/Buffer_Object_Streaming
        
        
        glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
        glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
        glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLfloat) * 4, g_particule_position_size_data);
        
        glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
        glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
        glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLubyte) * 4, g_particule_color_data);
        
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Use our shader
        glUseProgram(programID);
        
        // Bind our texture in Texture Unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, Texture);
        // Set our "myTextureSampler" sampler to user Texture Unit 0
        glUniform1i(TextureID, 0);
        
        // Same as the billboards tutorial
        glUniform3f(CameraRight_worldspace_ID, ViewMatrix[0][0], ViewMatrix[1][0], ViewMatrix[2][0]);
        glUniform3f(CameraUp_worldspace_ID   , ViewMatrix[0][1], ViewMatrix[1][1], ViewMatrix[2][1]);
        
        glUniformMatrix4fv(ViewProjMatrixID, 1, GL_FALSE, &ViewProjectionMatrix[0][0]);
        
        // 1rst attribute buffer : vertices
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer);
        glVertexAttribPointer(
        0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
        3,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        (void*)0            // array buffer offset
        );
        
        // 2nd attribute buffer : positions of particles' centers
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
        glVertexAttribPointer(
        1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
        4,                                // size : x + y + z + size => 4
        GL_FLOAT,                         // type
        GL_FALSE,                         // normalized?
        0,                                // stride
        (void*)0                          // array buffer offset
        );
        
        // 3rd attribute buffer : particles' colors
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
        glVertexAttribPointer(
        2,                                // attribute. No particular reason for 1, but must match the layout in the shader.
        4,                                // size : r + g + b + a => 4
        GL_UNSIGNED_BYTE,                 // type
        GL_TRUE,                          // normalized?    *** YES, this means that the unsigned char[4] will be accessible with a vec4 (floats) in the shader ***
        0,                                // stride
        (void*)0                          // array buffer offset
        );
        
        // These functions are specific to glDrawArrays*Instanced*.
        // The first parameter is the attribute buffer we're talking about.
        // The second parameter is the "rate at which generic vertex attributes advance when rendering multiple instances"
        // http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribDivisor.xml
        glVertexAttribDivisor(0, 0); // particles vertices : always reuse the same 4 vertices -> 0
        glVertexAttribDivisor(1, 1); // positions : one per quad (its center)                 -> 1
        glVertexAttribDivisor(2, 1); // color : one per quad                                  -> 1
        
        // Draw the particules !
        // This draws many times a small triangle_strip (which looks like a quad).
        // This is equivalent to :
        // for(i in ParticlesCount) : glDrawArrays(GL_TRIANGLE_STRIP, 0, 4),
        // but faster.
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, ParticlesCount);
        
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        
        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
        
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
    glfwWindowShouldClose(window) == 0 );
    
    
    delete[] g_particule_position_size_data;
    
    // Cleanup VBO and shader
    glDeleteBuffers(1, &particles_color_buffer);
    glDeleteBuffers(1, &particles_position_buffer);
    glDeleteBuffers(1, &billboard_vertex_buffer);
    glDeleteProgram(programID);
    glDeleteTextures(1, &TextureID);
    glDeleteVertexArrays(1, &VertexArrayID);
    
    
    // Close OpenGL window and terminate GLFW
    glfwTerminate();
    
    return 0;
}