// Proyecto #1: Despliegue y la manipulación de primitivas 2D
// Nombre: Oscary Arocha
// C.I: 30.697.617
// La funciones principales están en CMyTest.cpp


#include "CMyTest.h"

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