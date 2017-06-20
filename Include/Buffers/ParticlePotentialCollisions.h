#pragma once

#include "Shaders/Compute/ParticleCollisions/MaxNumPotentialCollisions.comp"


/*------------------------------------------------------------------------------------------------
Description:   
    Must match the corresponding structure in ParticlePotentialCollisionsBuffer.comp.

    This object is the blueprint for an array of potential collisions that each particle is 
    engaged in.  This buffer was created because potential collision candidates is used only 
    during collision detection and handling, so it did not make much sense for a particle to be 
    carrying around a list of potential collisions.
    
    In an earlier version of this project, particles could only record 1 collision, so collision 
    detection required sorting through each potential collision and deciding if it was the one 
    (simple analysis: greatest area of overlap).  By creating this SSBO, collision detection 
    only needs to dump all potential collisions into this buffer.  This is better encapsulation 
    of responsibility.  Collision detection should only perform detection, not deciding if a 
    particle should actually collide.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
struct ParticlePotentialCollisions
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Gives members initial values.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    ParticlePotentialCollisions() :
        _numPotentialCollisions(0)
    {
        for (size_t i = 0; i < MAX_NUM_POTENTIAL_COLLISIONS; i++)
        {
            _particleIndexes[i] = -1;
        }
    }

    int _numPotentialCollisions;
    int _particleIndexes[MAX_NUM_POTENTIAL_COLLISIONS];
};

