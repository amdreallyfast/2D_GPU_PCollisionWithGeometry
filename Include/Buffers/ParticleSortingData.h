#pragma once

/*------------------------------------------------------------------------------------------------
Description:
    This struct exists for two reasons:
    (1) Various attempts to move particles around on every loop of the parallel sorting routine 
        have demonstrated an ~2/3rds performance drop compared to using a lightweight 
        intermediate structure like this.
    (2) Particles are sorted over Morton Codes, and the codes are only used during particle 
        collision detection, so particles should not be required to carry them around at all 
        times.
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
struct ParticleSortingData
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Initializes members to 0.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    ParticleSortingData::ParticleSortingData() :
        _sortingData(0),
        _preSortedParticleIndex(0)
    {
    }

    // used for a "radix" algorithm, so this should be unsigned
    unsigned int _sortingData;

    // used to fish out the unsorted particle that this object was created from so that it can 
    // be moved to the sorted position
    int _preSortedParticleIndex;

    // no GLSL-native structures on the shader side, so no padding necessary
};