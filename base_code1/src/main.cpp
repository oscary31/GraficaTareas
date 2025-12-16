#include "PixelRender.h"
#include <iostream>
#include <vector>
#include <set>
#include <random>
#include <memory>
#include <imgui.h>
#include <cmath>

class CMyTest : public CPixelRender
{
private:
    int m_x0 = -1;
    int m_y0 = -1;
    int m_x1 = -1;
    int m_y1 = -1;
    RGBA m_borderColor = { 0,0,0,255 };
    RGBA m_fillColor = { 255,255,255,255 };
    int m_drawMode = 0; // 0=Line,1=Ellipse,2=Rectangle,3=Triangle, 4=Bezier
    int m_lineThickness = 1;
    bool m_useFilledShapes = false;

    // Colores para los puntos de control de Bézier
    RGBA m_controlPointColor = { 255, 119, 0, 255 };        // Naranja
    RGBA m_selectedControlPointColor = { 0, 119, 255, 255 }; // Azul
    RGBA m_controlPolygonColor = { 136, 136, 136, 255 };      // Gris para las líneas

    // Base class para todas las figuras
    struct Shape {
        RGBA borderColor;
        RGBA fillColor;
        int thickness;
        bool filled;
        virtual ~Shape() = default;
        virtual void draw(CMyTest* renderer) = 0;
    };

    struct Line : public Shape {
        int x0, y0, x1, y1;
        void draw(CMyTest* renderer) override {
            renderer->drawLine(x0, y0, x1, y1, borderColor, thickness);
        }
    };

    struct Ellipse : public Shape {
        int cx, cy;
        int a, b;
        void draw(CMyTest* renderer) override {
            if (filled) {
                renderer->drawEllipseFilled(cx, cy, a, b, fillColor, borderColor, thickness);
            }
            else {
                renderer->drawEllipseOutline(cx, cy, a, b, borderColor, thickness);
            }
        }
    };

    struct Rectangle : public Shape {
        int xmin, ymin, xmax, ymax;
        void draw(CMyTest* renderer) override {
            if (filled) {
                renderer->drawRectangleFilled(xmin, ymin, xmax, ymax, fillColor, borderColor, thickness);
            }
            else {
                renderer->drawRectangleOutline(xmin, ymin, xmax, ymax, borderColor, thickness);
            }
        }
    };

    struct Triangle : public Shape {
        int x0, y0, x1, y1, x2, y2;
        void draw(CMyTest* renderer) override {
            if (filled) {
                renderer->drawTriangleFilled(x0, y0, x1, y1, x2, y2, fillColor, borderColor, thickness);
            }
            else {
                renderer->drawTriangleOutline(x0, y0, x1, y1, x2, y2, borderColor, thickness);
            }
        }
    };

    // Definición completa de BezierCurve
    struct BezierCurve : public Shape {
        std::vector<std::pair<int, int>> controlPoints;

        void draw(CMyTest* renderer) override {
            if (controlPoints.size() < 2) return;

            // 1. Dibujar el Polígono de Control (líneas)
            for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
                const auto& pA = controlPoints[i];
                const auto& pB = controlPoints[i + 1];
                renderer->drawLine(pA.first, pA.second, pB.first, pB.second,
                    renderer->m_controlPolygonColor, 1);
            }

            // 2. Dibujar los Puntos de Control (círculos rellenos)
            int pointRadius = 5;
            for (size_t i = 0; i < controlPoints.size(); ++i) {
                const auto& p = controlPoints[i];
                RGBA pointColor = renderer->m_controlPointColor; // Usar el color de la clase

                // Resaltar el punto seleccionado si estamos editando esta curva
                if (renderer->m_editMode == 1 &&
                    renderer->m_editingCurve == this &&
                    (int)i == renderer->m_selectedControlPoint) {
                    pointColor = renderer->m_selectedControlPointColor; // Usar el color de selección
                }

                renderer->drawEllipseFilled(p.first, p.second, pointRadius, pointRadius,
                    pointColor, pointColor, 1);
            }

            // 3. Dibujar la Curva (Algoritmo de Casteljau)
            int segments = 100;
            std::pair<int, int> p0 = renderer->deCasteljau(controlPoints, 0.0f);

            for (int i = 1; i <= segments; ++i) {
                float t = (float)i / segments;
                std::pair<int, int> p1 = renderer->deCasteljau(controlPoints, t);
                renderer->drawLine(p0.first, p0.second, p1.first, p1.second, borderColor, thickness);
                p0 = p1;
            }
        }
    };

    // Para la edición de curvas Bézier
    int m_editMode = 0; // 0=crear nueva, 1=editar existente
    BezierCurve* m_editingCurve = nullptr;
    int m_selectedControlPoint = -1;
    std::pair<int, int> m_originalMousePos;
    bool m_isDraggingControlPoint = false;

    // Lista unificada de todas las figuras
    std::vector<std::unique_ptr<Shape>> m_shapes;

    // Temp storage for triangle clicks
    int m_triTempX[3];
    int m_triTempY[3];
    int m_triClicks = 0;

    int framesThisSecond = 0;

    // Set para evitar dibujar el mismo píxel dos veces
    std::set<std::pair<int, int>> m_drawnPixels;

    struct TriangleFillInfo {
        std::vector<std::pair<int, int>> scanlines;
    };

    // Modo 4: Bezier
    int m_bezierControlPointRadius = 5;
    std::vector<std::pair<int, int>> m_tempControlPoints;

public:
    CMyTest() {};
    ~CMyTest() {};

    void setThickPixel(int x, int y, RGBA color, int thickness)
    {
        if (thickness <= 1) {
            setPixel(x, y, color);
            return;
        }

        int half = (thickness - 1) / 2;
        for (int dy = -half; dy <= half; ++dy) {
            for (int dx = -half; dx <= half; ++dx) {
                int px = x + dx;
                int py = y + dy;

                if (m_drawnPixels.find({ px, py }) == m_drawnPixels.end()) {
                    setPixel(px, py, color);
                    m_drawnPixels.insert({ px, py });
                }
            }
        }
    }

    void drawLineBresenham(int x0, int y0, int x1, int y1, RGBA color, int thickness)
    {
        m_drawnPixels.clear();

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

            setThickPixel(x, y, color, thickness);
            while (x < x1)
            {
                if (d <= 0) { d += incNE; y++; }
                else { d += incE; }
                x++;
                setThickPixel(x, y, color, thickness);
            }
        }
        else if (absDy >= absDx && dx >= 0 && dy > 0)
        {
            int d = absDy - 2 * absDx;
            int incN = -2 * absDx;
            int incNE = 2 * (absDy - absDx);
            int x = x0, y = y0;

            setThickPixel(x, y, color, thickness);
            while (y < y1)
            {
                if (d <= 0) { d += incNE; x++; }
                else { d += incN; }
                y++;
                setThickPixel(x, y, color, thickness);
            }
        }
        else if (absDy < absDx && dx > 0 && dy < 0)
        {
            int d = absDx - 2 * absDy;
            int incE = -2 * absDy;
            int incSE = 2 * (absDx - absDy);
            int x = x0, y = y0;

            setThickPixel(x, y, color, thickness);
            while (x < x1)
            {
                if (d <= 0) { d += incSE; y--; }
                else { d += incE; }
                x++;
                setThickPixel(x, y, color, thickness);
            }
        }
        else if (absDy >= absDx && dx >= 0 && dy < 0)
        {
            int d = absDy - 2 * absDx;
            int incS = -2 * absDx;
            int incSE = 2 * (absDy - absDx);
            int x = x0, y = y0;

            setThickPixel(x, y, color, thickness);
            while (y > y1)
            {
                if (d <= 0) { d += incSE; x++; }
                else { d += incS; }
                y--;
                setThickPixel(x, y, color, thickness);
            }
        }
        else if (dx < 0)
        {
            drawLineBresenham(x1, y1, x0, y0, color, thickness);
        }

        m_drawnPixels.clear();
    }

    void drawLine(int x0, int y0, int x1, int y1, RGBA color, int thickness = 1)
    {
        drawLineBresenham(x0, y0, x1, y1, color, thickness);
    }

    void ellipsePoints4(long long cx, long long cy, long long x, long long y, RGBA color, int thickness)
    {
        setThickPixel(static_cast<int>(cx + x), static_cast<int>(cy + y), color, thickness);
        setThickPixel(static_cast<int>(cx - x), static_cast<int>(cy + y), color, thickness);
        setThickPixel(static_cast<int>(cx + x), static_cast<int>(cy - y), color, thickness);
        setThickPixel(static_cast<int>(cx - x), static_cast<int>(cy - y), color, thickness);
    }

    void drawEllipseOutline(int cx, int cy, int a, int b, RGBA color, int thickness=1)
    {
        m_drawnPixels.clear();

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
            ellipsePoints4(cx, cy, x, y, color, thickness);
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
            ellipsePoints4(cx, cy, x, y, color, thickness);
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

        //m_drawnPixels.clear();
    }

    void drawRectangleOutline(int x0, int y0, int x1, int y1, RGBA color, int thickness)
    {
        int xmin = std::min(x0, x1);
        int xmax = std::max(x0, x1);
        int ymin = std::min(y0, y1);
        int ymax = std::max(y0, y1);

        drawLine(xmin, ymin, xmax, ymin, color, thickness);
        drawLine(xmin, ymax, xmax, ymax, color, thickness);
        drawLine(xmin, ymin, xmin, ymax, color, thickness);
        drawLine(xmax, ymin, xmax, ymax, color, thickness);
    }

    void drawTriangleOutline(int x0, int y0, int x1, int y1, int x2, int y2, RGBA color, int thickness)
    {
        drawLine(x0, y0, x1, y1, color, thickness);
        drawLine(x1, y1, x2, y2, color, thickness);
        drawLine(x2, y2, x0, y0, color, thickness);
    }

    void drawHorizontalLine(int x0, int x1, int y, RGBA color)
    {
        if (x0 > x1) std::swap(x0, x1);
        for (int x = x0; x <= x1; ++x) {
            setPixel(x, y, color);
        }
    }

    void calculateTriangleFillLine(TriangleFillInfo& info, int xa, int ya, int xb, int yb)
    {
        int dx = xb - xa;
        int dy = yb - ya;
        int absDx = abs(dx);
        int absDy = abs(dy);

        if (absDy < absDx && dx > 0 && dy >= 0) {
            int d = absDx - 2 * absDy;
            int incE = -2 * absDy;
            int incNE = 2 * (absDx - absDy);
            int x = xa, y = ya;

            while (x <= xb) {
                if (y >= 0 && y < (int)info.scanlines.size()) {
                    info.scanlines[y].first = std::min(info.scanlines[y].first, x);
                    info.scanlines[y].second = std::max(info.scanlines[y].second, x);
                }
                if (x == xb) break;
                if (d <= 0) { d += incNE; y++; }
                else { d += incE; }
                x++;
            }
        }
        else if (absDy >= absDx && dx >= 0 && dy > 0) {
            int d = absDy - 2 * absDx;
            int incN = -2 * absDx;
            int incNE = 2 * (absDy - absDx);
            int x = xa, y = ya;

            while (y <= yb) {
                if (y >= 0 && y < (int)info.scanlines.size()) {
                    info.scanlines[y].first = std::min(info.scanlines[y].first, x);
                    info.scanlines[y].second = std::max(info.scanlines[y].second, x);
                }
                if (y == yb) break;
                if (d <= 0) { d += incNE; x++; }
                else { d += incN; }
                y++;
            }
        }
        else if (absDy < absDx && dx > 0 && dy < 0) {
            int d = absDx - 2 * absDy;
            int incE = -2 * absDy;
            int incSE = 2 * (absDx - absDy);
            int x = xa, y = ya;

            while (x <= xb) {
                if (y >= 0 && y < (int)info.scanlines.size()) {
                    info.scanlines[y].first = std::min(info.scanlines[y].first, x);
                    info.scanlines[y].second = std::max(info.scanlines[y].second, x);
                }
                if (x == xb) break;
                if (d <= 0) { d += incSE; y--; }
                else { d += incE; }
                x++;
            }
        }
        else if (absDy >= absDx && dx >= 0 && dy < 0) {
            int d = absDy - 2 * absDx;
            int incS = -2 * absDx;
            int incSE = 2 * (absDy - absDx);
            int x = xa, y = ya;

            while (y >= yb) {
                if (y >= 0 && y < (int)info.scanlines.size()) {
                    info.scanlines[y].first = std::min(info.scanlines[y].first, x);
                    info.scanlines[y].second = std::max(info.scanlines[y].second, x);
                }
                if (y == yb) break;
                if (d <= 0) { d += incSE; x++; }
                else { d += incS; }
                y--;
            }
        }
        else if (dx < 0) {
            calculateTriangleFillLine(info, xb, yb, xa, ya);
        }
    }

    void drawTriangleFilled(int x0, int y0, int x1, int y1, int x2, int y2, RGBA fillColor, RGBA borderColor, int thickness)
    {
        TriangleFillInfo fillInfo;
        fillInfo.scanlines.resize(height, { width, -1 });

        calculateTriangleFillLine(fillInfo, x0, y0, x1, y1);
        calculateTriangleFillLine(fillInfo, x1, y1, x2, y2);
        calculateTriangleFillLine(fillInfo, x2, y2, x0, y0);

        int ymin = std::min({ y0, y1, y2 });
        int ymax = std::max({ y0, y1, y2 });

        int innerMargin = (thickness - 1) / 2 + 1;

        for (int y = ymin; y <= ymax; ++y) {
            if (y < 0 || y >= height) continue;

            auto edge = fillInfo.scanlines[y];
            if (edge.second < 0) continue;

            int xmin = edge.first + innerMargin;
            int xmax = edge.second - innerMargin;

            if (xmin <= xmax) {
                drawHorizontalLine(xmin, xmax, y, fillColor);
            }
        }

        drawTriangleOutline(x0, y0, x1, y1, x2, y2, borderColor, thickness);
    }

    void drawEllipseFilled(int cx, int cy, int a, int b, RGBA fillColor, RGBA borderColor, int thickness)
    {
        if (a <= 0 || b <= 0) return;

        // Primero dibujamos el borde - m_drawnPixels se llena automáticamente
        drawEllipseOutline(cx, cy, a, b, borderColor, thickness);

        // Guardamos TODOS los píxeles del borde (incluyendo los que crecen hacia adentro)
        std::set<std::pair<int, int>> borderPixels = m_drawnPixels;
        m_drawnPixels.clear();

        // Ahora rellenamos, usando un radio generoso
        int searchRadius = a + b; // Radio de búsqueda

        for (int y = cy - searchRadius; y <= cy + searchRadius; ++y) {
            if (y < 0 || y >= height) continue;

            for (int x = cx - searchRadius; x <= cx + searchRadius; ++x) {
                if (x < 0 || x >= width) continue;

                // Si el píxel está en el borde (incluyendo thickness), saltarlo
                if (borderPixels.find({ x, y }) != borderPixels.end()) {
                    continue;
                }

                // Verificar si está dentro de la elipse
                float dx = static_cast<float>(x - cx) / static_cast<float>(a);
                float dy = static_cast<float>(y - cy) / static_cast<float>(b);

                if (dx * dx + dy * dy <= 1.0f) {
                    setPixel(x, y, fillColor);
                }
            }
        }
    }

    void drawRectangleFilled(int x0, int y0, int x1, int y1, RGBA fillColor, RGBA borderColor, int thickness)
    {
        int xmin = std::min(x0, x1);
        int xmax = std::max(x0, x1);
        int ymin = std::min(y0, y1);
        int ymax = std::max(y0, y1);

        drawRectangleOutline(xmin, ymin, xmax, ymax, borderColor, thickness);

        int innerMargin = (thickness - 1) / 2;
        int innerXmin = xmin + innerMargin + 1;
        int innerXmax = xmax - innerMargin - 1;
        int innerYmin = ymin + innerMargin + 1;
        int innerYmax = ymax - innerMargin - 1;

        if (innerXmin <= innerXmax && innerYmin <= innerYmax) {
            for (int y = innerYmin; y <= innerYmax; ++y) {
                drawHorizontalLine(innerXmin, innerXmax, y, fillColor);
            }
        }
    }

    std::pair<int, int> deCasteljau(const std::vector<std::pair<int, int>>& points, float t) {
        if (points.empty()) return { 0, 0 };

        std::vector<std::pair<float, float>> temp(points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            temp[i] = { static_cast<float>(points[i].first),
                       static_cast<float>(points[i].second) };
        }

        int n = temp.size() - 1;
        for (int k = 1; k <= n; ++k) {
            for (int i = 0; i <= n - k; ++i) {
                temp[i].first = (1.0f - t) * temp[i].first + t * temp[i + 1].first;
                temp[i].second = (1.0f - t) * temp[i].second + t * temp[i + 1].second;
            }
        }

        return { static_cast<int>(temp[0].first), static_cast<int>(temp[0].second) };
    }

    std::pair<int, int> deCasteljauTemp(const std::vector<std::pair<int, int>>& points, float t) {
        return deCasteljau(points, t);
    }

    BezierCurve* findBezierCurveNear(int x, int y, int& controlPointIndex) {
        controlPointIndex = -1;
        const int TOLERANCE = 15;

        for (auto it = m_shapes.rbegin(); it != m_shapes.rend(); ++it) {
            BezierCurve* bezier = dynamic_cast<BezierCurve*>(it->get());
            if (bezier) {
                for (size_t i = 0; i < bezier->controlPoints.size(); ++i) {
                    const auto& p = bezier->controlPoints[i];
                    int dx = p.first - x;
                    int dy = p.second - y;
                    if (dx * dx + dy * dy <= TOLERANCE * TOLERANCE) {
                        controlPointIndex = i;
                        return bezier;
                    }
                }

                int segments = 50;
                for (int j = 0; j <= segments; ++j) {
                    float t = (float)j / segments;
                    std::pair<int, int> p = deCasteljau(bezier->controlPoints, t);
                    int dx = p.first - x;
                    int dy = p.second - y;
                    if (dx * dx + dy * dy <= TOLERANCE * TOLERANCE) {
                        controlPointIndex = -1;
                        return bezier;
                    }
                }
            }
        }
        return nullptr;
    }

    void update()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), RGBA{ 201,201,201,255 });
        framesThisSecond++;

        for (const auto& shape : m_shapes) {
            shape->draw(this);
        }

        if (m_drawMode == 4 && m_editMode == 0) {
            for (size_t i = 0; i < m_tempControlPoints.size(); ++i) {
                const auto& p = m_tempControlPoints[i];
                if (i < m_tempControlPoints.size() - 1) {
                    const auto& pNext = m_tempControlPoints[i + 1];
                    drawLine(p.first, p.second, pNext.first, pNext.second,
                        m_controlPolygonColor, 1);
                }
                drawEllipseFilled(p.first, p.second, m_bezierControlPointRadius,
                    m_bezierControlPointRadius,
                    m_controlPointColor, m_controlPointColor, 1);
            }

            if (!m_tempControlPoints.empty()) {
                double xpos_d, ypos_d;
                glfwGetCursorPos(m_window, &xpos_d, &ypos_d);
                int cx = static_cast<int>(xpos_d);
                int cy = height - 1 - static_cast<int>(ypos_d);

                const auto& lastPoint = m_tempControlPoints.back();
                drawLine(lastPoint.first, lastPoint.second, cx, cy,
                    m_controlPolygonColor, 1);

                drawEllipseFilled(cx, cy, m_bezierControlPointRadius / 2,
                    m_bezierControlPointRadius / 2,
                    m_controlPolygonColor, m_controlPolygonColor, 1);
            }

            if (m_tempControlPoints.size() >= 2) {
                int segments = 100;
                std::pair<int, int> p0 = deCasteljauTemp(m_tempControlPoints, 0.0f);

                for (int i = 1; i <= segments; ++i) {
                    float t = (float)i / segments;
                    std::pair<int, int> p1 = deCasteljauTemp(m_tempControlPoints, t);
                    drawLine(p0.first, p0.second, p1.first, p1.second, m_borderColor, m_lineThickness);
                    p0 = p1;
                }
            }
        }

        if (m_x0 >= 0 && m_y0 >= 0 && m_x1 >= 0 && m_y1 >= 0)
        {
            if (m_drawMode == 0) {
                drawLine(m_x0, m_y0, m_x1, m_y1, m_borderColor, m_lineThickness);
            }
            else if (m_drawMode == 1) {
                int a = std::abs(m_x1 - m_x0);
                int b = std::abs(m_y1 - m_y0);
                if (m_useFilledShapes) {
                    drawEllipseFilled(m_x0, m_y0, a, b, m_fillColor, m_borderColor, m_lineThickness);
                }
                else {
                    drawEllipseOutline(m_x0, m_y0, a, b, m_borderColor, m_lineThickness);
                }
            }
            else if (m_drawMode == 2) {
                if (m_useFilledShapes) {
                    drawRectangleFilled(m_x0, m_y0, m_x1, m_y1, m_fillColor, m_borderColor, m_lineThickness);
                }
                else {
                    drawRectangleOutline(m_x0, m_y0, m_x1, m_y1, m_borderColor, m_lineThickness);
                }
            }
            else if (m_drawMode == 3) {
                if (m_triClicks == 1) {
                    drawLine(m_triTempX[0], m_triTempY[0], m_x1, m_y1, m_borderColor, m_lineThickness);
                }
            }
        }
        else {
            if (m_drawMode == 3 && m_triClicks > 0) {
                double xpos_d, ypos_d;
                glfwGetCursorPos(m_window, &xpos_d, &ypos_d);
                int cx = static_cast<int>(xpos_d);
                int cy = height - 1 - static_cast<int>(ypos_d);
                if (m_triClicks == 1) {
                    drawLine(m_triTempX[0], m_triTempY[0], cx, cy, m_borderColor, m_lineThickness);
                }
                else if (m_triClicks == 2) {
                    if (m_useFilledShapes) {
                        drawTriangleFilled(m_triTempX[0], m_triTempY[0], m_triTempX[1], m_triTempY[1], cx, cy, m_fillColor, m_borderColor, m_lineThickness);
                    }
                    else {
                        drawTriangleOutline(m_triTempX[0], m_triTempY[0], m_triTempX[1], m_triTempY[1], cx, cy, m_borderColor, m_lineThickness);
                    }
                }
            }
        }
    }

    void onKey(int key, int scancode, int action, int mods)
    {
        if (action == GLFW_PRESS)
        {
            std::cout << "Key " << key << " pressed\n";
            if (key == GLFW_KEY_ESCAPE)
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);

            // Tecla E para cambiar entre modo crear/editar
            if (key == GLFW_KEY_E && m_drawMode == 4) {
                m_editMode = (m_editMode == 0) ? 1 : 0;
                m_editingCurve = nullptr;
                m_selectedControlPoint = -1;
                m_tempControlPoints.clear();
                std::cout << "Edit mode: " << (m_editMode == 0 ? "Create" : "Edit") << "\n";
            }
        }
        else if (action == GLFW_RELEASE)
            std::cout << "Key " << key << " released\n";
    }

    void onMouseButton(int button, int action, int mods)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        if (button >= 0 && button < 3)
        {
            double xpos, ypos;
            glfwGetCursorPos(m_window, &xpos, &ypos);
            int tx = static_cast<int>(xpos);
            int ty = height - 1 - static_cast<int>(ypos);

            if (action == GLFW_PRESS)
            {
                mouseButtonsDown[button] = true;

                if (m_drawMode == 4) { // Bezier mode
                    if (m_editMode == 0) { // Modo crear
                        if (button == 0) {
                            m_tempControlPoints.push_back({ tx, ty });
                            std::cout << "Added control point (" << tx << ", " << ty
                                << "). Total: " << m_tempControlPoints.size() << "\n";
                        }
                        else if (button == 1) {
                            if (m_tempControlPoints.size() >= 2) {
                                auto bz = std::make_unique<BezierCurve>();
                                bz->controlPoints = m_tempControlPoints;
                                bz->borderColor = m_borderColor;
                                bz->fillColor = m_fillColor;
                                bz->thickness = m_lineThickness;
                                bz->filled = false;
                                m_shapes.push_back(std::move(bz));
                                std::cout << "Bezier Curve finalized with "
                                    << m_tempControlPoints.size() << " points.\n";
                                m_tempControlPoints.clear();
                            }
                            else {
                                std::cout << "Need at least 2 points to finalize a Bezier Curve.\n";
                            }
                        }
                        else if (button == 2) {
                            m_tempControlPoints.clear();
                            std::cout << "Bezier Curve canceled.\n";
                        }
                    }
                    else if (m_editMode == 1) { // Modo editar
                        if (button == 0) {
                            int cpIndex;
                            BezierCurve* curve = findBezierCurveNear(tx, ty, cpIndex);
                            if (curve && cpIndex >= 0) {
                                m_editingCurve = curve;
                                m_selectedControlPoint = cpIndex;
                                m_isDraggingControlPoint = true;
                                m_originalMousePos = { tx, ty };
                                std::cout << "Selected control point " << cpIndex << "\n";
                            }
                            else if (curve && cpIndex < 0) {
                                m_editingCurve = curve;
                                m_selectedControlPoint = -1;
                                std::cout << "Selected entire curve\n";
                            }
                        }
                    }
                }
                else if (m_drawMode < 3) {
                    if (button == 0)
                    {
                        m_x0 = tx;
                        m_y0 = ty;
                        m_x1 = m_x0;
                        m_y1 = m_y0;
                        std::cout << "Inicio de figura en (" << m_x0 << ", " << m_y0 << ")\n";
                    }
                }
                else if (m_drawMode == 3) {
                    if (button == 0) {
                        if (m_triClicks < 3) {
                            m_triTempX[m_triClicks] = tx;
                            m_triTempY[m_triClicks] = ty;
                            m_triClicks++;
                            std::cout << "Triangle click " << m_triClicks << " at (" << tx << ", " << ty << ")\n";
                        }
                        if (m_triClicks == 3) {
                            auto tr = std::make_unique<Triangle>();
                            tr->x0 = m_triTempX[0]; tr->y0 = m_triTempY[0];
                            tr->x1 = m_triTempX[1]; tr->y1 = m_triTempY[1];
                            tr->x2 = m_triTempX[2]; tr->y2 = m_triTempY[2];
                            tr->borderColor = m_borderColor;
                            tr->fillColor = m_fillColor;
                            tr->thickness = m_lineThickness;
                            tr->filled = m_useFilledShapes;
                            m_shapes.push_back(std::move(tr));
                            m_triClicks = 0;
                            std::cout << "Triangle finalized.\n";
                        }
                    }
                }
            }
            else if (action == GLFW_RELEASE)
            {
                mouseButtonsDown[button] = false;

                if (m_drawMode == 4 && m_editMode == 1 && button == 0) {
                    m_isDraggingControlPoint = false;
                }
                else if (m_drawMode != 3 && m_drawMode != 4) {
                    if (button == 0)
                    {
                        m_x1 = tx;
                        m_y1 = ty;
                        std::cout << "Figura finalizada en (" << m_x1 << ", " << m_y1 << ")\n";

                        if (m_drawMode == 0) {
                            auto ln = std::make_unique<Line>();
                            ln->x0 = m_x0; ln->y0 = m_y0; ln->x1 = m_x1; ln->y1 = m_y1;
                            ln->borderColor = m_borderColor;
                            ln->fillColor = m_fillColor;
                            ln->thickness = m_lineThickness;
                            ln->filled = m_useFilledShapes;
                            m_shapes.push_back(std::move(ln));
                        }
                        else if (m_drawMode == 1) {
                            auto el = std::make_unique<Ellipse>();
                            el->cx = m_x0; el->cy = m_y0;
                            el->a = std::abs(m_x1 - m_x0); el->b = std::abs(m_y1 - m_y0);
                            el->borderColor = m_borderColor;
                            el->fillColor = m_fillColor;
                            el->thickness = m_lineThickness;
                            el->filled = m_useFilledShapes;
                            m_shapes.push_back(std::move(el));
                        }
                        else if (m_drawMode == 2) {
                            auto rc = std::make_unique<Rectangle>();
                            rc->xmin = std::min(m_x0, m_x1);
                            rc->xmax = std::max(m_x0, m_x1);
                            rc->ymin = std::min(m_y0, m_y1);
                            rc->ymax = std::max(m_y0, m_y1);
                            rc->borderColor = m_borderColor;
                            rc->fillColor = m_fillColor;
                            rc->thickness = m_lineThickness;
                            rc->filled = m_useFilledShapes;
                            m_shapes.push_back(std::move(rc));
                        }

                        m_x0 = -1; m_y0 = -1; m_x1 = -1; m_y1 = -1;
                    }
                }
            }
        }
    }

    void onCursorPos(double xpos_d, double ypos_d)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        int xpos = static_cast<int>(xpos_d);
        int ypos = height - 1 - static_cast<int>(ypos_d);

        // Edición de curvas Bézier
        if (m_drawMode == 4 && m_editMode == 1 && m_isDraggingControlPoint && m_editingCurve && m_selectedControlPoint >= 0) {
            m_editingCurve->controlPoints[m_selectedControlPoint] = { xpos, ypos };
        }
        else if (m_drawMode != 3) {
            if (mouseButtonsDown[0])
            {
                m_x1 = xpos;
                m_y1 = ypos;
            }
        }
        else {
            m_x1 = xpos; m_y1 = ypos;
        }
    }

    void drawInterface() override
    {
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(350, (float)height), ImGuiCond_Always);
        ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("Control Panel", nullptr, panelFlags);

        const char* modes[] = { "Line", "Ellipse", "Rectangle", "Triangle", "Bezier Curve" };
        ImGui::Text("Draw Mode:");
        ImGui::ListBox("##mode", &m_drawMode, modes, IM_ARRAYSIZE(modes), 5);

        if (m_drawMode > 0 && m_drawMode < 4)
        {
            ImGui::Checkbox("Draw Filled Shapes", &m_useFilledShapes);
            ImGui::SameLine();
            ImGui::TextDisabled("(tip)");
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Cuando esta activado, las nuevas figuras se dibujaran con relleno.");
                ImGui::Text("Cuando esta desactivado, solo se dibujara el borde.");
                ImGui::Text("Las figuras ya dibujadas no cambian.");
                ImGui::EndTooltip();
            }
        }

        if (m_drawMode == 3) {
            ImGui::Separator();
            ImGui::TextWrapped("Triangle mode: click three times to place the three vertices. Current clicks: %d", m_triClicks);
            if (ImGui::Button("Reset Triangle Clicks")) m_triClicks = 0;
        }

        if (m_drawMode == 4) {
            ImGui::Separator();
            ImGui::TextWrapped("Bezier Curve mode:");

            const char* editModes[] = { "Create", "Edit" };
            ImGui::Text("Mode:");
            ImGui::RadioButton("Create", &m_editMode, 0); ImGui::SameLine();
            ImGui::RadioButton("Edit", &m_editMode, 1);

            // Controles de color para los puntos de control de Bézier
            ImGui::Separator();
            ImGui::Text("Control Point Colors:");

            float controlPointCol[4] = {
                m_controlPointColor.r / 255.0f,
                m_controlPointColor.g / 255.0f,
                m_controlPointColor.b / 255.0f,
                m_controlPointColor.a / 255.0f
            };
            if (ImGui::ColorEdit4("Normal Point", controlPointCol)) {
                m_controlPointColor.r = static_cast<unsigned char>(controlPointCol[0] * 255.0f);
                m_controlPointColor.g = static_cast<unsigned char>(controlPointCol[1] * 255.0f);
                m_controlPointColor.b = static_cast<unsigned char>(controlPointCol[2] * 255.0f);
                m_controlPointColor.a = static_cast<unsigned char>(controlPointCol[3] * 255.0f);
            }

            float selectedControlPointCol[4] = {
                m_selectedControlPointColor.r / 255.0f,
                m_selectedControlPointColor.g / 255.0f,
                m_selectedControlPointColor.b / 255.0f,
                m_selectedControlPointColor.a / 255.0f
            };
            if (ImGui::ColorEdit4("Selected Point", selectedControlPointCol)) {
                m_selectedControlPointColor.r = static_cast<unsigned char>(selectedControlPointCol[0] * 255.0f);
                m_selectedControlPointColor.g = static_cast<unsigned char>(selectedControlPointCol[1] * 255.0f);
                m_selectedControlPointColor.b = static_cast<unsigned char>(selectedControlPointCol[2] * 255.0f);
                m_selectedControlPointColor.a = static_cast<unsigned char>(selectedControlPointCol[3] * 255.0f);
            }

            float controlPolygonCol[4] = {
                m_controlPolygonColor.r / 255.0f,
                m_controlPolygonColor.g / 255.0f,
                m_controlPolygonColor.b / 255.0f,
                m_controlPolygonColor.a / 255.0f
            };
            if (ImGui::ColorEdit4("Polygon Lines", controlPolygonCol)) {
                m_controlPolygonColor.r = static_cast<unsigned char>(controlPolygonCol[0] * 255.0f);
                m_controlPolygonColor.g = static_cast<unsigned char>(controlPolygonCol[1] * 255.0f);
                m_controlPolygonColor.b = static_cast<unsigned char>(controlPolygonCol[2] * 255.0f);
                m_controlPolygonColor.a = static_cast<unsigned char>(controlPolygonCol[3] * 255.0f);
            }

            ImGui::Separator();

            if (m_editMode == 0) {
                ImGui::TextWrapped("- Left click: add control point");
                ImGui::TextWrapped("- Right click: finalize curve");
                ImGui::TextWrapped("- Middle click: cancel");
                ImGui::Text("Control points: %d", (int)m_tempControlPoints.size());
            }
            else {
                ImGui::TextWrapped("- Click on a control point to drag it");
                ImGui::TextWrapped("- Press E to toggle between Create/Edit modes");
                if (m_editingCurve) {
                    ImGui::Text("Editing curve with %d points", (int)m_editingCurve->controlPoints.size());
                    if (m_selectedControlPoint >= 0) {
                        ImGui::Text("Selected point: %d", m_selectedControlPoint);
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::SliderInt("Line Thickness", &m_lineThickness, 1, 31);
        ImGui::Separator();

        float borderCol[4] = {
            m_borderColor.r / 255.0f,
            m_borderColor.g / 255.0f,
            m_borderColor.b / 255.0f,
            m_borderColor.a / 255.0f
        };
        if (ImGui::ColorEdit4("Border Color", borderCol)) {
            m_borderColor.r = static_cast<unsigned char>(borderCol[0] * 255.0f);
            m_borderColor.g = static_cast<unsigned char>(borderCol[1] * 255.0f);
            m_borderColor.b = static_cast<unsigned char>(borderCol[2] * 255.0f);
            m_borderColor.a = static_cast<unsigned char>(borderCol[3] * 255.0f);
        }

        float fillCol[4] = {
            m_fillColor.r / 255.0f,
            m_fillColor.g / 255.0f,
            m_fillColor.b / 255.0f,
            m_fillColor.a / 255.0f
        };
        if (ImGui::ColorEdit4("Fill Color", fillCol)) {
            m_fillColor.r = static_cast<unsigned char>(fillCol[0] * 255.0f);
            m_fillColor.g = static_cast<unsigned char>(fillCol[1] * 255.0f);
            m_fillColor.b = static_cast<unsigned char>(fillCol[2] * 255.0f);
            m_fillColor.a = static_cast<unsigned char>(fillCol[3] * 255.0f);
        }

        ImGui::Separator();
        if (ImGui::Button("Clear screen")) {
            m_shapes.clear();
            m_triClicks = 0;
            m_tempControlPoints.clear();
            m_editingCurve = nullptr;
            m_selectedControlPoint = -1;
        }

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (deltaTime >= 1.0) {
            double fps = framesThisSecond / deltaTime;
            char title[256];
            snprintf(title, sizeof(title), "CPixelRender - Frames per second: %.2f", fps);
            glfwSetWindowTitle(m_window, title);
            framesThisSecond = 0;
            lastTime = currentTime;
        }
    }
};

int main() {
    CMyTest test;
    if (!test.setup()) {
        fprintf(stderr, "Failed to setup CPixelRender\n");
        return -1;
    }

    test.mainLoop();

    return 0;
}