#include "Line.h"
#include "CMyTest.h"

void Line::draw(CMyTest* renderer)
{
    renderer->drawLine(x0, y0, x1, y1, borderColor, thickness);
}

std::vector<std::pair<int, int>> Line::getControlPoints()
{
    return { { x0, y0 }, { x1, y1 } };
}

void Line::setControlPoint(int idx, int x, int y)
{
    if (idx == 0) { x0 = x; y0 = y; }
    else if (idx == 1) { x1 = x; y1 = y; }
}

void Line::moveBy(int dx, int dy)
{
    x0 += dx; y0 += dy; x1 += dx; y1 += dy;
}

bool Line::containsPoint(int x, int y)
{
    // Distancia punto-segmento (comprobación con tolerancia)
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

    const int TOL = 8;
    return dist2(x0, y0, x1, y1, x, y) <= (double)TOL * TOL;
}