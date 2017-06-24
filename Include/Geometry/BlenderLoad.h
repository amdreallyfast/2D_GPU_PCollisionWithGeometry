#pragma once

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include "Include/Geometry/PolygonFace.h"


/*-----------------------------------------------------------------------------------------------
Description:
    Loads a set of vertexes for each object in an exported Blender3D file into a single 
    collection.  

    Note: When Blender defines faces and lines, almost every single face-normal pair is unique.  
    This means that there is little benefit to using element arrays, which were created to 
    allow re-use of existing vertices.  Instead, I will simply use the vertex arrays.
Creator:    John Cox (10-23-2016)
-----------------------------------------------------------------------------------------------*/
class BlenderLoad
{
public:
    using PolygonCollection = std::vector<PolygonFace>;
    using GeometryByObject = std::map<std::string, PolygonCollection>;

    BlenderLoad(const std::string &filePath);

    const GeometryByObject &Geometry() const;

private:
    GeometryByObject _allGeometry;
};