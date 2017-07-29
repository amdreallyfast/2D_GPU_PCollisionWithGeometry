#include "Include/Buffers/SSBOs/VisualizationOnly/CollidableGeometryBoundingBoxGeometrySsbo.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"
#include "Shaders/ShaderStorage.h"

#include "Include/Geometry/Box2D.h"

#include <vector>


/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, then gives derived class members initial values and allocates space 
    for the SSBO.
Parameters: 
    numParticles    Self-explanatory.
Returns:    None
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
CollidableGeometryBoundingBoxGeometrySsbo::CollidableGeometryBoundingBoxGeometrySsbo(unsigned int numPolygons) :
    VertexSsboBase()  // generate buffers and configure VAO
{
    std::vector<Box2D> v(numPolygons);
    _numVertices = (v.size() * sizeof(Box2D)) / sizeof(MyVertex);

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COLLIDABLE_GEOMETRY_BOUNDING_BOX_GEOMETRY_BUFFER_BINDING, _bufferId);

    // and fill it with new data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(Box2D), v.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/*------------------------------------------------------------------------------------------------
Description:
    Defines the buffer's size uniform in the specified shader.  It uses the #define'd uniform 
    location found in CrossShaderUniformLocations.comp.
Parameters: 
    computeProgramId    Self-explanatory.
Returns:    None
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
void CollidableGeometryBoundingBoxGeometrySsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
    unsigned int bufferSizeUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uCollidableGeometryBoundingBoxGeometryBufferSize");

    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    glUniform1ui(bufferSizeUnifLoc, _numVertices);
    glUseProgram(0);
}
