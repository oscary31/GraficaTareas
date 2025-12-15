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
    RGBA m_borderColor = { 0,0,0,255 }; // Color del borde
    RGBA m_fillColor = { 255,255,255,255 }; // Color del relleno
    int m_drawMode = 0; // 0=Line,1=Ellipse,2=Rectangle,3=Triangle, 4=Bezier
    int m_lineThickness = 1; // line thickness
    bool m_useFilledShapes = false;

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
        int cx, cy; // center
        int a, b; // radii
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

    struct BezierCurve : public Shape {
        std::vector<std::pair<int, int>> controlPoints;

        void draw(CMyTest* renderer) override {
            if (controlPoints.size() < 2) return;

            // 1. Dibujar el Polígono de Control (líneas)
            for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
                const auto& pA = controlPoints[i];
                const auto& pB = controlPoints[i + 1];
                // Color de línea de control: Gris oscuro
                renderer->drawLine(pA.first, pA.second, pB.first, pB.second,
                    { 0x88, 0x88, 0x88, 0xFF }, 1);
            }

            // 2. Dibujar los Puntos de Control (círculos rellenos)
            int pointRadius = 5;
            for (const auto& p : controlPoints) {
                // Color de relleno de punto: Rojo/Naranja
                renderer->drawEllipseFilled(p.first, p.second, pointRadius, pointRadius,
                    { 0xFF, 0x77, 0x00, 0xFF }, { 0xFF, 0x77, 0x00, 0xFF }, 1);
            }

            // 3. Dibujar la Curva (Algoritmo de Casteljau)
            // Usaremos 100 segmentos para una aproximación suave
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

    // Lista unificada de todas las figuras en orden de creación
    std::vector<std::unique_ptr<Shape>> m_shapes;

    // Temp storage for triangle clicks
    int m_triTempX[3];
    int m_triTempY[3];
    int m_triClicks = 0;

    int framesThisSecond = 0; // contador de frames para calcular FPS

    // Set para evitar dibujar el mismo píxel dos veces en una misma operación
    std::set<std::pair<int, int>> m_drawnPixels;

    // Estructura para almacenar los límites internos del triángulo sin grosor
    struct TriangleFillInfo {
        std::vector<std::pair<int, int>> scanlines; // xmin, xmax por cada y
    };

    // Modo 4: Bezier
    int m_bezierControlPointRadius = 5;
    std::vector<std::pair<int, int>> m_tempControlPoints;

public:
    CMyTest() {};
    ~CMyTest() {};

    // Helper: set a thick pixel (square) centered at x,y
    // Usa un conjunto para evitar redibujar el mismo píxel
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

                // Solo dibujar si no hemos dibujado este píxel antes en esta operación
                if (m_drawnPixels.find({ px, py }) == m_drawnPixels.end()) {
                    setPixel(px, py, color);
                    m_drawnPixels.insert({ px, py });
                }
            }
        }
    }

    // Algoritmo de Bresenham para dibujar líneas (con grosor)
    void drawLineBresenham(int x0, int y0, int x1, int y1, RGBA color, int thickness)
    {
        // Limpiar el conjunto de píxeles dibujados al inicio de cada línea
        m_drawnPixels.clear();

        int dx = x1 - x0;
        int dy = y1 - y0;
        int absDx = abs(dx);
        int absDy = abs(dy);

        // Caso 1: 0 <= m < 1 
        if (absDy < absDx && dx > 0 && dy >= 0)
        {
            int d = absDx - 2 * absDy;
            int incE = -2 * absDy;
            int incNE = 2 * (absDx - absDy);
            int x = x0, y = y0;

            setThickPixel(x, y, color, thickness);
            while (x < x1)
            {
                if (d <= 0)
                {
                    d += incNE;
                    y++;
                }
                else
                {
                    d += incE;
                }
                x++;
                setThickPixel(x, y, color, thickness);
            }
        }
        // Caso 2: m >= 1 
        else if (absDy >= absDx && dx >= 0 && dy > 0)
        {
            int d = absDy - 2 * absDx;
            int incN = -2 * absDx;
            int incNE = 2 * (absDy - absDx);
            int x = x0, y = y0;

            setThickPixel(x, y, color, thickness);
            while (y < y1)
            {
                if (d <= 0)
                {
                    d += incNE;
                    x++;
                }
                else
                {
                    d += incN;
                }
                y++;
                setThickPixel(x, y, color, thickness);
            }
        }
        // Caso 3: -1 < m <= 0 
        else if (absDy < absDx && dx > 0 && dy < 0)
        {
            int d = absDx - 2 * absDy;
            int incE = -2 * absDy;
            int incSE = 2 * (absDx - absDy);
            int x = x0, y = y0;

            setThickPixel(x, y, color, thickness);
            while (x < x1)
            {
                if (d <= 0)
                {
                    d += incSE;
                    y--;
                }
                else
                {
                    d += incE;
                }
                x++;
                setThickPixel(x, y, color, thickness);
            }
        }
        // Caso 4: m <= -1 
        else if (absDy >= absDx && dx >= 0 && dy < 0)
        {
            int d = absDy - 2 * absDx;
            int incS = -2 * absDx;
            int incSE = 2 * (absDy - absDx);
            int x = x0, y = y0;

            setThickPixel(x, y, color, thickness);
            while (y > y1)
            {
                if (d <= 0)
                {
                    d += incSE;
                    x++;
                }
                else
                {
                    d += incS;
                }
                y--;
                setThickPixel(x, y, color, thickness);
            }
        }
        // Casos con dx < 0
        else if (dx < 0)
        {
            drawLineBresenham(x1, y1, x0, y0, color, thickness);
        }

        // Limpiar el conjunto al final
        m_drawnPixels.clear();
    }

    void drawLine(int x0, int y0, int x1, int y1, RGBA color, int thickness = 1)
    {
        drawLineBresenham(x0, y0, x1, y1, color, thickness);
    }

    // Dibuja 4 puntos simétricos de la elipse centrada en (cx,cy) con grosor
    void ellipsePoints4(long long cx, long long cy, long long x, long long y, RGBA color, int thickness)
    {
        setThickPixel(static_cast<int>(cx + x), static_cast<int>(cy + y), color, thickness);
        setThickPixel(static_cast<int>(cx - x), static_cast<int>(cy + y), color, thickness);
        setThickPixel(static_cast<int>(cx + x), static_cast<int>(cy - y), color, thickness);
        setThickPixel(static_cast<int>(cx - x), static_cast<int>(cy - y), color, thickness);
    }

    // drawEllipseOutline using long long integer arithmetic (with thickness)
    void drawEllipseOutline(int cx, int cy, int a, int b, RGBA color, int thickness)
    {
        // Limpiar el conjunto de píxeles dibujados
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

        // Limpiar el conjunto al final
        m_drawnPixels.clear();
    }

    void drawEllipseOutline(int cx, int cy, int a, int b, RGBA color)
    {
        drawEllipseOutline(cx, cy, a, b, color, 1);
    }

    // Draw rectangle by drawing its four edges (use drawLine with thickness)
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

    // Draw triangle by drawing three lines
    void drawTriangleOutline(int x0, int y0, int x1, int y1, int x2, int y2, RGBA color, int thickness)
    {
        drawLine(x0, y0, x1, y1, color, thickness);
        drawLine(x1, y1, x2, y2, color, thickness);
        drawLine(x2, y2, x0, y0, color, thickness);
    }

    // Helper: draw horizontal line (internal, no thick pixels)
    void drawHorizontalLine(int x0, int x1, int y, RGBA color)
    {
        if (x0 > x1) std::swap(x0, x1);
        for (int x = x0; x <= x1; ++x) {
            setPixel(x, y, color);
        }
    }

    // Calcular límites de relleno usando líneas sin grosor
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

    // Relleno de triángulo: primero relleno, luego borde
    void drawTriangleFilled(int x0, int y0, int x1, int y1, int x2, int y2, RGBA fillColor, RGBA borderColor, int thickness)
    {
        // Calcular los límites internos del triángulo (sin considerar grosor)
        TriangleFillInfo fillInfo;
        fillInfo.scanlines.resize(height, { width, -1 });

        // Calcular las tres aristas sin grosor
        calculateTriangleFillLine(fillInfo, x0, y0, x1, y1);
        calculateTriangleFillLine(fillInfo, x1, y1, x2, y2);
        calculateTriangleFillLine(fillInfo, x2, y2, x0, y0);

        // Encontrar el rango de y
        int ymin = std::min({ y0, y1, y2 });
        int ymax = std::max({ y0, y1, y2 });

        // Calcular el margen interno basado en el grosor del borde
        int innerMargin = (thickness - 1) / 2 + 1;

        // Dibujar el relleno, ajustando para que quede dentro del borde
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

        // Dibujar el borde encima del relleno
        drawTriangleOutline(x0, y0, x1, y1, x2, y2, borderColor, thickness);
    }

    // Relleno de elipse siguiendo el PDF
    void drawEllipseFilled(int cx, int cy, int a, int b, RGBA fillColor, RGBA borderColor, int thickness)
    {
        if (a <= 0 || b <= 0) return;

        // Primero dibujar el borde completo
        drawEllipseOutline(cx, cy, a, b, borderColor, thickness);

        // Calcular cuánto del grosor del borde se adentra hacia el interior
        int innerMargin = (thickness - 1) / 2;

        // Reducir los radios para el relleno, considerando que el borde ocupa espacio hacia adentro
        int innerA = a - innerMargin - 1;
        int innerB = b - innerMargin - 1;

        if (innerA <= 0 || innerB <= 0) return; // No hay espacio para rellenar

        // Rellenar el área interior de la elipse
        for (int y = cy - innerB; y <= cy + innerB; ++y) {
            if (y < 0 || y >= height) continue;

            // Calcular los límites horizontales usando la ecuación de la elipse interior
            float y_rel = static_cast<float>(y - cy) / innerB;
            float x_width = innerA * std::sqrt(1.0f - y_rel * y_rel);
            int x_start = static_cast<int>(cx - x_width);
            int x_end = static_cast<int>(cx + x_width);

            if (x_start <= x_end) {
                for (int x = x_start; x <= x_end; ++x) {
                    setPixel(x, y, fillColor);
                }
            }
        }
    }


    // Relleno de rectángulo siguiendo el PDF
    void drawRectangleFilled(int x0, int y0, int x1, int y1, RGBA fillColor, RGBA borderColor, int thickness)
    {
        int xmin = std::min(x0, x1);
        int xmax = std::max(x0, x1);
        int ymin = std::min(y0, y1);
        int ymax = std::max(y0, y1);

        // Primero dibujar el borde completo
        drawRectangleOutline(xmin, ymin, xmax, ymax, borderColor, thickness);

        // Calcular cuánto del grosor del borde se adentra hacia el interior
        int innerMargin = (thickness - 1) / 2;

        // Ajustar para que el relleno empiece DESPUÉS de la parte interior del borde
        int innerXmin = xmin + innerMargin + 1;
        int innerXmax = xmax - innerMargin - 1;
        int innerYmin = ymin + innerMargin + 1;
        int innerYmax = ymax - innerMargin - 1;

        // Solo rellenar si hay espacio interior después de considerar el borde
        if (innerXmin <= innerXmax && innerYmin <= innerYmax) {
            for (int y = innerYmin; y <= innerYmax; ++y) {
                drawHorizontalLine(innerXmin, innerXmax, y, fillColor);
            }
        }
    }

	// Algoritmo de De Casteljau para curvas Bézier
    std::pair<int, int> deCasteljau(const std::vector<std::pair<int, int>>& points, float t) {
        if (points.empty()) return { 0, 0 };

        // Copiar puntos a flotantes para el cálculo
        std::vector<std::pair<float, float>> temp(points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            temp[i] = { static_cast<float>(points[i].first),
                       static_cast<float>(points[i].second) };
        }

        // Algoritmo de De Casteljau
        int n = temp.size() - 1;
        for (int k = 1; k <= n; ++k) {
            for (int i = 0; i <= n - k; ++i) {
                temp[i].first = (1.0f - t) * temp[i].first + t * temp[i + 1].first;
                temp[i].second = (1.0f - t) * temp[i].second + t * temp[i + 1].second;
            }
        }

        return { static_cast<int>(temp[0].first), static_cast<int>(temp[0].second) };
    }

    // Función auxiliar para la previsualización (mismo algoritmo)
    std::pair<int, int> deCasteljauTemp(const std::vector<std::pair<int, int>>& points, float t) {
        return deCasteljau(points, t);
    }

    void update()
    {
        // Fondo gris #C9C9C9 (201,201,201) opaco
        std::fill(m_buffer.begin(), m_buffer.end(), RGBA{ 201,201,201,255 });
        framesThisSecond++;

        // Dibujar todas las figuras en el orden en que fueron creadas
        for (const auto& shape : m_shapes) {
            shape->draw(this);
        }
        // Previsualización de la curva Bézier en construcción
        if (m_drawMode == 4) {
            // Dibujar el polígono de control temporal
            for (size_t i = 0; i < m_tempControlPoints.size(); ++i) {
                const auto& p = m_tempControlPoints[i];
                // Línea de control al punto siguiente (si existe)
                if (i < m_tempControlPoints.size() - 1) {
                    const auto& pNext = m_tempControlPoints[i + 1];
                    drawLine(p.first, p.second, pNext.first, pNext.second,
                        { 0x88, 0x88, 0x88, 0xFF }, 1);
                }
                // Dibujar el punto de control
                drawEllipseFilled(p.first, p.second, m_bezierControlPointRadius,
                    m_bezierControlPointRadius,
                    { 0xFF, 0x77, 0x00, 0xFF }, { 0xFF, 0x77, 0x00, 0xFF }, 1);
            }

            // Dibujar línea temporal desde el último punto al cursor (similar al triángulo)
            if (!m_tempControlPoints.empty()) {
                // Obtener la posición actual del mouse
                double xpos_d, ypos_d;
                glfwGetCursorPos(m_window, &xpos_d, &ypos_d);
                int cx = static_cast<int>(xpos_d);
                int cy = height - 1 - static_cast<int>(ypos_d);

                const auto& lastPoint = m_tempControlPoints.back();
                drawLine(lastPoint.first, lastPoint.second, cx, cy,
                    { 0x88, 0x88, 0x88, 0x88 }, 1); // Color gris semitransparente

                // También dibujar un punto temporal en la posición del cursor
                drawEllipseFilled(cx, cy, m_bezierControlPointRadius / 2,
                    m_bezierControlPointRadius / 2,
                    { 0x88, 0x88, 0x88, 0x88 }, { 0x88, 0x88, 0x88, 0x88 }, 1);
            }

            // Dibujar la curva incompleta (si hay al menos 2 puntos)
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


        // Dibujar la forma actualmente en creación si ambos puntos están definidos
        if (m_x0 >= 0 && m_y0 >= 0 && m_x1 >= 0 && m_y1 >= 0)
        {
            if (m_drawMode == 0) { // Line
                drawLine(m_x0, m_y0, m_x1, m_y1, m_borderColor, m_lineThickness);
            }
            else if (m_drawMode == 1) { // Ellipse
                int a = std::abs(m_x1 - m_x0);
                int b = std::abs(m_y1 - m_y0);
                if (m_useFilledShapes) { // <-- ¡NUEVA LÓGICA DE PREVISUALIZACIÓN!
                    drawEllipseFilled(m_x0, m_y0, a, b, m_fillColor, m_borderColor, m_lineThickness);
                }
                else {
                    drawEllipseOutline(m_x0, m_y0, a, b, m_borderColor, m_lineThickness);
                }
            }
            else if (m_drawMode == 2) { // Rectangle
                if (m_useFilledShapes) { // <-- ¡NUEVA LÓGICA DE PREVISUALIZACIÓN!
                    drawRectangleFilled(m_x0, m_y0, m_x1, m_y1, m_fillColor, m_borderColor, m_lineThickness);
                }
                else {
                    drawRectangleOutline(m_x0, m_y0, m_x1, m_y1, m_borderColor, m_lineThickness);
                }
            }
            else if (m_drawMode == 3) { // Triangle preview
                if (m_triClicks == 1) {
                    drawLine(m_triTempX[0], m_triTempY[0], m_x1, m_y1, m_borderColor, m_lineThickness);
                }

            }

        }
        else {
            // For triangle mode, preview with current mouse position
            if (m_drawMode == 3 && m_triClicks > 0) {
                double xpos_d, ypos_d;
                glfwGetCursorPos(m_window, &xpos_d, &ypos_d);
                int cx = static_cast<int>(xpos_d);
                int cy = height - 1 - static_cast<int>(ypos_d);
                if (m_triClicks == 1) {
                    drawLine(m_triTempX[0], m_triTempY[0], cx, cy, m_borderColor, m_lineThickness);
                }
                else if (m_triClicks == 2) {
                    if (m_useFilledShapes) { // <-- ¡NUEVA LÓGICA DE PREVISUALIZACIÓN!
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

            if (action == GLFW_PRESS)
            {
                mouseButtonsDown[button] = true;

                if (m_drawMode < 3) { // Line/Ellipse/Rectangle: start drag
                    if (button == 0)
                    {
                        m_x0 = static_cast<int>(xpos);
                        m_y0 = height - 1 - static_cast<int>(ypos);
                        m_x1 = m_x0;
                        m_y1 = m_y0;
                        std::cout << "Inicio de figura en (" << m_x0 << ", " << m_y0 << ")\n";
                    }
                }
                else if (m_drawMode == 3) { // Triangle: use clicks to set vertices
                    if (button == 0) {
                        int tx = static_cast<int>(xpos);
                        int ty = height - 1 - static_cast<int>(ypos);
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
                            tr->filled = m_useFilledShapes; // <-- ¡ESTE ES EL CAMBIO CLAVE!
                            m_shapes.push_back(std::move(tr));
                            m_triClicks = 0;
                            std::cout << "Triangle finalized.\n";
                        }
                    }
                }
                else if (m_drawMode == 4) { // Bezier curve mode
                    int tx = static_cast<int>(xpos);
                    int ty = height - 1 - static_cast<int>(ypos);

                    if (button == 0) { // Clic izquierdo: Añadir punto de control
                        m_tempControlPoints.push_back({ tx, ty });
                        std::cout << "Added control point (" << tx << ", " << ty
                            << "). Total: " << m_tempControlPoints.size() << "\n";
                    }
                    else if (button == 1) { // Clic derecho: Finalizar curva
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
                    else if (button == 2) { // Clic medio: Cancelar curva actual
                        m_tempControlPoints.clear();
                        std::cout << "Bezier Curve canceled.\n";
                    }
                }
            }
            else if (action == GLFW_RELEASE)
            {
                mouseButtonsDown[button] = false;

                if (m_drawMode != 3 && m_drawMode != 4) {
                    if (button == 0)
                    {
                        m_x1 = static_cast<int>(xpos);
                        m_y1 = height - 1 - static_cast<int>(ypos);
                        std::cout << "Figura finalizada en (" << m_x1 << ", " << m_y1 << ")\n";

                        if (m_drawMode == 0) { // Line
                            auto ln = std::make_unique<Line>();
                            ln->x0 = m_x0; ln->y0 = m_y0; ln->x1 = m_x1; ln->y1 = m_y1;
                            ln->borderColor = m_borderColor;
                            ln->fillColor = m_fillColor;
                            ln->thickness = m_lineThickness;
                            // Lineas no tienen relleno visible en el código, pero por consistencia:
                            ln->filled = m_useFilledShapes; // <-- Añadido para consistencia, aunque no afecta.
                            m_shapes.push_back(std::move(ln));
                        }
                        else if (m_drawMode == 1) { // Ellipse
                            auto el = std::make_unique<Ellipse>();
                            el->cx = m_x0; el->cy = m_y0;
                            el->a = std::abs(m_x1 - m_x0); el->b = std::abs(m_y1 - m_y0);
                            el->borderColor = m_borderColor;
                            el->fillColor = m_fillColor;
                            el->thickness = m_lineThickness;
                            el->filled = m_useFilledShapes; // <-- ¡ESTE ES EL CAMBIO CLAVE!
                            m_shapes.push_back(std::move(el));
                        }
                        else if (m_drawMode == 2) { // Rectangle
                            auto rc = std::make_unique<Rectangle>();
                            rc->xmin = std::min(m_x0, m_x1);
                            rc->xmax = std::max(m_x0, m_x1);
                            rc->ymin = std::min(m_y0, m_y1);
                            rc->ymax = std::max(m_y0, m_y1);
                            rc->borderColor = m_borderColor;
                            rc->fillColor = m_fillColor;
                            rc->thickness = m_lineThickness;
                            rc->filled = m_useFilledShapes; // <-- ¡ESTE ES EL CAMBIO CLAVE!
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

        if (m_drawMode != 3) {
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

        const char* modes[] = { "Line", "Ellipse", "Rectangle", "Triangle", "Bezier Curve"};
        ImGui::Text("Draw Mode:");
        ImGui::ListBox("##mode", &m_drawMode, modes, IM_ARRAYSIZE(modes), 5);

        

        if (m_drawMode > 0 && m_drawMode < 4)
        {
            // Checkbox para elegir el relleno o solo borde
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

        // Instrucciones para Bézier
        if (m_drawMode == 4) {
            ImGui::Separator();
            ImGui::TextWrapped("Bezier Curve mode:");
            ImGui::TextWrapped("- Clic izquierdo: agrega punto de control");
            ImGui::TextWrapped("- Clic derecho: finalizar curva");
            ImGui::Text("Puntos actuales: %d", (int)m_tempControlPoints.size());
            
        }

        ImGui::Separator();
        ImGui::SliderInt("Line Thickness", &m_lineThickness, 1, 31);
        ImGui::Separator();

        // Color del borde
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

        // Color del relleno
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