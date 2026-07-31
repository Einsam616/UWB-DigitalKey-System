#include "includes.h"

int main(void)
{
    SystemCoreClockUpdate();
    App_Init();
    while (1)
    {
        App_Run();
    }
}
