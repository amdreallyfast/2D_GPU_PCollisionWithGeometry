

#include <iostream>
using std::cout;
using std::endl;

#include "Include/Geometry/BlenderLoad.h"
#include "ThirdParty/glload/include/glload/gl_4_4.h"


/*------------------------------------------------------------------------------------------------
Description:
    Lines have no normals in 3D space, or at least according to Blender3D's .obj files.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
struct LineVertices
{
    LineVertices(unsigned int p1Index, unsigned int p2Index) :
        _pos1Index(p1Index),
        _pos2Index(p2Index)
    {
    }

    unsigned int _pos1Index;
    unsigned int _pos2Index;
};

/*------------------------------------------------------------------------------------------------
Description:
    Each vertex on a face has a position, (possibly) a texture coordinate, and a normal.  This 
    only applies to 3D.

    This structure exists so that QuadVertices does not have to uniquely specify these three for 
    each of the quad's four vertices (12 items total).
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
struct RawVertex
{
    RawVertex(unsigned int pIndex, /*unsigned int tIndex,*/ unsigned int nIndex) :
        _pIndex(pIndex),
        //_tIndex(tIndex),
        _nIndex(nIndex)
    {
    }

    unsigned int _pIndex;
    //unsigned int _tIndex;
    unsigned int _nIndex;
};

/*------------------------------------------------------------------------------------------------
Description:
    Blender3D specifies faces as quads (4x vertices), but GL_QUADS has been deprecated in OpenGL 
    for a while now.  Triangles are the modern way to draw, so this structure serves as an 
    intermediatary between the .obj file's data and a function that can translate it i
    nto triangles.
    
    Note: GL_QUAD was deprecated in OpenGL 3.1 (2009).
    https://www.khronos.org/opengl/wiki/History_of_OpenGL
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
struct QuadVertices
{
    QuadVertices(const RawVertex &v1Indices, const RawVertex &v2Indices, 
        const RawVertex &v3Indices, const RawVertex &v4Indices) :
        _v1Indices(v1Indices),
        _v2Indices(v2Indices),
        _v3Indices(v3Indices),
        _v4Indices(v4Indices)
    {
    }

    RawVertex _v1Indices;
    RawVertex _v2Indices;
    RawVertex _v3Indices;
    RawVertex _v4Indices;
};

/*------------------------------------------------------------------------------------------------
Description:
    This is a middle ground between the Blender file's info and an ordered set of PolygonFace 
    objects that corresponds to a single Blender object.  All the information that is provided 
    by the former and that is necessary to generate the latter is held here.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
struct RawObjectData
{
    // if true, use _lineVertices, else use _quadVertices
    bool _isWireFrame;
    std::vector<glm::vec4> _vertPositions;
    //std::vector<glm::vec4> _vertTextureCoord;    // not supported (yet)
    std::vector<glm::vec4> _vertNormals;
    std::vector<LineVertices> _lineVertices;
    std::vector<QuadVertices> _quadVertices;
};

using RawObjectDataCollection = std::map<std::string, RawObjectData>;

/*-----------------------------------------------------------------------------------------------
Description:
    A support function that translates a vertex and normal collection into a PolygonFace 
    collection that this program can use in a PolygonFace-based SSBO.
Parameters: 
    rawObjectData   See description of RawObjectData.
Returns:    
    An ordered set of PolygonFace objects.
Creator:    John Cox, 6/2017
-----------------------------------------------------------------------------------------------*/
static BlenderLoad::PolygonCollection ParseRawBlenderData(const RawObjectData &rawObjectData)
{
    BlenderLoad::PolygonCollection newCollection;

    if (rawObjectData._isWireFrame)
    {
        // lines are fairly easy; no normals or texture coordinates, just lines
        for (size_t vertexIndex = 0; vertexIndex < rawObjectData._lineVertices.size(); vertexIndex++)
        {
            unsigned int p1Index = rawObjectData._lineVertices[vertexIndex]._pos1Index;
            unsigned int p2Index = rawObjectData._lineVertices[vertexIndex]._pos2Index;
            
            //glm::vec4 p1 = rawObjectData._vertPositions[p1Index];
            //glm::vec4 p2 = rawObjectData._vertPositions[p2Index];
            
            // no normals right now (TODO: ??these??)
            MyVertex v1(rawObjectData._vertPositions[p1Index], glm::vec4());
            MyVertex v2(rawObjectData._vertPositions[p2Index], glm::vec4());
            newCollection.push_back(PolygonFace(v1, v2));
        }
    }
    else  // read "faces, not lines"
    {
        // TODO: support these once in 3D
    }

    return newCollection;
}

/*-----------------------------------------------------------------------------------------------
Description:
    Loads a set of vertices from a .obj file.  
    
    Note: Blender3D has the option, when exporting the scene, to use +Y as "up" or +Z as "up".  
    The .obj file itself says nothing about the matter.  This loading function will assume that 
    +Z is set as up since that is the way that OpenGL does it in 3D.

    Also Note: The .obj file does not specify any transform, so all the vertices and normals in 
    a .obj file are "as is".  Since this demo operates in window space (X and Y on the range 
    [-1,+1]) and on the depth range 0 to -1, the objects in the file must be constrained within 
    those bounds.
    
Parameters: 
    filePath    The path to the .obj file
Returns:    None
Creator:    John Cox (originally 2-13-2016, modified 6-21-2017 for this project)
-----------------------------------------------------------------------------------------------*/
BlenderLoad::BlenderLoad(const std::string &filePath)
{
    std::ifstream fileStream(filePath, std::ios::in);
    if (!fileStream.is_open())
    {
        cout << "BlenderLoad(...): Could not find the .obj file " << filePath << endl;
        return;
    }

    // parse the file 1 line at a time
    std::string line;

    // make sure it's a .OBJ file
    std::getline(fileStream, line);
    if (line.find("OBJ") == -1)
    {
        cout << "File is not a Blender .obj file " << filePath << endl;
        return;
    }

    // skip "www.blender.org"
    std::getline(fileStream, line);

    // skip material file (don't care in this demo)
    std::getline(fileStream, line);

    // parse it line by line
    // Note: Each line of data is preceded by a 1-char symbol.
    // Also Note: It is possible for a single object to have both an "f" and an "l", but if this 
    // has happened, this means that there is a mix of 2D and 3D vertices and something has gone 
    // wrong when manipulating the object in Blender.  For the sake of this demo, assume that if 
    // the object specifies "f" then it has faces and if it specifies "l" then it has lines.
    std::string lineHeaderObjectName("o ");
    std::string lineHeaderVertexPosition("v ");
    std::string lineHeaderVertexNormal("vn ");
    std::string lineHeaderFace("f ");
    std::string lineHeaderLine("l ");
    std::string lineHeaderUseMaterial("usemtl ");
    std::string lineHeaderSmoothShading("s ");

    RawObjectDataCollection rawNumbersMap;
    std::string currentObjectName;
    while (std::getline(fileStream, line))
    {
        if (line.substr(0, lineHeaderObjectName.length()) == lineHeaderObjectName)
        {
            // new object; extract the object name
            currentObjectName = line.substr(lineHeaderObjectName.length());
            rawNumbersMap[currentObjectName] = RawObjectData();
        }
        else if (line.substr(0, lineHeaderVertexPosition.length()) == lineHeaderVertexPosition)
        {
            // as stated in the function description, expecting +Z to be up
            float x;
            float y;
            float z;
            std::string subStr = line.substr(lineHeaderVertexPosition.length());
            const char *readBuffer = subStr.c_str();
            sscanf(readBuffer, "%f %f %f", &x, &y, &z);
            rawNumbersMap[currentObjectName]._vertPositions.push_back(glm::vec4(x, y, z, 1.0f));
        }
        else if (line.substr(0, lineHeaderVertexNormal.length()) == lineHeaderVertexNormal)
        {
            float x;
            float y;
            float z;
            std::string subStr = line.substr(lineHeaderVertexPosition.length());
            const char *readBuffer = subStr.c_str();
            sscanf(readBuffer, "%f %f %f", &x, &y, &z);
            rawNumbersMap[currentObjectName]._vertNormals.push_back(glm::vec4(x, y, z, 0.0f));
        }
        else if (line.substr(0, lineHeaderLine.length()) == lineHeaderLine)
        {
            // data comes in lines
            rawNumbersMap[currentObjectName]._isWireFrame = true;

            // 2D lines in Blender only have positions, not normals nor texture coordinates
            unsigned short p1Index = 0;
            unsigned short p2Index = 0;

            // the 'h' indicates the reading of a short integer
            // Note: http://www.cprogramming.com/tutorial/printf-format-strings.html
            std::string subStr = line.substr(lineHeaderLine.length());
            const char *readBuffer = subStr.c_str();
            sscanf(readBuffer, "%hd %hd", &p1Index, &p2Index);

            // -1 because Blender3D indexes start at 1, not 0
            rawNumbersMap[currentObjectName]._lineVertices.push_back(
                LineVertices(p1Index - 1, p2Index - 1));
        }
        else if (line.substr(0, lineHeaderFace.length()) == lineHeaderFace)
        {
            // data comes in quads (4 vertexes)
            // Note: I am assuming (for demo) that it only has quads, but quads have been 
            // deprecated for years, so I need to turn the quads into triangles.
            rawNumbersMap[currentObjectName]._isWireFrame = false;

            // 'p' = position, 't' texture coordinate, 'n' = normal
            // Note: Texture coordinates not available yet, but handling them here anyway.
            unsigned short p1Index = 0, t1Index = 0, n1Index = 0;
            unsigned short p2Index = 0, t2Index = 0, n2Index = 0;
            unsigned short p3Index = 0, t3Index = 0, n3Index = 0;
            unsigned short p4Index = 0, t4Index = 0, n4Index = 0;

            // the 'h' indicates the reading of a short integer
            // Note: http://www.cprogramming.com/tutorial/printf-format-strings.html
            std::string subStr = line.substr(lineHeaderFace.length());
            const char *readBuffer = subStr.c_str();
            sscanf_s(readBuffer, "%hd/%hd/%hd %hd/%hd/%hd %hd/%hd/%hd %hd/%hd/%hd",
                &p1Index, &t1Index, &n1Index,
                &p2Index, &t2Index, &n2Index,
                &p3Index, &t3Index, &n3Index,
                &p4Index, &t4Index, &n4Index);

            // there is always position and normal, but not always texture
            // Note: The face indices begin at 1 (not 0), so if the index is 0, then there is no 
            // value.
            if (t1Index == 0 || t2Index == 0 || t3Index == 0 || t4Index == 0)
            {
                // no texture coordinates, so run the parse again
                sscanf(readBuffer, "%hd//%hd %hd//%hd %hd//%hd %hd//%hd",
                    &p1Index, &n1Index,
                    &p2Index, &n2Index,
                    &p3Index, &n3Index,
                    &p4Index, &n4Index);
            }

            // -1 because Blender3D indexes start at 1, not 0
            RawVertex v1(p1Index, n1Index);
            RawVertex v2(p2Index, n2Index);
            RawVertex v3(p3Index, n3Index);
            RawVertex v4(p4Index, n4Index);

            rawNumbersMap[currentObjectName]._quadVertices.push_back(
                QuadVertices(v1, v2, v3, v4));
        }
        else if (line.substr(0, lineHeaderUseMaterial.length()) == lineHeaderUseMaterial)
        {
            // materials ignored in this demo
        }
        else if (line.substr(0, lineHeaderSmoothShading.length()) == lineHeaderSmoothShading)
        {
            // smooth shading ignored in this demo
        }
        else
        {
            // ??unknown header??
            cout << "unknown line header for line '" << line << "'" << endl;
            // ??return false??
        }
    }

    for (RawObjectDataCollection::const_iterator itr = rawNumbersMap.begin(); 
        itr != rawNumbersMap.end(); itr++)
    {
        _allGeometry[itr->first] = ParseRawBlenderData(itr->second);
    }
    printf("");
}

/*------------------------------------------------------------------------------------------------
Description:
    A getter so that some SSBO can get at the vertex data.
Parameters: None
Returns:    
    A const reference to this object's geometry collection as parsed from a Blender3D .obj file.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
const BlenderLoad::GeometryByObject &BlenderLoad::Geometry() const
{
    return _allGeometry;
}
