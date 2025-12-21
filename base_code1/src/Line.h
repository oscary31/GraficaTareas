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
};

#endif // LINE_H