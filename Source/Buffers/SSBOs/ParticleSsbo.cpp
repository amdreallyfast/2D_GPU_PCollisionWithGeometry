#include "Include/Buffers/SSBOs/ParticleSsbo.h"

#include <vector>
#include <random>   // for generating initial data
#include <time.h>

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"

#include "Include/Buffers/Particle.h"


/*------------------------------------------------------------------------------------------------
Description:
    Particle resetting on the GPU requires several random numbers.  GLSL doesn't have GPU 
    clock-reading functions, so the next best thing is a chaotic hash function 
    (see Random.comp).  Being a hash function though, if the same values are put into it (like 
    the Particle's default position XY and velocity XY values of 0), then the result won't 
    change.  

    This method gives the particle position and velocity initial random values.

    All particles start as "inactive", so they'll be reset immediately.  This method just gives 
    them a bump toward randomization.
Parameters: 
    initThese   Self-explanatory.
Returns:    None
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
static void InitializeWithRandomData(std::vector<Particle> &initThese)
{
    srand(static_cast<unsigned int>(time(0)));

    float inverseRandMax = 1.0f / RAND_MAX;
    for (size_t particleIndex = 0; particleIndex < initThese.size(); particleIndex++)
    {
        // randomized X and Y values
        // Note: Once the sim gets running, the particles will be in window space (X and Y along 
        // the range [-1,+1]), but for initialiation, this 0-1 range will suffice for 
        // randomizing float values.  The 0-1 range isn't necessary, but the position and 
        // velocity values are floats, and dividing by RAND_MAX is an easy way to get a float.  
        // It just so happens to be along the range 0-1.
        initThese[particleIndex]._currPos.x = static_cast<float>(rand()) * inverseRandMax;
        initThese[particleIndex]._currPos.y = static_cast<float>(rand()) * inverseRandMax;
        
        // just outside the Z buffer (0 (far) to -1 (near)), so it won't draw
        initThese[particleIndex]._currPos.z = +0.1f;

        initThese[particleIndex]._vel.x = static_cast<float>(rand()) * inverseRandMax;
        initThese[particleIndex]._vel.y = static_cast<float>(rand()) * inverseRandMax;
    }
}

/*------------------------------------------------------------------------------------------------
Description:
    Currently sets all particles to a generic type.
Parameters: 
    initThese   Self-explanatory.
Returns:    None
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
static void InitializeParticleTypes(std::vector<Particle> &initThese)
{
    for (size_t particleIndex = 0; particleIndex < initThese.size(); particleIndex++)
    {
        initThese[particleIndex]._particleTypeIndex = ParticleProperties::ParticleType::GENERIC;
    }
}

/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, then gives derived class members initial values and allocates space 
    for the SSBO.

    Allocates space for 2x the number of particles.  The second half is used during particle 
    sorting so that particles can be copied to the second half, then copied back to their sorted 
    positions in the first half.

    Also generates random initial particle positions and velocities, but all particles are still 
    default inactive.  See description in InitializeWithRandomData(...).

    Uploads the initialized particles to the SSBOs newly allocated buffer memory.

Parameters: 
    numParticles    However many instances of Particle the user wants to store.
Returns:    None
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
ParticleSsbo::ParticleSsbo(unsigned int numParticles) :
    SsboBase()  // generate buffers
{
    std::vector<Particle> v(numParticles);
    InitializeWithRandomData(v);
    InitializeParticleTypes(v);

    // each particle is 1 vertex, so for particles, "num vertices" == "num items"
    // Note: This can't be set in the class initializer list.  The class initializer list is for 
    // members of this class only (ParticleSsbo), not for base class members. 
    _numVertices = numParticles;
    _numParticles = numParticles;

    // the second half has no need for initial data
    v.resize(numParticles * 2);

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, PARTICLE_BUFFER_BINDING, _bufferId);

    // and fill it with the new data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(Particle), v.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // set up the VAO
    ConfigureRender();
}

/*------------------------------------------------------------------------------------------------
Description:
    Defines the buffer's size uniform in the specified shader.  It uses the #define'd uniform 
    location found in CrossShaderUniformLocations.comp.
Parameters: 
    computeProgramId    Self-explanatory.
Returns:    None
Creator:    John Cox, 3/2017
------------------------------------------------------------------------------------------------*/
void ParticleSsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    glUniform1ui(UNIFORM_LOCATION_MAX_NUM_PARTICLES, _numVertices);
    glUseProgram(0);
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the value that was passed in on creation.
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
unsigned int ParticleSsbo::NumParticles() const
{
    return _numParticles;
}

/*------------------------------------------------------------------------------------------------
Description:
    Sets up the vertex attribute pointers for this SSBO's VAO.
Parameters: None
Returns:    None
Creator:    John Cox, 11-24-2016
------------------------------------------------------------------------------------------------*/
void ParticleSsbo::ConfigureRender()
{
    // particles shall always be rendered as points
    _drawStyle = GL_POINTS;

    // the vertex array attributes will make an association between whatever is bound to the 
    // array buffer and whatever is set up with glVertexAttrib*(...)
    glBindVertexArray(_vaoId);
    glBindBuffer(GL_ARRAY_BUFFER, _bufferId);

    // vertex attribute order is same as the structure members

    GLenum itemType = 0;
    unsigned int vertexArrayIndex = 0;
    unsigned int vertexArrayCounter = 0;
    unsigned int bufferStartOffset = 0;
    unsigned int bytesPerStep = sizeof(Particle);
    unsigned int sizeOfItem = 0;
    unsigned int numItems = 0;

    // current position
    itemType = GL_FLOAT;
    sizeOfItem = sizeof(Particle::_currPos);
    numItems = sizeOfItem / sizeof(float);
    vertexArrayIndex = vertexArrayCounter++;
    glEnableVertexAttribArray(vertexArrayIndex);
    glVertexAttribPointer(vertexArrayIndex, numItems, itemType, GL_FALSE, bytesPerStep, (void *)bufferStartOffset);
    bufferStartOffset += sizeOfItem;

    // previous position
    itemType = GL_FLOAT;
    sizeOfItem = sizeof(Particle::_prevPos);
    numItems = sizeOfItem / sizeof(float);
    vertexArrayIndex = vertexArrayCounter++;
    glEnableVertexAttribArray(vertexArrayIndex);
    glVertexAttribPointer(vertexArrayIndex, numItems, itemType, GL_FALSE, bytesPerStep, (void *)bufferStartOffset);
    bufferStartOffset += sizeOfItem;

    // velocity
    itemType = GL_FLOAT;
    sizeOfItem = sizeof(Particle::_vel);
    numItems = sizeOfItem / sizeof(float);
    vertexArrayIndex = vertexArrayCounter++;
    glEnableVertexAttribArray(vertexArrayIndex);
    glVertexAttribPointer(vertexArrayIndex, numItems, itemType, GL_FALSE, bytesPerStep, (void *)bufferStartOffset);
    bufferStartOffset += sizeOfItem;

    // particle type index
    itemType = GL_INT;
    sizeOfItem = sizeof(Particle::_particleTypeIndex);
    numItems = sizeOfItem / sizeof(float);
    vertexArrayIndex = vertexArrayCounter++;
    glEnableVertexAttribArray(vertexArrayIndex);
    glVertexAttribIPointer(vertexArrayIndex, numItems, itemType, bytesPerStep, (void *)bufferStartOffset);
    bufferStartOffset += sizeOfItem;

    // number of nearby particles
    itemType = GL_INT;
    sizeOfItem = sizeof(Particle::_numNearbyParticles);
    numItems = sizeOfItem / sizeof(float);
    vertexArrayIndex = vertexArrayCounter++;
    glEnableVertexAttribArray(vertexArrayIndex);
    glVertexAttribIPointer(vertexArrayIndex, numItems, itemType, bytesPerStep, (void *)bufferStartOffset);
    bufferStartOffset += sizeOfItem;

    // "is active" flag
    itemType = GL_INT;
    sizeOfItem = sizeof(Particle::_isActive);
    numItems = sizeof(Particle::_isActive) / sizeof(int);
    vertexArrayIndex = vertexArrayCounter++;
    glEnableVertexAttribArray(vertexArrayIndex);
    glVertexAttribIPointer(vertexArrayIndex, numItems, itemType, bytesPerStep, (void *)bufferStartOffset);
    bufferStartOffset += sizeOfItem;

    // cleanup
    glBindVertexArray(0);   // unbind this BEFORE the array or else the VAO will bind to buffer 0
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


