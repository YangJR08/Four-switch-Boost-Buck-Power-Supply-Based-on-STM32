#include "Key.h"
#include <stddef.h> // 确保 NULL 的定义

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

/**
 * @brief 长按判定的计数阈值。
 * 当按键计数达到此值时（80 * 10ms = 800ms），触发长按事件。
 */
#define KEY_LONG_PRESS_TICKS 80

/**
 * @brief 连按（重复）事件的计数间隔。
 * 在长按期间，每隔此值（10 * 10ms = 100ms）触发一次重复事件。
 */
#define KEY_REPEAT_TICKS 10

/**
 * @brief 按键事件队列的大小。
 */
#define KEY_EVENT_QUEUE_SIZE 8

/**
 * @brief 各按键的当前按下状态标志。
 * 索引 0 保留不使用，1..KEY_NUM 对应各按键。
 * 值为 1 表示按键当前在"按下稳定"状态，值为 0 表示非按下状态。
 */
static uint8_t key_pressed[KEY_NUM + 1];

/**
 * @brief 各按键的按下计数（单位：10ms）。
 * 用于判断是否达到长按阈值。
 */
static uint16_t key_press_ticks[KEY_NUM + 1];

/**
 * @brief 各按键的重复事件计数（单位：10ms）。
 * 在长按期间用于周期性触发重复事件。
 */
static uint16_t key_repeat_ticks[KEY_NUM + 1];

/**
 * @brief 各按键的长按事件触发标志。
 * 值为 1 表示该按键的长按事件已触发，避免重复触发。
 */
static uint8_t key_long_fired[KEY_NUM + 1];

/**
 * @brief 按键事件循环队列。
 * 存储待处理的按键事件。
 */
static KeyEvent key_queue[KEY_EVENT_QUEUE_SIZE];

/**
 * @brief 事件队列的头指针（写入位置）。
 */
static volatile uint8_t key_queue_head = 0;

/**
 * @brief 事件队列的尾指针（读取位置）。
 */
static volatile uint8_t key_queue_tail = 0;


static void Key_PushEvent(uint8_t key, KeyEventType type)
{
    uint8_t next_head = (uint8_t)((key_queue_head + 1U) % KEY_EVENT_QUEUE_SIZE);
    if (next_head == key_queue_tail)
    {
        return; // 队列满，安全退出
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
    }// 按键状态机全部初始化为按键松开状态
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
        if (KEY == 0)
        {
            KEY_Status[key_num][g_keyStatus] = KS_PRESS_DEBOUNCE;
        }
        break;
	// 按下消抖：确认按下稳定或回到松开稳定
    case KS_PRESS_DEBOUNCE:
        if (KEY == 1)
        {
            KEY_Status[key_num][g_keyStatus] = KS_RELEASE_STABLE;
        }
        else
        {
            KEY_Status[key_num][g_keyStatus] = KS_PRESS_STABLE;
        }
        break;
	// 按下稳定：等待松开消抖
    case KS_PRESS_STABLE:
        if (KEY == 1)
        {
            KEY_Status[key_num][g_keyStatus] = KS_RELEASE_DEBOUNCE;
        }
        break;
	// 松开消抖：确认松开稳定或回到按下稳定
    case KS_RELEASE_DEBOUNCE:
        if (KEY == 0)
        {
            KEY_Status[key_num][g_keyStatus] = KS_PRESS_STABLE;
        }
        else
        {
            KEY_Status[key_num][g_keyStatus] = KS_RELEASE_STABLE;
        }
        break;

    default:
        break;
    }

    // 状态记录历史
    if (KEY_Status[key_num][g_keyStatus] != KEY_Status[key_num][g_nowKeyStatus])
    {
        KEY_Status[key_num][g_lastKeyStatus] = KEY_Status[key_num][g_nowKeyStatus];
        KEY_Status[key_num][g_nowKeyStatus] = KEY_Status[key_num][g_keyStatus];
    }

    // 【修复】：取消以前的直接 return。
    // 只有当状态真正稳定在“按下稳定”时，才认为按键有效输入，其余均视为 0。
    // 这样避免了消抖中途截断导致的 ticks 逻辑混乱。
    if (KEY_Status[key_num][g_keyStatus] == KS_PRESS_STABLE)
    {
        key_pressed[key_num] = 1U;
    }
    else
    {
        key_pressed[key_num] = 0U;
    }


}

/**
 * @brief 按键时间驱动处理（每 10ms 调用一次）
 */
void Key_Tick10ms(void)
{
    uint8_t key;
	KEY_Scan(1, KEY1);        // 按键1扫描
    KEY_Scan(2, KEY2);        // 按键2扫描
    KEY_Scan(3, KEY3);        // 按键3扫描
    KEY_Scan(4, KEY4);        // 按键4扫描
    for (key = 1; key <= KEY_NUM; key++)
    {
        if (key_pressed[key])
        {
            if (key_press_ticks[key] < 0xFFFF)
            {
                key_press_ticks[key]++;
            }
            // 判定长按
            if ((key_long_fired[key] == 0) && (key_press_ticks[key] >= KEY_LONG_PRESS_TICKS))
            {
                key_long_fired[key] = 1;
                key_repeat_ticks[key] = 0;
                Key_PushEvent(key, KEY_EVENT_LONG);
            }
            // 连按模式
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
            // 【重点优化】：只有当有物理按下计数，且没触发过长按时，才算作有效的短按抬起
            if (key_press_ticks[key] > 0)
            {
                if (key_long_fired[key] == 0)
                {
                    Key_PushEvent(key, KEY_EVENT_SHORT);
                }
            }
            // 释放后彻底清空标志位
            key_press_ticks[key] = 0;
            key_repeat_ticks[key] = 0;
            key_long_fired[key] = 0;
        }
    }
}

/**
 * @brief 从事件队列中应用数据（主循环调用）
 */
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