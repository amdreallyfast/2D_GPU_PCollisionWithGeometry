
#include "Include/Buffers/SSBOs/CollideableGeometrySsbo.h"
#include "Include/Geometry/BlenderLoad.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"

#include "Include/Geometry/PolygonFace.h"
#include <vector>

// comment out to remove the face normals from the polygon buffer
#define MAKE_LINES_OUT_OF_NORMALS


/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, calls the BlenderLoad object to load a set of lines from a Blender3D 
    .obj file, then uses this data to allocates space for the SSBO.
Parameters: 
    blenderMeshFilePath     The path to the .obj file
Returns:    None
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
CollideableGeometrySsbo::CollideableGeometrySsbo(const std::string &blenderMeshFilePath) :
    VertexSsboBase(),  // generate buffers and configure VAO
    _numPolygons(0)
{
    // load vertices from file
    BlenderLoad bl(blenderMeshFilePath);

    // dump all vertices into this buffer
    // Note: For the sake of this demo, there will be no distinction between object geometry.  
    // All the geometry loaded from this file will be used.
    std::vector<PolygonFace> v;
    const BlenderLoad::GeometryByObject &allGeometry = bl.Geometry();
    for (BlenderLoad::GeometryByObject::const_iterator objectItr = allGeometry.begin(); 
        objectItr != allGeometry.end(); objectItr++)
    {
        const BlenderLoad::PolygonCollection &objectPolygons = objectItr->second;
        for (BlenderLoad::PolygonCollection::const_iterator polyItr = objectPolygons.begin();
            polyItr != objectPolygons.end();
            polyItr++)
        {
            v.push_back(*polyItr);

#ifdef MAKE_LINES_OUT_OF_NORMALS
            // for the sake of visualization, make 2D polygons for the normals as well
            // Note: Remove these before performing particle-geometry collision detection or 
            // else the particles will try to bounce off these as well, and since they have dud 
            // (zero) normals, the dot products and/or other calculations involved in the 
            // collision detection and resolution will likely result in inf or nan.
            v.push_back(PolygonFace(
                MyVertex(polyItr->_start._position, glm::vec4()),
                MyVertex(polyItr->_start._position + polyItr->_start._normal, glm::vec4())));
            v.push_back(PolygonFace(
                MyVertex(polyItr->_end._position, glm::vec4()),
                MyVertex(polyItr->_end._position + polyItr->_end._normal, glm::vec4())));
#endif
        }
    }
    _numPolygons = v.size();
    _numVertices = (v.size() * sizeof(PolygonFace)) / sizeof(MyVertex);

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COLLIDEABLE_GEOMETRY_BUFFER_BINDING, _bufferId);

    // and fill it with new data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(PolygonFace), v.data(), GL_DYNAMIC_DRAW);
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
void CollideableGeometrySsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    glUniform1ui(UNIFORM_LOCATION_COLLIDEABLE_GEOMETRY_BUFFER_SIZE, _numPolygons);
    glUseProgram(0);
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the value that was determined during creation.
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
unsigned int CollideableGeometrySsbo::NumPolygons() const
{
    return _numPolygons;
}