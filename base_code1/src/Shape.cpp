#include "Shape.h"
#include "CMyTest.h"
#include <cmath>
#include <algorithm>

// Implementación de Line
void Line::draw(CMyTest* renderer) {
    renderer->drawLine(x0, y0, x1, y1, borderColor, thickness);
}

std::vector<std::pair<int, int>> Line::getControlPoints() {
    return { {x0,y0}, {x1,y1} };
}

void Line::setControlPoint(int idx, int x, int y) {
    if (idx == 0) { x0 = x; y0 = y; }
    else if (idx == 1) { x1 = x; y1 = y; }
}

void Line::moveBy(int dx, int dy) {
    x0 += dx; y0 += dy; x1 += dx; y1 += dy;
}

bool Line::containsPoint(int x, int y) {
    auto dist2 = [](int x0, int y0, int x1, int y1, int x, int y)->double {
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

// Implementación de Ellipse
void Ellipse::draw(CMyTest* renderer) {
    if (filled) {
        renderer->drawEllipseFilled(cx, cy, a, b, fillColor, borderColor, thickness);
    }
    else {
        renderer->drawEllipseOutline(cx, cy, a, b, borderColor, thickness);
    }
}

std::vector<std::pair<int, int>> Ellipse::getControlPoints() {
    return { {cx - a, cy - b}, {cx + a, cy - b}, {cx + a, cy + b}, {cx - a, cy + b} };
}

void Ellipse::setControlPoint(int idx, int x, int y) {
    auto pts = getControlPoints();
    if (idx < 0 || idx >= (int)pts.size()) return;
    std::pair<int, int> other;
    if (idx == 0) other = pts[2];
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

void Ellipse::moveBy(int dx, int dy) {
    cx += dx; cy += dy;
}

bool Ellipse::containsPoint(int x, int y) {
    if (a <= 0 || b <= 0) return false;
    double dx = (double)(x - cx) / (double)a;
    double dy = (double)(y - cy) / (double)b;
    return dx * dx + dy * dy <= 1.0;
}

// Implementación de Rectangle
void Rectangle::draw(CMyTest* renderer) {
    if (filled) {
        renderer->drawRectangleFilled(xmin, ymin, xmax, ymax, fillColor, borderColor, thickness);
    }
    else {
        renderer->drawRectangleOutline(xmin, ymin, xmax, ymax, borderColor, thickness);
    }
}

std::vector<std::pair<int, int>> Rectangle::getControlPoints() {
    return { {xmin,ymin}, {xmax,ymin}, {xmax,ymax}, {xmin,ymax} };
}

void Rectangle::setControlPoint(int idx, int x, int y) {
    std::vector<std::pair<int, int>> pts = getControlPoints();
    if (idx < 0 || idx >= (int)pts.size()) return;
    if (idx == 0) { xmin = x; ymin = y; }
    else if (idx == 1) { xmax = x; ymin = y; }
    else if (idx == 2) { xmax = x; ymax = y; }
    else { xmin = x; ymax = y; }
    if (xmin > xmax) std::swap(xmin, xmax);
    if (ymin > ymax) std::swap(ymin, ymax);
}

void Rectangle::moveBy(int dx, int dy) {
    xmin += dx; xmax += dx; ymin += dy; ymax += dy;
}

bool Rectangle::containsPoint(int x, int y) {
    return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
}

// Implementación de Triangle
void Triangle::draw(CMyTest* renderer) {
    if (filled) {
        renderer->drawTriangleFilled(x0, y0, x1, y1, x2, y2, fillColor, borderColor, thickness);
    }
    else {
        renderer->drawTriangleOutline(x0, y0, x1, y1, x2, y2, borderColor, thickness);
    }
}

std::vector<std::pair<int, int>> Triangle::getControlPoints() {
    return { {x0,y0}, {x1,y1}, {x2,y2} };
}

void Triangle::setControlPoint(int idx, int x, int y) {
    if (idx == 0) { x0 = x; y0 = y; }
    else if (idx == 1) { x1 = x; y1 = y; }
    else if (idx == 2) { x2 = x; y2 = y; }
}

void Triangle::moveBy(int dx, int dy) {
    x0 += dx; y0 += dy; x1 += dx; y1 += dy; x2 += dx; y2 += dy;
}

bool Triangle::containsPoint(int x, int y) {
    auto sign = [](int px, int py, int ax, int ay, int bx, int by) -> float {
        return (px - bx) * (ay - by) - (ax - bx) * (py - by);
    };
    float d1 = sign(x, y, x0, y0, x1, y1);
    float d2 = sign(x, y, x1, y1, x2, y2);
    float d3 = sign(x, y, x2, y2, x0, y0);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

// Implementación de BezierCurve
void BezierCurve::elevateDegree() {
    int n = (int)controlPoints.size() - 1;
    if (n < 0) return;
    std::vector<std::pair<int,int>> Q;
    Q.reserve(n + 2);
    Q.push_back(controlPoints[0]);
    for (int i = 1; i <= n; ++i) {
        float alpha = (float)i / (float)(n + 1);
        float x = alpha * controlPoints[i-1].first + (1.0f - alpha) * controlPoints[i].first;
        float y = alpha * controlPoints[i-1].second + (1.0f - alpha) * controlPoints[i].second;
        Q.push_back({ static_cast<int>(std::round(x)), static_cast<int>(std::round(y)) });
    }
    Q.push_back(controlPoints[n]);
    controlPoints = std::move(Q);
}

std::pair<std::vector<std::pair<int,int>>, std::vector<std::pair<int,int>>> BezierCurve::subdivideAt(float t) const {
    std::vector<std::vector<std::pair<float,float>>> b;
    int n = (int)controlPoints.size() - 1;
    if (n < 0) return { {}, {} };
    b.resize(n+1);
    b[0].resize(n+1);
    for (int i = 0; i <= n; ++i) {
        b[0][i].first = (float)controlPoints[i].first;
        b[0][i].second = (float)controlPoints[i].second;
    }
    for (int r = 1; r <= n; ++r) {
        b[r].resize(n+1-r);
        for (int i = 0; i <= n - r; ++i) {
            float x = (1.0f - t) * b[r-1][i].first + t * b[r-1][i+1].first;
            float y = (1.0f - t) * b[r-1][i].second + t * b[r-1][i+1].second;
            b[r][i].first = x;
            b[r][i].second = y;
        }
    }

    std::vector<std::pair<int,int>> left, right;
    left.reserve(n+1);
    right.reserve(n+1);
    for (int r = 0; r <= n; ++r) {
        left.push_back({ static_cast<int>(std::round(b[r][0].first)), static_cast<int>(std::round(b[r][0].second)) });
    }
    for (int r = n; r >= 0; --r) {
        int idx = n - r;
        auto p = b[r][idx];
        right.push_back({ static_cast<int>(std::round(p.first)), static_cast<int>(std::round(p.second)) });
    }
    return { left, right };
}

void BezierCurve::draw(CMyTest* renderer) {
    if (controlPoints.size() < 2) return;

    int segments = 100;
    std::pair<int, int> p0 = CMyTest::deCasteljau(controlPoints, 0.0f);
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / segments;
        std::pair<int, int> p1 = CMyTest::deCasteljau(controlPoints, t);
        renderer->drawLine(p0.first, p0.second, p1.first, p1.second, borderColor, thickness);
        p0 = p1;
    }

    if (renderer->m_selectedShape == this) {
        // Polígono de control
        RGBA polyCol = renderer->m_controlPolygonColor;
        for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
            const auto& pA = controlPoints[i];
            const auto& pB = controlPoints[i + 1];
            renderer->drawLine(pA.first, pA.second, pB.first, pB.second,
                polyCol, 1);
        }

    }
}

std::vector<std::pair<int, int>> BezierCurve::getControlPoints() {
    return controlPoints;
}

void BezierCurve::setControlPoint(int idx, int x, int y) {
    if (idx >= 0 && idx < (int)controlPoints.size()) {
        controlPoints[idx] = { x,y };
    }
}

void BezierCurve::moveBy(int dx, int dy) {
    for (auto& p : controlPoints) { p.first += dx; p.second += dy; }
}

bool BezierCurve::containsPoint(int x, int y) {
    int segments = 80;
    std::pair<int, int> prev = CMyTest::deCasteljau(controlPoints, 0.0f);
    const int TOL = 10;
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / segments;
        std::pair<int, int> cur = CMyTest::deCasteljau(controlPoints, t);
        auto dist2 = [](int x0, int y0, int x1, int y1, int x, int y)->double {
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
        if (dist2(prev.first, prev.second, cur.first, cur.second, x, y) <= (double)TOL * TOL) return true;
        prev = cur;
    }
    return false;
}