#ifndef FRACTIONAL_STEP_MULTIPHASE_QUADRATIC_IBM_H
#define FRACTIONAL_STEP_MULTIPHASE_QUADRATIC_IBM_H

#include "FractionalStepMultiphase.h"
#include "Multiphase/Celeste.h"

class FractionalStepMultiphaseQuadraticIbm: public FractionalStepMultiphase
{
public:
    FractionalStepMultiphaseQuadraticIbm(const Input& input,
                             std::shared_ptr<FiniteVolumeGrid2D> &grid);

    virtual Scalar solve(Scalar timeStep);

    ScalarFiniteVolumeField &gammaCl;
    ScalarGradient &gradGammaCl;

protected:

    Scalar solveGammaEqn(Scalar timeStep);

    Scalar solveUEqn(Scalar timeStep);

    Scalar solvePEqn(Scalar timeStep);

    void correctVelocity(Scalar timeStep);

    void updateProperties(Scalar timeStep);
};

#endif
