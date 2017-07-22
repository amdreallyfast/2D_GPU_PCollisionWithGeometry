#pragma once

#include "Include/Geometry/PolygonFace.h"


/*------------------------------------------------------------------------------------------------
Description:
    This structure holds the vertices necessary to draw a 2D bounding box.

    Note: No constructor necessary because PolygonFace has its own 0-initializers.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
struct Box2D
{
    PolygonFace _left;
    PolygonFace _right;
    PolygonFace _top;
    PolygonFace _bottom;
};
