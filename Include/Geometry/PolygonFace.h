#pragma once

#include "MyVertex.h"

/*-----------------------------------------------------------------------------------------------
Description:
    This is a simple structure that describes the face of a 2D polygon.  The face begins at P1 
    and ends at P2.  

    Note: I tried to determine if such as structure was necessary or if I could simply use an 
    array of MyVertex values.  I decided to go with the Polygon as an encapsulation of 2 
    vertices for the sake of the compute shaders, in which this structure makes the code more 
    readable.

    Also Note: Using the name "PolygonFace" instead of simply "Polygon" because there is an old 
    winapi C-era function pointer called Polygon.
Creator:    John Cox (9-8-2016)
-----------------------------------------------------------------------------------------------*/
struct PolygonFace
{
    /*-------------------------------------------------------------------------------------------
    Description:
        Gives members default values.  This default constructor was created so that a 
        std::vector<...> of them could be created with null values and then shoved into an SSBO 
        for alteration in a compute shader.
    Parameters: None
    Returns:    None
    Creator: John Cox, 1-16-2017
    -------------------------------------------------------------------------------------------*/
    PolygonFace() :
        _start(glm::vec4(), glm::vec4()),
        _end(glm::vec4(), glm::vec4())
    {
    }

    /*-------------------------------------------------------------------------------------------
    Description:
        Gives members initial values to describe the face of a 2D polygon face.  Without the 
        normals, this would just be a 2D line, but with surface normals the line is technically 
        considered a face.
    Parameters: 
        start   Self-eplanatory.
    Returns:    None
    Creator: John Cox, 9-25-2016
    -------------------------------------------------------------------------------------------*/
    PolygonFace(const MyVertex &start, const MyVertex &end) :
        _start(start),
        _end(end)
    {
    }

    /*-------------------------------------------------------------------------------------------
    Description:
        So that PolygonSsbo doesn't have to hardcode a multiplyer for the number of vertices per 
        polygon.
    Parameters: None
    Returns:    None
    Creator: John Cox, 5/2017
    -------------------------------------------------------------------------------------------*/
    static int NumVerticesPerFace() 
    {
        return 2;
    }

    MyVertex _start;
    MyVertex _end;
};
