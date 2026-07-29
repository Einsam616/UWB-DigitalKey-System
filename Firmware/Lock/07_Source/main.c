#include "includes.h"

int main(void)
{
    SystemCoreClockUpdate();

    while (1) {
        delay_ms(500u);
    }
}
