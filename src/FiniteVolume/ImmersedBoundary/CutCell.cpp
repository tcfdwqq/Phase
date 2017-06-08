#include "CutCell.h"
#include "LineSegment2D.h"

CutCell::CutCell(const Cell &cell)
    :
      cell_(cell)
{
    for (const InteriorLink &nb: cell.neighbours())
        cutCellLinks_.push_back(CutCellLink(cell, nb.face(), nb.cell()));
}

CutCell::CutCell(const Cell &cell, const ImmersedBoundaryObject &ibObj)
    :
      cell_(cell),
      ibObj_(&ibObj)
{
    std::vector<Point2D> fluidVerts, solidVerts, xcs;
    const Polygon &cellShape = cell.shape();

    auto vtxA = cellShape.vertices().begin();
    auto vtxB = vtxA + 1;

    for (; vtxB != cellShape.vertices().end(); ++vtxA, ++vtxB)
    {
        LineSegment2D edge(*vtxA, *vtxB);
        bool inSolid = ibObj.shape().isInside(edge.ptA());

        if (inSolid)
            solidVerts.push_back(edge.ptA());
        else
            fluidVerts.push_back(edge.ptA());

        auto xc = ibObj.shape().intersections(edge);

        if(xc.size() > 1)
            xc.pop_back();

        if (xc.size() == 1)
        {
            if(xcs.size() == 2)
            {
                Scalar sNormMagSqr = (xcs[1] - xcs[0]).magSqr();
                Scalar distSqrVertexA = (xc[0] - xcs[0]).magSqr();
                Scalar distSqrVertexB = (xc[0] - xcs[1]).magSqr();

                if(distSqrVertexB > sNormMagSqr)
                    xcs[0] = xc[0];
                else if(distSqrVertexA > sNormMagSqr)
                    xcs[1] = xc[0];
            }
            else
                xcs.push_back(xc[0]);

            solidVerts.push_back(xc[0]);
            fluidVerts.push_back(xc[0]);
        }
        else if (xc.size() > 1)
        {
            for (Point2D pt: xc)
                std::cout << pt << std::endl;

            throw Exception("CutCell", "CutCell", "too many face intersections!");
        }
    }

    solid_ = Polygon(solidVerts.begin(), solidVerts.end());
    fluid_ = Polygon(fluidVerts.begin(), fluidVerts.end());

    if (xcs.size() == 2) // Only a boundary face if at least two intersections were found
    {
        bFace_ = LineSegment2D(xcs[0], xcs[1]);
        bFaceNorm_ = (xcs[1] - xcs[0]).normalVec();

        if(dot(bFace_.center() - fluid_.centroid(), bFaceNorm_) < 0) //- Should point away from fluid
        {
            bFace_ = LineSegment2D(bFace_.ptB(), bFace_.ptA());
            bFaceNorm_ = -bFaceNorm_;
        }
    }
    else if (xcs.size() > 2) // Invalid solution
    {
        for (Point2D pt: xcs)
            std::cout << pt << std::endl;

        std::cout << cell.id() << std::endl;

        throw Exception("CutCell", "CutCell", "too many cell intersections!");
    }

    for (const InteriorLink &nb: cell.neighbours())
        cutCellLinks_.push_back(CutCellLink(cell, nb.face(), nb.cell(), ibObj.shape()));

//    if(cell.id() == 12059 || cell.id() == 19724)
//    {
//        std::cout << "Cell id = " << cell.id() << "\n"
//                  << "Fluid vol = " << fluid_.area() << " Solid vol = " << solid_.area() << "\n"
//                  << "Face fractions...\n";

//        for(const CutCellLink& nb: this->neighbours())
//            std::cout << nb.zeta() << std::endl;
//    }
}

bool CutCell::isSmall() const
{
    return fluidVolume() / cell_.volume() <= 0.5;
}

const std::vector<Ref<const CutCellLink> > CutCell::neighbours(const CellGroup &cellGroup) const
{
    std::vector<Ref<const CutCellLink>> cutCells;

    for(const CutCellLink& nb: cutCellLinks_)
        if(cellGroup.isInGroup(nb.cell()))
            cutCells.push_back(std::cref(nb));

    return cutCells;
}
