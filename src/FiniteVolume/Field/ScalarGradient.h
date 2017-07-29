#ifndef SCALAR_GRADIENT_H
#define SCALAR_GRADIENT_H

#include "VectorFiniteVolumeField.h"

class ScalarGradient: public VectorFiniteVolumeField
{
public:
    explicit ScalarGradient(const ScalarFiniteVolumeField& phi);

    void computeFaces();

    void compute(const CellGroup& cells);

private:
    const ScalarFiniteVolumeField& phi_;
};

#endif