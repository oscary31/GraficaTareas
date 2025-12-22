#include "Ellipse.h"
#include "CMyTest.h"
#include "PixelRender.h"
#include <algorithm>
#include <cmath>
#include <set>

void Ellipse::ellipsePoints4(CMyTest* renderer, long long cx, long long cy, long long x, long long y, RGBA color, int thickness)
{
    renderer->setThickPixel(static_cast<int>(cx + x), static_cast<int>(cy + y), color, thickness);
    renderer->setThickPixel(static_cast<int>(cx - x), static_cast<int>(cy + y), color, thickness);
    renderer->setThickPixel(static_cast<int>(cx + x), static_cast<int>(cy - y), color, thickness);
    renderer->setThickPixel(static_cast<int>(cx - x), static_cast<int>(cy - y), color, thickness);
}

void Ellipse::drawEllipseOutline(CMyTest* renderer, int cx, int cy, int a, int b, RGBA color, int thickness)
{
    renderer->m_drawnPixels.clear();

    if (a <= 0 || b <= 0) return;

    long long a2 = static_cast<long long>(a) * static_cast<long long>(a);
    long long b2 = static_cast<long long>(b) * static_cast<long long>(b);

    long long x = 0;
    long long y = b;

    long long dx = 2 * b2 * x;
    long long dy = 2 * a2 * y;

    long long d1 = b2 - a2 * b + (a2 + 3) / 4;

    long long twoB2 = 2 * b2;
    long long twoA2 = 2 * a2;

    while (dx < dy) {
        ellipsePoints4(renderer, cx, cy, x, y, color, thickness);
        if (d1 < 0) {
            x += 1;
            dx += twoB2;
            d1 += dx + b2;
        }
        else {
            x += 1;
            y -= 1;
            dx += twoB2;
            dy -= twoA2;
            d1 += dx - dy + b2;
        }
    }

    long long d2_num_x = (2 * x + 1);
    long long d2 = b2 * (d2_num_x * d2_num_x) / 4 + a2 * (y - 1) * (y - 1) - a2 * b2;
    while (y >= 0) {
        ellipsePoints4(renderer, cx, cy, x, y, color, thickness);
        if (d2 > 0) {
            y -= 1;
            dy -= twoA2;
            d2 += a2 - dy;
        }
        else {
            y -= 1;
            x += 1;
            dx += twoB2;
            dy -= twoA2;
            d2 += dx - dy + a2;
        }
    }
}

void Ellipse::drawEllipseFilled(CMyTest* renderer, int cx, int cy, int a, int b, RGBA fillColor, RGBA borderColor, int thickness)
{
    if (a <= 0 || b <= 0) return;

    // Dibujar borde primero (m_drawnPixels se llena)
    drawEllipseOutline(renderer, cx, cy, a, b, borderColor, thickness);

    // Guardar todos los píxeles del borde
    std::set<std::pair<int, int>> borderPixels = renderer->m_drawnPixels;
    renderer->m_drawnPixels.clear();

    // Rellenar usando un radio amplio
    int searchRadius = a + b;

    for (int y = cy - searchRadius; y <= cy + searchRadius; ++y) {
        if (y < 0 || y >= renderer->height) continue;

        for (int x = cx - searchRadius; x <= cx + searchRadius; ++x) {
            if (x < 0 || x >= renderer->width) continue;

            if (borderPixels.find({ x, y }) != borderPixels.end()) {
                continue;
            }

            float dx = static_cast<float>(x - cx) / static_cast<float>(a);
            float dy = static_cast<float>(y - cy) / static_cast<float>(b);

            if (dx * dx + dy * dy <= 1.0f) {
                renderer->setPixel(x, y, fillColor);
            }
        }
    }
}

void Ellipse::draw(CMyTest* renderer)
{
    if (filled) {
        drawEllipseFilled(renderer, cx, cy, a, b, fillColor, borderColor, thickness);
    }
    else {
        drawEllipseOutline(renderer, cx, cy, a, b, borderColor, thickness);
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