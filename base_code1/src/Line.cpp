#include "Line.h"
#include "CMyTest.h"
#include <cmath>

void Line::drawLineBresenham(CMyTest* renderer, int x0, int y0, int x1, int y1,
    RGBA color, int thickness, bool clear)
{
    if (clear) renderer->m_drawnPixels.clear();

    int dx = x1 - x0;
    int dy = y1 - y0;
    int absDx = abs(dx);
    int absDy = abs(dy);

    if (absDy < absDx && dx > 0 && dy >= 0)
    {
        int d = absDx - 2 * absDy;
        int incE = -2 * absDy;
        int incNE = 2 * (absDx - absDy);
        int x = x0, y = y0;

        renderer->setThickPixel(x, y, color, thickness);
        while (x < x1)
        {
            if (d <= 0) { d += incNE; y++; }
            else { d += incE; }
            x++;
            renderer->setThickPixel(x, y, color, thickness);
        }
    }
    else if (absDy >= absDx && dx >= 0 && dy > 0)
    {
        int d = absDy - 2 * absDx;
        int incN = -2 * absDx;
        int incNE = 2 * (absDy - absDx);
        int x = x0, y = y0;

        renderer->setThickPixel(x, y, color, thickness);
        while (y < y1)
        {
            if (d <= 0) { d += incNE; x++; }
            else { d += incN; }
            y++;
            renderer->setThickPixel(x, y, color, thickness);
        }
    }
    else if (absDy < absDx && dx > 0 && dy < 0)
    {
        int d = absDx - 2 * absDy;
        int incE = -2 * absDy;
        int incSE = 2 * (absDx - absDy);
        int x = x0, y = y0;

        renderer->setThickPixel(x, y, color, thickness);
        while (x < x1)
        {
            if (d <= 0) { d += incSE; y--; }
            else { d += incE; }
            x++;
            renderer->setThickPixel(x, y, color, thickness);
        }
    }
    else if (absDy >= absDx && dx >= 0 && dy < 0)
    {
        int d = absDy - 2 * absDx;
        int incS = -2 * absDx;
        int incSE = 2 * (absDy - absDx);
        int x = x0, y = y0;

        renderer->setThickPixel(x, y, color, thickness);
        while (y > y1)
        {
            if (d <= 0) { d += incSE; x++; }
            else { d += incS; }
            y--;
            renderer->setThickPixel(x, y, color, thickness);
        }
    }
    else if (dx < 0)
    {
        drawLineBresenham(renderer, x1, y1, x0, y0, color, thickness, clear);
    }
}

void Line::draw(CMyTest* renderer)
{
    drawLineBresenham(renderer, x0, y0, x1, y1, borderColor, thickness, true);
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