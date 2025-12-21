#pragma once
#ifndef SHAPE_H
#define SHAPE_H

#include "PixelRender.h"
#include <vector>
#include <utility>

class CMyTest;

// Clase base abstracta para todas las figuras
class Shape
{
public:
    RGBA borderColor;
    RGBA fillColor;
    int thickness;
    bool filled;

    virtual ~Shape() = default;
    virtual void draw(CMyTest* renderer) = 0;

    // Métodos para edición y selección
    virtual std::vector<std::pair<int, int>> getControlPoints() { return {}; }
    virtual void setControlPoint(int idx, int x, int y) {}
    virtual void moveBy(int dx, int dy) {}
    virtual bool containsPoint(int x, int y) { return false; }
};

#endif // SHAPE_H