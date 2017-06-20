#pragma once


/*------------------------------------------------------------------------------------------------
Description:
    One for each type of particle.  Rather than haul around particle mass and size (collision 
    radius) with particle position and velocity, it was decided that this was a property of the 
    type of particle, so the particle itself only needs to carry around the particle type.

Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
struct ParticleProperties
{
    /*--------------------------------------------------------------------------------------------
    Description:
        At this time (5/27/2017) there is only one type of particle.  Eventually (and hopefully), 
        this particle collision demo will handle more than 1, but for now there is only one.

        Note: This is a weakly-typed enum because 
        (1) it will be used as an index
        (2) it will be used in GLSL shaders, which only have integers and no enums, much less 
        "enum class".
        (3) There is no need to enforce strong typing past initialization, during which these 
        enums will be used as integers.

    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    enum ParticleType
    {
        // anything that auto-initializes to 0 will be a dud particle
        // Note: Yes, that does mean that there will be a slot in the ParticlePropertiesSsbo 
        // that has 0 mass and 0 collision radius.  This is intended.  The mass of 0 could cause 
        // a division to blow up, but if a particle was created with no type, then I want it to 
        // blow up so that I can fix it.
        NO_PARTICLE_TYPE = 0,
        GENERIC,
        NUM_PARTICLE_PROPERTIES,
    };

    ParticleProperties() :
        _mass(0.0f),
        _collisionRadius(0.0f)
    {

    }

    // Note: Do NOT attempt "inverse mass".  
    // Ex: 1/(0.05 + 0.05) != (1/0.05) + (1/0.05).
    float _mass;
    float _collisionRadius;
};
