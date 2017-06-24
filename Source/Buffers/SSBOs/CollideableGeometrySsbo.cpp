
#include "Include/Buffers/SSBOs/CollideableGeometrySsbo.h"
#include "Include/Geometry/BlenderLoad.h"


/*-----------------------------------------------------------------------------------------------
Description:
    Calls the BlenderLoad object to load a set of lines from a Blender3D .obj file.  
Parameters: 
    blenderMeshFilePath     The path to the .obj file
Returns:    None
Creator:    John Cox, 6/2017
-----------------------------------------------------------------------------------------------*/
CollideableGeometrySsbo::CollideableGeometrySsbo(const std::string &blenderMeshFilePath)
{
    BlenderLoad bl(blenderMeshFilePath);
    printf("");
}

void CollideableGeometrySsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{

}
