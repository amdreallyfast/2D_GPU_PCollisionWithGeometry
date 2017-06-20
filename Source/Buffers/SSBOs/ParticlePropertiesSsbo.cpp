#include "Include/Buffers/SSBOs/ParticlePropertiesSsbo.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"

#include "Include/Buffers/ParticleProperties.h"

#include <vector>


/*------------------------------------------------------------------------------------------------
Description:
    A helper function to clean up the construction.
Parameters: 
    An STL container of ParticleProperty structures.  It will be emptied and built from scratch.
Returns:    None
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
static void GenerateParticleProperties(std::vector<ParticleProperties> &initThis)
{
    initThis.clear();
    initThis.resize(ParticleProperties::ParticleType::NUM_PARTICLE_PROPERTIES);

    ParticleProperties pp;

    // the dud property
    initThis[ParticleProperties::ParticleType::NO_PARTICLE_TYPE] = pp;

    // generic
    pp._mass = 0.05f;
    pp._collisionRadius = 0.005f;
    initThis[ParticleProperties::ParticleType::GENERIC] = pp;
}


/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, then gives derived class members initial values and allocates space 
    for the SSBO.
Parameters: None
Returns:    None
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
ParticlePropertiesSsbo::ParticlePropertiesSsbo() :
    SsboBase(),
    _numProperties(0)
{
    std::vector<ParticleProperties> v;
    GenerateParticleProperties(v);
    _numProperties = v.size();

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, PARTICLE_PROPERTIES_BUFFER_BINDING, _bufferId);

    // and fill it with new data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(ParticleProperties), v.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/*------------------------------------------------------------------------------------------------
Description:
    Defines the buffer's size uniform in the specified shader.  It uses the #define'd uniform 
    location found in CrossShaderUniformLocations.comp.
Parameters: 
    computeProgramId    Self-explanatory.
Returns:    None
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
void ParticlePropertiesSsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    glUniform1ui(UNIFORM_LOCATION_NUM_PARTICLE_PROPERTIES, _numProperties);
    glUseProgram(0);
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the value that was generated on creation.
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
unsigned int ParticlePropertiesSsbo::NumProperties() const
{
    return _numProperties;
}
