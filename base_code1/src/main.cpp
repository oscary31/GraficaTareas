
#include "PixelRender.h"
#include <iostream>
#include <vector>
#include <set>
#include <random>
#include <memory>
#include <imgui.h>
#include <cmath>
#include <fstream>
#include <sstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define M_PI 3.14159265358979323846

class CMyTest : public CPixelRender
{
private:
    int m_x0 = -1;
    int m_y0 = -1;
    int m_x1 = -1;
    int m_y1 = -1;
    RGBA m_borderColor = { 0, 0, 0, 255 };
    RGBA m_fillColor = { 255, 255, 255, 255 };
    int m_drawMode = 0; // 0=Line,1=Ellipse,2=Rectangle,3=Triangle,4=Bezier
    int m_lineThickness = 1;
    bool m_useFilledShapes = false;

    // Colores para los puntos de control y selección
    RGBA m_controlPointColor = { 255, 119, 0, 255 };         // Naranja
    RGBA m_selectedControlPointColor = { 0, 119, 255, 255 }; // Azul
	RGBA m_controlPolygonColor = { 136, 136, 136, 255 };     // Gris para las líneas del polígono de control
	RGBA m_selectionHandleColor = m_controlPointColor;       // color del box de selección

    // Colores de fondo
    float m_bgColorArray[4] = { 201.0f / 255.0f, 201.0f / 255.0f, 201.0f / 255.0f, 1.0f };
    RGBA m_bgColor = { 216, 216, 216, 255 };

    // Bandera: usar primitivas de ImGui (ImDrawList) en vez de las implementaciones propias
    bool m_useImGuiPrimitives = false;

    // Clase base para todas las figuras
    struct Shape
    {
        RGBA borderColor;
        RGBA fillColor;
        int thickness;
        bool filled;

        virtual ~Shape() = default;
        virtual void draw(CMyTest* renderer) = 0;

        // Métodos para edición y selección
        virtual std::vector<std::pair<int, int>> getControlPoints() { return {}; }
        virtual void setControlPoint(int idx, int x, int y) {}
        virtual void moveBy(int dx, int dy) {}
        virtual bool containsPoint(int x, int y) { return false; }
    };

    struct Line : public Shape
    {
        int x0, y0, x1, y1;

        void draw(CMyTest* renderer) override
        {
            renderer->drawLine(x0, y0, x1, y1, borderColor, thickness);
        }

        std::vector<std::pair<int, int>> getControlPoints() override
        {
            return { { x0, y0 }, { x1, y1 } };
        }

        void setControlPoint(int idx, int x, int y) override
        {
            if (idx == 0) { x0 = x; y0 = y; }
            else if (idx == 1) { x1 = x; y1 = y; }
        }

        void moveBy(int dx, int dy) override
        {
            x0 += dx; y0 += dy; x1 += dx; y1 += dy;
        }

        bool containsPoint(int x, int y) override
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
    };

    struct Ellipse : public Shape
    {
        int cx, cy;
        int a, b;

        void draw(CMyTest* renderer) override
        {
            if (filled) {
                renderer->drawEllipseFilled(cx, cy, a, b, fillColor, borderColor, thickness);
            }
            else {
                renderer->drawEllipseOutline(cx, cy, a, b, borderColor, thickness);
            }
        }

        std::vector<std::pair<int, int>> getControlPoints() override
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

        void setControlPoint(int idx, int x, int y) override
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

        void moveBy(int dx, int dy) override
        {
            cx += dx; cy += dy;
        }

        bool containsPoint(int x, int y) override
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
    };

    struct Rectangle : public Shape
    {
        int xmin, ymin, xmax, ymax;

        void draw(CMyTest* renderer) override
        {
            if (filled) {
                renderer->drawRectangleFilled(xmin, ymin, xmax, ymax, fillColor, borderColor, thickness);
            }
            else {
                renderer->drawRectangleOutline(xmin, ymin, xmax, ymax, borderColor, thickness);
            }
        }

        std::vector<std::pair<int, int>> getControlPoints() override
        {
            // Cuatro esquinas + centro
            int cx = (xmin + xmax) / 2;
            int cy = (ymin + ymax) / 2;
            return { { xmin, ymin }, { xmax, ymin }, { xmax, ymax }, { xmin, ymax }, { cx, cy } };
        }

        void setControlPoint(int idx, int x, int y) override
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

        void moveBy(int dx, int dy) override
        {
            xmin += dx; xmax += dx; ymin += dy; ymax += dy;
        }

        bool containsPoint(int x, int y) override
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
    };

    struct Triangle : public Shape
    {
        int x0, y0, x1, y1, x2, y2;

        void draw(CMyTest* renderer) override
        {
            if (filled) {
                renderer->drawTriangleFilled(x0, y0, x1, y1, x2, y2, fillColor, borderColor, thickness);
            }
            else {
                renderer->drawTriangleOutline(x0, y0, x1, y1, x2, y2, borderColor, thickness);
            }
        }

        std::vector<std::pair<int, int>> getControlPoints() override
        {
            // Tres vértices + centro como handle de movimiento
            int cx = (x0 + x1 + x2) / 3;
            int cy = (y0 + y1 + y2) / 3;
            return { { x0, y0 }, { x1, y1 }, { x2, y2 }, { cx, cy } };
        }

        void setControlPoint(int idx, int x, int y) override
        {
            if (idx == 0) { x0 = x; y0 = y; }
            else if (idx == 1) { x1 = x; y1 = y; }
            else if (idx == 2) { x2 = x; y2 = y; }
            else if (idx == 3) {
                int cx = (x0 + x1 + x2) / 3;
                int cy = (y0 + y1 + y2) / 3;
                int dx = x - cx;
                int dy = y - cy;
                moveBy(dx, dy);
            }
        }

        void moveBy(int dx, int dy) override
        {
            x0 += dx; y0 += dy;
            x1 += dx; y1 += dy;
            x2 += dx; y2 += dy;
        }

        bool containsPoint(int x, int y) override
        {
            
            auto sign = [](int px, int py, int ax, int ay, int bx, int by) -> float {
                return (px - bx) * (ay - by) - (ax - bx) * (py - by);
                };

            float d1 = sign(x, y, x0, y0, x1, y1);
            float d2 = sign(x, y, x1, y1, x2, y2);
            float d3 = sign(x, y, x2, y2, x0, y0);
            bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
            bool inside = !(has_neg && has_pos);
            if (filled) return inside;

            // Si no está relleno: detectar si está cerca del borde (distancia a segmentos)
            const int TOL = std::max(6, thickness + 2);
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

            if (dist2(x0, y0, x1, y1, x, y) <= (double)TOL * TOL) return true;
            if (dist2(x1, y1, x2, y2, x, y) <= (double)TOL * TOL) return true;
            if (dist2(x2, y2, x0, y0, x, y) <= (double)TOL * TOL) return true;
            return false;
        }
    };

    // Definición completa de BezierCurve
    struct BezierCurve : public Shape
    {
        std::vector<std::pair<int, int>> controlPoints;

        // Elevar el grado en 1 
        void elevateDegree()
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

        // Subdividir en t [0,1], devuelve par (left,right) con puntos de control
        std::pair<std::vector<std::pair<int, int>>, std::vector<std::pair<int, int>>> subdivideAt(float t) const
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

        void draw(CMyTest* renderer) override
        {
            if (controlPoints.size() < 2) return;

            // Dibujar la curva muestreada
            int segments = 100;

            // evitar que setThickPixelpintar píxeles duplicados dentro de la misma curva.
            renderer->m_drawnPixels.clear();

            std::pair<int, int> p0 = CMyTest::deCasteljau(controlPoints, 0.0f);
            for (int i = 1; i <= segments; ++i) {
                float t = (float)i / segments;
                std::pair<int, int> p1 = CMyTest::deCasteljau(controlPoints, t);
                renderer->drawLineBresenham(p0.first, p0.second, p1.first, p1.second, borderColor, thickness, false);
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
            }
        }

        std::vector<std::pair<int, int>> getControlPoints() override
        {
            return controlPoints;
        }

        void setControlPoint(int idx, int x, int y) override
        {
            if (idx >= 0 && idx < (int)controlPoints.size()) {
                controlPoints[idx] = { x, y };
            }
        }

        void moveBy(int dx, int dy) override
        {
            for (auto& p : controlPoints) { p.first += dx; p.second += dy; }
        }

        bool containsPoint(int x, int y) override
        {
            // Test de distancia a la curva muestreada
            int segments = 80;
            std::pair<int, int> prev = CMyTest::deCasteljau(controlPoints, 0.0f);
            const int TOL = 10;

            for (int i = 1; i <= segments; ++i) {
                float t = (float)i / segments;
                std::pair<int, int> cur = CMyTest::deCasteljau(controlPoints, t);

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
    };

    BezierCurve* m_editingCurve = nullptr;
    int m_selectedControlPoint = -1;
    std::pair<int, int> m_originalMousePos;
    bool m_isDraggingControlPoint = false;

    // Parámetro de subdivisión de Bézier
    float m_bezierT = 0.5f;

    // Lista centralizada de todas las figuras
    std::vector<std::unique_ptr<Shape>> m_shapes;

    // Almacenamiento temporal para clicks del triángulo
    int m_triTempX[3];
    int m_triTempY[3];
    int m_triClicks = 0;

    int framesThisSecond = 0;

    // Conjunto para evitar dibujar el mismo píxel dos veces
    std::set<std::pair<int, int>> m_drawnPixels;

    // Cursores GLFW (creados bajo demanda)
    GLFWcursor* m_cursorArrow = nullptr;
    GLFWcursor* m_cursorHand = nullptr;
    GLFWcursor* m_cursorResize = nullptr;

    // Crear cursores estándar si es necesario (llamar cuando m_window esté disponible)
    void ensureCursorsCreated()
    {
        if (!m_window) return;
        if (!m_cursorArrow) m_cursorArrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        if (!m_cursorHand)  m_cursorHand = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

        // Usamos HRESIZE como fallback para indicar cambio de tamaño 
#if defined(GLFW_HRESIZE_CURSOR)
        if (!m_cursorResize) m_cursorResize = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
#else
        if (!m_cursorResize) m_cursorResize = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
#endif
    }

    struct TriangleFillInfo
    {
        std::vector<std::pair<int, int>> scanlines;
    };

    // Modo Bézier: radio de puntos de control y lista temporal
    int m_controlPointRadius = 5; // radio unificado para puntos de control
    std::vector<std::pair<int, int>> m_tempControlPoints;

    // Selección y arrastre genérico
    Shape* m_selectedShape = nullptr;
    int m_selectedHandleIndex = -1; // -1 => mover figura completa, >=0 => índice de handle
    bool m_isDraggingHandle = false;
    std::pair<int, int> m_dragStartMouse;
    std::vector<std::pair<int, int>> m_dragStartPoints; // snapshot de puntos de control al iniciar el arrastre

    // Indica si el press inicializó sobre un handle/punto de control
    bool m_pressedOnHandle = false;

    // Indica si se inició la creación de una figura (press en el lienzo)
    bool m_isCreatingShape = false;

    // Helpers para manipular capas
    void bringSelectedForward()
    {
        if (!m_selectedShape) return;
        for (size_t i = 0; i < m_shapes.size(); ++i) {
            if (m_shapes[i].get() == m_selectedShape) {
                if (i + 1 < m_shapes.size()) {
                    recordChangeLayer(i, i + 1);
                    std::swap(m_shapes[i], m_shapes[i + 1]);
                }
                return;
            }
        }
    }

    void sendSelectedBackward()
    {
        if (!m_selectedShape) return;
        for (size_t i = 0; i < m_shapes.size(); ++i) {
            if (m_shapes[i].get() == m_selectedShape) {
                if (i > 0) {
                    recordChangeLayer(i, i - 1);
                    std::swap(m_shapes[i], m_shapes[i - 1]);
                }
                return;
            }
        }
    }

    void bringSelectedToFront()
    {
        if (!m_selectedShape) return;
        for (size_t i = 0; i < m_shapes.size(); ++i) {
            if (m_shapes[i].get() == m_selectedShape) {
                if (i != m_shapes.size() - 1) {
                    recordChangeLayer(i, m_shapes.size() - 1);
                    auto temp = std::move(m_shapes[i]);
                    m_shapes.erase(m_shapes.begin() + i);
                    m_shapes.push_back(std::move(temp));
                }
                return;
            }
        }
    }

    void sendSelectedToBack()
    {
        if (!m_selectedShape) return;
        for (size_t i = 0; i < m_shapes.size(); ++i) {
            if (m_shapes[i].get() == m_selectedShape) {
                if (i != 0) {
                    recordChangeLayer(i, 0);
                    auto temp = std::move(m_shapes[i]);
                    m_shapes.erase(m_shapes.begin() + i);
                    m_shapes.insert(m_shapes.begin(), std::move(temp));
                }
                return;
            }
        }
    }


    //--------
    // Estructura para almacenar un estado de acción
    struct UndoAction
    {
        enum Type {
            ADD_SHAPE,
            DELETE_SHAPE,
            MOVE_SHAPE,
            CHANGE_COLOR,
            CHANGE_BACKGROUND,
            CHANGE_LAYER
        };

        Type type;

        // Para ADD_SHAPE y DELETE_SHAPE
        std::unique_ptr<Shape> shape;
        size_t shapeIndex;

        // Para MOVE_SHAPE
        Shape* shapePtr = nullptr;
        std::vector<std::pair<int, int>> oldPoints;
        std::vector<std::pair<int, int>> newPoints;

        // Para CHANGE_COLOR
        RGBA oldBorderColor;
        RGBA newBorderColor;
        RGBA oldFillColor;
        RGBA newFillColor;
        bool affectsBorder = false;
        bool affectsFill = false;

        // Para CHANGE_BACKGROUND
        RGBA oldBgColor;
        RGBA newBgColor;

        // Para CHANGE_LAYER
        size_t fromIndex;
        size_t toIndex;
    };

    std::vector<UndoAction> m_undoStack;
    std::vector<UndoAction> m_redoStack;
    const int MAX_UNDO_STACK = 100;

    // Aplicar una acción
    void applyAction(UndoAction& action, bool isUndo)
    {
        switch (action.type)
        {
        case UndoAction::ADD_SHAPE:
            if (isUndo) {
                // Deshacer: quitar la figura
                action.shape = std::move(m_shapes[action.shapeIndex]);
                m_shapes.erase(m_shapes.begin() + action.shapeIndex);
            }
            else {
                // Rehacer: agregar la figura
                m_shapes.insert(m_shapes.begin() + action.shapeIndex, std::move(action.shape));
            }
            break;

        case UndoAction::DELETE_SHAPE:
            if (isUndo) {
                // Deshacer: restaurar la figura
                m_shapes.insert(m_shapes.begin() + action.shapeIndex, std::move(action.shape));
            }
            else {
                // Rehacer: eliminar la figura
                action.shape = std::move(m_shapes[action.shapeIndex]);
                m_shapes.erase(m_shapes.begin() + action.shapeIndex);
            }
            break;

        case UndoAction::MOVE_SHAPE:
            if (action.shapePtr) {
                auto& points = isUndo ? action.oldPoints : action.newPoints;
                for (size_t i = 0; i < points.size(); ++i) {
                    action.shapePtr->setControlPoint(i, points[i].first, points[i].second);
                }
            }
            break;

        case UndoAction::CHANGE_COLOR:
            if (action.shapePtr) {
                if (isUndo) {
                    if (action.affectsBorder) action.shapePtr->borderColor = action.oldBorderColor;
                    if (action.affectsFill) action.shapePtr->fillColor = action.oldFillColor;
                }
                else {
                    if (action.affectsBorder) action.shapePtr->borderColor = action.newBorderColor;
                    if (action.affectsFill) action.shapePtr->fillColor = action.newFillColor;
                }
            }
            break;

        case UndoAction::CHANGE_BACKGROUND:
        {
            RGBA& color = isUndo ? action.oldBgColor : action.newBgColor;
            m_bgColor = color;
            m_bgColorArray[0] = color.r / 255.0f;
            m_bgColorArray[1] = color.g / 255.0f;
            m_bgColorArray[2] = color.b / 255.0f;
            m_bgColorArray[3] = color.a / 255.0f;
            if (!m_buffer.empty()) {
                std::fill(m_buffer.begin(), m_buffer.end(), color);
            }
        }
        break;

        case UndoAction::CHANGE_LAYER:
        {
            size_t from = isUndo ? action.toIndex : action.fromIndex;
            size_t to = isUndo ? action.fromIndex : action.toIndex;
            if (from < m_shapes.size() && to < m_shapes.size()) {
                auto temp = std::move(m_shapes[from]);
                m_shapes.erase(m_shapes.begin() + from);
                m_shapes.insert(m_shapes.begin() + to, std::move(temp));
            }
        }
        break;
        }
    }

    // Funciones para registrar acciones
    void recordAddShape(size_t index)
    {
        UndoAction action;
        action.type = UndoAction::ADD_SHAPE;
        action.shapeIndex = index;
        m_undoStack.push_back(std::move(action));
        m_redoStack.clear();

        if (m_undoStack.size() > MAX_UNDO_STACK) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }

    void recordDeleteShape(size_t index)
    {
        UndoAction action;
        action.type = UndoAction::DELETE_SHAPE;
        action.shapeIndex = index;
        action.shape = std::move(m_shapes[index]);
        m_shapes.erase(m_shapes.begin() + index);

        m_undoStack.push_back(std::move(action));
        m_redoStack.clear();

        if (m_undoStack.size() > MAX_UNDO_STACK) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }

    void recordMoveShape(Shape* shape, const std::vector<std::pair<int, int>>& oldPts,
        const std::vector<std::pair<int, int>>& newPts)
    {
        UndoAction action;
        action.type = UndoAction::MOVE_SHAPE;
        action.shapePtr = shape;
        action.oldPoints = oldPts;
        action.newPoints = newPts;

        m_undoStack.push_back(std::move(action));
        m_redoStack.clear();

        if (m_undoStack.size() > MAX_UNDO_STACK) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }

    void recordChangeColor(Shape* shape, RGBA oldBorder, RGBA newBorder,
        RGBA oldFill, RGBA newFill, bool border, bool fill)
    {
        UndoAction action;
        action.type = UndoAction::CHANGE_COLOR;
        action.shapePtr = shape;
        action.oldBorderColor = oldBorder;
        action.newBorderColor = newBorder;
        action.oldFillColor = oldFill;
        action.newFillColor = newFill;
        action.affectsBorder = border;
        action.affectsFill = fill;

        m_undoStack.push_back(std::move(action));
        m_redoStack.clear();

        if (m_undoStack.size() > MAX_UNDO_STACK) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }

    void recordChangeBackground(RGBA oldColor, RGBA newColor)
    {
        UndoAction action;
        action.type = UndoAction::CHANGE_BACKGROUND;
        action.oldBgColor = oldColor;
        action.newBgColor = newColor;

        m_undoStack.push_back(std::move(action));
        m_redoStack.clear();

        if (m_undoStack.size() > MAX_UNDO_STACK) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }

    void recordChangeLayer(size_t from, size_t to)
    {
        UndoAction action;
        action.type = UndoAction::CHANGE_LAYER;
        action.fromIndex = from;
        action.toIndex = to;

        m_undoStack.push_back(std::move(action));
        m_redoStack.clear();

        if (m_undoStack.size() > MAX_UNDO_STACK) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }

    void performUndo()
    {
        if (m_undoStack.empty()) return;

        UndoAction action = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        applyAction(action, true);

        m_redoStack.push_back(std::move(action));
    }

    void performRedo()
    {
        if (m_redoStack.empty()) return;

        UndoAction action = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        applyAction(action, false);

        m_undoStack.push_back(std::move(action));
    }

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    std::string getLastActionDescription() const
    {
        if (m_undoStack.empty()) return "No action";

        switch (m_undoStack.back().type) {
        case UndoAction::ADD_SHAPE: return "Add Shape";
        case UndoAction::DELETE_SHAPE: return "Delete Shape";
        case UndoAction::MOVE_SHAPE: return "Move Shape";
        case UndoAction::CHANGE_COLOR: return "Change Color";
        case UndoAction::CHANGE_BACKGROUND: return "Change Background";
        case UndoAction::CHANGE_LAYER: return "Change Layer";
        default: return "Unknown";
        }
    }

    // Función auxiliar: Convertir buffer RGBA a formato compatible con stb
    std::vector<unsigned char> prepareImageBuffer()
    {
        // m_buffer está en formato RGBA con origen en bottom-left
        // stb_image_write espera origen en top-left

        std::vector<unsigned char> imageData(width * height * 4);

        for (int y = 0; y < height; ++y) {
            for (int x = 350; x < width; ++x) {
                // Voltear verticalmente (origen bottom-left -> top-left)
                int srcIndex = (y * width + x);
                int dstIndex = ((height - 1 - y) * width + x) * 4;

                imageData[dstIndex + 0] = m_buffer[srcIndex].r;
                imageData[dstIndex + 1] = m_buffer[srcIndex].g;
                imageData[dstIndex + 2] = m_buffer[srcIndex].b;
                imageData[dstIndex + 3] = m_buffer[srcIndex].a;
            }
        }

        return imageData;
    }

public:
    CMyTest() {};
    ~CMyTest()
    {
        // Destruir cursores creados
        if (m_cursorArrow) {
            glfwDestroyCursor(m_cursorArrow);
            m_cursorArrow = nullptr;
        }
        if (m_cursorHand) {
            glfwDestroyCursor(m_cursorHand);
            m_cursorHand = nullptr;
        }
        if (m_cursorResize) {
            glfwDestroyCursor(m_cursorResize);
            m_cursorResize = nullptr;
        }
    };

    // Guardar como PNG
    void saveToPNG(const std::string& filename)
    {
        std::string fname = filename;
        // Asegurar extensión .png
        if (fname.find_last_of(".") == std::string::npos ||
            fname.substr(fname.find_last_of(".")) != ".png") {
            fname += ".png";
        }

        auto imageData = prepareImageBuffer();

        // stbi_write_png(filename, width, height, channels, data, stride_in_bytes)
        int result = stbi_write_png(fname.c_str(), width, height, 4,
            imageData.data(), width * 4);

        if (result) {
            std::cout << "Image saved successfully to " << fname << std::endl;
        }
        else {
            std::cerr << "Failed to save image to " << fname << std::endl;
        }
    }

    // Guardar como JPG
    void saveToJPG(const std::string& filename, int quality = 90)
    {
        std::string fname = filename;
        // Asegurar extensión .jpg
        if (fname.find_last_of(".") == std::string::npos ||
            (fname.substr(fname.find_last_of(".")) != ".jpg" &&
                fname.substr(fname.find_last_of(".")) != ".jpeg")) {
            fname += ".jpg";
        }

        auto imageData = prepareImageBuffer();

        // Convertir RGBA a RGB (JPG no soporta alpha)
        std::vector<unsigned char> rgbData(width * height * 3);
        for (int i = 0; i < width * height; ++i) {
            rgbData[i * 3 + 0] = imageData[i * 4 + 0]; // R
            rgbData[i * 3 + 1] = imageData[i * 4 + 1]; // G
            rgbData[i * 3 + 2] = imageData[i * 4 + 2]; // B
            // Se descarta el canal alpha
        }

        // stbi_write_jpg(filename, width, height, channels, data, quality)
        // quality: 1-100, donde 100 es mejor calidad
        int result = stbi_write_jpg(fname.c_str(), width, height, 3,
            rgbData.data(), quality);

        if (result) {
            std::cout << "Image saved successfully to " << fname << std::endl;
        }
        else {
            std::cerr << "Failed to save image to " << fname << std::endl;
        }
    }

    // Función combinada que detecta formato por extensión
    void saveToImage(const std::string& filename, int jpgQuality = 90)
    {
        std::string fname = filename;
        std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);

        if (fname.find(".png") != std::string::npos) {
            saveToPNG(filename);
        }
        else if (fname.find(".jpg") != std::string::npos ||
            fname.find(".jpeg") != std::string::npos) {
            saveToJPG(filename, jpgQuality);
        }
        else {
            // Por defecto, guardar como PNG
            std::cout << "No extension specified, saving as PNG" << std::endl;
            saveToPNG(filename);
        }
    }

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

    void drawLineBresenham(int x0, int y0, int x1, int y1, RGBA color, int thickness, bool clear = true)
    {
        if (clear) m_drawnPixels.clear();

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
            drawLineBresenham(x1, y1, x0, y0, color, thickness, clear);
        }
    }

    void drawLine(int x0, int y0, int x1, int y1, RGBA color, int thickness = 1)
    {
        drawLineBresenham(x0, y0, x1, y1, color, thickness, true);
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

        // Dibujar cada arista y acumular sus píxeles
        m_drawnPixels.clear();
        drawLine(x0, y0, x1, y1, borderColor, thickness);
        borderPixels.insert(m_drawnPixels.begin(), m_drawnPixels.end());

        m_drawnPixels.clear();
        drawLine(x1, y1, x2, y2, borderColor, thickness);
        borderPixels.insert(m_drawnPixels.begin(), m_drawnPixels.end());

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

        // Bounding box del triángulo
        int xmin = std::min({ x0, x1, x2 });
        int xmax = std::max({ x0, x1, x2 });
        int ymin = std::min({ y0, y1, y2 });
        int ymax = std::max({ y0, y1, y2 });

        // Rellenar solo los píxeles que están dentro y no son del borde
        for (int y = ymin; y <= ymax; ++y) {
            if (y < 0 || y >= height) continue;

            for (int x = xmin; x <= xmax; ++x) {
                if (x < 0 || x >= width) continue;

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

        // Dibujar borde primero (m_drawnPixels se llena)
        drawEllipseOutline(cx, cy, a, b, borderColor, thickness);

        // Guardar todos los píxeles del borde
        std::set<std::pair<int, int>> borderPixels = m_drawnPixels;
        m_drawnPixels.clear();

        // Rellenar usando un radio amplio
        int searchRadius = a + b;

        for (int y = cy - searchRadius; y <= cy + searchRadius; ++y) {
            if (y < 0 || y >= height) continue;

            for (int x = cx - searchRadius; x <= cx + searchRadius; ++x) {
                if (x < 0 || x >= width) continue;

                if (borderPixels.find({ x, y }) != borderPixels.end()) {
                    continue;
                }

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

    static std::pair<int, int> deCasteljau(const std::vector<std::pair<int, int>>& points, float t)
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

        // Usar redondeo en lugar de truncamiento para evitar saltos pequeños
        // entre puntos muestreados consecutivos que rompan la continuidad visual.
        int rx = static_cast<int>(std::lround(temp[0].first));
        int ry = static_cast<int>(std::lround(temp[0].second));
        return { rx, ry };
    }

    // Pequeño wrapper usado al dibujar una Bézier temporal en creación
    std::pair<int, int> deCasteljauTemp(const std::vector<std::pair<int, int>>& points, float t)
    {
        return CMyTest::deCasteljau(points, t);
    }

    // Buscar figura y handle cerca del punto (tx,ty)
    Shape* findShapeAt(int tx, int ty, int& outHandleIndex)
    {
        const int HANDLE_TOL = 10;
        outHandleIndex = -2;
        for (auto it = m_shapes.rbegin(); it != m_shapes.rend(); ++it) {
            Shape* s = it->get();

            // Primero comprobar los handles (puntos de control)
            auto pts = s->getControlPoints();
            for (size_t i = 0; i < pts.size(); ++i) {
                int dx = pts[i].first - tx;
                int dy = pts[i].second - ty;
                if (dx * dx + dy * dy <= HANDLE_TOL * HANDLE_TOL) {
                    outHandleIndex = (int)i;
                    return s;
                }
            }

            // Luego comprobación de contenido (si el punto está en la figura)
            if (s->containsPoint(tx, ty)) {
                outHandleIndex = -1; // seleccionar figura completa
                return s;
            }
        }

        outHandleIndex = -2;
        return nullptr;
    }

    BezierCurve* findBezierCurveNear(int x, int y, int& controlPointIndex)
    {
        controlPointIndex = -1;
        const int TOLERANCE = 20;

        for (auto it = m_shapes.rbegin(); it != m_shapes.rend(); ++it) {
            BezierCurve* bezier = dynamic_cast<BezierCurve*>(it->get());
            if (bezier) {
                // Comprobar cercanía a puntos de control
                for (size_t i = 0; i < bezier->controlPoints.size(); ++i) {
                    const auto& p = bezier->controlPoints[i];
                    int dx = p.first - x;
                    int dy = p.second - y;
                    if (dx * dx + dy * dy <= TOLERANCE * TOLERANCE) {
                        controlPointIndex = i;
                        return bezier;
                    }
                }

                // Comprobar cercanía a la curva muestreada
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

        // Dibujar con primitivas propias si NO se usan las primitivas de ImGui
        if (!m_useImGuiPrimitives) {
            for (const auto& shape : m_shapes) {
                shape->draw(this);
            }
        }

        // Dibujar handles para la figura seleccionada (si existe)
        if (m_selectedShape) {
            auto pts = m_selectedShape->getControlPoints();

            // Dibujar puntos de control circulares unificados
            for (size_t i = 0; i < pts.size(); ++i) {
                int px = pts[i].first;
                int py = pts[i].second;
                RGBA col = (m_selectedHandleIndex == (int)i) ? m_selectedControlPointColor : m_controlPointColor;
                drawEllipseFilled(px, py, m_controlPointRadius, m_controlPointRadius, col, col, 1);
            }

            // Si se seleccionó la figura completa, dibujar un rectángulo de selección
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
                    m_selectionHandleColor = m_controlPointColor;
                    drawRectangleOutline(xmin - 8, ymin - 8, xmax + 8, ymax + 8, m_selectionHandleColor, 1);
                }
            }
        }

        // Si estamos creando una Bézier temporal, dibujar polígono/preview
        if (m_drawMode == 4 && m_tempControlPoints.size() > 0) {
            for (size_t i = 0; i < m_tempControlPoints.size(); ++i) {
                const auto& p = m_tempControlPoints[i];
                if (i < m_tempControlPoints.size() - 1) {
                    const auto& pNext = m_tempControlPoints[i + 1];
                    drawLine(p.first, p.second, pNext.first, pNext.second, m_controlPolygonColor, 1);
                }
                drawEllipseFilled(p.first, p.second, m_controlPointRadius, m_controlPointRadius, m_controlPointColor, m_controlPointColor, 1);
            }

            if (!m_tempControlPoints.empty()) {
                double xpos_d, ypos_d;
                glfwGetCursorPos(m_window, &xpos_d, &ypos_d);
                int cx = static_cast<int>(xpos_d);
                int cy = height - 1 - static_cast<int>(ypos_d);

                const auto& lastPoint = m_tempControlPoints.back();
                drawLine(lastPoint.first, lastPoint.second, cx, cy, m_controlPolygonColor, 1);

                drawEllipseFilled(cx, cy, m_controlPointRadius / 2, m_controlPointRadius / 2, m_controlPolygonColor, m_controlPolygonColor, 1);
            }

            if (m_tempControlPoints.size() >= 2) {
                int segments = 100;
                this->m_drawnPixels.clear();
                std::pair<int, int> p0 = deCasteljauTemp(m_tempControlPoints, 0.0f);

                for (int i = 1; i <= segments; ++i) {
                    float t = (float)i / segments;
                    std::pair<int, int> p1 = deCasteljauTemp(m_tempControlPoints, t);
                    drawLineBresenham(p0.first, p0.second, p1.first, p1.second, m_borderColor, m_lineThickness, false);
                    p0 = p1;
                }
            }
        }

        // Dibujar figura en creación (linea, elipse, rectángulo, triángulo en progreso)
        if (m_x0 >= 0 && m_y0 >= 0 && m_x1 >= 0 && m_y1 >= 0) {
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
            if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(m_window, GLFW_TRUE);

            // ==== UNDO / REDO ====
                if (key == GLFW_KEY_Z && (mods & GLFW_MOD_CONTROL)) {
                    if (mods & GLFW_MOD_SHIFT) {
                        performRedo();
                        std::cout << "Redo\n";
                    }
                    else {
                        performUndo();
                        std::cout << "Undo\n";
                    }
                }

            if (key == GLFW_KEY_Y && (mods & GLFW_MOD_CONTROL)) {
                performRedo();
                std::cout << "Redo\n";
            }

            // Tecla S para deseleccionar
            if (key == GLFW_KEY_S) {
                m_selectedShape = nullptr;
                m_selectedHandleIndex = -1;
                m_isDraggingHandle = false;
            }

            if (key == GLFW_KEY_SPACE) {
                if (m_drawMode == 4 && !m_tempControlPoints.empty()) {
                    m_tempControlPoints.clear();
                    std::cout << "Bezier Curve creation canceled.\n";
                }

                if (m_drawMode == 3 && m_triClicks > 0 && m_triClicks < 3) {
                    m_triClicks = 0;
                    std::cout << "Triangle creation canceled.\n";
                }
            }

            // // Tecla DEL o BACKSPACE para borrar la figura seleccionadacon registro de undo
            if (key == GLFW_KEY_DELETE || key == GLFW_KEY_BACKSPACE) {
                if (m_selectedShape) {
                    size_t idx = 0;
                    for (; idx < m_shapes.size(); ++idx) {
                        if (m_shapes[idx].get() == m_selectedShape) break;
                    }
                    if (idx < m_shapes.size()) {
                        recordDeleteShape(idx);
                        std::cout << "Selected shape deleted.\n";
                    }
                    m_selectedHandleIndex = -1;
                    m_isDraggingHandle = false;
                    m_selectedShape = nullptr;
                }
            }
        }
        else if (action == GLFW_RELEASE) {
            std::cout << "Key " << key << " released\n";
        }
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

                // Si ya estamos arrastrando un handle/punto de control, ignorar clicks adicionales
                if (button == 0 && (m_isDraggingHandle || m_isDraggingControlPoint)) {
                    return;
                }

                bool prevSelected = (m_selectedShape != nullptr);

                // Si ya empezamos a crear una figura, los clicks siguientes (excepto el inicial)
                // deben tratarse como parte de la creación y NO seleccionar figuras existentes.
                bool creatingInProgress = m_isCreatingShape || (m_drawMode == 4 && !m_tempControlPoints.empty()) || (m_drawMode == 3 && m_triClicks > 0);

                // Si estamos creando, saltar la detección de colisiones y pasar a la lógica de creación
                if (!creatingInProgress) {
                    // Detección unificada: buscar curva Bézier o manejadores cercanos
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

                        // Click en fondo vacío: si había selección previa, deseleccionar
                        if (prevSelected) {
                            m_selectedShape = nullptr;
                            m_selectedHandleIndex = -1;
                            m_pressedOnHandle = false;
                            m_isCreatingShape = false;
                            return;
                        }
                    }
                }

                if (m_drawMode == 4) { // Modo Bézier (creación con puntos temporales)
                    if (button == 0) {
                        m_tempControlPoints.push_back({ tx, ty });
                        std::cout << "Added control point (" << tx << ", " << ty << "). Total: " << m_tempControlPoints.size() << "\n";
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
                            recordAddShape(m_shapes.size() - 1);
                            std::cout << "Bezier Curve finalized with " << m_tempControlPoints.size() << " points.\n";
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
                        if (button == 0) {
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
                                recordAddShape(m_shapes.size() - 1);
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

                // Si el release corresponde a finalizar un arrastre de handle/punto de control,
                // terminar el arrastre y no crear nuevas figuras.
                if (button == 0 && (m_isDraggingHandle || m_isDraggingControlPoint)) {
                    if (m_isDraggingHandle && m_selectedShape) {
                        auto finalPoints = m_selectedShape->getControlPoints();
                        bool changed = false;
                        if (finalPoints.size() == m_dragStartPoints.size()) {
                            for (size_t i = 0; i < finalPoints.size(); ++i) {
                                if (finalPoints[i] != m_dragStartPoints[i]) {
                                    changed = true;
                                    break;
                                }
                            }
                        }
                        else {
                            changed = true;
                        }

                        if (changed) {
                            recordMoveShape(m_selectedShape, m_dragStartPoints, finalPoints);
                        }
                    }

                    m_isDraggingHandle = false;
                    m_isDraggingControlPoint = false;
                    m_pressedOnHandle = false;
                    return;
                }

                if (m_drawMode == 4 && button == 0) {
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
                            recordAddShape(m_shapes.size() - 1);
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
                            recordAddShape(m_shapes.size() - 1);
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
                            recordAddShape(m_shapes.size() - 1);
                            };

                        // Crear figura solo si el press inicial NO fue sobre un handle
                        if (!m_pressedOnHandle) {
                            if (m_drawMode == 0) createLine();
                            else if (m_drawMode == 1) createEllipse();
                            else if (m_drawMode == 2) createRectangle();
                        }

                        m_x0 = -1; m_y0 = -1; m_x1 = -1; m_y1 = -1;
                        m_isCreatingShape = false;
                    }
                }

                // Terminar arrastre de handle cuando se suelta el botón izquierdo
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

        // Asegurar cursores (m_window viene de CPixelRender)
        ensureCursorsCreated();

        int xpos = static_cast<int>(xpos_d);
        int ypos = height - 1 - static_cast<int>(ypos_d);

        // Detectar hover sobre curvas/figuras/puntos de control
        int bezCp = -1;
        BezierCurve* bzHit = findBezierCurveNear(xpos, ypos, bezCp);

        int handleIdx = -2;
        Shape* sHit = findShapeAt(xpos, ypos, handleIdx); // handleIdx >= 0 => sobre handle

        bool overControlPoint = (bzHit && bezCp >= 0) || (sHit && handleIdx >= 0);
        bool overShape = (bzHit != nullptr) || (sHit != nullptr);

        // Ayudante: determinar si un handle es el "central" (último índice)
        auto isCentralHandle = [](Shape* s, int idx) -> bool {
            if (!s || idx < 0) return false;
            auto pts = s->getControlPoints();
            if (pts.empty()) return false;
            return idx == (int)pts.size() - 1;
            };

        // Determinar cursor según estado (arrastre, hover, normal)
        GLFWcursor* targetCursor = m_cursorArrow;
        if (mouseButtonsDown[0] && (m_isDraggingHandle || m_isDraggingControlPoint)) {
            if (m_selectedHandleIndex >= 0) {
                bool central = isCentralHandle(m_selectedShape, m_selectedHandleIndex);
                targetCursor = central ? m_cursorHand : m_cursorResize;
            }
            else {
                targetCursor = m_cursorHand;
            }
        }
        else {
            if (overControlPoint) {
                bool centralHover = false;
                if (bzHit && bezCp >= 0) {
                    centralHover = false;
                }
                else if (sHit && handleIdx >= 0) {
                    centralHover = isCentralHandle(sHit, handleIdx);
                }
                targetCursor = centralHover ? m_cursorHand : m_cursorResize;
            }
            else if (overShape) {
                targetCursor = m_cursorHand;
            }
            else {
                targetCursor = m_cursorArrow;
            }
        }

        // Aplicar cursor si es distinto
        if (mouseButtonsDown[0]) {
            glfwSetCursor(m_window, targetCursor);
        }

        // Comportamiento de arrastre / movimiento
        // Edición de Bézier (arrastrar puntos) manejada por la rutina genérica:
        if (m_drawMode == 4 && m_isDraggingControlPoint && m_editingCurve && m_selectedControlPoint >= 0) {
            m_editingCurve->controlPoints[m_selectedControlPoint] = { xpos, ypos };
        }
        else {
            // Arrastre de handles / mover figura
            if (m_isDraggingHandle && m_selectedShape) {
                int dx = xpos - m_dragStartMouse.first;
                int dy = ypos - m_dragStartMouse.second;
                targetCursor = m_cursorHand;

                if (m_selectedHandleIndex == -1) {
                    // Mover figura completa usando el snapshot inicial (evita acumulación)
                    for (size_t i = 0; i < m_dragStartPoints.size(); ++i) {
                        m_selectedShape->setControlPoint((int)i,
                            m_dragStartPoints[i].first + dx,
                            m_dragStartPoints[i].second + dy);
                    }
                }
                else if (m_selectedHandleIndex >= 0) {
                    // Mover solo el handle seleccionado, usando el snapshot original
                    if (m_selectedHandleIndex < (int)m_dragStartPoints.size()) {
                        int origx = m_dragStartPoints[m_selectedHandleIndex].first;
                        int origy = m_dragStartPoints[m_selectedHandleIndex].second;

                        int newX = origx + dx;
                        int newY = origy + dy;

                        // Si Shift está presionado, forzar proporciones para elipse/rectángulo
                        bool shift = (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) || (glfwGetKey(m_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
                        if (shift) {
                            Rectangle* rc = dynamic_cast<Rectangle*>(m_selectedShape);
                            Ellipse* el = dynamic_cast<Ellipse*>(m_selectedShape);
                            if (rc || el) {
                                int idx = m_selectedHandleIndex;
                                if (idx >= 0 && idx <= 3) {
                                    int opIdx = (idx + 2) % 4;
                                    if (opIdx < (int)m_dragStartPoints.size()) {
                                        int opx = m_dragStartPoints[opIdx].first;
                                        int opy = m_dragStartPoints[opIdx].second;

                                        int dxAbs = std::abs(newX - opx);
                                        int dyAbs = std::abs(newY - opy);
                                        int d = std::max(dxAbs, dyAbs);
                                        int sx = (newX >= opx) ? 1 : -1;
                                        int sy = (newY >= opy) ? 1 : -1;
                                        newX = opx + sx * d;
                                        newY = opy + sy * d;
                                    }
                                }
                            }
                        }

                        m_selectedShape->setControlPoint(m_selectedHandleIndex, newX, newY);
                    }
                }
            }
            else if (m_drawMode != 3) {
                if (mouseButtonsDown[0]) {
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

        // Botones para guardar y cargar
        if (ImGui::Button("Save Shapes")) {
            ImGui::OpenPopup("Save Shapes");
        }

        if (ImGui::BeginPopup("Save Shapes")) {
            static char filename[128] = "drawing";
            static int saveFormat = 0; // 0=JSON, 1=PNG, 2=JPG, 3=BMP
            static int jpgQuality = 90;

            ImGui::Text("Save your drawing in /proy_OA/:");
            ImGui::Separator();

            ImGui::InputText("Filename", filename, IM_ARRAYSIZE(filename));

            const char* formats[] = { "JSON (Project file)", "PNG (Image)", "JPG (Image)"};
            ImGui::Combo("Format", &saveFormat, formats, IM_ARRAYSIZE(formats));

            ImGui::Separator();

            // Descripción según el formato seleccionado
            if (saveFormat == 0) {
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "JSON Format");
                ImGui::TextWrapped("Saves all shapes with their properties. Can be loaded later to continue editing.");
                ImGui::TextDisabled("File extension: .json");
            }
            else if (saveFormat == 1) {
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "PNG Format");
                ImGui::TextWrapped("Lossless image format with transparency support. Best for sharing or printing.");
                ImGui::TextDisabled("File extension: .png");
            }
            else if (saveFormat == 2) {
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "JPG Format");
                ImGui::TextWrapped("Compressed image format. Smaller file size but no transparency.");
                ImGui::TextDisabled("File extension: .jpg");

                ImGui::Spacing();
                ImGui::SliderInt("Quality", &jpgQuality, 1, 100);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Quality setting:");
                    ImGui::Text("  90-100: Excellent (larger file)");
                    ImGui::Text("  75-89:  Good (balanced)");
                    ImGui::Text("  50-74:  Fair (smaller file)");
                    ImGui::Text("  1-49:   Poor (very small)");
                    ImGui::EndTooltip();
                }
            }

            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Save", ImVec2(120, 0))) {
                std::string fname = filename;

                // Agregar extensión según el formato
                std::string extension;
                switch (saveFormat) {
                case 0: extension = ".json"; break;
                case 1: extension = ".png"; break;
                case 2: extension = ".jpg"; break;
                }

                // Si no tiene extensión o tiene una diferente, agregar la correcta
                size_t dotPos = fname.find_last_of(".");
                if (dotPos == std::string::npos) {
                    fname += extension;
                }
                else {
                    std::string currentExt = fname.substr(dotPos);
                    std::transform(currentExt.begin(), currentExt.end(), currentExt.begin(), ::tolower);

                    // Si la extensión no coincide con el formato, reemplazar
                    if (currentExt != extension) {
                        fname = fname.substr(0, dotPos) + extension;
                    }
                }

                // Guardar según el formato
                bool success = false;
                switch (saveFormat) {
                case 0: // JSON
                    saveToJSON(fname);
                    success = true;
                    break;
                case 1: // PNG
                    saveToPNG(fname);
                    success = true;
                    break;
                case 2: // JPG
                    saveToJPG(fname, jpgQuality);
                    success = true;
                    break;
                }

                if (success) {
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Shapes")) {
            ImGui::OpenPopup("Load Shapes");
        }

        if (ImGui::BeginPopup("Load Shapes")) {
            static char filename[128] = "shapes.json";
            ImGui::InputText("Filename", filename, IM_ARRAYSIZE(filename));
            ImGui::TextDisabled("Only JSON format is supported.");

            if (ImGui::Button("Load")) {
                std::string fname = filename;
                std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
                if (fname.find_last_of(".") == std::string::npos) {
                    fname += ".json";
                }
                loadFromJSON(fname);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::Separator();

        const char* modes[] = { "Line", "Ellipse", "Rectangle", "Triangle", "Bezier Curve" };
        ImGui::Text("Draw Mode:");
        ImGui::SameLine();
        ImGui::TextDisabled("(How to delete shapes?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Press DEL or BACKSPACE to delete the selected shape.");
            ImGui::EndTooltip();
        }

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

		ImGui::Separator();
        ImGui::Checkbox("Use ImGui Primitives", &m_useImGuiPrimitives);

        ImGui::Separator();

        ImGui::SliderInt("Line Thickness", &m_lineThickness, 1, 31);

        ImGui::Separator();
        ImGui::Text("Shape Colors:");

        float borderCol[4];
        if (m_selectedShape) {
            borderCol[0] = m_selectedShape->borderColor.r / 255.0f;
            borderCol[1] = m_selectedShape->borderColor.g / 255.0f;
            borderCol[2] = m_selectedShape->borderColor.b / 255.0f;
            borderCol[3] = m_selectedShape->borderColor.a / 255.0f;
        }
        else {
            borderCol[0] = m_borderColor.r / 255.0f;
            borderCol[1] = m_borderColor.g / 255.0f;
            borderCol[2] = m_borderColor.b / 255.0f;
            borderCol[3] = m_borderColor.a / 255.0f;
        }

        if (ImGui::ColorEdit4("Border Color", borderCol)) {
            RGBA newColor;
            newColor.r = static_cast<unsigned char>(borderCol[0] * 255.0f);
            newColor.g = static_cast<unsigned char>(borderCol[1] * 255.0f);
            newColor.b = static_cast<unsigned char>(borderCol[2] * 255.0f);
            newColor.a = static_cast<unsigned char>(borderCol[3] * 255.0f);

            if (m_selectedShape) {
                RGBA oldColor = m_selectedShape->borderColor;
                if (oldColor.r != newColor.r || oldColor.g != newColor.g ||
                    oldColor.b != newColor.b || oldColor.a != newColor.a) {
                    recordChangeColor(m_selectedShape, oldColor, newColor,
                        m_selectedShape->fillColor, m_selectedShape->fillColor, true, false);
                    m_selectedShape->borderColor = newColor;
                }
            }
            else {
                m_borderColor = newColor;
            }
        }

        float fillCol[4];
        if (m_selectedShape && m_selectedShape->filled) {
            fillCol[0] = m_selectedShape->fillColor.r / 255.0f;
            fillCol[1] = m_selectedShape->fillColor.g / 255.0f;
            fillCol[2] = m_selectedShape->fillColor.b / 255.0f;
            fillCol[3] = m_selectedShape->fillColor.a / 255.0f;
        }
        else {
            fillCol[0] = m_fillColor.r / 255.0f;
            fillCol[1] = m_fillColor.g / 255.0f;
            fillCol[2] = m_fillColor.b / 255.0f;
            fillCol[3] = m_fillColor.a / 255.0f;
        }

        if (ImGui::ColorEdit4("Fill Color", fillCol)) {
            RGBA newColor;
            newColor.r = static_cast<unsigned char>(fillCol[0] * 255.0f);
            newColor.g = static_cast<unsigned char>(fillCol[1] * 255.0f);
            newColor.b = static_cast<unsigned char>(fillCol[2] * 255.0f);
            newColor.a = static_cast<unsigned char>(fillCol[3] * 255.0f);

            if (m_selectedShape && m_selectedShape->filled) {
                RGBA oldColor = m_selectedShape->fillColor;
                if (oldColor.r != newColor.r || oldColor.g != newColor.g ||
                    oldColor.b != newColor.b || oldColor.a != newColor.a) {
                    recordChangeColor(m_selectedShape,
                        m_selectedShape->borderColor, m_selectedShape->borderColor,
                        oldColor, newColor, false, true);
                    m_selectedShape->fillColor = newColor;
                }
            }
            else {
                m_fillColor = newColor;
            }
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
        }

        ImGui::Separator();
     
        BezierCurve* bz = dynamic_cast<BezierCurve*>(m_selectedShape);
        if (bz) {
            ImGui::Separator();
            ImGui::Text("Bezier Curve Tools (degree: %d)", (int)bz->controlPoints.size() - 1);
            ImGui::SliderFloat("t (subdivide)", &m_bezierT, 0.0f, 1.0f, "%.3f");
            ImGui::SameLine(); ImGui::Text("Value: %.3f", m_bezierT);
            if (ImGui::Button("Subdivide at t")) {
                size_t idx = 0;
                for (; idx < m_shapes.size(); ++idx) {
                    if (m_shapes[idx].get() == m_selectedShape) break;
                }
                if (idx < m_shapes.size()) {
                    auto pr = bz->subdivideAt(m_bezierT);
                    auto left = std::make_unique<BezierCurve>();
                    left->controlPoints = pr.first;
                    left->borderColor = bz->borderColor;
                    left->fillColor = bz->fillColor;
                    left->thickness = bz->thickness;
                    left->filled = bz->filled;

                    auto right = std::make_unique<BezierCurve>();
                    right->controlPoints = pr.second;
                    right->borderColor = bz->borderColor;
                    right->fillColor = bz->fillColor;
                    right->thickness = bz->thickness;
                    right->filled = bz->filled;

                    m_shapes.erase(m_shapes.begin() + idx);
                    m_shapes.insert(m_shapes.begin() + idx, std::move(right));
                    m_shapes.insert(m_shapes.begin() + idx, std::move(left));
                    m_selectedShape = m_shapes[idx].get();
                    m_selectedHandleIndex = -1;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Elevate Degree")) {
                bz->elevateDegree();
            }
        }

        if (m_selectedShape) {
            ImGui::Separator();
            ImGui::Text("Layer Controls:");
            if (ImGui::Button("Bring Forward")) {
                bringSelectedForward();
            }
            ImGui::SameLine();
            if (ImGui::Button("Send Backward")) {
                sendSelectedBackward();
            }

            if (ImGui::Button("Bring to Front")) {
                bringSelectedToFront();
            }
            ImGui::SameLine();
            if (ImGui::Button("Send to Back")) {
                sendSelectedToBack();
            }
        }

        ImGui::Separator();
        ImGui::Text("Background Color:");
        if (ImGui::ColorEdit4("##bgcolor", m_bgColorArray)) {
            RGBA oldBg = m_bgColor;
            RGBA newBg;
            newBg.r = static_cast<unsigned char>(m_bgColorArray[0] * 255.0f);
            newBg.g = static_cast<unsigned char>(m_bgColorArray[1] * 255.0f);
            newBg.b = static_cast<unsigned char>(m_bgColorArray[2] * 255.0f);
            newBg.a = static_cast<unsigned char>(m_bgColorArray[3] * 255.0f);

            if (oldBg.r != newBg.r || oldBg.g != newBg.g ||
                oldBg.b != newBg.b || oldBg.a != newBg.a) {
                recordChangeBackground(oldBg, newBg);
                m_bgColor = newBg;
                if (!m_buffer.empty()) {
                    std::fill(m_buffer.begin(), m_buffer.end(), newBg);
                }
            }
        }

        // UI para Undo/Redo:
        ImGui::Separator();
        ImGui::Text("History (Undo/Redo):");

        bool canUndoNow = canUndo();
        bool canRedoNow = canRedo();

        if (!canUndoNow) ImGui::BeginDisabled();
        if (ImGui::Button("Undo (Ctrl+Z)")) {
            performUndo();
        }
        if (!canUndoNow) ImGui::EndDisabled();

        ImGui::SameLine();

        if (!canRedoNow) ImGui::BeginDisabled();
        if (ImGui::Button("Redo (Ctrl+Y)")) {
            performRedo();
        }
        if (!canRedoNow) ImGui::EndDisabled();

        ImGui::Text("Last action: %s", getLastActionDescription().c_str());

        // En Clear screen:
        if (ImGui::Button("Clear screen")) {
            m_shapes.clear();
            m_triClicks = 0;
            m_tempControlPoints.clear();
            m_editingCurve = nullptr;
            m_selectedControlPoint = -1;
            m_selectedShape = nullptr;
            m_selectedHandleIndex = -1;
            m_undoStack.clear();
            m_redoStack.clear();
        }

        ImGui::End();
        ImGui::PopStyleVar();

        // --------------------- Dibujado overlay con ImGui::ImDrawList ---------------------
        if (m_useImGuiPrimitives) {
            // Posición del canvas en pantalla (panel izquierdo fijo de 350 px)
            ImDrawList* draw_list = ImGui::GetForegroundDrawList();
            const float canvasOffsetX = 0.0f;
            const float canvasOffsetY = 0.0f;

            // Convertir coordenadas del motor (origen bottom-left) a coordenadas de pantalla (top-left)
            auto toScreen = [&](int x, int y) -> ImVec2 {
                float sx = canvasOffsetX + (float)x;
                float sy = (float)(height - 1 - y) + canvasOffsetY;
                return ImVec2(sx, sy);
                };

            ImVec2 topLeft = ImVec2(canvasOffsetX, canvasOffsetY);
            ImVec2 bottomRight = ImVec2(canvasOffsetX + (float)width - 350.0f, canvasOffsetY + (float)height);

            // Iterar figuras para dibujarlas con ImDrawList
            for (const auto& sPtr : m_shapes) {
                Shape* s = sPtr.get();

                if (auto ln = dynamic_cast<Line*>(s)) {
                    ImVec2 a = toScreen(ln->x0, ln->y0);
                    ImVec2 b = toScreen(ln->x1, ln->y1);
                    draw_list->AddLine(a, b, IM_COL32(ln->borderColor.r, ln->borderColor.g, ln->borderColor.b, ln->borderColor.a), (float)ln->thickness);
                }
                else if (auto rc = dynamic_cast<Rectangle*>(s)) {
                    ImVec2 p0 = toScreen(rc->xmin, rc->ymin);
                    ImVec2 p1 = toScreen(rc->xmax, rc->ymax);
                    if (rc->filled) {
                        ImVec2 tl = ImVec2(p0.x, p1.y);
                        ImVec2 br = ImVec2(p1.x, p0.y);
                        draw_list->AddRectFilled(tl, br, IM_COL32(rc->fillColor.r, rc->fillColor.g, rc->fillColor.b, rc->fillColor.a));
                        draw_list->AddRect(tl, br, IM_COL32(rc->borderColor.r, rc->borderColor.g, rc->borderColor.b, rc->borderColor.a), 0.0f, 0, (float)rc->thickness);
                    }
                    else {
                        ImVec2 tl = ImVec2(p0.x, p1.y);
                        ImVec2 br = ImVec2(p1.x, p0.y);
                        draw_list->AddRect(tl, br, IM_COL32(rc->borderColor.r, rc->borderColor.g, rc->borderColor.b, rc->borderColor.a), 0.0f, 0, (float)rc->thickness);
                    }
                }
                else if (auto el = dynamic_cast<Ellipse*>(s)) {
                    int segments = 64;
                    std::vector<ImVec2> pts;
                    pts.reserve(segments + 1);
                    for (int i = 0; i <= segments; ++i) {
                        float t = (float)i / (float)segments;
                        float ang = t * (float)(2.0 * M_PI);
                        float px = (float)el->cx + std::cos(ang) * (float)el->a;
                        float py = (float)el->cy + std::sin(ang) * (float)el->b;
                        pts.push_back(toScreen((int)std::lround(px), (int)std::lround(py)));
                    }
                    if (el->filled) {
                        draw_list->AddConvexPolyFilled(pts.data(), (int)pts.size(), IM_COL32(el->fillColor.r, el->fillColor.g, el->fillColor.b, el->fillColor.a));
                        draw_list->AddPolyline(pts.data(), (int)pts.size(), IM_COL32(el->borderColor.r, el->borderColor.g, el->borderColor.b, el->borderColor.a), true, (float)el->thickness);
                    }
                    else {
                        draw_list->AddPolyline(pts.data(), (int)pts.size(), IM_COL32(el->borderColor.r, el->borderColor.g, el->borderColor.b, el->borderColor.a), true, (float)el->thickness);
                    }
                }
                else if (auto tr = dynamic_cast<Triangle*>(s)) {
                    ImVec2 p0 = toScreen(tr->x0, tr->y0);
                    ImVec2 p1 = toScreen(tr->x1, tr->y1);
                    ImVec2 p2 = toScreen(tr->x2, tr->y2);
                    ImVec2 arr[3] = { p0, p1, p2 };
                    if (tr->filled) {
                        draw_list->AddConvexPolyFilled(arr, 3, IM_COL32(tr->fillColor.r, tr->fillColor.g, tr->fillColor.b, tr->fillColor.a));
                        draw_list->AddPolyline(arr, 3, IM_COL32(tr->borderColor.r, tr->borderColor.g, tr->borderColor.b, tr->borderColor.a), true, (float)tr->thickness);
                    }
                    else {
                        draw_list->AddPolyline(arr, 3, IM_COL32(tr->borderColor.r, tr->borderColor.g, tr->borderColor.b, tr->borderColor.a), true, (float)tr->thickness);
                    }
                }
                else if (auto bz = dynamic_cast<BezierCurve*>(s)) {
                    int segments = 100;
                    ImVec2 prev = toScreen(deCasteljau(bz->controlPoints, 0.0f).first, deCasteljau(bz->controlPoints, 0.0f).second);
                    for (int i = 1; i <= segments; ++i) {
                        float t = (float)i / (float)segments;
                        auto p = deCasteljau(bz->controlPoints, t);
                        ImVec2 cur = toScreen(p.first, p.second);
                        draw_list->AddLine(prev, cur, IM_COL32(bz->borderColor.r, bz->borderColor.g, bz->borderColor.b, bz->borderColor.a), (float)bz->thickness);
                        prev = cur;
                    }

                    // Si la curva está seleccionada, dibujar polígono de control
                    if (m_selectedShape == bz) {
                        for (size_t i = 0; i + 1 < bz->controlPoints.size(); ++i) {
                            ImVec2 A = toScreen(bz->controlPoints[i].first, bz->controlPoints[i].second);
                            ImVec2 B = toScreen(bz->controlPoints[i + 1].first, bz->controlPoints[i + 1].second);
                            draw_list->AddLine(A, B, IM_COL32(m_controlPolygonColor.r, m_controlPolygonColor.g, m_controlPolygonColor.b, m_controlPolygonColor.a), 1.0f);
                        }
                    }
                }
            }

            // Dibujar handles / puntos de control con ImDrawList si hay selección
            if (m_selectedShape) {
                auto pts = m_selectedShape->getControlPoints();
                for (size_t i = 0; i < pts.size(); ++i) {
                    ImVec2 p = toScreen(pts[i].first, pts[i].second);
                    RGBA col = (m_selectedHandleIndex == (int)i) ? m_selectedControlPointColor : m_controlPointColor;
                    draw_list->AddCircleFilled(p, (float)m_controlPointRadius, IM_COL32(col.r, col.g, col.b, col.a));
                }

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
                        ImVec2 tl = toScreen(xmin - 8, ymax + 8);
                        ImVec2 br = toScreen(xmax + 8, ymin - 8);
                        draw_list->AddRect(tl, br, IM_COL32(m_selectionHandleColor.r, m_selectionHandleColor.g, m_selectionHandleColor.b, m_selectionHandleColor.a), 0.0f, 0, 1.0f);
                    }
                }
            }

            // Dibujar Bézier temporal si procede
            if (m_drawMode == 4 && m_tempControlPoints.size() > 0) {
                for (size_t i = 0; i < m_tempControlPoints.size(); ++i) {
                    ImVec2 p = toScreen(m_tempControlPoints[i].first, m_tempControlPoints[i].second);
                    draw_list->AddCircleFilled(p, (float)m_controlPointRadius, IM_COL32(m_controlPointColor.r, m_controlPointColor.g, m_controlPointColor.b, m_controlPointColor.a));
                    if (i + 1 < m_tempControlPoints.size()) {
                        ImVec2 q = toScreen(m_tempControlPoints[i + 1].first, m_tempControlPoints[i + 1].second);
                        draw_list->AddLine(p, q, IM_COL32(m_controlPolygonColor.r, m_controlPolygonColor.g, m_controlPolygonColor.b, m_controlPolygonColor.a), 1.0f);
                    }
                }

                double xpos_d, ypos_d;
                glfwGetCursorPos(m_window, &xpos_d, &ypos_d);
                int cx = static_cast<int>(xpos_d);
                int cy = height - 1 - static_cast<int>(ypos_d);
                ImVec2 last = toScreen(m_tempControlPoints.back().first, m_tempControlPoints.back().second);
                ImVec2 cur = toScreen(cx, cy);
                draw_list->AddLine(last, cur, IM_COL32(m_controlPolygonColor.r, m_controlPolygonColor.g, m_controlPolygonColor.b, m_controlPolygonColor.a), 1.0f);

                if (m_tempControlPoints.size() >= 2) {
                    int segments = 100;
                    auto p0 = deCasteljauTemp(m_tempControlPoints, 0.0f);
                    ImVec2 prev = toScreen(p0.first, p0.second);
                    for (int i = 1; i <= segments; ++i) {
                        float t = (float)i / (float)segments;
                        auto p = deCasteljauTemp(m_tempControlPoints, t);
                        ImVec2 curp = toScreen(p.first, p.second);
                        draw_list->AddLine(prev, curp, IM_COL32(m_borderColor.r, m_borderColor.g, m_borderColor.b, m_borderColor.a), (float)m_lineThickness);
                        prev = curp;
                    }
                }
            }
        }

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

    // Guardar en archivo JSON
    void saveToJSON(const std::string& filename)
    {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for saving: " << filename << std::endl;
            return;
        }

        file << "{\n";

        // Color de fondo
        file << "  \"backgroundColor\": ["
            << (int)m_bgColor.r << ", "
            << (int)m_bgColor.g << ", "
            << (int)m_bgColor.b << ", "
            << (int)m_bgColor.a << "],\n";

        file << "  \"shapes\": [\n";

        for (size_t i = 0; i < m_shapes.size(); ++i) {
            const auto& shape = m_shapes[i];
            file << "    {\n";
            file << "      \"type\": ";

            if (dynamic_cast<Line*>(shape.get())) {
                file << "\"Line\"";
            }
            else if (dynamic_cast<Ellipse*>(shape.get())) {
                file << "\"Ellipse\"";
            }
            else if (dynamic_cast<Rectangle*>(shape.get())) {
                file << "\"Rectangle\"";
            }
            else if (dynamic_cast<Triangle*>(shape.get())) {
                file << "\"Triangle\"";
            }
            else if (dynamic_cast<BezierCurve*>(shape.get())) {
                file << "\"BezierCurve\"";
            }

            file << ",\n";
            file << "      \"borderColor\": [" << (int)shape->borderColor.r << ", " << (int)shape->borderColor.g << ", " << (int)shape->borderColor.b << ", " << (int)shape->borderColor.a << "],\n";
            file << "      \"fillColor\": [" << (int)shape->fillColor.r << ", " << (int)shape->fillColor.g << ", " << (int)shape->fillColor.b << ", " << (int)shape->fillColor.a << "],\n";
            file << "      \"thickness\": " << shape->thickness << ",\n";
            file << "      \"filled\": " << (shape->filled ? "true" : "false") << ",\n";

            // Puntos de control
            file << "      \"controlPoints\": [\n";
            auto pts = shape->getControlPoints();
            for (size_t j = 0; j < pts.size(); ++j) {
                file << "        [" << pts[j].first << ", " << pts[j].second << "]";
                if (j < pts.size() - 1) file << ",";
                file << "\n";
            }
            file << "      ]\n";

            file << "    }";
            if (i < m_shapes.size() - 1) file << ",";
            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        file.close();
        std::cout << "Shapes saved to " << filename << std::endl;
    }

    void loadFromJSON(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "ERROR: No se pudo abrir: " << filename << std::endl;
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        std::cout << "\n=======================================" << std::endl;
        std::cout << "CARGANDO: " << filename << std::endl;
        std::cout << "=======================================" << std::endl;

        // Limpiar estado
        m_shapes.clear();
        m_selectedShape = nullptr;
        m_selectedHandleIndex = -1;
        m_triClicks = 0;
        m_tempControlPoints.clear();
        m_editingCurve = nullptr;
        m_selectedControlPoint = -1;
        m_isDraggingHandle = false;
        m_isDraggingControlPoint = false;
        m_x0 = -1; m_y0 = -1; m_x1 = -1; m_y1 = -1;

        // Cargar color de fondo si existe
        size_t bgColorPos = content.find("\"backgroundColor\"");
        if (bgColorPos != std::string::npos) {
            std::vector<int> bgVec = parseNestedIntArray(content, bgColorPos);
            if (bgVec.size() >= 4) {
                m_bgColor.r = (unsigned char)bgVec[0];
                m_bgColor.g = (unsigned char)bgVec[1];
                m_bgColor.b = (unsigned char)bgVec[2];
                m_bgColor.a = (unsigned char)bgVec[3];

                // Actualizar también el array float para ImGui
                m_bgColorArray[0] = bgVec[0] / 255.0f;
                m_bgColorArray[1] = bgVec[1] / 255.0f;
                m_bgColorArray[2] = bgVec[2] / 255.0f;
                m_bgColorArray[3] = bgVec[3] / 255.0f;

                std::cout << "Color de fondo cargado: (" << bgVec[0] << "," << bgVec[1] << "," << bgVec[2] << "," << bgVec[3] << ")" << std::endl;
            }
        }

        // Buscar array de shapes
        size_t shapesPos = content.find("\"shapes\"");
        if (shapesPos == std::string::npos) {
            std::cerr << "ERROR: No se encontro 'shapes'" << std::endl;
            return;
        }

        size_t arrayStart = content.find('[', shapesPos);
        if (arrayStart == std::string::npos) {
            std::cerr << "ERROR: No se encontro array" << std::endl;
            return;
        }

        size_t pos = arrayStart + 1;
        int shapesLoaded = 0;

        // Procesar cada objeto
        while (pos < content.size()) {
            size_t objStart = content.find('{', pos);
            if (objStart == std::string::npos) break;

            // Encontrar cierre del objeto (balanceo de llaves)
            int braceDepth = 0;
            size_t i = objStart;
            size_t objEnd = std::string::npos;

            for (; i < content.size(); ++i) {
                if (content[i] == '{') braceDepth++;
                else if (content[i] == '}') {
                    braceDepth--;
                    if (braceDepth == 0) {
                        objEnd = i;
                        break;
                    }
                }
            }

            if (objEnd == std::string::npos) break;

            std::string shapeObj = content.substr(objStart, objEnd - objStart + 1);

            // Parsear tipo
            std::string type = parseQuotedString(shapeObj, "\"type\"", 0);
            if (type.empty()) {
                pos = objEnd + 1;
                continue;
            }

            std::cout << "\nFigura #" << (shapesLoaded + 1) << ": " << type << std::endl;

            // Parsear colores (arrays anidados)
            std::vector<int> borderVec = parseNestedIntArray(shapeObj, shapeObj.find("\"borderColor\""));
            std::vector<int> fillVec = parseNestedIntArray(shapeObj, shapeObj.find("\"fillColor\""));

            // Parsear thickness
            int thickness = 1;
            size_t thPos = shapeObj.find("\"thickness\"");
            if (thPos != std::string::npos) {
                size_t colonPos = shapeObj.find(':', thPos);
                if (colonPos != std::string::npos) {
                    size_t start = colonPos + 1;
                    while (start < shapeObj.size() && isspace((unsigned char)shapeObj[start])) ++start;
                    size_t end = start;
                    if (end < shapeObj.size() && (shapeObj[end] == '+' || shapeObj[end] == '-')) ++end;
                    while (end < shapeObj.size() && isdigit((unsigned char)shapeObj[end])) ++end;
                    if (end > start) {
                        thickness = std::stoi(shapeObj.substr(start, end - start));
                    }
                }
            }

            // Parsear filled
            bool filled = false;
            size_t fPos = shapeObj.find("\"filled\"");
            if (fPos != std::string::npos) {
                size_t colon = shapeObj.find(':', fPos);
                if (colon != std::string::npos) {
                    size_t start = colon + 1;
                    while (start < shapeObj.size() && isspace((unsigned char)shapeObj[start])) ++start;
                    if (shapeObj.compare(start, 4, "true") == 0) filled = true;
                }
            }

            // Parsear control points (array anidado plano)
            std::vector<int> flatPts = parseNestedIntArray(shapeObj, shapeObj.find("\"controlPoints\""));
            std::vector<std::pair<int, int>> controlPoints;
            for (size_t k = 0; k + 1 < flatPts.size(); k += 2) {
                controlPoints.push_back({ flatPts[k], flatPts[k + 1] });
            }

            // Crear figura según tipo y puntos
            std::unique_ptr<Shape> shape;

            if (type == "Line" && controlPoints.size() >= 2) {
                auto line = std::make_unique<Line>();
                line->x0 = controlPoints[0].first;
                line->y0 = controlPoints[0].second;
                line->x1 = controlPoints[1].first;
                line->y1 = controlPoints[1].second;

                if (borderVec.size() >= 4) {
                    line->borderColor = { (unsigned char)borderVec[0], (unsigned char)borderVec[1], (unsigned char)borderVec[2], (unsigned char)borderVec[3] };
                }
                if (fillVec.size() >= 4) {
                    line->fillColor = { (unsigned char)fillVec[0], (unsigned char)fillVec[1], (unsigned char)fillVec[2], (unsigned char)fillVec[3] };
                }
                line->thickness = thickness;
                line->filled = filled;
                shape = std::move(line);
            }
            else if (type == "Ellipse" && controlPoints.size() >= 1) {
                auto ellipse = std::make_unique<Ellipse>();

                if (controlPoints.size() >= 4) {
                    int xmin = controlPoints[0].first, xmax = controlPoints[0].first;
                    int ymin = controlPoints[0].second, ymax = controlPoints[0].second;

                    for (const auto& p : controlPoints) {
                        xmin = std::min(xmin, p.first);
                        xmax = std::max(xmax, p.first);
                        ymin = std::min(ymin, p.second);
                        ymax = std::max(ymax, p.second);
                    }

                    ellipse->cx = (xmin + xmax) / 2;
                    ellipse->cy = (ymin + ymax) / 2;
                    ellipse->a = std::max(1, (xmax - xmin) / 2);
                    ellipse->b = std::max(1, (ymax - ymin) / 2);
                }
                else {
                    // Elipse degenerada (punto único)
                    ellipse->cx = controlPoints[0].first;
                    ellipse->cy = controlPoints[0].second;
                    ellipse->a = 1;
                    ellipse->b = 1;
                }

                if (borderVec.size() >= 4) {
                    ellipse->borderColor = { (unsigned char)borderVec[0], (unsigned char)borderVec[1], (unsigned char)borderVec[2], (unsigned char)borderVec[3] };
                }
                if (fillVec.size() >= 4) {
                    ellipse->fillColor = { (unsigned char)fillVec[0], (unsigned char)fillVec[1], (unsigned char)fillVec[2], (unsigned char)fillVec[3] };
                }
                ellipse->thickness = thickness;
                ellipse->filled = filled;
                shape = std::move(ellipse);
            }
            else if (type == "Rectangle" && controlPoints.size() >= 4) {
                auto rect = std::make_unique<Rectangle>();
                int xmin = controlPoints[0].first, xmax = controlPoints[0].first;
                int ymin = controlPoints[0].second, ymax = controlPoints[0].second;

                for (const auto& p : controlPoints) {
                    xmin = std::min(xmin, p.first);
                    xmax = std::max(xmax, p.first);
                    ymin = std::min(ymin, p.second);
                    ymax = std::max(ymax, p.second);
                }

                rect->xmin = xmin; rect->xmax = xmax;
                rect->ymin = ymin; rect->ymax = ymax;

                if (borderVec.size() >= 4) {
                    rect->borderColor = { (unsigned char)borderVec[0], (unsigned char)borderVec[1], (unsigned char)borderVec[2], (unsigned char)borderVec[3] };
                }
                if (fillVec.size() >= 4) {
                    rect->fillColor = { (unsigned char)fillVec[0], (unsigned char)fillVec[1], (unsigned char)fillVec[2], (unsigned char)fillVec[3] };
                }
                rect->thickness = thickness;
                rect->filled = filled;
                shape = std::move(rect);
            }
            else if (type == "Triangle" && controlPoints.size() >= 3) {
                auto triangle = std::make_unique<Triangle>();
                triangle->x0 = controlPoints[0].first; triangle->y0 = controlPoints[0].second;
                triangle->x1 = controlPoints[1].first; triangle->y1 = controlPoints[1].second;
                triangle->x2 = controlPoints[2].first; triangle->y2 = controlPoints[2].second;

                if (borderVec.size() >= 4) {
                    triangle->borderColor = { (unsigned char)borderVec[0], (unsigned char)borderVec[1], (unsigned char)borderVec[2], (unsigned char)borderVec[3] };
                }
                if (fillVec.size() >= 4) {
                    triangle->fillColor = { (unsigned char)fillVec[0], (unsigned char)fillVec[1], (unsigned char)fillVec[2], (unsigned char)fillVec[3] };
                }
                triangle->thickness = thickness;
                triangle->filled = filled;
                shape = std::move(triangle);
            }
            else if (type == "BezierCurve" && controlPoints.size() >= 2) {
                auto bezier = std::make_unique<BezierCurve>();
                bezier->controlPoints = controlPoints;

                if (borderVec.size() >= 4) {
                    bezier->borderColor = { (unsigned char)borderVec[0], (unsigned char)borderVec[1], (unsigned char)borderVec[2], (unsigned char)borderVec[3] };
                }
                if (fillVec.size() >= 4) {
                    bezier->fillColor = { (unsigned char)fillVec[0], (unsigned char)fillVec[1], (unsigned char)fillVec[2], (unsigned char)fillVec[3] };
                }
                bezier->thickness = thickness;
                bezier->filled = filled;
                shape = std::move(bezier);
            }

            if (shape) {
                m_shapes.push_back(std::move(shape));
                shapesLoaded++;
            }
            else {
                std::cout << " No existe la figura o tiene insuficientes puntos" << std::endl;
            }

            pos = objEnd + 1;
        }

        std::cout << "\n=======================================" << std::endl;
        std::cout << shapesLoaded << " figuras cargadas con exito" << std::endl;
        std::cout << "=======================================\n" << std::endl;
    }

    // Funciones auxiliares para parsear el JSON
    // Parsear un valor entero
    static int parseIntValue(const std::string& s, const std::string& key, int defaultValue)
    {
        size_t keyPos = s.find(key);
        if (keyPos == std::string::npos) return defaultValue;

        size_t colonPos = s.find(':', keyPos);
        if (colonPos == std::string::npos) return defaultValue;

        size_t numStart = colonPos + 1;
        while (numStart < s.size() && isspace((unsigned char)s[numStart])) ++numStart;

        if (numStart >= s.size()) return defaultValue;

        // Manejar signo
        bool negative = false;
        if (s[numStart] == '-') {
            negative = true;
            ++numStart;
        }
        else if (s[numStart] == '+') {
            ++numStart;
        }

        // Extraer dígitos
        size_t numEnd = numStart;
        while (numEnd < s.size() && isdigit((unsigned char)s[numEnd])) ++numEnd;
        if (numEnd > numStart) {
            int value = std::stoi(s.substr(numStart, numEnd - numStart));
            return negative ? -value : value;
        }

        return defaultValue;
    }

    // Parsear un valor booleano
    static bool parseBoolValue(const std::string& s, const std::string& key, bool defaultValue)
    {
        size_t keyPos = s.find(key);
        if (keyPos == std::string::npos) return defaultValue;

        size_t colonPos = s.find(':', keyPos);
        if (colonPos == std::string::npos) return defaultValue;

        size_t valueStart = colonPos + 1;
        while (valueStart < s.size() && isspace((unsigned char)s[valueStart])) ++valueStart;

        if (s.compare(valueStart, 4, "true") == 0) return true;
        if (s.compare(valueStart, 5, "false") == 0) return false;

        return defaultValue;
    }

    // Crear figura según el tipo y puntos
    std::unique_ptr<Shape> createShapeFromType(const std::string& type, const std::vector<std::pair<int, int>>& points)
    {
        if (type == "Line") {
            auto line = std::make_unique<Line>();
            if (points.size() >= 2) {
                line->x0 = points[0].first;
                line->y0 = points[0].second;
                line->x1 = points[1].first;
                line->y1 = points[1].second;
            }
            return line;
        }
        else if (type == "Ellipse") {
            auto ellipse = std::make_unique<Ellipse>();
            if (points.size() >= 4) {
                int xmin = points[0].first, xmax = points[0].first;
                int ymin = points[0].second, ymax = points[0].second;
                for (const auto& p : points) {
                    xmin = std::min(xmin, p.first);
                    xmax = std::max(xmax, p.first);
                    ymin = std::min(ymin, p.second);
                    ymax = std::max(ymax, p.second);
                }
                ellipse->cx = (xmin + xmax) / 2;
                ellipse->cy = (ymin + ymax) / 2;
                ellipse->a = std::max(1, (xmax - xmin) / 2);
                ellipse->b = std::max(1, (ymax - ymin) / 2);
            }
            return ellipse;
        }
        else if (type == "Rectangle") {
            auto rect = std::make_unique<Rectangle>();
            if (points.size() >= 4) {
                int xmin = points[0].first, xmax = points[0].first;
                int ymin = points[0].second, ymax = points[0].second;
                for (const auto& p : points) {
                    xmin = std::min(xmin, p.first);
                    xmax = std::max(xmax, p.first);
                    ymin = std::min(ymin, p.second);
                    ymax = std::max(ymax, p.second);
                }
                rect->xmin = xmin;
                rect->xmax = xmax;
                rect->ymin = ymin;
                rect->ymax = ymax;
            }
            return rect;
        }
        else if (type == "Triangle") {
            auto triangle = std::make_unique<Triangle>();
            if (points.size() >= 3) {
                triangle->x0 = points[0].first;
                triangle->y0 = points[0].second;
                triangle->x1 = points[1].first;
                triangle->y1 = points[1].second;
                triangle->x2 = points[2].first;
                triangle->y2 = points[2].second;
            }
            return triangle;
        }
        else if (type == "BezierCurve") {
            auto bezier = std::make_unique<BezierCurve>();
            bezier->controlPoints = points;
            return bezier;
        }

        return nullptr;
    }

    // Parsea arrays anidados como [[x1,y1], [x2,y2]]
    static std::vector<int> parseNestedIntArray(const std::string& s, size_t startPos)
    {
        std::vector<int> result;

        if (startPos == std::string::npos) {
            return result;
        }

        // Buscar primer '['
        size_t arrayStart = s.find('[', startPos);
        if (arrayStart == std::string::npos) {
            return result;
        }

        size_t pos = arrayStart + 1;

        // Extraer números ignorando estructura de arrays
        while (pos < s.size()) {
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r' || s[pos] == '[' || s[pos] == ',')) {
                ++pos;
            }

            if (pos >= s.size()) break;

            if (s[pos] == ']') {
                size_t next = pos + 1;
                while (next < s.size() && (s[next] == ' ' || s[next] == '\t' || s[next] == '\n' || s[next] == '\r' || s[next] == ',')) {
                    ++next;
                }
                if (next < s.size() && s[next] == ']') {
                    break; // cierre final
                }
                ++pos;
                continue;
            }

            // Parsear número (posible negativo)
            bool negative = false;
            if (s[pos] == '-') {
                negative = true;
                ++pos;
            }

            if (pos < s.size() && isdigit((unsigned char)s[pos])) {
                int value = 0;
                while (pos < s.size() && isdigit((unsigned char)s[pos])) {
                    value = value * 10 + (s[pos] - '0');
                    ++pos;
                }
                result.push_back(negative ? -value : value);
            }
            else {
                ++pos;
            }
        }

        return result;
    }

    // Parsear string entre comillas
    static std::string parseQuotedString(const std::string& s, const std::string& key, size_t startPos)
    {
        size_t keyPos = s.find(key, startPos);
        if (keyPos == std::string::npos) return "";

        size_t firstQuote = s.find('"', keyPos + key.size());
        if (firstQuote == std::string::npos) return "";

        size_t secondQuote = s.find('"', firstQuote + 1);
        if (secondQuote == std::string::npos) return "";

        return s.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    }

};

int main()
{
    CMyTest test;
    if (!test.setup()) {
        fprintf(stderr, "Failed to setup CPixelRender\n");
        return -1;
    }

    test.mainLoop();

    return 0;
}