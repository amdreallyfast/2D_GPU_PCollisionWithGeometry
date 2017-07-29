#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonPrefixSumSsbo.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"

#include "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"
#include "Shaders/ShaderStorage.h"

#include <vector>


/*------------------------------------------------------------------------------------------------
Description:
    Initializes the base class, then initializes derived class members and allocates space for 
    the SSBO.
Parameters: 
    numDataEntries  How many items the user wants to have.  The only restriction is that it be 
    less than (due to restrictions in the PrefixScan) 1024x1024 = 1,048,576.
Returns:    None
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
CollidablePolygonPrefixSumSsbo::CollidablePolygonPrefixSumSsbo(unsigned int numDataEntries) :
    SsboBase(),  // generate buffers
    _numDataEntries(0)
{
    // see explanation essay at the top of ParticlePrefixSumSsbo.cpp 
    unsigned int prefixScanWorkGroupSize = WORK_GROUP_SIZE_X * 2;
    _numDataEntries = (numDataEntries / prefixScanWorkGroupSize);
    _numDataEntries += (numDataEntries % prefixScanWorkGroupSize == 0) ? 0 : 1;
    _numDataEntries *= prefixScanWorkGroupSize;

    // the std::vector<...>(...) constructor will set everything to 0
    // Note: The +1 is because of a single uint in the buffer, totalNumberOfOnes.
    std::vector<unsigned int> v(1 + _numDataEntries);

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COLLIDABLE_POLYGON_PREFIX_SCAN_BUFFER_BINDING, _bufferId);

    // and fill it with 0s
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(unsigned int), v.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/*------------------------------------------------------------------------------------------------
Description:
    Defines the buffer's size uniform in the specified shader.  It uses the #define'd uniform 
    location found in CrossShaderUniformLocations.comp.
Parameters: 
    computeProgramId    Self-explanatory.
Returns:    None
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/  
void CollidablePolygonPrefixSumSsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
    unsigned int bufferSizeUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uMaxGeometryPrefixSums");

    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    glUniform1ui(bufferSizeUnifLoc, _numDataEntries);
    glUseProgram(0);
}

/*------------------------------------------------------------------------------------------------
Description:
    Returns the number of integers that have been allocated for the AllCollidableGeometryPrefixSums array.  The 
    constructor ensures that there are enough entries for every item to be part of a work group.  
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
unsigned int CollidablePolygonPrefixSumSsbo::NumDataEntries() const
{
    return _numDataEntries;
}

/*------------------------------------------------------------------------------------------------
Description:
    Returns the number of prefix sum entries in CollidableGeometryPrefixScanBuffer plus 1 
    (for totalNumberOfOnes).  
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
unsigned int CollidablePolygonPrefixSumSsbo::TotalBufferEntries() const
{
    return _numDataEntries + 1;
}

