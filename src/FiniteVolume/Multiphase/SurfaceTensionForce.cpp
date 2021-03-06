#include "SurfaceTensionForce.h"
#include "GhostCellStencil.h"
#include "BilinearInterpolator.h"

SurfaceTensionForce::SurfaceTensionForce(const Input &input,
                                         const std::shared_ptr<const FiniteVolumeGrid2D> &grid,
                                         const std::weak_ptr<ImmersedBoundary> &ib)
        :
        VectorFiniteVolumeField(grid, "ft", Vector2D(0., 0.), true, false),
        ib_(ib),
        kappa_(std::make_shared<ScalarFiniteVolumeField>(grid_, "kappa")),
        gammaTilde_(std::make_shared<ScalarFiniteVolumeField>(grid_, "gammaTilde")),
        gradGammaTilde_(std::make_shared<ScalarGradient>(*gammaTilde_)),
        n_(std::make_shared<VectorFiniteVolumeField>(grid_, "n"))
{
    //- Input properties
    sigma_ = input.caseInput().get<Scalar>("Properties.sigma");
    kernelWidth_ = input.caseInput().get<Scalar>("Solver.smoothingKernelRadius");

    //- Determine which patches contact angles will be enforced on
    for (const auto &input: input.boundaryInput().get_child("Boundaries.gamma"))
    {
        if (input.first == "*" || !grid_->hasPatch(input.first))
            continue;

        patchContactAngles_.insert(std::make_pair(
                grid_->patch(input.first).id(),
                input.second.get<Scalar>("contactAngle", 90) * M_PI / 180.
        ));
    }

    //- Determine which IBs contact angles will be enforced on
    if (ib_.lock())
        for (const auto &ibObj: *ib_.lock())
        {
            ibContactAngles_[ibObj->id()] = input.boundaryInput().get<Scalar>(
                    "ImmersedBoundaries." + ibObj->name() + ".gamma.contactAngle", 90) * M_PI / 180.;
        }
}

void SurfaceTensionForce::computeInterfaceNormals()
{
    const VectorFiniteVolumeField &gradGammaTilde = *gradGammaTilde_;
    VectorFiniteVolumeField &n = *n_;

    for (const Cell &cell: grid_->cellZone("fluid"))
        n(cell) = gradGammaTilde(cell).magSqr() >= eps_ * eps_ ? -gradGammaTilde(cell).unitVec() : Vector2D(0., 0.);

    //- Boundary faces set from contact line orientation
    for (const Patch &patch: grid_->patches())
    {
        for (const Face &face: patch)
        {
            if (n(face.lCell()).magSqr() == 0.)
            {
                n(face) = Vector2D(0., 0.);
                continue;
            }

            Vector2D ns = -face.outwardNorm(face.lCell().centroid());
            Vector2D ts = (n(face.lCell()) - dot(n(face.lCell()), ns) * ns).unitVec();

            Scalar theta = this->theta(patch);

            n(face) = ns * std::cos(theta) + ts * std::sin(theta);
        }
    }

    grid_->sendMessages(n);
}

Scalar SurfaceTensionForce::theta(const Patch &patch) const
{
    auto it = patchContactAngles_.find(patch.id());
    return it != patchContactAngles_.end() ? it->second : M_PI_2;
}

Scalar SurfaceTensionForce::theta(const ImmersedBoundaryObject &ibObj) const
{
    auto it = ibContactAngles_.find(ibObj.id());
    return it != ibContactAngles_.end() ? it->second : M_PI_2;
}

Vector2D SurfaceTensionForce::contactLineNormal(const Vector2D &n,
                                                const Point2D &pt,
                                                const ImmersedBoundaryObject &ibObj) const
{
    Vector2D ns = -ibObj.nearestEdgeNormal(pt).unitVec();
    Vector2D ts = (n - dot(n, ns) * ns).unitVec();

    if (n.magSqr() == 0.)
        return n;

    Scalar theta = this->theta(ibObj);
    return ns * std::cos(theta) + ts * std::sin(theta);
}

Vector2D SurfaceTensionForce::contactLineNormal(const Cell &cell,
                                                const Point2D &pt,
                                                const ImmersedBoundaryObject &ibObj) const
{
    return contactLineNormal((*n_)(cell), pt, ibObj);
}

Vector2D SurfaceTensionForce::contactLineTangent(const Vector2D &n,
                                                 const Point2D &pt,
                                                 const ImmersedBoundaryObject &ibObj) const
{
    Vector2D ns = -ibObj.nearestEdgeNormal(pt).unitVec();
    Vector2D ts = (n - dot(n, ns) * ns).unitVec();

    if (n.magSqr() == 0.)
        return n;

    Scalar theta = this->theta(ibObj);
    return ns * std::sin(theta) - ts * std::cos(theta);
}

Vector2D SurfaceTensionForce::contactLineTangent(const Cell &cell,
                                                 const Point2D &pt,
                                                 const ImmersedBoundaryObject &ibObj) const
{
    return contactLineTangent((*n_)(cell), pt, ibObj);
}

void SurfaceTensionForce::smoothGammaField(const ScalarFiniteVolumeField &gamma)
{
    gammaTilde_->fill(0);

    if (ib_.lock())
    {
        CellGroup cellsToSmooth = grid_->localActiveCells() - ib_.lock()->solidCells();

        smooth(gamma,
               cellsToSmooth,
               grid_->globalCellGroup(cellsToSmooth),
               kernelWidth_,
               *gammaTilde_,
               [](const Cell &cell, const Cell &kCell, Scalar e)
               {
                   Scalar r = (cell.centroid() - kCell.centroid()).mag() / e;
                   return r < 1. ? std::cos(M_PI * r) + 1. : 0.;
               });

        grid_->sendMessages(*gammaTilde_);
    }
    else
    {
        smooth(gamma,
               grid_->localActiveCells(),
               grid_->globalActiveCells(),
               kernelWidth_,
               *gammaTilde_,
               [](const Cell &cell, const Cell &kCell, Scalar e)
               {
                   Scalar r = (cell.centroid() - kCell.centroid()).mag() / e;
                   return r < 1. ? std::cos(M_PI * r) + 1. : 0.;
               });
    }

    gammaTilde_->setBoundaryFaces();
}

Vector2D SurfaceTensionForce::computeCapillaryForce(const ScalarFiniteVolumeField &gamma,
                                                    const ImmersedBoundaryObject &ibObj) const
{
    typedef std::tuple<Point2D, Scalar> IbPoint;

    std::vector<IbPoint> ibPoints(ibObj.ibCells().size());
    std::transform(ibObj.ibCells().begin(), ibObj.ibCells().end(), ibPoints.begin(),
                   [this, &gamma, &ibObj](const Cell &cell)
                   {
                       Point2D pt = ibObj.nearestIntersect(cell.centroid());
                       Scalar g = BilinearInterpolator(grid_, pt)(gamma);
                       return std::make_tuple(pt, g);
                   });

    ibPoints = grid_->comm().gatherv(grid_->comm().mainProcNo(), ibPoints);

    Vector2D force = Vector2D(0., 0.);

    if (grid_->comm().isMainProc())
    {
        std::sort(ibPoints.begin(), ibPoints.end(), [&ibObj](const IbPoint &lhs, const IbPoint &rhs)
        {
            return (std::get<0>(lhs) - ibObj.shape().centroid()).angle()
                   < (std::get<0>(rhs) - ibObj.shape().centroid()).angle();
        });

        for (int i = 0; i < ibPoints.size(); ++i)
        {
            const auto &a = ibPoints[i];
            const auto &b = ibPoints[(i + 1) % ibPoints.size()];

            if ((std::get<1>(a) < 0.5) != (std::get<1>(b) <= 0.5))
            {
                Scalar alpha = (0.5 - std::get<1>(b)) / (std::get<1>(a) - std::get<1>(b));
                Point2D xcl = ibObj.nearestIntersect(alpha * std::get<0>(a) + (1. - alpha) * std::get<0>(b));
                Vector2D tcl = contactLineTangent(
                        std::get<1>(a) < std::get<1>(b) ?
                        std::get<0>(a) - std::get<0>(b) : std::get<0>(b) - std::get<0>(a),
                        xcl,
                        ibObj);

                force += sigma_ * tcl;
            }
        }

        std::cout << "Capillary force = " << force << "\n";
    }

    return grid_->comm().broadcast(grid_->comm().mainProcNo(), force);
}

Equation<Scalar> SurfaceTensionForce::contactLineBcs(ScalarFiniteVolumeField &gamma)
{
    Equation<Scalar> eqn(gamma);

    for (auto ibObj: *ib_.lock())
    {
        switch (ibObj->type())
        {
            case ImmersedBoundaryObject::GHOST_CELL:eqn += ibObj->contactLineBcs(gamma, theta(*ibObj));
                break;
            case ImmersedBoundaryObject::QUADRATIC:
            {
                Scalar theta = this->theta(*ibObj);

                for (const Cell &cell: ibObj->ibCells())
                {
                    Vector2D wn = -ibObj->nearestEdgeNormal(cell.centroid());

                    Ray2D r1 = Ray2D(cell.centroid(), wn.rotate(M_PI_2 - theta));
                    Ray2D r2 = Ray2D(cell.centroid(), wn.rotate(theta - M_PI_2));

                    GhostCellStencil m1(cell, ibObj->nearestIntersect(cell.centroid()), r1.r(), *grid_);
                    GhostCellStencil m2(cell, ibObj->nearestIntersect(cell.centroid()), r2.r(), *grid_);

                    Vector2D grad1 = m1.bpGrad(gamma);
                    Vector2D grad2 = m2.bpGrad(gamma);

                    if (dot(grad2, r2.r()) < dot(grad1, r1.r()))
                        std::swap(m1, m2);

                    if (theta > M_PI_2)
                        eqn.add(m1.cell(), m1.neumannCells(), m1.neumannCoeffs());
                    else
                        eqn.add(m2.cell(), m2.neumannCells(), m2.neumannCoeffs());
                }

                for (const Cell &cell: ibObj->solidCells())
                {
                    eqn.add(cell, cell, 1.);
                    eqn.setSource(cell, -1.);
                }
            }

                break;

            case ImmersedBoundaryObject::HIGH_ORDER:eqn += ibObj->contactLineBcs(gamma, theta(*ibObj));

                break;
            default:
                throw Exception("SurfaceTensionForce", "contactLineBcs", "unrecognized immersed boundary object type.");
        }
    }

    return eqn;
}
