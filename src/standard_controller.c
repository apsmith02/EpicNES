#include "standard_controller.h"
#include <string.h>

void StdController_Init(StandardController *controller)
{
    memset(controller, 0, sizeof(StandardController));
}

uint8_t StdController_Read(StandardController *controller, uint16_t addr)
{
    if (controller->strobe == 1) {
        //While strobe is high, continuously output state of first button
        return controller->button_state & 0x01;
    } else {
        //When strobe is low, read last captured state from shift register 1 bit at a time
        uint8_t val = controller->button_shift & 0x01;
        controller->button_shift >>= 1;
        return val;
    }
}

void StdController_Write(StandardController *controller, uint16_t addr, uint8_t data)
{
    controller->strobe = data & 0x01;
    if (controller->strobe == 0) { //If turning off strobe, capture last state in shift register
        controller->button_shift = controller->button_state;
    }
}

void StdController_PressButton(StandardController *controller, ControllerButton button)
{
    controller->button_state |= button;
}

void StdController_ReleaseButton(StandardController *controller, ControllerButton button)
{
    controller->button_state &= ~button;
}
