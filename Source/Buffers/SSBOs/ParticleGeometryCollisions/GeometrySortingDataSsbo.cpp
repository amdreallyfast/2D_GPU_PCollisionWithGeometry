#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Like ParticleSortingDataSsbo, but for the collidable geometry.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class GeometrySortingDataSsbo: public SsboBase
{
public:
    GeometrySortingDataSsbo(unsigned int numPolygons);
    ~GeometrySortingDataSsbo() = default;
    using SharedPtr = std::shared_ptr<GeometrySortingDataSsbo>;
    using SharedConstPtr = std::shared_ptr<const GeometrySortingDataSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumItems() const;

private:
    unsigned int _numItems;
};
