#include "Rectangle.h"
#include "CMyTest.h"
#include <algorithm>
#include <cmath>

void Rectangle::draw(CMyTest* renderer)
{
    if (filled) {
        renderer->drawRectangleFilled(xmin, ymin, xmax, ymax, fillColor, borderColor, thickness);
    }
    else {
        renderer->drawRectangleOutline(xmin, ymin, xmax, ymax, borderColor, thickness);
    }
}

std::vector<std::pair<int, int>> Rectangle::getControlPoints()
{
    // Cuatro esquinas + centro
    int cx = (xmin + xmax) / 2;
    int cy = (ymin + ymax) / 2;
    return { { xmin, ymin }, { xmax, ymin }, { xmax, ymax }, { xmin, ymax }, { cx, cy } };
}

void Rectangle::setControlPoint(int idx, int x, int y)
{
    std::vector<std::pair<int, int>> pts = getControlPoints();
    if (idx < 0 || idx >= (int)pts.size()) return;

    // Handle central (último índice): mover rectángulo
    if (idx == (int)pts.size() - 1) {
        int cx = (xmin + xmax) / 2;
        int cy = (ymin + ymax) / 2;
        int dx = x - cx;
        int dy = y - cy;
        moveBy(dx, dy);
        return;
    }

    if (idx == 0) { xmin = x; ymin = y; }
    else if (idx == 1) { xmax = x; ymin = y; }
    else if (idx == 2) { xmax = x; ymax = y; }
    else { xmin = x; ymax = y; }

    if (xmin > xmax) std::swap(xmin, xmax);
    if (ymin > ymax) std::swap(ymin, ymax);
}

void Rectangle::moveBy(int dx, int dy)
{
    xmin += dx; xmax += dx; ymin += dy; ymax += dy;
}

bool Rectangle::containsPoint(int x, int y)
{
    // Si está relleno, el interior cuenta
    if (filled) {
        return x >= xmin + 3 && x <= xmax + 3 && y >= ymin + 3 && y <= ymax + 3;
    }

    // Solo contorno: punto cerca de cualquiera de los 4 bordes
    int TOL = std::max(6, thickness + 2);

    // Rango ampliado por grosor
    if (x < xmin - TOL || x > xmax + TOL || y < ymin - TOL || y > ymax + TOL) return false;

    if (std::abs(x - xmin) <= TOL) return true;
    if (std::abs(x - xmax) <= TOL) return true;
    if (std::abs(y - ymin) <= TOL) return true;
    if (std::abs(y - ymax) <= TOL) return true;

    return false;
}