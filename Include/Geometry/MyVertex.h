#pragma once

#include "ThirdParty/glm/vec4.hpp"

/*-----------------------------------------------------------------------------------------------
Description:
    Stores all info necessary to draw a single vertex.
Creator: John Cox (6-12-2016)
-----------------------------------------------------------------------------------------------*/
struct MyVertex
{
    /*-------------------------------------------------------------------------------------------
    Description:
        Does nothing.  The GLM objects have their own constructors.  The only reason this exists 
        is to allow a default constructor.
    Parameters:
        pos     The particle's position in window space.  
        normal  Self-explanatory.
    Returns:    None
    Creator: John Cox, 9-25-2016
    -------------------------------------------------------------------------------------------*/
    MyVertex()
    {

    }

    /*-------------------------------------------------------------------------------------------
    Description:
        Ensures that the object starts object with initialized values.
    Parameters:
        pos     The particle's position in window space.  
        normal  Self-explanatory.
    Returns:    None
    Creator: John Cox, 9-25-2016
    -------------------------------------------------------------------------------------------*/
    MyVertex(const glm::vec4 &pos, const glm::vec4 &normal) :
        _position(pos),
        _normal(normal)
    {

    }



    // even though this is a 2D program at this time (and still is as of 5-3-2017), vec4s were 
    // chosen because it is easier than trying to match GLSL's 16-bytes-per-variable with arrays 
    // of dummy floats
    glm::vec4 _position;
    glm::vec4 _normal;
};

