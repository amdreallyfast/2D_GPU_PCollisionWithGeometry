#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/PotentialParticlePolygonCollisionsSsbo.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"
#include "Shaders/ShaderStorage.h"

#include "Include/Buffers/PotentialParticleCollisions.h"

#include <vector>

/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, then gives derived class members initial values and allocates space 
    for the SSBO.

    Note: This is identical to 
    SSBOs/ParticleParticleCollisions/PotentialParticleParticleCollisionsSsbo, but this one is 
    contained by the ParticlePolygonCollisions shader controller instead of the 
    ParticleParticleCollisions shader controller, and all buffers must be unique (under my 
    current design of hard-coding SSBO buffer binding locations), so the potential 
    particle-polygon collisions buffer needs a different SSBO.
Parameters: 
    numParticles    Self-explanatory.
Returns:    None
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
PotentialParticlePolygonCollisionsSsbo::PotentialParticlePolygonCollisionsSsbo(unsigned int numParticles) :
    SsboBase(),  // generate buffers
    _numItems(numParticles)
{
    std::vector<PotentialParticleCollisions> v(numParticles);

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, POTENTIAL_PARTICLE_POLYGON_COLLISIONS_BUFFER_BINDING, _bufferId);

    // and fill it with new data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(PotentialParticleCollisions), v.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/*------------------------------------------------------------------------------------------------
Description:
    Defines the buffer's size uniform in the specified shader.  
Parameters: 
    computeProgramId    Self-explanatory.
Returns:    None
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
void PotentialParticlePolygonCollisionsSsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
    unsigned int bufferSizeUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uPotentialParticlePolygonCollisionsBufferSize");

    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    glUniform1ui(bufferSizeUnifLoc, _numItems);
    glUseProgram(0);
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the value that was passed in on creation.
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
unsigned int PotentialParticlePolygonCollisionsSsbo::NumItems() const
{
    return _numItems;
}
