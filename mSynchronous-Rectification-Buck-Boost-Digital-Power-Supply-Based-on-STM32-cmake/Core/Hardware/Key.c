#include "Key.h"

// 按键定义在.h文件中

extern uint8_t OUT_Status;
extern uint8_t OC_Status;

// 定义按键状态的枚举变量类型
typedef enum
{
	KS_RELEASE_STABLE = 0, // 松开稳定
	KS_PRESS_DEBOUNCE,     // 按下消抖
	KS_PRESS_STABLE,       // 按下稳定
	KS_RELEASE_DEBOUNCE,   // 松开消抖
} KEY_STATUS;



uint8_t KEY_Status[KEY_NUM + 1][3]; // 记录各按键状态，第一层数组表示按键编号，第二层数组表示按键各阶段状态，按键数量+1跳过0号直观一点

#define KEY_LONG_PRESS_TICKS 80
#define KEY_REPEAT_TICKS 10
#define KEY_EVENT_QUEUE_SIZE 8

static uint8_t key_pressed[KEY_NUM + 1];
static uint16_t key_press_ticks[KEY_NUM + 1];
static uint16_t key_repeat_ticks[KEY_NUM + 1];
static uint8_t key_long_fired[KEY_NUM + 1];

static KeyEvent key_queue[KEY_EVENT_QUEUE_SIZE];
static volatile uint8_t key_queue_head = 0;
static volatile uint8_t key_queue_tail = 0;

static void Key_PushEvent(uint8_t key, KeyEventType type)
{
	uint8_t next_head = (uint8_t)((key_queue_head + 1U) % KEY_EVENT_QUEUE_SIZE);  //计算下一个格子（环形队列的核心）
	if (next_head == key_queue_tail)
	{
		return;
	}
	key_queue[key_queue_head].key = key;
	key_queue[key_queue_head].type = type;
	key_queue_head = next_head;
}

/**
 * @brief 初始化按键状态机。
 * 将所有按键状态初始化为松开稳定，清零事件与锁定标志。
 */
void Key_Init(void)
{
	uint8_t i;
	for (i = 0; i < KEY_NUM + 1; i++)
	{
		KEY_Status[i][g_keyStatus] = KS_RELEASE_STABLE;
		KEY_Status[i][g_nowKeyStatus] = KS_RELEASE_STABLE;
		KEY_Status[i][g_lastKeyStatus] = KS_RELEASE_STABLE;
		key_pressed[i] = 0;
		key_press_ticks[i] = 0;
		key_repeat_ticks[i] = 0;
		key_long_fired[i] = 0;
	} // 按键状态机全部初始化为按键松开状态
	key_queue_head = 0;
	key_queue_tail = 0;
}

/**
 * @brief 按键状态机扫描程序。
 * 每 10ms 扫描一次按键输入，采用四态消抖并在按下稳定时触发短按事件。
 * @param key_num 按键编号
 * @param KEY 按键当前状态，读取按键输入电平传入这里（0 表示按下，1 表示松开）
 */
void KEY_Scan(uint8_t key_num, uint8_t KEY)
{
	if (key_num == 0 || key_num > KEY_NUM)
	{
		return; // 防止数组越界，保留跳过 0 号按键的约定
	}

	switch (KEY_Status[key_num][g_keyStatus])
	{
	// 松开稳定：检测到按下倾向，进入按下消抖
	case KS_RELEASE_STABLE:
	{
		if (KEY == 0)
		{
			KEY_Status[key_num][g_keyStatus] = KS_PRESS_DEBOUNCE; // 进入按下消抖
		}
	}
	break;

	// 按下消抖：确认按下稳定或回到松开稳定
	case KS_PRESS_DEBOUNCE:
	{
		if (KEY == 1)
		{
			KEY_Status[key_num][g_keyStatus] = KS_RELEASE_STABLE; // 抖动结束，回到松开稳定
		}
		else
		{
			KEY_Status[key_num][g_keyStatus] = KS_PRESS_STABLE; // 按下稳定成立
		}
	}
	break;

	// 按下稳定：等待松开消抖
	case KS_PRESS_STABLE:
	{
		if (KEY == 1)
		{
			KEY_Status[key_num][g_keyStatus] = KS_RELEASE_DEBOUNCE; // 进入松开消抖
		}
	}
	break;
	// 松开消抖：确认松开稳定或回到按下稳定
	case KS_RELEASE_DEBOUNCE:
	{
		if (KEY == 0)
		{
			KEY_Status[key_num][g_keyStatus] = KS_PRESS_STABLE; // 抖动回落，仍视为按下稳定
		}
		else
		{
			KEY_Status[key_num][g_keyStatus] = KS_RELEASE_STABLE; // 松开稳定成立
		}
	}
	break;

	default:
		break;
	}

	if (KEY_Status[key_num][g_keyStatus] == KS_PRESS_DEBOUNCE || KEY_Status[key_num][g_keyStatus] == KS_RELEASE_DEBOUNCE)
    {
        // 还在消抖，直接结束，保留上一次的 key_pressed 状态，不让它乱变
        return; 
    }

	if (KEY_Status[key_num][g_keyStatus] != KEY_Status[key_num][g_nowKeyStatus])	// 当前状态与前一次状态不一致
	{
		KEY_Status[key_num][g_lastKeyStatus] = KEY_Status[key_num][g_nowKeyStatus];	// 记录上一次状态
		KEY_Status[key_num][g_nowKeyStatus] = KEY_Status[key_num][g_keyStatus];		// 记录当前状态
	}

	key_pressed[key_num] = (KEY_Status[key_num][g_nowKeyStatus] == KS_PRESS_STABLE) ? 1U : 0U;
}


void Key_Tick10ms(void)
{
	uint8_t key;
	for (key = 1; key <= KEY_NUM; key++)
	{
		if (key_pressed[key])
		{
			if (key_press_ticks[key] < 0xFFFF)
			{
				key_press_ticks[key]++;
			}
			if ((key_long_fired[key] == 0) && (key_press_ticks[key] >= KEY_LONG_PRESS_TICKS))
			{
				key_long_fired[key] = 1;
				key_repeat_ticks[key] = 0;
				Key_PushEvent(key, KEY_EVENT_LONG);
			}
			if (key_long_fired[key])
			{
				key_repeat_ticks[key]++;
				if (key_repeat_ticks[key] >= KEY_REPEAT_TICKS)
				{
					key_repeat_ticks[key] = 0;
					Key_PushEvent(key, KEY_EVENT_REPEAT);
				}
			}
		}
		else
		{
			if (key_press_ticks[key] > 0)
			{
				if (key_long_fired[key] == 0)
				{
					Key_PushEvent(key, KEY_EVENT_SHORT);
				}
			}
			key_press_ticks[key] = 0;
			key_repeat_ticks[key] = 0;
			key_long_fired[key] = 0;
		}
	}
}

uint8_t Key_PollEvent(KeyEvent *event)
{
	if (event == NULL)
	{
		return 0;
	}
	if (key_queue_tail == key_queue_head)
	{
		return 0;
	}
	*event = key_queue[key_queue_tail];
	key_queue_tail = (uint8_t)((key_queue_tail + 1U) % KEY_EVENT_QUEUE_SIZE);
	return 1;
}
