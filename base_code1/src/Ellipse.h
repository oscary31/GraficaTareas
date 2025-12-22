#pragma once
#ifndef ELLIPSE_H
#define ELLIPSE_H

#include "Shape.h"

class Ellipse : public Shape
{
public:
    int cx, cy;
    int a, b;

    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;


    // Métodos internos para dibujar elipse
    static void ellipsePoints4(CMyTest* renderer, long long cx, long long cy, long long x, long long y, RGBA color, int thickness);
    static void drawEllipseOutline(CMyTest* renderer, int cx, int cy, int a, int b, RGBA color, int thickness);
    static void drawEllipseFilled(CMyTest* renderer, int cx, int cy, int a, int b, RGBA fillColor, RGBA borderColor, int thickness);
};

#endif // ELLIPSE_H