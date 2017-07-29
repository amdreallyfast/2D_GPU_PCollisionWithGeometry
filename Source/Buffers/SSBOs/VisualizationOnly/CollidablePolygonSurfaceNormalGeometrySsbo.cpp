
#include "Include/Buffers/SSBOs/VisualizationOnly/CollidablePolygonSurfaceNormalGeometrySsbo.h"
#include "Include/Geometry/BlenderLoad.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"
#include "Shaders/ShaderStorage.h"

#include "Include/Geometry/PolygonFace.h"
#include <vector>


/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, calls the BlenderLoad object to load a set of lines from a Blender3D 
    .obj file, then uses this data to allocates space for the SSBO.
Parameters: 
    blenderMeshFilePath     The path to the .obj file
Returns:    None
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
CollidablePolygonSurfaceNormalGeometrySsbo::CollidablePolygonSurfaceNormalGeometrySsbo(const std::string &blenderMeshFilePath) :
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
            glm::vec4 polygonCenter = (polyItr->_start._position + polyItr->_end._position) * 0.5f;

            // Note: Due to the way that I am currently (7-29-2017) using vertex buffers with 
            // OpenGL and how I am specifying the vertex attribute pointers, each vertex is a 
            // combination of position and normal.  This geometry is for the normals, so it will 
            // have a null normal (all zeros).
            glm::vec4 nullNormal;
            v.push_back(PolygonFace(
                MyVertex(polygonCenter, nullNormal),
                MyVertex(polygonCenter + polyItr->_start._normal, nullNormal)));
        }
    }
    _numPolygons = v.size();
    _numVertices = (v.size() * sizeof(PolygonFace)) / sizeof(MyVertex);

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COLLIDABLE_GEOMETRY_SURFACE_NORMAL_GEOMETRY_BUFFER_BINDING, _bufferId);

    // and fill it with new data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(PolygonFace), v.data(), GL_DYNAMIC_DRAW);
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
void CollidablePolygonSurfaceNormalGeometrySsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
    unsigned int bufferSizeUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uCollidableGeometrySurfaceNormalGeometryBufferSize");

    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    glUniform1ui(bufferSizeUnifLoc, _numPolygons);
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
unsigned int CollidablePolygonSurfaceNormalGeometrySsbo::NumPolygons() const
{
    return _numPolygons;
}