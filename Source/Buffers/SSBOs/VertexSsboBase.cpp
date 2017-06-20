#include "Include/Buffers/SSBOs/VertexSsboBase.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Include/Geometry/MyVertex.h"


/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, then configures the VAO for drawing.
Parameters: None
Returns:    None
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
VertexSsboBase::VertexSsboBase() :
    SsboBase()  // generate buffers
{
    // generate the VAO
    ConfigureRender();
}

/*-----------------------------------------------------------------------------------------------
Description:
    Sets up the vertex attribute pointers for a VAO that draws buffers of MyVertex objects.
Parameters: None
Returns:    None
Creator:    John Cox, 6/2017
-----------------------------------------------------------------------------------------------*/
void VertexSsboBase::ConfigureRender() 
{
    // 2D polygons shall always be rendered as pairs of points
    // Note: This SSBO exists as a base class to buffers that must be created and/or used in 
    // compute shaders.  Each line is considered an independent object for ease of computing 
    // (that is, pairs of points), so there WILL be duplicate points and they should rendered as 
    // independent lines, not as line loops.
    _drawStyle = GL_LINES;

    glBindVertexArray(_vaoId);
    glBindBuffer(GL_ARRAY_BUFFER, _bufferId);

    unsigned int vertexArrayIndex = 0;
    unsigned int bufferStartOffset = 0;
    unsigned int bytesPerStep = sizeof(MyVertex);
    unsigned int sizeOfItem = 0;
    unsigned int numItems = 0;

    // position
    GLenum itemType = GL_FLOAT;
    sizeOfItem = sizeof(MyVertex::_position);
    numItems = sizeOfItem / sizeof(float);
    glEnableVertexAttribArray(vertexArrayIndex);
    glVertexAttribPointer(vertexArrayIndex, numItems, itemType, GL_FALSE, bytesPerStep, (void *)bufferStartOffset);
    bufferStartOffset += sizeOfItem;

    // normal
    itemType = GL_FLOAT;
    sizeOfItem = sizeof(MyVertex::_normal);
    numItems = sizeOfItem / sizeof(float);
    vertexArrayIndex++;
    glEnableVertexAttribArray(vertexArrayIndex);
    glVertexAttribPointer(vertexArrayIndex, numItems, itemType, GL_FALSE, bytesPerStep, (void *)bufferStartOffset);
    bufferStartOffset += sizeOfItem;

    // cleanup
    glBindVertexArray(0);   // unbind this BEFORE the array
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
