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

    // Colores para los puntos de control de Bézier y selección (unificados)
    RGBA m_controlPointColor = { 255, 119, 0, 255 };        // Naranja
    RGBA m_selectedControlPointColor = { 0, 119, 255, 255 }; // Azul
    RGBA m_controlPolygonColor = { 136, 136, 136, 255 };      // Gris para las líneas
    RGBA m_selectionHandleColor = m_controlPointColor;       // Cyan para handles

    // Colores de fondo
    float m_bgColorArray[4] = { 201.0f / 255.0f, 201.0f / 255.0f, 201.0f / 255.0f, 1.0f };
    RGBA m_bgColor = { 216, 216, 216, 255 };

    // Base class para todas las figuras
    struct Shape {
        RGBA borderColor;
        RGBA fillColor;
        int thickness;
        bool filled;
        virtual ~Shape() = default;
        virtual void draw(CMyTest* renderer) = 0;

        // Nuevos métodos para edición y selección
        virtual std::vector<std::pair<int, int>> getControlPoints() { return {}; }
        virtual void setControlPoint(int idx, int x, int y) {}
        virtual void moveBy(int dx, int dy) {}
        virtual bool containsPoint(int x, int y) { return false; }
    };

    struct Line : public Shape {
        int x0, y0, x1, y1;
        void draw(CMyTest* renderer) override {
            renderer->drawLine(x0, y0, x1, y1, borderColor, thickness);
        }
        std::vector<std::pair<int, int>> getControlPoints() override {
            return { {x0,y0}, {x1,y1} };
        }
        void setControlPoint(int idx, int x, int y) override {
            if (idx == 0) { x0 = x; y0 = y; }
            else if (idx == 1) { x1 = x; y1 = y; }
        }
        void moveBy(int dx, int dy) override {
            x0 += dx; y0 += dy; x1 += dx; y1 += dy;
        }
        bool containsPoint(int x, int y) override {
            // distancia punto-segmento
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
        std::vector<std::pair<int, int>> getControlPoints() override {
            // usar bounding box 4 esquinas
            return { {cx - a, cy - b}, {cx + a, cy - b}, {cx + a, cy + b}, {cx - a, cy + b} };
        }
        void setControlPoint(int idx, int x, int y) override {
            auto pts = getControlPoints();
            if (idx < 0 || idx >= (int)pts.size()) return;
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
        void moveBy(int dx, int dy) override {
            cx += dx; cy += dy;
        }
        bool containsPoint(int x, int y) override {
            if (a <= 0 || b <= 0) return false;
            double dx = (double)(x - cx) / (double)a;
            double dy = (double)(y - cy) / (double)b;
            return dx * dx + dy * dy <= 1.0;
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
        std::vector<std::pair<int, int>> getControlPoints() override {
            return { {xmin,ymin}, {xmax,ymin}, {xmax,ymax}, {xmin,ymax} };
        }
        void setControlPoint(int idx, int x, int y) override {
            std::vector<std::pair<int, int>> pts = getControlPoints();
            if (idx < 0 || idx >= (int)pts.size()) return;
            if (idx == 0) { xmin = x; ymin = y; }
            else if (idx == 1) { xmax = x; ymin = y; }
            else if (idx == 2) { xmax = x; ymax = y; }
            else { xmin = x; ymax = y; }
            if (xmin > xmax) std::swap(xmin, xmax);
            if (ymin > ymax) std::swap(ymin, ymax);
        }
        void moveBy(int dx, int dy) override {
            xmin += dx; xmax += dx; ymin += dy; ymax += dy;
        }
        bool containsPoint(int x, int y) override {
            return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
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
        std::vector<std::pair<int, int>> getControlPoints() override {
            return { {x0,y0}, {x1,y1}, {x2,y2} };
        }
        void setControlPoint(int idx, int x, int y) override {
            if (idx == 0) { x0 = x; y0 = y; }
            else if (idx == 1) { x1 = x; y1 = y; }
            else if (idx == 2) { x2 = x; y2 = y; }
        }
        void moveBy(int dx, int dy) override {
            x0 += dx; y0 += dy; x1 += dx; y1 += dy; x2 += dx; y2 += dy;
        }
        bool containsPoint(int x, int y) override {
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
    };

    // Definición completa de BezierCurve
    struct BezierCurve : public Shape {
        std::vector<std::pair<int, int>> controlPoints;

        void draw(CMyTest* renderer) override {
            if (controlPoints.size() < 2) return;

            // 3. Dibujar la Curva (siempre)
            int segments = 100;
            std::pair<int, int> p0 = CMyTest::deCasteljau(controlPoints, 0.0f);

            for (int i = 1; i <= segments; ++i) {
                float t = (float)i / segments;
                std::pair<int, int> p1 = CMyTest::deCasteljau(controlPoints, t);
                renderer->drawLine(p0.first, p0.second, p1.first, p1.second, borderColor, thickness);
                p0 = p1;
            }

            // Dibujar polígono de control y puntos solo si la curva está seleccionada
            if (renderer->m_selectedShape == this) {
                // Polígono de control
                for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
                    const auto& pA = controlPoints[i];
                    const auto& pB = controlPoints[i + 1];
                    renderer->drawLine(pA.first, pA.second, pB.first, pB.second,
                        renderer->m_controlPolygonColor, 1);
                }

                // control points are drawn centrally in update() for all shapes
            }
        }

        std::vector<std::pair<int, int>> getControlPoints() override {
            return controlPoints;
        }
        void setControlPoint(int idx, int x, int y) override {
            if (idx >= 0 && idx < (int)controlPoints.size()) {
                controlPoints[idx] = { x,y };
            }
        }
        void moveBy(int dx, int dy) override {
            for (auto& p : controlPoints) { p.first += dx; p.second += dy; }
        }
        bool containsPoint(int x, int y) override {
            // test distancia a la curva muestreada
            int segments = 80;
            std::pair<int, int> prev = CMyTest::deCasteljau(controlPoints, 0.0f);
            const int TOL = 10;
            for (int i = 1; i <= segments; ++i) {
                float t = (float)i / segments;
                std::pair<int, int> cur = CMyTest::deCasteljau(controlPoints, t);
                // distancia punto a segmento prev-cur
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
    };

    // Para la edición de curvas Bézier
    int m_editMode = 0; // mantenemos pero ya no obligatorio
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
    int m_controlPointRadius = 5; // unified radius for all control points
    std::vector<std::pair<int, int>> m_tempControlPoints;

    // Nueva: selección y arrastre genérico
    Shape* m_selectedShape = nullptr;
    int m_selectedHandleIndex = -1; // -1 => mover figura completa, >=0 => handle index
    bool m_isDraggingHandle = false;
    std::pair<int, int> m_dragStartMouse;
    std::vector<std::pair<int, int>> m_dragStartPoints; // snapshot de control points al iniciar drag

    // Nuevo: indica que el press inicializó sobre un handle / punto de control
    bool m_pressedOnHandle = false;
    // Nuevo: indica si se inició la creación de una figura (press en lienzo)
    bool m_isCreatingShape = false;

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

        //m_drawnPixels.clear();
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

    void drawEllipseOutline(int cx, int cy, int a, int b, RGBA color, int thickness = 1)
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

    void drawTriangleFilled(int x0, int y0, int x1, int y1, int x2, int y2, RGBA fillColor, RGBA borderColor, int thickness)
    {
        // Variable auxiliar para acumular los píxeles del borde
        std::set<std::pair<int, int>> borderPixels;

        // Dibujamos cada línea y acumulamos sus píxeles
        // Línea 1: (x0,y0) -> (x1,y1)
        m_drawnPixels.clear();
        drawLine(x0, y0, x1, y1, borderColor, thickness);
        borderPixels.insert(m_drawnPixels.begin(), m_drawnPixels.end());

        // Línea 2: (x1,y1) -> (x2,y2)
        m_drawnPixels.clear();
        drawLine(x1, y1, x2, y2, borderColor, thickness);
        borderPixels.insert(m_drawnPixels.begin(), m_drawnPixels.end());

        // Línea 3: (x2,y2) -> (x0,y0)
        m_drawnPixels.clear();
        drawLine(x2, y2, x0, y0, borderColor, thickness);
        borderPixels.insert(m_drawnPixels.begin(), m_drawnPixels.end());

        m_drawnPixels.clear();

        // Función para verificar si un punto está dentro del triángulo
        auto isInsideTriangle = [](int px, int py, int x0, int y0, int x1, int y1, int x2, int y2) -> bool {
            auto sign = [](int px, int py, int ax, int ay, int bx, int by) -> float {
                return (px - bx) * (ay - by) - (ax - bx) * (py - by);
                };

            float d1 = sign(px, py, x0, y0, x1, y1);
            float d2 = sign(px, py, x1, y1, x2, y2);
            float d3 = sign(px, py, x2, y2, x0, y0);

            bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

            return !(has_neg && has_pos);
            };

        // Calculamos el bounding box del triángulo
        int xmin = std::min({ x0, x1, x2 });
        int xmax = std::max({ x0, x1, x2 });
        int ymin = std::min({ y0, y1, y2 });
        int ymax = std::max({ y0, y1, y2 });

        // Rellenamos solo los píxeles que están dentro y NO en el borde
        for (int y = ymin; y <= ymax; ++y) {
            if (y < 0 || y >= height) continue;

            for (int x = xmin; x <= xmax; ++x) {
                if (x < 0 || x >= width) continue;

                // Si está dentro del triángulo y NO está en el borde
                if (isInsideTriangle(x, y, x0, y0, x1, y1, x2, y2) &&
                    borderPixels.find({ x, y }) == borderPixels.end()) {
                    setPixel(x, y, fillColor);
                }
            }
        }
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

    static std::pair<int, int> deCasteljau(const std::vector<std::pair<int, int>>& points, float t) {
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
        return CMyTest::deCasteljau(points, t);
    }

    // Nueva función: busca figura y handle cerca del punto (tx,ty)
    Shape* findShapeAt(int tx, int ty, int& outHandleIndex) {
        const int HANDLE_TOL = 10;
        outHandleIndex = -2;
        for (auto it = m_shapes.rbegin(); it != m_shapes.rend(); ++it) {
            Shape* s = it->get();
            // primero comprobar handlers
            auto pts = s->getControlPoints();
            for (size_t i = 0; i < pts.size(); ++i) {
                int dx = pts[i].first - tx;
                int dy = pts[i].second - ty;
                if (dx * dx + dy * dy <= HANDLE_TOL * HANDLE_TOL) {
                    outHandleIndex = (int)i;
                    return s;
                }
            }
            // luego comprobación de contenido
            if (s->containsPoint(tx, ty)) {
                outHandleIndex = -1; // seleccionar figura completa
                return s;
            }
        }
        outHandleIndex = -2;
        return nullptr;
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
        std::fill(m_buffer.begin(), m_buffer.end(), m_bgColor);
        framesThisSecond++;

        for (const auto& shape : m_shapes) {
            shape->draw(this);
        }

        // Dibujar handles para la figura seleccionada (si existe)
        if (m_selectedShape) {
            auto pts = m_selectedShape->getControlPoints();
            // Draw unified circular control points
            for (size_t i = 0; i < pts.size(); ++i) {
                int px = pts[i].first;
                int py = pts[i].second;
                RGBA col = (m_selectedHandleIndex == (int)i) ? m_selectedControlPointColor : m_controlPointColor;
                drawEllipseFilled(px, py, m_controlPointRadius, m_controlPointRadius, col, col, 1);
            }
             // Si se seleccionó la figura completa dibujar un bounding box
             if (m_selectedHandleIndex == -1) {
                 auto pts2 = m_selectedShape->getControlPoints();
                 if (!pts2.empty()) {
                     int xmin = pts2[0].first, xmax = pts2[0].first, ymin = pts2[0].second, ymax = pts2[0].second;
                     for (auto& p : pts2) {
                         xmin = std::min(xmin, p.first);
                         xmax = std::max(xmax, p.first);
                         ymin = std::min(ymin, p.second);
                         ymax = std::max(ymax, p.second);
                     }
                     drawRectangleOutline(xmin - 6, ymin - 6, xmax + 6, ymax + 6, m_selectionHandleColor, 1);
                 }
             }
         }

        if (m_drawMode == 4 && m_tempControlPoints.size() > 0) {
            // Cuando estamos creando (temp control points) dibujamos el polígono/handles aquí
            for (size_t i = 0; i < m_tempControlPoints.size(); ++i) {
                const auto& p = m_tempControlPoints[i];
                if (i < m_tempControlPoints.size() - 1) {
                    const auto& pNext = m_tempControlPoints[i + 1];
                    drawLine(p.first, p.second, pNext.first, pNext.second,
                        m_controlPolygonColor, 1);
                }
                drawEllipseFilled(p.first, p.second, m_controlPointRadius,
                    m_controlPointRadius,
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

                drawEllipseFilled(cx, cy, m_controlPointRadius / 2,
                    m_controlPointRadius / 2,
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

            // Tecla S para deseleccionar
            if (key == GLFW_KEY_S) {
                m_selectedShape = nullptr;
                m_selectedHandleIndex = -1;
                m_isDraggingHandle = false;
            }

            // Tecla Espacio para cancelar la creación de la curva Bezier
            if (key == GLFW_KEY_SPACE ) {
                if (m_drawMode == 4 && !m_tempControlPoints.empty())
                {
                    m_tempControlPoints.clear();
                    std::cout << "Bezier Curve creation canceled.\n";
                }

                if (m_drawMode == 3 && m_triClicks > 0 && m_triClicks < 3)
                {
                    m_triClicks = 0;
					std::cout << "Triangle creation canceled.\n";
                    
                }
                
            }

			// Tecla DEL para borrar la figura seleccionada
            if (key == GLFW_KEY_DELETE || key == GLFW_KEY_BACKSPACE) {
                if (m_selectedShape) {
                    auto it = std::remove_if(m_shapes.begin(), m_shapes.end(),
                        [this](const std::unique_ptr<Shape>& s) {
                            return s.get() == m_selectedShape;
                        });
                    if (it != m_shapes.end()) {
                        m_shapes.erase(it, m_shapes.end());
                        std::cout << "Selected shape deleted.\n";
                    }
                    m_selectedHandleIndex = -1;
                    m_isDraggingHandle = false;
                    m_selectedShape = nullptr;
				}
                
                
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
                if (button == 0 && (m_isDraggingHandle || m_isDraggingControlPoint)) {
                    return;
                }

                bool prevSelected = (m_selectedShape != nullptr);

                // If we already started creating a shape, subsequent clicks (except the initial one)
                // must be treated as part of creation and must NOT select existing shapes.
                bool creatingInProgress = m_isCreatingShape || (m_drawMode == 4 && !m_tempControlPoints.empty()) || (m_drawMode == 3 && m_triClicks > 0);

                // If creatingInProgress, skip hit detection/selection and go directly to creation logic below.
                if (!creatingInProgress) {
                    // Unified hit detection for any shape or bezier control point
                    if (button == 0) {
                        int bzCp;
                        BezierCurve* bzHit = findBezierCurveNear(tx, ty, bzCp);
                        if (bzHit) {
                            m_selectedShape = bzHit;
                            m_selectedHandleIndex = (bzCp >= 0) ? bzCp : -1;
                            m_isDraggingHandle = true;
                            m_dragStartMouse = { tx, ty };
                            m_dragStartPoints = m_selectedShape->getControlPoints();
                            m_pressedOnHandle = true;
                            std::cout << "Selected Bezier. handle=" << m_selectedHandleIndex << "\n";
                            return;
                        }

                        int handleIdx;
                        Shape* sHit = findShapeAt(tx, ty, handleIdx);
                        if (sHit) {
                            m_selectedShape = sHit;
                            m_selectedHandleIndex = handleIdx;
                            m_isDraggingHandle = true;
                            m_dragStartMouse = { tx, ty };
                            m_dragStartPoints = m_selectedShape->getControlPoints();
                            m_pressedOnHandle = true;
                            std::cout << "Selected shape. handle=" << handleIdx << "\n";
                            return;
                        }

                        // Click on empty background
                        if (prevSelected) {
                            // deselect and do not start creation
                            m_selectedShape = nullptr;
                            m_selectedHandleIndex = -1;
                            m_pressedOnHandle = false;
                            m_isCreatingShape = false;
                            return;
                        }
                    }
                }

                if (m_drawMode == 4) { // Bezier mode (creación con m_tempControlPoints)
                    if (button == 0) {
                        m_tempControlPoints.push_back({ tx, ty });
                        std::cout << "Added control point (" << tx << ", " << ty
                            << "). Total: " << m_tempControlPoints.size() << "\n";
                        // not a normal "shape creation" for m_isCreatingShape
                        m_isCreatingShape = false;
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
                        m_isCreatingShape = false;
                    }
                }
                else {
                    // Modo general: creación o selección/arrastre de figuras existentes
                    if (m_drawMode < 3) {
                        if (button == 0)
                        {
                            m_x0 = tx;
                            m_y0 = ty;
                            m_x1 = m_x0;
                            m_y1 = m_y0;
                            m_isCreatingShape = true;
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
            }
            else if (action == GLFW_RELEASE)
            {
                mouseButtonsDown[button] = false;

                // Si el release ocurre tras arrastrar un handle/punto de control,
                // finalizamos el drag y evitamos ejecutar la lógica de "finalizar figura".
                if (button == 0 && (m_isDraggingHandle || m_isDraggingControlPoint)) {
                    m_isDraggingHandle = false;
                    m_isDraggingControlPoint = false;
                    m_pressedOnHandle = false;
                    // no creamos nuevas figuras, retornamos ya que el release corresponde al drag
                    return;
                }

                if (m_drawMode == 4 && m_editMode == 1 && button == 0) {
                    m_isDraggingControlPoint = false;
                }
                else if (m_drawMode != 3 && m_drawMode != 4) {
                    if (button == 0 && m_isCreatingShape)
                    {
                        m_x1 = tx;
                        m_y1 = ty;
                        std::cout << "Figura finalizada en (" << m_x1 << ", " << m_y1 << ")\n";

                        auto createLine = [&]() {
                            auto ln = std::make_unique<Line>();
                            ln->x0 = m_x0; ln->y0 = m_y0; ln->x1 = m_x1; ln->y1 = m_y1;
                            ln->borderColor = m_borderColor;
                            ln->fillColor = m_fillColor;
                            ln->thickness = m_lineThickness;
                            ln->filled = m_useFilledShapes;
                            m_shapes.push_back(std::move(ln));
                            };

                        auto createEllipse = [&]() {
                            auto el = std::make_unique<Ellipse>();
                            el->cx = m_x0; el->cy = m_y0;
                            el->a = std::abs(m_x1 - m_x0); el->b = std::abs(m_y1 - m_y0);
                            el->borderColor = m_borderColor;
                            el->fillColor = m_fillColor;
                            el->thickness = m_lineThickness;
                            el->filled = m_useFilledShapes;
                            m_shapes.push_back(std::move(el));
                            };

                        auto createRectangle = [&]() {
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
                            };

                        // Solo crear figura si el press inicial NO fue sobre un handle/punto de control
                        if (!m_pressedOnHandle) {
                            if (m_drawMode == 0) createLine();
                            else if (m_drawMode == 1) createEllipse();
                            else if (m_drawMode == 2) createRectangle();
                        }

                        m_x0 = -1; m_y0 = -1; m_x1 = -1; m_y1 = -1;
                        m_isCreatingShape = false;
                    }
                }

                // terminar arrastre de handle cuando se suelta el botón izquierdo
                if (button == 0) {
                    m_isDraggingHandle = false;
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

        // Edición de curvas Bézier (dragging puntos) se maneja con la rutina genérica:
        if (m_drawMode == 4 && m_editMode == 1 && m_isDraggingControlPoint && m_editingCurve && m_selectedControlPoint >= 0) {
            m_editingCurve->controlPoints[m_selectedControlPoint] = { xpos, ypos };
        }
        else {
            // Drag de handles / mover figura
            if (m_isDraggingHandle && m_selectedShape) {
                int dx = xpos - m_dragStartMouse.first;
                int dy = ypos - m_dragStartMouse.second;

                if (m_selectedHandleIndex == -1) {
                    // Mover figura completa usando el snapshot de puntos iniciales (evita acumulación)
                    for (size_t i = 0; i < m_dragStartPoints.size(); ++i) {
                        m_selectedShape->setControlPoint((int)i,
                            m_dragStartPoints[i].first + dx,
                            m_dragStartPoints[i].second + dy);
                    }
                }
                else if (m_selectedHandleIndex >= 0) {
                    // mover solo el handle seleccionado, usando snapshot original
                    if (m_selectedHandleIndex < (int)m_dragStartPoints.size()) {
                        int origx = m_dragStartPoints[m_selectedHandleIndex].first;
                        int origy = m_dragStartPoints[m_selectedHandleIndex].second;
                        m_selectedShape->setControlPoint(m_selectedHandleIndex, origx + dx, origy + dy);
                    }
                }
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
            ImGui::TextWrapped("Triangle mode:");
            ImGui::TextWrapped("- Click three times to place the three vertices.");
            ImGui::TextWrapped("- Space click: Cancel triangle");
            ImGui::TextWrapped("- Current clicks: %d", m_triClicks);

        }

        if (m_drawMode == 4) {
            ImGui::Separator();
            ImGui::TextWrapped("Bezier Curve mode:");
            ImGui::TextWrapped("- Left click: Add control point");
            ImGui::TextWrapped("- Right click: Finalize curve");
            ImGui::TextWrapped("- Space click: Cancel curve");
            ImGui::Text("Control points: %d", (int)m_tempControlPoints.size());
            
        }

        // Existing Bezier-mode color editors remain for legacy; add global editors when a shape is selected
        

        ImGui::Separator();
        ImGui::SliderInt("Line Thickness", &m_lineThickness, 1, 31);
        ImGui::Separator();
        ImGui::Text("Shape Colors:");
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

        if (m_selectedShape) {
            ImGui::Separator();
            ImGui::Text("Control Point Colors:");
            float normalControlPointCol[4] = {
                m_controlPointColor.r / 255.0f,
                m_controlPointColor.g / 255.0f,
                m_controlPointColor.b / 255.0f,
                m_controlPointColor.a / 255.0f
            };
            if (ImGui::ColorEdit4("Normal Color", normalControlPointCol)) {
                m_controlPointColor.r = static_cast<unsigned char>(normalControlPointCol[0] * 255.0f);
                m_controlPointColor.g = static_cast<unsigned char>(normalControlPointCol[1] * 255.0f);
                m_controlPointColor.b = static_cast<unsigned char>(normalControlPointCol[2] * 255.0f);
                m_controlPointColor.a = static_cast<unsigned char>(normalControlPointCol[3] * 255.0f);
            }
            
            float selControlPointCol[4] = {
                m_selectedControlPointColor.r / 255.0f,
                m_selectedControlPointColor.g / 255.0f,
                m_selectedControlPointColor.b / 255.0f,
                m_selectedControlPointColor.a / 255.0f
            };
            if (ImGui::ColorEdit4("Selected Color", selControlPointCol)) {
                m_selectedControlPointColor.r = static_cast<unsigned char>(selControlPointCol[0] * 255.0f);
                m_selectedControlPointColor.g = static_cast<unsigned char>(selControlPointCol[1] * 255.0f);
                m_selectedControlPointColor.b = static_cast<unsigned char>(selControlPointCol[2] * 255.0f);
                m_selectedControlPointColor.a = static_cast<unsigned char>(selControlPointCol[3] * 255.0f);
            }
        }

        ImGui::Separator();
        ImGui::Text("Background Color:");
        if (ImGui::ColorEdit4("##bgcolor", m_bgColorArray)) {
            // Actualizar struct RGBA a partir del arreglo float
            m_bgColor.r = static_cast<unsigned char>(m_bgColorArray[0] * 255.0f);
            m_bgColor.g = static_cast<unsigned char>(m_bgColorArray[1] * 255.0f);
            m_bgColor.b = static_cast<unsigned char>(m_bgColorArray[2] * 255.0f);
            m_bgColor.a = static_cast<unsigned char>(m_bgColorArray[3] * 255.0f);

            // Rellenar el buffer de pixeles con el nuevo color de fondo
            if (!m_buffer.empty()) {
                std::fill(m_buffer.begin(), m_buffer.end(), m_bgColor);
            }
        }
            
        
        ImGui::Separator();
        if (ImGui::Button("Clear screen")) {
            m_shapes.clear();
            m_triClicks = 0;
            m_tempControlPoints.clear();
            m_editingCurve = nullptr;
            m_selectedControlPoint = -1;
            m_selectedShape = nullptr;
            m_selectedHandleIndex = -1;
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