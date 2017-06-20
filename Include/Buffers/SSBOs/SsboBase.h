#pragma once

#include <string>
#include <memory>

/*------------------------------------------------------------------------------------------------
Description:
    Defines the constructor, which gives the members zero values, and the destructor, which 
    deletes any allocated buffers.  
Creator:    John Cox, 9-20-2016
------------------------------------------------------------------------------------------------*/
class SsboBase
{
public:
    SsboBase();
    virtual ~SsboBase();
    using SharedPtr = std::shared_ptr<SsboBase>;
    using SharedConstPtr = std::shared_ptr<const SsboBase>;

    virtual void ConfigureConstantUniforms(unsigned int computeProgramId) const;

    unsigned int VaoId() const;
    unsigned int BufferId() const;
    unsigned int DrawStyle() const;
    unsigned int NumVertices() const;

    //static unsigned int GetStorageBlockBindingPointIndexForBuffer(const std::string &bufferNameInShader);

protected:
    // can't be private because the derived classes need them
    virtual void ConfigureRender();

    // save on the large header inclusion of OpenGL and write out these primitive types instead 
    // of using the OpenGL typedefs
    // Note: IDs are GLuint (unsigned int), draw style is GLenum (unsigned int), GLushort is 
    // unsigned short.
    
    unsigned int _vaoId;
    unsigned int _bufferId;
    unsigned int _drawStyle;    // GL_TRIANGLES, GL_LINES, etc.
    unsigned int _numVertices;
};
