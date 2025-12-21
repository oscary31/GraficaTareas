#pragma once
#ifndef CMYTEST_H
#define CMYTEST_H

#include "PixelRender.h"
#include "Shape.h"
#include "UndoAction.h"
#include <vector>
#include <set>
#include <memory>
#include <string>

// Forward declarations
class Line;
class Ellipse;
class Rectangle;
class Triangle;
class BezierCurve;

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
    RGBA m_controlPointColor = { 255, 119, 0, 255 };
    RGBA m_selectedControlPointColor = { 0, 119, 255, 255 };
    RGBA m_controlPolygonColor = { 136, 136, 136, 255 };
    RGBA m_selectionHandleColor = m_controlPointColor;

    // Colores de fondo
    float m_bgColorArray[4] = { 201.0f / 255.0f, 201.0f / 255.0f, 201.0f / 255.0f, 1.0f };
    RGBA m_bgColor = { 216, 216, 216, 255 };

    // Bandera: usar primitivas de ImGui (ImDrawList) en vez de las implementaciones propias
    bool m_useImGuiPrimitives = false;

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

    // Cursores GLFW (creados bajo demanda)
    GLFWcursor* m_cursorArrow = nullptr;
    GLFWcursor* m_cursorHand = nullptr;
    GLFWcursor* m_cursorResize = nullptr;

    // Modo Bézier: radio de puntos de control y lista temporal
    int m_controlPointRadius = 5;
    std::vector<std::pair<int, int>> m_tempControlPoints;

    // Selección y arrastre genérico
    Shape* m_selectedShape = nullptr;
    int m_selectedHandleIndex = -1;
    bool m_isDraggingHandle = false;
    std::pair<int, int> m_dragStartMouse;
    std::vector<std::pair<int, int>> m_dragStartPoints;

    // Indica si el press inicializó sobre un handle/punto de control
    bool m_pressedOnHandle = false;

    // Indica si se inició la creación de una figura (press en el lienzo)
    bool m_isCreatingShape = false;

    // Undo/Redo stacks
    std::vector<UndoAction> m_undoStack;
    std::vector<UndoAction> m_redoStack;
    const int MAX_UNDO_STACK = 100;

    // Métodos privados
    void ensureCursorsCreated();
    void bringSelectedForward();
    void sendSelectedBackward();
    void bringSelectedToFront();
    void sendSelectedToBack();

    void applyAction(UndoAction& action, bool isUndo);
    void recordAddShape(size_t index);
    void recordDeleteShape(size_t index);
    void recordMoveShape(Shape* shape, const std::vector<std::pair<int, int>>& oldPts,
        const std::vector<std::pair<int, int>>& newPts);
    void recordChangeColor(Shape* shape, RGBA oldBorder, RGBA newBorder,
        RGBA oldFill, RGBA newFill, bool border, bool fill);
    void recordChangeBackground(RGBA oldColor, RGBA newColor);
    void recordChangeLayer(size_t from, size_t to);
    void performUndo();
    void performRedo();
    bool canUndo() const;
    bool canRedo() const;
    std::string getLastActionDescription() const;

    std::vector<unsigned char> prepareImageBuffer();

    Shape* findShapeAt(int tx, int ty, int& outHandleIndex);
    BezierCurve* findBezierCurveNear(int x, int y, int& controlPointIndex);

    void saveToJSON(const std::string& filename);
    void loadFromJSON(const std::string& filename);

public:
    // Conjunto para evitar dibujar el mismo píxel dos veces
    std::set<std::pair<int, int>> m_drawnPixels;

    CMyTest();
    ~CMyTest();

    // Métodos públicos de dibujo
    void setThickPixel(int x, int y, RGBA color, int thickness);
    void drawLineBresenham(int x0, int y0, int x1, int y1, RGBA color, int thickness, bool clear = true);
    void drawLine(int x0, int y0, int x1, int y1, RGBA color, int thickness = 1);
    void ellipsePoints4(long long cx, long long cy, long long x, long long y, RGBA color, int thickness);
    void drawEllipseOutline(int cx, int cy, int a, int b, RGBA color, int thickness = 1);
    void drawRectangleOutline(int x0, int y0, int x1, int y1, RGBA color, int thickness);
    void drawTriangleOutline(int x0, int y0, int x1, int y1, int x2, int y2, RGBA color, int thickness);
    void drawHorizontalLine(int x0, int x1, int y, RGBA color);
    void drawTriangleFilled(int x0, int y0, int x1, int y1, int x2, int y2, RGBA fillColor, RGBA borderColor, int thickness);
    void drawEllipseFilled(int cx, int cy, int a, int b, RGBA fillColor, RGBA borderColor, int thickness);
    void drawRectangleFilled(int x0, int y0, int x1, int y1, RGBA fillColor, RGBA borderColor, int thickness);

    static std::pair<int, int> deCasteljau(const std::vector<std::pair<int, int>>& points, float t);
    std::pair<int, int> deCasteljauTemp(const std::vector<std::pair<int, int>>& points, float t);

    // Métodos de guardado
    void saveToPNG(const std::string& filename);
    void saveToJPG(const std::string& filename, int quality = 90);
    void saveToImage(const std::string& filename, int jpgQuality = 90);

    // Override de métodos virtuales
    void update() override;
    void onKey(int key, int scancode, int action, int mods) override;
    void onMouseButton(int button, int action, int mods) override;
    void onCursorPos(double xpos_d, double ypos_d) override;
    void drawInterface() override;

    // Getters para acceso desde shapes
    Shape* getSelectedShape() const { return m_selectedShape; }
    RGBA getControlPolygonColor() const { return m_controlPolygonColor; }
};

#endif // CMYTEST_H