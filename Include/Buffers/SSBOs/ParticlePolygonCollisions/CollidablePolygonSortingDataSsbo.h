#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Like ParticleSortingDataSsbo, but for the collidable geometry.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class CollidableGeometrySortingDataSsbo: public SsboBase
{
public:
    CollidableGeometrySortingDataSsbo(unsigned int numPolygons);
    ~CollidableGeometrySortingDataSsbo() = default;
    using SharedPtr = std::shared_ptr<CollidableGeometrySortingDataSsbo>;
    using SharedConstPtr = std::shared_ptr<const CollidableGeometrySortingDataSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumItems() const;

private:
    unsigned int _numItems;
};
