#pragma once
#ifndef UNDOACTION_H
#define UNDOACTION_H

#include "Shape.h"
#include "PixelRender.h"
#include <memory>
#include <vector>

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

#endif // UNDOACTION_H