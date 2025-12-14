#include "PixelRender.h"
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <imgui.h>

class CMyTest : public CPixelRender
{
private:
    int m_x0 = -1;
    int m_y0 = -1;
    int m_x1 = -1;
    int m_y1 = -1;
    RGBA m_currentColor = { 0,0,0,255 }; // Color negro
    int m_drawMode = 0; // 0=Line,1=Ellipse,2=Rectangle,3=Triangle
    int m_lineThickness = 1; // line thickness

    // Base class para todas las figuras
    struct Shape {
        RGBA color;
        int thickness;
        virtual ~Shape() = default;
        virtual void draw(CMyTest* renderer) = 0;
    };

    struct Line : public Shape {
        int x0, y0, x1, y1;
        void draw(CMyTest* renderer) override {
            renderer->drawLine(x0, y0, x1, y1, color, thickness);
        }
    };

    struct Ellipse : public Shape {
        int cx, cy; // center
        int a, b; // radii
        void draw(CMyTest* renderer) override {
            renderer->drawEllipse2(cx, cy, a, b, color, thickness);
        }
    };

    struct Rectangle : public Shape {
        int xmin, ymin, xmax, ymax;
        void draw(CMyTest* renderer) override {
            renderer->drawRectangle(xmin, ymin, xmax, ymax, color, thickness);
        }
    };

    struct Triangle : public Shape {
        int x0, y0, x1, y1, x2, y2;
        void draw(CMyTest* renderer) override {
            renderer->drawTriangle(x0, y0, x1, y1, x2, y2, color, thickness);
        }
    };

    // Lista unificada de todas las figuras en orden de creación
    std::vector<std::unique_ptr<Shape>> m_shapes;

    // Temp storage for triangle clicks
    int m_triTempX[3];
    int m_triTempY[3];
    int m_triClicks = 0;

    int framesThisSecond = 0; // contador de frames para calcular FPS

public:
    CMyTest() {};
    ~CMyTest() {};

    // Nuevas funciones auxiliares para reducir el Alpha de la fuente
    RGBA adjustAlphaForThickness(const RGBA& color, int thickness) {
        if (thickness <= 1) return color;

        // Factor de reducción para compensar la re-escritura/solapamiento.
        float reduction_factor = 5.0f / (float)thickness;

        RGBA adjusted = color;
        float alpha_float = color.a / 255.0f;

        // Multiplicar el alpha de la fuente por un factor para reducir la opacidad de cada 'capa'
        alpha_float = std::min(1.0f, alpha_float * reduction_factor);

        adjusted.a = static_cast<unsigned char>(alpha_float * 255.0f);

        return adjusted;
    }

    // helper: set a thick pixel (square) centered at x,y
    void setThickPixel(int x, int y, RGBA color, int thickness)
    {
        if (thickness <= 1) {
            setPixel(x, y, color);
            return;
        }
        RGBA adjusted_color = color;
        if (color.a != 255.0)
        {
            adjusted_color = adjustAlphaForThickness(color, thickness);
        }

        int half = (thickness - 1) / 2;
        for (int dy = -half; dy <= half; ++dy) {
            for (int dx = -half; dx <= half; ++dx) {
                setPixel(x + dx, y + dy, adjusted_color);
            }
        }
    }

    // Algoritmo de Bresenham para dibujar líneas (con grosor)
    void drawLineBresenham(int x0, int y0, int x1, int y1, RGBA color, int thickness)
    {
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
    }

    void drawLine(int x0, int y0, int x1, int y1, RGBA color)
    {
        drawLine(x0, y0, x1, y1, color, 1);
    }
    void drawLine(int x0, int y0, int x1, int y1, RGBA color, int thickness)
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

    // drawEllipse2 using long long integer arithmetic (with thickness)
    void drawEllipse2(int cx, int cy, int a, int b, RGBA color, int thickness)
    {
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

    void drawEllipse2(int cx, int cy, int a, int b, RGBA color)
    {
        drawEllipse2(cx, cy, a, b, color, 1);
    }

    // Draw rectangle by drawing its four edges (use drawLine with thickness)
    void drawRectangle(int x0, int y0, int x1, int y1, RGBA color, int thickness)
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
    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, RGBA color, int thickness)
    {
        drawLine(x0, y0, x1, y1, color, thickness);
        drawLine(x1, y1, x2, y2, color, thickness);
        drawLine(x2, y2, x0, y0, color, thickness);
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

        // Dibujar la forma actualmente en creación si ambos puntos están definidos
        if (m_x0 >= 0 && m_y0 >= 0 && m_x1 >= 0 && m_y1 >= 0)
        {
            if (m_drawMode == 0) { // Line
                drawLine(m_x0, m_y0, m_x1, m_y1, m_currentColor, m_lineThickness);
            }
            else if (m_drawMode == 1) { // Ellipse
                int a = std::abs(m_x1 - m_x0);
                int b = std::abs(m_y1 - m_y0);
                drawEllipse2(m_x0, m_y0, a, b, m_currentColor, m_lineThickness);
            }
            else if (m_drawMode == 2) { // Rectangle
                drawRectangle(m_x0, m_y0, m_x1, m_y1, m_currentColor, m_lineThickness);
            }
            else if (m_drawMode == 3) { // Triangle preview
                if (m_triClicks == 1) {
                    drawLine(m_triTempX[0], m_triTempY[0], m_x1, m_y1, m_currentColor, m_lineThickness);
                }
                else if (m_triClicks == 2) {
                    drawLine(m_triTempX[0], m_triTempY[0], m_triTempX[1], m_triTempY[1], m_currentColor, m_lineThickness);
                    drawLine(m_triTempX[1], m_triTempY[1], m_x1, m_y1, m_currentColor, m_lineThickness);
                    drawLine(m_x1, m_y1, m_triTempX[0], m_triTempY[0], m_currentColor, m_lineThickness);
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
                    drawLine(m_triTempX[0], m_triTempY[0], cx, cy, m_currentColor, m_lineThickness);
                }
                else if (m_triClicks == 2) {
                    drawLine(m_triTempX[0], m_triTempY[0], m_triTempX[1], m_triTempY[1], m_currentColor, m_lineThickness);
                    drawLine(m_triTempX[1], m_triTempY[1], cx, cy, m_currentColor, m_lineThickness);
                    drawLine(cx, cy, m_triTempX[0], m_triTempY[0], m_currentColor, m_lineThickness);
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

                if (m_drawMode != 3) { // Line/Ellipse/Rectangle: start drag
                    if (button == 0)
                    {
                        m_x0 = static_cast<int>(xpos);
                        m_y0 = height - 1 - static_cast<int>(ypos);
                        m_x1 = m_x0;
                        m_y1 = m_y0;
                        std::cout << "Inicio de figura en (" << m_x0 << ", " << m_y0 << ")\n";
                    }
                }
                else { // Triangle: use clicks to set vertices
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
                            tr->color = m_currentColor;
                            tr->thickness = m_lineThickness;
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

                if (m_drawMode != 3) {
                    if (button == 0)
                    {
                        m_x1 = static_cast<int>(xpos);
                        m_y1 = height - 1 - static_cast<int>(ypos);
                        std::cout << "Figura finalizada en (" << m_x1 << ", " << m_y1 << ")\n";

                        if (m_drawMode == 0) { // Line
                            auto ln = std::make_unique<Line>();
                            ln->x0 = m_x0; ln->y0 = m_y0; ln->x1 = m_x1; ln->y1 = m_y1;
                            ln->color = m_currentColor; ln->thickness = m_lineThickness;
                            m_shapes.push_back(std::move(ln));
                        }
                        else if (m_drawMode == 1) { // Ellipse
                            auto el = std::make_unique<Ellipse>();
                            el->cx = m_x0; el->cy = m_y0;
                            el->a = std::abs(m_x1 - m_x0); el->b = std::abs(m_y1 - m_y0);
                            el->color = m_currentColor; el->thickness = m_lineThickness;
                            m_shapes.push_back(std::move(el));
                        }
                        else if (m_drawMode == 2) { // Rectangle
                            auto rc = std::make_unique<Rectangle>();
                            rc->xmin = std::min(m_x0, m_x1);
                            rc->xmax = std::max(m_x0, m_x1);
                            rc->ymin = std::min(m_y0, m_y1);
                            rc->ymax = std::max(m_y0, m_y1);
                            rc->color = m_currentColor; rc->thickness = m_lineThickness;
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
        ImGui::SetNextWindowSize(ImVec2(300, (float)height), ImGuiCond_Always);
        ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("Control Panel", nullptr, panelFlags);

        const char* modes[] = { "Line", "Ellipse", "Rectangle", "Triangle" };
        ImGui::Text("Draw Mode:");
        ImGui::ListBox("##mode", &m_drawMode, modes, IM_ARRAYSIZE(modes), 4);
        ImGui::Separator();

        ImGui::SliderInt("Line Thickness", &m_lineThickness, 1, 31);
        ImGui::Separator();

        float col[4] = {
            m_currentColor.r / 255.0f,
            m_currentColor.g / 255.0f,
            m_currentColor.b / 255.0f,
            m_currentColor.a / 255.0f
        };
        if (ImGui::ColorEdit4("Line Color", col)) {
            m_currentColor.r = static_cast<unsigned char>(col[0] * 255.0f);
            m_currentColor.g = static_cast<unsigned char>(col[1] * 255.0f);
            m_currentColor.b = static_cast<unsigned char>(col[2] * 255.0f);
            m_currentColor.a = static_cast<unsigned char>(col[3] * 255.0f);
        }

        ImGui::Separator();
        if (ImGui::Button("Clear screen")) {
            m_shapes.clear();
            m_triClicks = 0;
        }

        if (m_drawMode == 3) {
            ImGui::TextWrapped("Triangle mode: click three times to place the three vertices. Current clicks: %d", m_triClicks);
            if (ImGui::Button("Reset Triangle Clicks")) m_triClicks = 0;
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