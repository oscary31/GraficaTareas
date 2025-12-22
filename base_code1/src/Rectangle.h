#pragma once
#ifndef RECTANGLE_H
#define RECTANGLE_H

#include "Shape.h"

class Rectangle : public Shape
{
public:
    int xmin, ymin, xmax, ymax;

    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;


    // Métodos internos para dibujar rectángulo
    static void drawRectangleOutline(CMyTest* renderer, int x0, int y0, int x1, int y1, RGBA color, int thickness);
    static void drawRectangleFilled(CMyTest* renderer, int x0, int y0, int x1, int y1, RGBA fillColor, RGBA borderColor, int thickness);
    static void drawHorizontalLine(CMyTest* renderer, int x0, int x1, int y, RGBA color);
};

#endif // RECTANGLE_H