#include "Ellipse.h"
#include "CMyTest.h"
#include <algorithm>
#include <cmath>

void Ellipse::draw(CMyTest* renderer)
{
    if (filled) {
        renderer->drawEllipseFilled(cx, cy, a, b, fillColor, borderColor, thickness);
    }
    else {
        renderer->drawEllipseOutline(cx, cy, a, b, borderColor, thickness);
    }
}

std::vector<std::pair<int, int>> Ellipse::getControlPoints()
{
    // Cuatro esquinas del bounding box + centro
    std::vector<std::pair<int, int>> pts = {
        { cx - a, cy - b },
        { cx + a, cy - b },
        { cx + a, cy + b },
        { cx - a, cy + b }
    };
    pts.push_back({ cx, cy });
    return pts;
}

void Ellipse::setControlPoint(int idx, int x, int y)
{
    auto pts = getControlPoints();
    if (idx < 0 || idx >= (int)pts.size()) return;

    // Handle central (último índice): mover la elipse manteniendo a,b
    if (idx == (int)pts.size() - 1) {
        int dx = x - cx;
        int dy = y - cy;
        moveBy(dx, dy);
        return;
    }

    std::pair<int, int> other;
    if (idx == 0) other = pts[2]; // opuesto
    else if (idx == 1) other = pts[3];
    else if (idx == 2) other = pts[0];
    else other = pts[1];

    int xmin = std::min(x, other.first);
    int xmax = std::max(x, other.first);
    int ymin = std::min(y, other.second);
    int ymax = std::max(y, other.second);
    cx = (xmin + xmax) / 2;
    cy = (ymin + ymax) / 2;
    a = std::max(1, (xmax - xmin) / 2);
    b = std::max(1, (ymax - ymin) / 2);
}

void Ellipse::moveBy(int dx, int dy)
{
    cx += dx; cy += dy;
}

bool Ellipse::containsPoint(int x, int y)
{
    if (a <= 0 || b <= 0) return false;

    // Si la elipse está rellena, usar la comprobación clásica
    if (filled) {
        double dx = (double)(x - cx) / (double)a;
        double dy = (double)(y - cy) / (double)b;
        return dx * dx + dy * dy <= 1.0;
    }

    // Para solo contorno: detectar si el punto está cerca del borde en píxeles.
    double dxp = (double)(x - cx);
    double dyp = (double)(y - cy);
    double val = (dxp * dxp) / ((double)a * (double)a) + (dyp * dyp) / ((double)b * (double)b);
    double r = std::sqrt(val);
    double meanRadius = ((double)a + (double)b) * 0.5;
    double pixelDist = std::abs(r - 1.0) * (meanRadius > 0.0 ? meanRadius : 1.0);

    int TOL = std::max(10, thickness + 3); // tolerancia en píxeles (ajustable)
    return pixelDist <= (double)TOL;
}