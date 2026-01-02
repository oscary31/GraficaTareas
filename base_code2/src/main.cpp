#include "3DViewer.h"
#include <iostream>



int main() 
{
    C3DViewer test;
    if (!test.setup()) 
    {
        fprintf(stderr, "Failed to setup C3DViewer\n");
        return -1;
    }

    test.mainLoop();

    return 0;
}
