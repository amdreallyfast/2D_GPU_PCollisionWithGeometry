#include "Include/Buffers/SSBOs/SsboBase.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"

/*------------------------------------------------------------------------------------------------
Description:
    This was created for convenience as I've been making more and more SSBOs.
Parameters: None
Returns:    
    See descriptions.
Creator:    John Cox, 1/2017
------------------------------------------------------------------------------------------------*/
static unsigned int GetNewStorageBlockBindingPointIndex()
{
    static GLuint ssboBindingPointIndex = 0;

    return ssboBindingPointIndex++;
}

/*------------------------------------------------------------------------------------------------
Description:
    Gives members default values.

    Note: As part of the Resource Acquisition is Initialization (RAII) approach that I am trying 
    to use in this program, the SSBOs are generated here.  This means that the OpenGL context 
    MUST be started up prior to initialization.  Prior to OpenGL context instantiation, any 
    gl*(...) function calls will throw an exception.

    The SSBO is linked up with the compute shader in ConfigureCompute(...).
    The VAO is initialized in ConfigureRender(...).

Parameters: None
Returns:    None
Creator:    John Cox, 9-20-2016
------------------------------------------------------------------------------------------------*/
SsboBase::SsboBase() :
    _vaoId(0),
    _bufferId(0),
    _drawStyle(0),
    _numVertices(0)
{
    glGenBuffers(1, &_bufferId);
    glGenVertexArrays(1, &_vaoId);
}

/*------------------------------------------------------------------------------------------------
Description:
    Cleans up the buffer and VAO.  If either is 0, then the glDelete*(...) call silently does 
    nothing.
Parameters: None
Returns:    None
Creator:    John Cox, 9-20-2016
------------------------------------------------------------------------------------------------*/
SsboBase::~SsboBase()
{
    glDeleteBuffers(1, &_bufferId);
    glDeleteVertexArrays(1, &_vaoId);
}

/*------------------------------------------------------------------------------------------------
Description:
    // TODO: ??make this pure virtual??

    This is a convenience method for setting constant values, like buffer sizes, that must be 
    set for the same SSBO in multiple shaders.  This is not unusual in the parallel sort 
    algorithm, which has multiple sets, and in each step data may be taken from one buffer, have 
    calculations performed on it, and then stuck into another buffer.

    If the shader does not have the uniform or if the shader compiler optimized it out, then
    OpenGL will complain about not finding it.  Enable debugging in main() in main.cpp for more
    detail.

    The method does nothing though.  Override as needed.
Parameters: 
    Ignored
Returns:    None
Creator:    John Cox, 3/2017
------------------------------------------------------------------------------------------------*/
void SsboBase::ConfigureConstantUniforms(unsigned int) const
{
    // nothing
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the vertex array object ID.
Parameters: None
Returns:    
    A copy of the VAO's ID.
Creator:    John Cox, 9-20-2016
------------------------------------------------------------------------------------------------*/
unsigned int SsboBase::VaoId() const
{
    return _vaoId;
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the vertex buffer object's ID.
Parameters: None
Returns:
    A copy of the VBO's ID.
Creator:    John Cox, 9-20-2016
------------------------------------------------------------------------------------------------*/
unsigned int SsboBase::BufferId() const
{
    return _bufferId;
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the draw style for the contained object (GL_TRIANGLES, GL_LINES, 
    GL_POINTS, etc.)
Parameters: None
Returns:
    A copy of the draw style GLenum.
Creator:    John Cox, 9-20-2016
------------------------------------------------------------------------------------------------*/
unsigned int SsboBase::DrawStyle() const
{
    return _drawStyle;
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the number of vertices in the SSBO.  Used in the glDrawArrays(...) call.
Parameters: None
Returns:
    See description.
Creator:    John Cox, 9-20-2016
------------------------------------------------------------------------------------------------*/
unsigned int SsboBase::NumVertices() const
{
    return _numVertices;
}

/*------------------------------------------------------------------------------------------------
Description:
    Define in derived class if the SSBO's data will be used during rendering.  
    Example: ParticleSsbo and VertexSsboBase.
    
    Counter example: BvhNodeSsbo and ParticleSortingDataSsbo do not draw and therefore do not 
    override this.

    Note: This method cannot be const because the the user needs to record the draw style.
Parameters: None
Returns:    None
Creator:    John Cox, 3/2017
------------------------------------------------------------------------------------------------*/
void SsboBase::ConfigureRender()
{
    // nothing
}
