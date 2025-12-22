#include "Rectangle.h"
#include "CMyTest.h"
#include "Line.h"
#include <algorithm>
#include <cmath>

void Rectangle::drawHorizontalLine(CMyTest* renderer, int x0, int x1, int y, RGBA color)
{
    if (x0 > x1) std::swap(x0, x1);
    for (int x = x0; x <= x1; ++x) {
        renderer->setPixel(x, y, color);
    }
}

void Rectangle::drawRectangleOutline(CMyTest* renderer, int x0, int y0, int x1, int y1, RGBA color, int thickness)
{
    int xmin = std::min(x0, x1);
    int xmax = std::max(x0, x1);
    int ymin = std::min(y0, y1);
    int ymax = std::max(y0, y1);

    // Limpiar el conjunto de píxeles dibujados
    renderer->m_drawnPixels.clear();

    // Función para dibujar una línea evitando duplicar extremos
    auto drawLineProtected = [&](int sx, int sy, int ex, int ey, bool isLast = false) {
        // Marcar el inicio como dibujado
        renderer->m_drawnPixels.insert({ sx, sy });

        // Dibujar la línea
        Line::drawLineBresenham(renderer, sx, sy, ex, ey, color, thickness, false);

        // Si no es la última línea, eliminar el extremo final
        if (!isLast) {
            auto it = renderer->m_drawnPixels.find({ ex, ey });
            if (it != renderer->m_drawnPixels.end()) {
                renderer->m_drawnPixels.erase(it);
            }
        }
        };

    // Dibujar los 4 lados
    drawLineProtected(xmin, ymin, xmax, ymin); // Lado superior
    drawLineProtected(xmax, ymin, xmax, ymax); // Lado derecho
    drawLineProtected(xmax, ymax, xmin, ymax); // Lado inferior
    drawLineProtected(xmin, ymax, xmin, ymin, true); // Lado izquierdo (último)

    // Dibujar los vértices 
    for (const auto& p : renderer->m_drawnPixels) {
        renderer->setPixel(p.first, p.second, color);
    }
}

void Rectangle::drawRectangleFilled(CMyTest* renderer, int x0, int y0, int x1, int y1, RGBA fillColor, RGBA borderColor, int thickness)
{
    int xmin = std::min(x0, x1);
    int xmax = std::max(x0, x1);
    int ymin = std::min(y0, y1);
    int ymax = std::max(y0, y1);

    drawRectangleOutline(renderer, xmin, ymin, xmax, ymax, borderColor, thickness);

    int innerMargin = (thickness - 1) / 2;
    int innerXmin = xmin + innerMargin + 1;
    int innerXmax = xmax - innerMargin - 1;
    int innerYmin = ymin + innerMargin + 1;
    int innerYmax = ymax - innerMargin - 1;

    if (innerXmin <= innerXmax && innerYmin <= innerYmax) {
        for (int y = innerYmin; y <= innerYmax; ++y) {
            drawHorizontalLine(renderer, innerXmin, innerXmax, y, fillColor);
        }
    }
}

void Rectangle::draw(CMyTest* renderer)
{
    if (filled) {
        drawRectangleFilled(renderer, xmin, ymin, xmax, ymax, fillColor, borderColor, thickness);
    }
    else {
        drawRectangleOutline(renderer, xmin, ymin, xmax, ymax, borderColor, thickness);
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