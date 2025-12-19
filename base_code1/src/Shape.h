#pragma once
#ifndef SHAPE_H
#define SHAPE_H

#include "PixelRender.h"
#include <vector>
#include <utility>

// Clase base abstracta para todas las figuras
struct Shape {
    RGBA borderColor;
    RGBA fillColor;
    int thickness;
    bool filled;

    virtual ~Shape() = default;
    virtual void draw(class CMyTest* renderer) = 0;

    // Métodos para edición y selección
    virtual std::vector<std::pair<int, int>> getControlPoints() { return {}; }
    virtual void setControlPoint(int idx, int x, int y) {}
    virtual void moveBy(int dx, int dy) {}
    virtual bool containsPoint(int x, int y) { return false; }
};

// Declaraciones de figuras específicas
struct Line : public Shape {
    int x0, y0, x1, y1;
    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;
};

struct Ellipse : public Shape {
    int cx, cy;
    int a, b;
    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;
};

struct Rectangle : public Shape {
    int xmin, ymin, xmax, ymax;
    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;
};

struct Triangle : public Shape {
    int x0, y0, x1, y1, x2, y2;
    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;
};

struct BezierCurve : public Shape {
    std::vector<std::pair<int, int>> controlPoints;

    void elevateDegree();
    std::pair<std::vector<std::pair<int, int>>, std::vector<std::pair<int, int>>> subdivideAt(float t) const;

    void draw(CMyTest* renderer) override;
    std::vector<std::pair<int, int>> getControlPoints() override;
    void setControlPoint(int idx, int x, int y) override;
    void moveBy(int dx, int dy) override;
    bool containsPoint(int x, int y) override;
private:
    // Para que BezierCurve pueda acceder a variables privadas de CMyTest
    friend class CMyTest;
};


#endif // SHAPE_H