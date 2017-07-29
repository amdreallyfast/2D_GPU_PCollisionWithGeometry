#pragma once

/*------------------------------------------------------------------------------------------------
Description:
    This struct exists for two reasons:
    (1) Various attempts to move particles around on every loop of the parallel sorting routine 
        have demonstrated an ~2/3rds performance drop (~60fps -> ~20fps) compared to using a 
        lightweight intermediate structure like this.
    (2) Particles and collidable geometry are sorted over Morton Codes, and the codes are only 
        used during sorting and BVH generation, so the objects being sorted should not be 
        required to carry these values around at all times.

    Therefore make a buffer.  This is a single entry in such a buffer.  See 
    ParticleSortingDataBuffer.comp and CollidablePolygonSortingDataBuffer.comp.
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
struct SortingData
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Initializes members to 0.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    SortingData::SortingData() :
        _sortingData(0),
        _preSortedIndex(0)
    {
    }

    // used for a "radix" algorithm, so this should be unsigned
    unsigned int _sortingData;

    // used to fish out the unsorted thing that this object was created from so that it can 
    // be moved to the sorted position
    int _preSortedIndex;

    // no GLSL-native structures on the shader side, so no padding necessary
};