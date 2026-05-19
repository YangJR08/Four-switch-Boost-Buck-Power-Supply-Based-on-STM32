#ifndef __UI_H
#define __UI_H

#include "Key.h"

typedef enum
{
    UI_STATE_HOME = 0,
    UI_STATE_MENU,
    UI_STATE_EDIT,
    UI_STATE_RUN_CONFIRM
} UI_State;

void UI_Init(void);
void UI_HandleEvent(const KeyEvent *event);
void UI_Render(void);

#endif
