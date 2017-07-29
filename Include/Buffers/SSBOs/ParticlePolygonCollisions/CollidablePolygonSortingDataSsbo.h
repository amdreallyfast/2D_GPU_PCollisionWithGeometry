#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Like ParticleSortingDataSsbo, but for the collidable geometry.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class CollidablePolygonSortingDataSsbo: public SsboBase
{
public:
    CollidablePolygonSortingDataSsbo(unsigned int numPolygons);
    ~CollidablePolygonSortingDataSsbo() = default;
    using SharedPtr = std::shared_ptr<CollidablePolygonSortingDataSsbo>;
    using SharedConstPtr = std::shared_ptr<const CollidablePolygonSortingDataSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumItems() const;

private:
    unsigned int _numItems;
};
