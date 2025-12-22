#include "BezierCurve.h"
#include "Line.h"
#include "CMyTest.h"
#include <cmath>

// Algoritmo de De Casteljau para evaluar la curva de Bézier en el parámetro t
std::pair<int, int> BezierCurve::deCasteljau(const std::vector<std::pair<int, int>>& points, float t)
{
    if (points.empty()) return { 0, 0 };

    std::vector<std::pair<float, float>> temp(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        temp[i] = { static_cast<float>(points[i].first), static_cast<float>(points[i].second) };
    }

    int n = temp.size() - 1;
    for (int k = 1; k <= n; ++k) {
        for (int i = 0; i <= n - k; ++i) {
            temp[i].first = (1.0f - t) * temp[i].first + t * temp[i + 1].first;
            temp[i].second = (1.0f - t) * temp[i].second + t * temp[i + 1].second;
        }
    }

    int rx = static_cast<int>(std::lround(temp[0].first));
    int ry = static_cast<int>(std::lround(temp[0].second));
    return { rx, ry };
}

void BezierCurve::elevateDegree()
{
    int n = (int)controlPoints.size() - 1;
    if (n < 0) return;

    std::vector<std::pair<int, int>> Q;
    Q.reserve(n + 2);
    Q.push_back(controlPoints[0]);

    for (int i = 1; i <= n; ++i) {
        float alpha = (float)i / (float)(n + 1);
        float x = alpha * controlPoints[i - 1].first + (1.0f - alpha) * controlPoints[i].first;
        float y = alpha * controlPoints[i - 1].second + (1.0f - alpha) * controlPoints[i].second;
        Q.push_back({ static_cast<int>(std::round(x)), static_cast<int>(std::round(y)) });
    }

    Q.push_back(controlPoints[n]);
    controlPoints = std::move(Q);
}

std::pair<std::vector<std::pair<int, int>>, std::vector<std::pair<int, int>>> BezierCurve::subdivideAt(float t) const
{
    std::vector<std::vector<std::pair<float, float>>> b;
    int n = (int)controlPoints.size() - 1;
    if (n < 0) return { {}, {} };

    b.resize(n + 1);
    // Nivel 0
    b[0].resize(n + 1);
    for (int i = 0; i <= n; ++i) {
        b[0][i].first = (float)controlPoints[i].first;
        b[0][i].second = (float)controlPoints[i].second;
    }

    // Construir tabla de De Casteljau
    for (int r = 1; r <= n; ++r) {
        b[r].resize(n + 1 - r);
        for (int i = 0; i <= n - r; ++i) {
            float x = (1.0f - t) * b[r - 1][i].first + t * b[r - 1][i + 1].first;
            float y = (1.0f - t) * b[r - 1][i].second + t * b[r - 1][i + 1].second;
            b[r][i].first = x;
            b[r][i].second = y;
        }
    }

    std::vector<std::pair<int, int>> left, right;
    left.reserve(n + 1);
    right.reserve(n + 1);

    // left: b[0][0], b[1][0], ..., b[n][0]
    for (int r = 0; r <= n; ++r) {
        left.push_back({ static_cast<int>(std::round(b[r][0].first)), static_cast<int>(std::round(b[r][0].second)) });
    }

    // right: b[n][0], b[n-1][1], ..., b[0][n]
    for (int r = n; r >= 0; --r) {
        int idx = n - r;
        auto p = b[r][idx];
        right.push_back({ static_cast<int>(std::round(p.first)), static_cast<int>(std::round(p.second)) });
    }

    return { left, right };
}

void BezierCurve::draw(CMyTest* renderer)
{
    if (controlPoints.size() < 2) return;

    // Dibujar la curva muestreada
    int segments = 100;

    // evitar que setThickPixel pinte píxeles duplicados dentro de la misma curva.
    renderer->m_drawnPixels.clear();

    std::pair<int, int> p0 = BezierCurve::deCasteljau(controlPoints, 0.0f);
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / segments;
        std::pair<int, int> p1 = BezierCurve::deCasteljau(controlPoints, t);
        Line::drawLineBresenham(renderer, p0.first, p0.second, p1.first, p1.second, borderColor, thickness, false);
        p0 = p1;
    }

    // Dibujar polígono de control y puntos solo si la curva está seleccionada
    if (renderer->getSelectedShape() == this) {
        // Polígono de control
        for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
            const auto& pA = controlPoints[i];
            const auto& pB = controlPoints[i + 1];
            // Usa el color de polígono de control de renderer
            // Asegúrate de que m_controlPolygonColor sea accesible desde aquí
            Line::drawLineBresenham(renderer, pA.first, pA.second, pB.first, pB.second, renderer->m_controlPolygonColor, 1);
        }
    }
}

std::vector<std::pair<int, int>> BezierCurve::getControlPoints()
{
    return controlPoints;
}

void BezierCurve::setControlPoint(int idx, int x, int y)
{
    if (idx >= 0 && idx < (int)controlPoints.size()) {
        controlPoints[idx] = { x, y };
    }
}

void BezierCurve::moveBy(int dx, int dy)
{
    for (auto& p : controlPoints) { p.first += dx; p.second += dy; }
}

bool BezierCurve::containsPoint(int x, int y)
{
    // Test de distancia a la curva muestreada
    int segments = 80;
    std::pair<int, int> prev = BezierCurve::deCasteljau(controlPoints, 0.0f);
    const int TOL = 10;

    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / segments;
        std::pair<int, int> cur = BezierCurve::deCasteljau(controlPoints, t);

        // Distancia punto a segmento prev-cur
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

        if (dist2(prev.first, prev.second, cur.first, cur.second, x, y) <= (double)TOL * TOL) return true;
        prev = cur;
    }

    return false;
}