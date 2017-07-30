#include "Include/Buffers/SSBOs/ParticleParticleCollisions/PotentialParticleParticleCollisionsSsbo.h"

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
Parameters: 
    numParticles    Self-explanatory.
Returns:    None
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
PotentialParticleParticleCollisionsSsbo::PotentialParticleParticleCollisionsSsbo(unsigned int numParticles) :
    SsboBase(),  // generate buffers
    _numItems(numParticles)
{
    std::vector<PotentialParticleCollisions> v(numParticles);

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, POTENTIAL_PARTICLE_PARTICLE_COLLISIONS_BUFFER_BINDING, _bufferId);

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
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
void PotentialParticleParticleCollisionsSsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
    unsigned int bufferSizeUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uPotentialParticleParticleCollisionsBufferSize");

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
unsigned int PotentialParticleParticleCollisionsSsbo::NumItems() const
{
    return _numItems;
}
