#ifndef STANDARD_CONTROLLER_H
#define STANDARD_CONTROLLER_H
#include <stdint.h>

typedef enum {
    BUTTON_NONE     = 0,
    BUTTON_A        = 1 << 0,
    BUTTON_B        = 1 << 1,
    BUTTON_SELECT   = 1 << 2,
    BUTTON_START    = 1 << 3,
    BUTTON_UP       = 1 << 4,
    BUTTON_DOWN     = 1 << 5,
    BUTTON_LEFT     = 1 << 6,
    BUTTON_RIGHT    = 1 << 7
} ControllerButton;

typedef struct {
    uint8_t strobe; //Button shift register strobe, written through $4016. While 1, reloads the shift register with the current button state.
    uint8_t button_state;
    uint8_t button_shift; //Button state shift register, read through $4016/4017.
} StandardController;

void StdController_Init(StandardController* controller);

uint8_t StdController_Read(StandardController* controller, uint16_t addr);
void StdController_Write(StandardController* controller, uint16_t addr, uint8_t data);

void StdController_PressButton(StandardController* controller, ControllerButton button);
void StdController_ReleaseButton(StandardController* controller, ControllerButton button);

#endif