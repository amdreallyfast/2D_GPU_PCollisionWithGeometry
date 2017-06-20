#include "Include/ShaderControllers/RenderParticles.h"

#include <string>
#include "Shaders/ShaderStorage.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"

namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Gives members initial values.
        
        Constructs the RenderParticles shader out of ParticleRender.vert and ParticleRender.frag.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    RenderParticles::RenderParticles() :
        _renderProgramId(0)
    {
        // construct the compute shader
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "particle render";
        shaderStorageRef.NewCompositeShader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/Version.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/MaxNumPotentialCollisions.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Render/ParticleRender.vert");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_VERTEX_SHADER);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, "Shaders/Render/ParticleRender.frag", GL_FRAGMENT_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _renderProgramId = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Cleans up the shader that was created for this shader controller.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    RenderParticles::~RenderParticles()
    {
        glDeleteProgram(_renderProgramId);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Binds the VAO for the particle SSBO, then calls glDrawArrays(...).
    Parameters: 
        particleSsboToRender    Contains the VAO ID, draw style, and number of vertices.
    Returns:    None
    Creator:    John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    void RenderParticles::Render(const ParticleSsbo::SharedPtr &particleSsboToRender) const
    {
        glUseProgram(_renderProgramId);
        glBindVertexArray(particleSsboToRender->VaoId());

        // in the case of particles, "num items" == "num vertices", so either getter is fine
        glDrawArrays(particleSsboToRender->DrawStyle(), 0, particleSsboToRender->NumVertices());
        glBindVertexArray(0);
        glUseProgram(0);
    }

}
