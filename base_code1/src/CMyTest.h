#pragma once
#ifndef CMYTEST_H
#define CMYTEST_H

#include "PixelRender.h"
#include "Shape.h"
#include <vector>
#include <memory>

class CMyTest : public CPixelRender {
public:
    CMyTest();
    ~CMyTest();

    // Drawing helpers used by shapes
    void drawLine(int x0, int y0, int x1, int y1, RGBA color, int thickness = 1);
    void drawEllipseOutline(int cx, int cy, int a, int b, RGBA color, int thickness = 1);
    void drawEllipseFilled(int cx, int cy, int a, int b, RGBA fillColor, RGBA borderColor, int thickness);
    void drawRectangleOutline(int x0, int y0, int x1, int y1, RGBA color, int thickness);
    void drawRectangleFilled(int x0, int y0, int x1, int y1, RGBA fillColor, RGBA borderColor, int thickness);
    void drawTriangleOutline(int x0, int y0, int x1, int y1, int x2, int y2, RGBA color, int thickness);
    void drawTriangleFilled(int x0, int y0, int x1, int y1, int x2, int y2, RGBA fillColor, RGBA borderColor, int thickness);
    void drawHorizontalLine(int x0, int x1, int y, RGBA color);

    static std::pair<int, int> deCasteljau(const std::vector<std::pair<int, int>>& points, float t);
    void update() override;

    // Public members accessed by Shape implementations
    RGBA m_controlPolygonColor = { 136, 136, 136, 255 };
    Shape* m_selectedShape = nullptr;
    RGBA m_controlPointColor = { 255, 119, 0, 255 };
    RGBA m_selectedControlPointColor = { 0, 119, 255, 255 };
    int m_controlPointRadius = 5;
};

#endif // CMYTEST_H