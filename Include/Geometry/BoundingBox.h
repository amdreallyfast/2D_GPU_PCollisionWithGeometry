#pragma once

#include "Include/Geometry/PolygonFace.h"


/*------------------------------------------------------------------------------------------------
Description:
    This structure holds the vertices necessary to draw a 2D bounding box (just a box, really, 
    but it's only used for bounding boxes at this time (6-13-2017), so I'm calling it a bounding 
    box).

    Note: No constructor necessary because PolygonFace has its own 0-initializers.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
struct BoundingBox
{
    PolygonFace _left;
    PolygonFace _right;
    PolygonFace _top;
    PolygonFace _bottom;
};
