#include "Triangle.h"
#include "CMyTest.h"
#include <algorithm>
#include <cmath>

void Triangle::draw(CMyTest* renderer)
{
    if (filled) {
        renderer->drawTriangleFilled(x0, y0, x1, y1, x2, y2, fillColor, borderColor, thickness);
    }
    else {
        renderer->drawTriangleOutline(x0, y0, x1, y1, x2, y2, borderColor, thickness);
    }
}

std::vector<std::pair<int, int>> Triangle::getControlPoints()
{
    // Tres vértices + centro como handle de movimiento
    int cx = (x0 + x1 + x2) / 3;
    int cy = (y0 + y1 + y2) / 3;
    return { { x0, y0 }, { x1, y1 }, { x2, y2 }, { cx, cy } };
}

void Triangle::setControlPoint(int idx, int x, int y)
{
    if (idx == 0) { x0 = x; y0 = y; }
    else if (idx == 1) { x1 = x; y1 = y; }
    else if (idx == 2) { x2 = x; y2 = y; }
    else if (idx == 3) {
        int cx = (x0 + x1 + x2) / 3;
        int cy = (y0 + y1 + y2) / 3;
        int dx = x - cx;
        int dy = y - cy;
        moveBy(dx, dy);
    }
}

void Triangle::moveBy(int dx, int dy)
{
    x0 += dx; y0 += dy;
    x1 += dx; y1 += dy;
    x2 += dx; y2 += dy;
}

bool Triangle::containsPoint(int x, int y)
{
    auto sign = [](int px, int py, int ax, int ay, int bx, int by) -> float {
        return (px - bx) * (ay - by) - (ax - bx) * (py - by);
        };

    float d1 = sign(x, y, x0, y0, x1, y1);
    float d2 = sign(x, y, x1, y1, x2, y2);
    float d3 = sign(x, y, x2, y2, x0, y0);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    bool inside = !(has_neg && has_pos);
    if (filled) return inside;

    // Si no está relleno: detectar si está cerca del borde (distancia a segmentos)
    const int TOL = std::max(6, thickness + 2);
    auto dist2 = [](int x0, int y0, int x1, int y1, int x, int y) -> double {
        double vx = x1 - x0, vy = y1 - y0;
        double wx = x - x0, wy = y - y0;
        double c1 = vx * wx + vy * wy;
        double c2 = vx * vx + vy * vy;
        double t = (c2 == 0) ? 0.0 : c1 / c2;
        if (t < 0) t = 0; if (t > 1) t = 1;
        double px = x0 + t * vx, py = y0 + t * vy;
        double dx = x - px, dy = y - py;
        return dx * dx + dy * dy;
        };

    if (dist2(x0, y0, x1, y1, x, y) <= (double)TOL * TOL) return true;
    if (dist2(x1, y1, x2, y2, x, y) <= (double)TOL * TOL) return true;
    if (dist2(x2, y2, x0, y0, x, y) <= (double)TOL * TOL) return true;
    return false;
}