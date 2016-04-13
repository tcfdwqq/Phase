#include "Multiphase.h"

Multiphase::Multiphase(const FiniteVolumeGrid2D &grid, const Input &input)
    :
      Piso(grid, input),
      gamma(grid.addScalarField(input, "gamma")),
      gammaEqn_(gamma, "gamma")
{
    rho1_ = input.caseInput().get<Scalar>("Properties.rho1");
    rho2_ = input.caseInput().get<Scalar>("Properties.rho2");
    mu1_ = input.caseInput().get<Scalar>("Properties.mu1");
    mu2_ = input.caseInput().get<Scalar>("Properties.mu2");
    sigma_ = input.caseInput().get<Scalar>("Properties.sigma");

    input.setInitialConditions(grid);

    computeRho();
    computeMu();
}

Scalar Multiphase::solve(Scalar timeStep)
{
    computeRho();
    computeMu();

    Piso::solve(timeStep);

    solveGammaEqn(timeStep);
}

//- Private methods

void Multiphase::computeRho()
{
    for(const Cell& cell: rho.grid.cells)
    {
        size_t id = cell.id();
        rho[id] = rho1_*(1. - gamma[id]) + rho2_*gamma[id];
    }

    interpolateFaces(rho);
}

void Multiphase::computeMu()
{
    for(const Cell& cell: mu.grid.cells)
    {
        size_t id = cell.id();
        mu[id] = mu1_*(1. - gamma[id]) + mu2_*gamma[id];
    }

    interpolateFaces(mu);
}

Scalar Multiphase::solveGammaEqn(Scalar timeStep)
{
    gamma.save();
    gammaEqn_ = (fv::ddt(gamma, timeStep) + fv::div(u, gamma) == 0.);
    return gammaEqn_.solve();
}

//- External functions

namespace hc
{

Equation<ScalarFiniteVolumeField> div(const VectorFiniteVolumeField &u, ScalarFiniteVolumeField &field)
{
    Equation<ScalarFiniteVolumeField> eqn(field);

    for(const Cell &cell: field.grid.cells)
    {
        for(const InteriorLink &nb: cell.neighbours())
        {
            if(dot(nb.rCellVec(), u.faces()[nb.face().id()]) < 0.) // check to see if current cell is a donor cell for this face
                continue;


        }
    }

    return eqn;
}

}
