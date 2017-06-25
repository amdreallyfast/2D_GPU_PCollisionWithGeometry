#pragma once

#include "ThirdParty/glm/vec4.hpp"
#include "Include/Buffers/ParticleProperties.h"

/*------------------------------------------------------------------------------------------------
Description:
    This is a simple structure that says where a particle is, where it is going, and whether it
    has gone out of bounds ("is active" flag).  That flag also serves to prevent all particles
    from going out all at once upon creation by letting the "particle updater" regulate how many
    are emitted every frame.
Creator:    John Cox (7-2-2016)
------------------------------------------------------------------------------------------------*/
struct Particle
{
    /*-------------------------------------------------------------------------------------------
    Description:
        Sets initial values.  The glm structures have their own zero initialization.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 10-2-2016
    -------------------------------------------------------------------------------------------*/
    Particle() :
        _particleTypeIndex(ParticleProperties::ParticleType::NO_PARTICLE_TYPE),
        _numNearbyParticles(0),
        _isActive(0)
    {
    }

    glm::vec4 _currPos;
    glm::vec4 _prevPos;
    glm::vec4 _vel;

    // index into the ParticlePropertiesBuffer
    int _particleTypeIndex;

    // used for color
    int _numNearbyParticles;

    // if off (0), then it won't be updated
    int _isActive;

    // any necessary padding out to 16 bytes to match the GPU's version
    int _padding[1];

    glm::vec4 _extraVec1;
    glm::vec4 _extraVec2;
    glm::vec4 _extraVec3;
    glm::vec4 _extraVec4;
    glm::vec4 _extraVec5;
    glm::vec4 _extraVec6;
    glm::vec4 _extraVec7;
    glm::vec4 _extraVec8;

};
