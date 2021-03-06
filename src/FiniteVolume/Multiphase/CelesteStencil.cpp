#include "Celeste.h"

Celeste::CelesteStencil::CelesteStencil(const Cell &cell, bool weighted)
        :
        cellPtr_(&cell)
{
    init(weighted);
}

Celeste::CelesteStencil::CelesteStencil(const Cell &cell, const ImmersedBoundary &ib, bool weighted)
        :
        cellPtr_(&cell)
{
    init(ib, weighted);
}

void Celeste::CelesteStencil::init(bool weighted)
{
    weighted_ = weighted;
    truncated_ = false;

    const Cell &cell = *cellPtr_;

    reset();

    for (const InteriorLink &nb: cell.neighbours())
    {
        cells_.push_back(std::cref(nb.cell()));

        if (!cell.boundaries().empty())
            for (const BoundaryLink &bd: nb.cell().boundaries())
                faces_.push_back(std::cref(bd.face()));
    }

    for (const CellLink &dg: cell.diagonals())
        cells_.push_back(std::cref(dg.cell()));

    for (const BoundaryLink &bd: cell.boundaries())
        faces_.push_back(std::cref(bd.face()));

    initMatrix();
}

void Celeste::CelesteStencil::init(const ImmersedBoundary &ib, bool weighted)
{
    weighted_ = weighted;
    truncated_ = false;

    const Cell &cell = *cellPtr_;

    reset();

    for (const InteriorLink &nb: cell.neighbours())
    {
        auto ibObj = ib.ibObj(nb.cell().centroid());

        if (!ibObj)
        {
            cells_.push_back(std::cref(nb.cell()));

            if (!cell.boundaries().empty())
                for (const BoundaryLink &bd: nb.cell().boundaries())
                    faces_.push_back(std::cref(bd.face()));
        }
        else
        {
            compatPts_.push_back(
                    std::make_pair(std::cref(nb.cell()), std::weak_ptr<const ImmersedBoundaryObject>(ibObj)));
        }
    }

    for (const CellLink &dg: cell.diagonals())
    {
        auto ibObj = ib.ibObj(dg.cell().centroid());

        if (!ibObj)
            cells_.push_back(std::cref(dg.cell()));
        else
        {
            //compatPts_.push_back(std::make_pair(ibObj, dg.cell().centroid()));
            compatPts_.push_back(
                    std::make_pair(std::cref(dg.cell()), std::weak_ptr<const ImmersedBoundaryObject>(ibObj)));
        }
    }

    for (const BoundaryLink &bd: cell.boundaries())
        faces_.push_back(std::cref(bd.face()));

    initMatrix();
}

void Celeste::CelesteStencil::reset()
{
    cells_.clear();
    faces_.clear();
    compatPts_.clear();
}

Vector2D Celeste::CelesteStencil::grad(const ScalarFiniteVolumeField &phi) const
{
    Matrix b(pInv_.n(), 1);
    int i = 0;

    const Cell &cell = *cellPtr_;

    for (const Cell &kCell: cells_)
    {
        Scalar s = weighted_ ? (kCell.centroid() - cell.centroid()).magSqr() : 1.;
        b(i++, 0) = (phi(kCell) - phi(cell)) / s;
    }

    for (const Face &face: faces_)
    {
        Scalar s = weighted_ ? (face.centroid() - cell.centroid()).magSqr() : 1.;
        b(i++, 0) = (phi(face) - phi(cell)) / s;
    }

    b = pInv_ * b;
    return Vector2D(b(b.m() - 2, 0), b(b.m() - 1, 0));
}

Scalar Celeste::CelesteStencil::div(const VectorFiniteVolumeField &u) const
{
    Matrix b(pInv_.n(), 2);

    const Cell &cell = *cellPtr_;

    int i = 0;
    for (const Cell &kCell: cells_)
    {
        Scalar s = weighted_ ? (kCell.centroid() - cell.centroid()).magSqr() : 1.;
        Vector2D du = (u(kCell) - u(cell)) / s;
        b(i, 0) = du.x;
        b(i++, 1) = du.y;
    }

    for (const Face &face: faces_)
    {
        Scalar s = weighted_ ? (face.centroid() - cell.centroid()).magSqr() : 1.;
        Vector2D du = (u(face) - u(cell)) / s;
        b(i, 0) = du.x;
        b(i++, 1) = du.y;
    }

    b = pInv_ * b;
    return b(b.m() - 2, 0) + b(b.m() - 1, 1);
}

Scalar Celeste::CelesteStencil::kappa(const VectorFiniteVolumeField &n,
                                      const Celeste &fst) const
{
    return div(n);
}

Scalar Celeste::CelesteStencil::kappa(const VectorFiniteVolumeField &n,
                                      const ImmersedBoundary &ib,
                                      const Celeste &fst) const
{
    const Cell &cell = *cellPtr_;

    //- Check if it needs to be computed
    if(n(cell).magSqr() == 0.)
        return 0.;

    for(const Cell& cell: cells_)
        if(n(cell).magSqr() == 0.)
            return 0.;

    Matrix b(pInv_.n(), 2);
    int i = 0;
    for (const Cell &kCell: cells_)
    {
        Vector2D dn = n(kCell) - n(cell);
        Scalar s = weighted_ ? (kCell.centroid() - cell.centroid()).magSqr() : 1.;

        b(i, 0) = dn.x / s;
        b(i++, 1) = dn.y / s;
    }

    for (const Face &face: faces_)
    {
        Vector2D dn = n(face) - n(cell);
        Scalar s = weighted_ ? (face.centroid() - cell.centroid()).magSqr() : 1.;

        b(i, 0) = dn.x / s;
        b(i++, 1) = dn.y / s;
    }

    for (const auto &compatPt: compatPts_)
    {
        auto ibObj = compatPt.second.lock();
        Point2D pt = ibObj->intersectionLine(cell.centroid(), compatPt.first.get().centroid()).ptB();
        Vector2D dn = fst.contactLineNormal(cell, pt, *ibObj) - n(cell);
        Scalar s = weighted_ ? (compatPt.first.get().centroid() - cell.centroid()).magSqr() : 1.;

        b(i, 0) = dn.x / s;
        b(i++, 1) = dn.y / s;
    }

    b = pInv_ * b;
    return b(b.m() - 2, 0) + b(b.m() - 1, 1);
}

//- Private methods

void Celeste::CelesteStencil::initMatrix()
{
    Matrix A(cells_.size() + compatPts_.size() + faces_.size(), 5);
    const Cell &cell = *cellPtr_;

    int i = 0;
    for (const Cell &kCell: cells_)
    {
        Vector2D r = (kCell.centroid() - cell.centroid());
        Scalar s = weighted_ ? r.magSqr() : 1.;

        A.setRow(i++, {
                r.x * r.x / 2. / s,
                r.y * r.y / 2. / s,
                r.x * r.y / s,
                r.x / s,
                r.y / s
        });
    }

    for (const Face &face: faces_)
    {
        Vector2D r = face.centroid() - cell.centroid();
        Scalar s = weighted_ ? r.magSqr() : 1.;

        A.setRow(i++, {
                r.x * r.x / 2. / s,
                r.y * r.y / 2. / s,
                r.x * r.y / s,
                r.x / s,
                r.y / s
        });
    }

    for (const auto &compatPt: compatPts_)
    {
        Vector2D r = compatPt.first.get().centroid() - cell.centroid();
        Scalar s = weighted_ ? r.magSqr() : 1.;

        A.setRow(i++, {
                r.x * r.x / 2. / s,
                r.y * r.y / 2. / s,
                r.x * r.y / s,
                r.x / s,
                r.y / s
        });
    }

    pInv_ = pseudoInverse(A);
}