#ifndef __KEY_H
#define __KEY_H

#include "main.h"

#define KEY_NUM 4 // 按键数量

#define KEY1 HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)
#define KEY2 HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)
#define KEY3 HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)
#define KEY4 HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin)

extern uint8_t KEY_Status[KEY_NUM + 1][3]; // 记录各按键状态，按键数量+1跳过0号直观一点

// 当前循环结束的(状态机的)状态
#define g_keyStatus 0
// 当前状态(每次循环后与g_keyStatus保持一致)
#define g_nowKeyStatus 1
// 上次状态(用于记录前一状态以区分状态的来源)
#define g_lastKeyStatus 2

/**
 * @brief 初始化按键扫描状态。
 */
void Key_Init(void);

/**
 * @brief 扫描单个按键并更新事件标志。
 * @param key_num 按键编号（1..KEY_NUM），0 号保留不使用
 * @param KEY 按键电平输入（0 表示按下，1 表示松开）
 */
void KEY_Scan(uint8_t key_num, uint8_t KEY);

/**
 * @brief 按键事件类型枚举。
 * 用于表示按键扫描结果的各种事件状态。
 */
typedef enum
{
	KEY_EVENT_NONE = 0,		/**< 无按键事件 */
	KEY_EVENT_SHORT,		/**< 短按事件 */
	KEY_EVENT_LONG,			/**< 长按事件 */
	KEY_EVENT_REPEAT		/**< 重复事件（按键保持按下时的周期性事件）*/
} KeyEventType;

/**
 * @brief 按键事件结构体。
 * 用于封装按键事件的信息。
 */
typedef struct
{
	uint8_t key;			/**< 按键编号（1..KEY_NUM）*/
	KeyEventType type;		/**< 按键事件类型 */
} KeyEvent;

void Key_Tick10ms(void);
uint8_t Key_PollEvent(KeyEvent *event);

#endif
