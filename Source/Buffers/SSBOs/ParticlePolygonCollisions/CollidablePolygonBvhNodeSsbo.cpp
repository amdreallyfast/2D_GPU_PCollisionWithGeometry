#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonBvhNodeSsbo.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"
#include "Shaders/ShaderStorage.h"

#include "Include/Buffers/BvhNode.h"

#include <vector>


/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, then gives derived class members initial values and allocates space 
    for the SSBO.
Parameters: 
    numLeaves   Expected to be the same size as the number of polygons.  If it isn't, then 
                there is a risk of buffer overrun.
Returns:    None
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
CollidablePolygonBvhNodeSsbo::CollidablePolygonBvhNodeSsbo(unsigned int numPolygons) :
    SsboBase()
{
    // binary trees with N leaves have N-1 branches
    _numLeaves = numPolygons;
    _numInternalNodes = numPolygons - 1;
    _numTotalNodes = _numLeaves + _numInternalNodes;
    std::vector<BvhNode> v(_numTotalNodes);

    for (unsigned int leafNodeIndex = 0; leafNodeIndex < _numLeaves; leafNodeIndex++)
    {
        v[leafNodeIndex]._isLeaf = true;
    }

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COLLIDABLE_POLYGON_BVH_NODE_BUFFER_BINDING, _bufferId);

    // and fill it with new data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(BvhNode), v.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/*------------------------------------------------------------------------------------------------
Description:
    Defines the buffer's size uniform in the specified shader.  
Parameters: 
    computeProgramId    Self-explanatory.
Returns:    
    See Description.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
void CollidablePolygonBvhNodeSsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    unsigned int numLeavesUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uCollidablePolygonBvhNumberLeaves");
    unsigned int numInternalNodesUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uCollidablePolygonBvhNumberInternalNodes");
    unsigned int bufferSizeUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uCollidablePolygonBvhNodeBufferSize");

    glUniform1ui(numLeavesUnifLoc, _numLeaves);
    glUniform1ui(numInternalNodesUnifLoc, _numInternalNodes);
    glUniform1ui(bufferSizeUnifLoc, _numTotalNodes);
    glUseProgram(0);
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the value that was passed in on creation.
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
unsigned int CollidablePolygonBvhNodeSsbo::NumLeafNodes() const
{
    return _numLeaves;
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
unsigned int CollidablePolygonBvhNodeSsbo::NumTotalNodes() const
{
    return _numTotalNodes;
}