#include "ui.h"
#include "OLED.h"
#include "function.h"
#include "param_store.h"

#define UI_VALUE_X 48
#define UI_UNIT_X 96

typedef enum
{
    UI_MENU_ROOT = 0,
    UI_MENU_PROTECT
} UiMenuLevel;

typedef enum
{
    UI_EDIT_VSET = 0,
    UI_EDIT_ISET,
    UI_EDIT_OTP,
    UI_EDIT_OCP,
    UI_EDIT_OVP
} UiEditTarget;

static UI_State ui_state = UI_STATE_HOME;
static UiMenuLevel ui_menu_level = UI_MENU_ROOT;
static uint8_t ui_menu_index = 0;
static UiEditTarget ui_edit_target = UI_EDIT_VSET;
static float ui_edit_value = 0.0f;
static float ui_edit_original = 0.0f;
static uint8_t ui_edit_digit = 0;
static uint8_t ui_run_confirm_yes = 1;

static char menu_root_items[][8] = {"VSET", "ISET", "PROT", "BACK"};
static char menu_prot_items[][8] = {"OTP", "OCP", "OVP", "BACK"};

static float UI_Clamp(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static void UI_DrawValue(uint8_t x, uint8_t y, float value)
{
    uint16_t int_part = (uint16_t)value;
    uint16_t frac = (uint16_t)((value + 0.005f) * 100.0f) % 100u;

    OLED_ShowNum(x, y, int_part, 2, OLED_8X16);
    OLED_ShowChar(x + 16, y, '.', OLED_8X16);
    OLED_ShowNum(x + 24, y, frac, 2, OLED_8X16);
}

static void UI_DrawValueLine(uint8_t y, char *label, float value, char unit)
{
    OLED_ShowString(0, y, label, OLED_8X16);
    OLED_ShowChar(UI_VALUE_X - 8, y, ':', OLED_8X16);
    UI_DrawValue(UI_VALUE_X, y, value);
    OLED_ShowChar(UI_UNIT_X, y, unit, OLED_8X16);
}

static void UI_EditRange(UiEditTarget target, float *min_value, float *max_value)
{
    switch (target)
    {
    case UI_EDIT_VSET:
        *min_value = 0.5f;
        *max_value = 48.5f;
        break;
    case UI_EDIT_ISET:
        *min_value = 0.01f;
        *max_value = 10.1f;
        break;
    case UI_EDIT_OTP:
        *min_value = 40.0f;
        *max_value = 99.99f;
        break;
    case UI_EDIT_OCP:
        *min_value = 0.01f;
        *max_value = 10.5f;
        break;
    case UI_EDIT_OVP:
        *min_value = 0.5f;
        *max_value = 50.0f;
        break;
    default:
        *min_value = 0.0f;
        *max_value = 0.0f;
        break;
    }
}

static char *UI_EditLabel(UiEditTarget target)
{
    switch (target)
    {
    case UI_EDIT_VSET:
        return "VSET";
    case UI_EDIT_ISET:
        return "ISET";
    case UI_EDIT_OTP:
        return "OTP";
    case UI_EDIT_OCP:
        return "OCP";
    case UI_EDIT_OVP:
        return "OVP";
    default:
        return "EDIT";
    }
}

static char UI_EditUnit(UiEditTarget target)
{
    switch (target)
    {
    case UI_EDIT_VSET:
    case UI_EDIT_OVP:
        return 'V';
    case UI_EDIT_ISET:
    case UI_EDIT_OCP:
        return 'A';
    case UI_EDIT_OTP:
        return 'C';
    default:
        return ' ';
    }
}

static void UI_EnterEdit(UiEditTarget target)
{
    ui_edit_target = target;
    ui_edit_digit = 0;
    switch (target)
    {
    case UI_EDIT_VSET:
        ui_edit_value = SET_Value.Vout;
        break;
    case UI_EDIT_ISET:
        ui_edit_value = SET_Value.Iout;
        break;
    case UI_EDIT_OTP:
        ui_edit_value = MAX_OTP_VAL;
        break;
    case UI_EDIT_OCP:
        ui_edit_value = MAX_VOUT_OCP_VAL;
        break;
    case UI_EDIT_OVP:
        ui_edit_value = MAX_VOUT_OVP_VAL;
        break;
    default:
        ui_edit_value = 0.0f;
        break;
    }
    ui_edit_original = ui_edit_value;
    ui_state = UI_STATE_EDIT;
}

static void UI_SaveEdit(void)
{
    switch (ui_edit_target)
    {
    case UI_EDIT_VSET:
        SET_Value.Vout = ui_edit_value;
        Power_ApplySetpoints();
        break;
    case UI_EDIT_ISET:
        SET_Value.Iout = ui_edit_value;
        Power_ApplySetpoints();
        break;
    case UI_EDIT_OTP:
        MAX_OTP_VAL = ui_edit_value;
        break;
    case UI_EDIT_OCP:
        MAX_VOUT_OCP_VAL = ui_edit_value;
        break;
    case UI_EDIT_OVP:
        MAX_VOUT_OVP_VAL = ui_edit_value;
        break;
    default:
        break;
    }
    SET_Value.SET_modified_flag = 1;
    ParamStore_RequestCommit();
}

static void UI_MoveMenu(int8_t direction)
{
    uint8_t count = (ui_menu_level == UI_MENU_ROOT) ? 4u : 4u;

    if (direction > 0)
    {
        ui_menu_index = (uint8_t)((ui_menu_index + 1u) % count);
    }
    else
    {
        if (ui_menu_index == 0)
        {
            ui_menu_index = (uint8_t)(count - 1u);
        }
        else
        {
            ui_menu_index--;
        }
    }
}

void UI_Init(void)
{
    ui_state = UI_STATE_HOME;
    ui_menu_level = UI_MENU_ROOT;
    ui_menu_index = 0;
    ui_edit_digit = 0;
    ui_run_confirm_yes = 1;
}

void UI_HandleEvent(const KeyEvent *event)
{
    float min_value = 0.0f;
    float max_value = 0.0f;
    float step_values[4] = {10.0f, 1.0f, 0.1f, 0.01f};

    if (event == NULL)
    {
        return;
    }

    if ((DF.SMFlag == Err) && (event->type != KEY_EVENT_REPEAT))
    {
        Power_ClearError();
        return;
    }

    if ((event->key == 4u) && (event->type == KEY_EVENT_LONG) && (ui_state != UI_STATE_RUN_CONFIRM))
    {
        ui_state = UI_STATE_RUN_CONFIRM;
        ui_run_confirm_yes = 1;
        return;
    }

    switch (ui_state)
    {
    case UI_STATE_HOME:
        if ((event->key == 3u) && (event->type == KEY_EVENT_SHORT))
        {
            ui_state = UI_STATE_MENU;
            ui_menu_level = UI_MENU_ROOT;
            ui_menu_index = 0;
        }
        else if ((event->key == 4u) && (event->type == KEY_EVENT_SHORT))
        {
            ui_state = UI_STATE_RUN_CONFIRM;
            ui_run_confirm_yes = 1;
        }
        break;
    case UI_STATE_MENU:
        if ((event->key == 1u) && ((event->type == KEY_EVENT_SHORT) || (event->type == KEY_EVENT_REPEAT)))
        {
            UI_MoveMenu(-1);
        }
        else if ((event->key == 2u) && ((event->type == KEY_EVENT_SHORT) || (event->type == KEY_EVENT_REPEAT)))
        {
            UI_MoveMenu(1);
        }
        else if ((event->key == 3u) && (event->type == KEY_EVENT_SHORT))
        {
            if (ui_menu_level == UI_MENU_ROOT)
            {
                if (ui_menu_index == 0)
                {
                    UI_EnterEdit(UI_EDIT_VSET);
                }
                else if (ui_menu_index == 1)
                {
                    UI_EnterEdit(UI_EDIT_ISET);
                }
                else if (ui_menu_index == 2)
                {
                    ui_menu_level = UI_MENU_PROTECT;
                    ui_menu_index = 0;
                }
                else
                {
                    ui_state = UI_STATE_HOME;
                }
            }
            else
            {
                if (ui_menu_index == 0)
                {
                    UI_EnterEdit(UI_EDIT_OTP);
                }
                else if (ui_menu_index == 1)
                {
                    UI_EnterEdit(UI_EDIT_OCP);
                }
                else if (ui_menu_index == 2)
                {
                    UI_EnterEdit(UI_EDIT_OVP);
                }
                else
                {
                    ui_menu_level = UI_MENU_ROOT;
                    ui_menu_index = 0;
                }
            }
        }
        else if ((event->key == 4u) && (event->type == KEY_EVENT_SHORT))
        {
            ui_state = UI_STATE_RUN_CONFIRM;
            ui_run_confirm_yes = 1;
        }
        break;
    case UI_STATE_EDIT:
        UI_EditRange(ui_edit_target, &min_value, &max_value);
        if ((event->key == 1u) && ((event->type == KEY_EVENT_SHORT) || (event->type == KEY_EVENT_REPEAT)))
        {
            ui_edit_value = UI_Clamp(ui_edit_value + step_values[ui_edit_digit], min_value, max_value);
        }
        else if ((event->key == 2u) && ((event->type == KEY_EVENT_SHORT) || (event->type == KEY_EVENT_REPEAT)))
        {
            ui_edit_value = UI_Clamp(ui_edit_value - step_values[ui_edit_digit], min_value, max_value);
        }
        else if ((event->key == 3u) && (event->type == KEY_EVENT_SHORT))
        {
            ui_edit_digit = (uint8_t)((ui_edit_digit + 1u) % 4u);
        }
        else if ((event->key == 3u) && (event->type == KEY_EVENT_LONG))
        {
            ui_edit_value = ui_edit_original;
            ui_state = UI_STATE_MENU;
        }
        else if ((event->key == 4u) && (event->type == KEY_EVENT_SHORT))
        {
            UI_SaveEdit();
            ui_state = UI_STATE_MENU;
        }
        break;
    case UI_STATE_RUN_CONFIRM:
        if ((event->key == 1u) && ((event->type == KEY_EVENT_SHORT) || (event->type == KEY_EVENT_REPEAT)))
        {
            ui_run_confirm_yes = 1;
        }
        else if ((event->key == 2u) && ((event->type == KEY_EVENT_SHORT) || (event->type == KEY_EVENT_REPEAT)))
        {
            ui_run_confirm_yes = 0;
        }
        else if ((event->key == 4u) && (event->type == KEY_EVENT_SHORT))
        {
            if (ui_run_confirm_yes)
            {
                uint8_t enable = (DF.OUTPUT_Flag == 0) ? 1u : 0u;
                Power_RequestOutput(enable);
            }
            ui_state = UI_STATE_HOME;
        }
        else if ((event->key == 3u) && (event->type == KEY_EVENT_SHORT))
        {
            ui_state = UI_STATE_HOME;
        }
        break;
    default:
        ui_state = UI_STATE_HOME;
        break;
    }
}

void UI_Render(void)
{
    OLED_Clear();

    if (DF.SMFlag == Err)
    {
        if (getRegBits(DF.ErrFlag, F_SW_VOUT_OVP))
        {
            OLED_ShowString(0, 0, "VOUT OVP", OLED_8X16);
        }
        if (getRegBits(DF.ErrFlag, F_SW_IOUT_OCP))
        {
            OLED_ShowString(0, 16, "IOUT OCP", OLED_8X16);
        }
        if (getRegBits(DF.ErrFlag, F_SW_SHORT))
        {
            OLED_ShowString(0, 32, "SHORT", OLED_8X16);
        }
        if (getRegBits(DF.ErrFlag, F_OTP))
        {
            OLED_ShowString(0, 48, "OVER TEMP", OLED_8X16);
        }
        OLED_Update();
        return;
    }

    switch (ui_state)
    {
    case UI_STATE_HOME:
        UI_DrawValueLine(0, "VSET", SET_Value.Vout, 'V');
        UI_DrawValueLine(16, "ISET", SET_Value.Iout, 'A');
        UI_DrawValueLine(32, "VOUT", VOUT, 'V');
        UI_DrawValueLine(48, "IOUT", IOUT, 'A');
        break;
    case UI_STATE_MENU:
    {
        char (*items)[8] = (ui_menu_level == UI_MENU_ROOT) ? menu_root_items : menu_prot_items;
        uint8_t i;
        for (i = 0; i < 4u; i++)
        {
            OLED_ShowString(0, (uint8_t)(i * 16u), items[i], OLED_8X16);
            if (i == ui_menu_index)
            {
                OLED_ReverseArea(0, (uint8_t)(i * 16u), 128, 16);
            }
        }
        break;
    }
    case UI_STATE_EDIT:
    {
        char *label = UI_EditLabel(ui_edit_target);
        char unit = UI_EditUnit(ui_edit_target);
        uint8_t digit_x = 0;

        OLED_ShowString(0, 0, label, OLED_8X16);
        UI_DrawValue(UI_VALUE_X, 16, ui_edit_value);
        OLED_ShowChar(UI_UNIT_X, 16, unit, OLED_8X16);

        if (ui_edit_digit == 0)
        {
            digit_x = UI_VALUE_X;
        }
        else if (ui_edit_digit == 1)
        {
            digit_x = UI_VALUE_X + 8;
        }
        else if (ui_edit_digit == 2)
        {
            digit_x = UI_VALUE_X + 24;
        }
        else
        {
            digit_x = UI_VALUE_X + 32;
        }
        OLED_ReverseArea(digit_x, 16, 8, 16);
        OLED_ShowString(0, 48, "OK=SAVE", OLED_6X8);
        break;
    }
    case UI_STATE_RUN_CONFIRM:
        if (DF.OUTPUT_Flag == 0)
        {
            OLED_ShowString(0, 0, "START?", OLED_8X16);
        }
        else
        {
            OLED_ShowString(0, 0, "STOP?", OLED_8X16);
        }
        OLED_ShowString(0, 16, "YES", OLED_8X16);
        OLED_ShowString(0, 32, "NO", OLED_8X16);
        if (ui_run_confirm_yes)
        {
            OLED_ReverseArea(0, 16, 128, 16);
        }
        else
        {
            OLED_ReverseArea(0, 32, 128, 16);
        }
        break;
    default:
        break;
    }

    OLED_Update();
}
