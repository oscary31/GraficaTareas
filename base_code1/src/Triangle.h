#pragma once
#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "Shape.h"

class Triangle : public Shape
{
public:
    int x0, y0, x1, y1, x2, y2;

    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;


    // Métodos internos para dibujar triángulo
    static void drawTriangleOutline(CMyTest* renderer, int x0, int y0, int x1, int y1, int x2, int y2, RGBA color, int thickness);
    static void drawTriangleFilled(CMyTest* renderer, int x0, int y0, int x1, int y1, int x2, int y2, RGBA fillColor, RGBA borderColor, int thickness);
};

#endif // TRIANGLE_H