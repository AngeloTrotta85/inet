//
// Copyright (C) 2013 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "IRadioMedium.h"
#include "ObstacleLoss.h"
#include "PhysicalObject.h"

namespace inet {

namespace physicallayer {

Define_Module(ObstacleLoss);

ObstacleLoss::ObstacleLoss() :
    medium(NULL),
    environment(NULL),
    leaveIntersectionTrail(false),
    intersectionTrail(NULL),
    intersectionComputationCount(0),
    intersectionCount(0)
{
}

void ObstacleLoss::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        medium = check_and_cast<IRadioMedium *>(getParentModule());
        const char *environmentModule = par("environmentModule");
        environment = check_and_cast<PhysicalEnvironment *>(simulation.getModuleByPath(environmentModule));
        leaveIntersectionTrail = par("leaveIntersectionTrail");
        if (leaveIntersectionTrail) {
            intersectionTrail = new TrailFigure(100, "obstacle intersection trail");
            cCanvas *canvas = simulation.getSystemModule()->getCanvas();
            canvas->addFigure(intersectionTrail, canvas->findFigure("submodules"));
        }
    }
}

void ObstacleLoss::printToStream(std::ostream& stream) const
{
    stream << "obstacle loss";
}

double ObstacleLoss::computeDielectricLoss(const Material *material, Hz frequency, m distance) const
{
    double lossTangent = material->getDielectricLossTangent(frequency);
    mps propagationSpeed = material->getPropagationSpeed();
    double factor = std::exp(-atan(lossTangent) * unit(2 * M_PI * frequency * distance / propagationSpeed).get());
    ASSERT(0 <= factor && factor <= 1);
    return factor;
}

double ObstacleLoss::computeReflectionLoss(const Material *incidentMaterial, const Material *refractiveMaterial, double angle) const
{
    // NOTE: based on http://en.wikipedia.org/wiki/Fresnel_equations
    double n1 = incidentMaterial->getRefractiveIndex();
    double n2 = refractiveMaterial->getRefractiveIndex();
    double st = sin(angle);
    double ct = cos(angle);
    double n1ct = n1 * ct;
    double n2ct = n2 * ct;
    double k = sqrt(1 - pow(n1 / n2 * st, 2));
    double n1k = n1 * k;
    double n2k = n2 * k;
    double rs = pow((n1ct - n2k) / (n1ct + n2k), 2);
    double rp = pow((n1k - n2ct) / (n1k + n2ct), 2);
    double r = (rs + rp) / 2;
    double transmittance = 1 - r;
    ASSERT(0 <= transmittance && transmittance <= 1);
    return transmittance;
}

void ObstacleLoss::finish()
{
    std::cout << "Number of intersection computation attempt: " << intersectionComputationCount << endl;
    std::cout << "Number of successful intersection computation: " << intersectionCount << endl;
}

double ObstacleLoss::computeObstacleLoss(Hz frequency, const Coord& transmissionPosition, const Coord& receptionPosition) const
{
    double totalLoss = 1;
    if (environment->hasObjectCache())
    {
        ObstacleLossVisitor *obstacleLossVisitor = new ObstacleLossVisitor(this, frequency, transmissionPosition, receptionPosition);
        environment->visitObjects(obstacleLossVisitor, LineSegment(transmissionPosition, receptionPosition));
        totalLoss = obstacleLossVisitor->getTotalLoss();
        delete obstacleLossVisitor;
    }
    else
    {
        const std::vector<PhysicalObject *>& objects = environment->getObjects();
        for (std::vector<PhysicalObject *>::const_iterator it = objects.begin(); it != objects.end(); it++) {
            const PhysicalObject *object = *it;
            const Shape *shape = object->getShape();
            const Coord& obstaclePosition = object->getPosition();
            const LineSegment lineSegment(transmissionPosition - obstaclePosition, receptionPosition - obstaclePosition);
            Coord intersection1, intersection2, normal1, normal2;
            intersectionComputationCount++;
            if (shape->computeIntersection(lineSegment, intersection1, intersection2, normal1, normal2))
            {
                intersectionCount++;
    #ifdef __CCANVAS_H
                if (leaveIntersectionTrail) {
                    cLineFigure *intersectionLine = new cLineFigure();
                    intersectionLine->setStart(environment->computeCanvasPoint(intersection1 + obstaclePosition));
                    intersectionLine->setEnd(environment->computeCanvasPoint(intersection2 + obstaclePosition));
                    intersectionLine->setLineColor(cFigure::GREY);
                    intersectionTrail->addChildFigure(intersectionLine);
                    cLineFigure *normal1Line = new cLineFigure();
                    normal1Line->setStart(environment->computeCanvasPoint(intersection1 + obstaclePosition));
                    normal1Line->setEnd(environment->computeCanvasPoint(intersection1 + obstaclePosition + normal1));
                    normal1Line->setLineColor(cFigure::RED);
                    intersectionTrail->addChildFigure(normal1Line);
                    cLineFigure *normal2Line = new cLineFigure();
                    normal2Line->setStart(environment->computeCanvasPoint(intersection2 + obstaclePosition));
                    normal2Line->setEnd(environment->computeCanvasPoint(intersection2 + obstaclePosition + normal2));
                    normal2Line->setLineColor(cFigure::RED);
                    intersectionTrail->addChildFigure(normal2Line);
                }
    #endif
                const Material *material = object->getMaterial();
                totalLoss *= computeDielectricLoss(material, frequency, m(intersection2.distance(intersection1)));
                if (normal1 != Coord(0,0,0)) {
                    double angle1 = (intersection1 - intersection2).angle(normal1);
                    totalLoss *= computeReflectionLoss(medium->getMaterial(), material, angle1);
                }
                // TODO: this returns NaN because n1 > n2
    //            if (!normal2.isUnspecified()) {
    //                double angle2 = (intersection2 - intersection1).angle(normal2);
    //                totalLoss *= computeReflectionLoss(material, medium->getMaterial(), angle2);
    //            }
            }
        }
    }
    return totalLoss;
}

void ObstacleLoss::ObstacleLossVisitor::visit(const cObject *object) const
{
    PhysicalObject *physicalObject = check_and_cast<PhysicalObject *>(object);
    const Shape *shape = physicalObject->getShape();
    const Coord& obstaclePosition = physicalObject->getPosition();
    const LineSegment lineSegment(transmissionPosition - obstaclePosition, receptionPosition - obstaclePosition);
    Coord intersection1, intersection2, normal1, normal2;
    obstacleLoss->intersectionComputationCount++;
    if (shape->computeIntersection(lineSegment, intersection1, intersection2, normal1, normal2))
    {
        obstacleLoss->intersectionCount++;
#ifdef __CCANVAS_H
        if (obstacleLoss->leaveIntersectionTrail) {
            cLineFigure *intersectionLine = new cLineFigure();
            intersectionLine->setStart(obstacleLoss->environment->computeCanvasPoint(intersection1 + obstaclePosition));
            intersectionLine->setEnd(obstacleLoss->environment->computeCanvasPoint(intersection2 + obstaclePosition));
            intersectionLine->setLineColor(cFigure::GREY);
            obstacleLoss->intersectionTrail->addChildFigure(intersectionLine);
            cLineFigure *normal1Line = new cLineFigure();
            normal1Line->setStart(obstacleLoss->environment->computeCanvasPoint(intersection1 + obstaclePosition));
            normal1Line->setEnd(obstacleLoss->environment->computeCanvasPoint(intersection1 + obstaclePosition + normal1));
            normal1Line->setLineColor(cFigure::RED);
            obstacleLoss->intersectionTrail->addChildFigure(normal1Line);
            cLineFigure *normal2Line = new cLineFigure();
            normal2Line->setStart(obstacleLoss->environment->computeCanvasPoint(intersection2 + obstaclePosition));
            normal2Line->setEnd(obstacleLoss->environment->computeCanvasPoint(intersection2 + obstaclePosition + normal2));
            normal2Line->setLineColor(cFigure::RED);
            obstacleLoss->intersectionTrail->addChildFigure(normal2Line);
        }
#endif
        const Material *material = physicalObject->getMaterial();
        totalLoss *= obstacleLoss->computeDielectricLoss(material, frequency, m(intersection2.distance(intersection1)));
        if (normal1 != Coord(0,0,0)) {
            double angle1 = (intersection1 - intersection2).angle(normal1);
            totalLoss *= obstacleLoss->computeReflectionLoss(obstacleLoss->medium->getMaterial(), material, angle1);
        }
        // TODO: this returns NaN because n1 > n2
//            if (!normal2.isUnspecified()) {
//                double angle2 = (intersection2 - intersection1).angle(normal2);
//                totalLoss *= obstacleLoss->computeReflectionLoss(material, obstacleLoss->medium->getMaterial(), angle2);
//            }
    }
}

} // namespace physicallayer

} // namespace inet

