#pragma once
#ifndef LINE_H
#define LINE_H

#include "Shape.h"

class Line : public Shape
{
public:
    int x0, y0, x1, y1;
    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;

    // Método para dibujar línea
    static void drawLineBresenham(CMyTest* renderer, int x0, int y0, int x1, int y1, RGBA color, int thickness, bool clear = true);
};

#endif // LINE_H