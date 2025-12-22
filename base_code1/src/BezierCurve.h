#pragma once
#ifndef BEZIERCURVE_H
#define BEZIERCURVE_H

#include "Shape.h"

class BezierCurve : public Shape
{
public:
    std::vector<std::pair<int, int>> controlPoints;

    // Elevar el grado en 1 
    void elevateDegree();

    // Subdividir en t [0,1], devuelve par (left,right) con puntos de control
    std::pair<std::vector<std::pair<int, int>>, std::vector<std::pair<int, int>>> subdivideAt(float t) const;

    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;

    // Algoritmo de De Casteljau para evaluar la curva de Bézier
    static std::pair<int, int> deCasteljau(const std::vector<std::pair<int, int>>& points, float t);
};

#endif // BEZIERCURVE_H