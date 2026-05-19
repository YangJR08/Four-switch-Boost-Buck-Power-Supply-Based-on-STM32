#include "PID.h"
#include "hrtim.h"
#include "function.h"

/*
 * 定义一个宏 CCMRAM，用于将函数或变量指定到CCM RAM段。
 * 使用此宏的声明将会被编译器放置在CCM（Cacheable Memory）RAM区域中。
 * 这对于需要快速访问且不被系统缓存机制影响的变量或函数非常有用。
 */
#define CCMRAM __attribute__((section("ccmram")))

extern volatile uint16_t ADC1_RESULT[4];          // ADC1通道1~4采样结果
volatile int32_t VErr0 = 0, VErr1 = 0, VErr2 = 0; // 电压误差
volatile int32_t IErr0 = 0, IErr1 = 0;            // 电流误差
volatile int32_t u0 = 0, u1 = 0;                  // 电压环输出量
volatile int32_t i0 = 0, i1 = 0;                  // 电流环输出量
volatile _CVCC_Mode CVCC_Mode = CV;               // 恒流恒压模式标志位
static int32_t I_Integral = 0;                    // 电流环路积分量（文件级静态，便于统一清零）

void PID_Init(void)
{
    VErr0 = 0;
    VErr1 = 0;
    VErr2 = 0;
    u0 = 0;
    u1 = 0;
    i0 = 0;
    IErr0 = 0;
    I_Integral = 0; // 彻底清除电流环的积分记忆，才是真正的安全初始化
}

// 环路的参数buck输出-恒压-PID型补偿器
#define BUCKPIDb0 5271
#define BUCKPIDb1 -10363
#define BUCKPIDb2 5093
// 环路的参数BOOST输出-恒压-PID型补偿器
#define BOOSTPIDb0 8044
#define BOOSTPIDb1 -15813
#define BOOSTPIDb2 7772

#define ILOOP_KP 6 // 电流环PID补偿器P值
#define ILOOP_KI 3 // 电流环PID补偿器I值
#define ILOOP_KD 1 // 电流环PID补偿器D值

/**
 * @brief BuckBoost电压电流环路控制PID函数。
 * 该函数用于实现BuckBoost电压电流环路控制的PID算法。
 * 在stm32g4xx_it.c文件中的HRTIM1_TIMD_IRQHandler中断函数里调用此函数。
 */
CCMRAM void BuckBoostVILoopCtlPID(void)
{
    CtrValue.Vout_ref = CtrValue.Vout_SETref; // 输出参考电压设置为设置电压

    int32_t VoutTemp = (ADC1_RESULT[2] * CAL_VOUT_K >> 12) + CAL_VOUT_B; // 获取矫正后的输出电压
    int32_t IoutTemp = (ADC1_RESULT[3] * CAL_IOUT_K >> 12) + CAL_IOUT_B; // 获取矫正后的输出电流

    // 计算电流误差量，当输出电流小于参考电流，输出量增加
    IErr0 = CtrValue.Iout_ref - IoutTemp;
    // 电流环路输出= 积分量 + KP*误差量 + KD*当前误差减上次误差
    i0 = I_Integral + IErr0 * ILOOP_KP + (IErr0 - IErr1) * ILOOP_KD;
    // 先试探积分累加，再根据Vout_ref边界决定是否真正落地
    int32_t Next_Integral = I_Integral + IErr0 * ILOOP_KI;

    if (CtrValue.Vout_ref <= 0 && IErr0 < 0)
    {
        // Vout_ref已触底且误差仍要求继续下压，冻结积分
    }
    else if (CtrValue.Vout_ref >= CtrValue.Vout_SETref && IErr0 > 0)
    {
        // Vout_ref已触顶且误差仍要求继续上冲，冻结积分
    }
    else
    {
        I_Integral = Next_Integral;
    }

    // 积分量限制，积分量最大值限制
    if (I_Integral > I_INTEGRAL_MAX)
        I_Integral = I_INTEGRAL_MAX;
    else if (I_Integral < I_INTEGRAL_MIN)
        I_Integral = I_INTEGRAL_MIN;

    // 4. 【优化重点】软启动与模式控制逻辑整合
    // 动态决定当前的电压环参考上限 (V_Max_Limit)
    int32_t V_Max_Limit = CtrValue.Vout_SETref;
    
    // 使用右移 1 位代替除以 2，避免除法指令
    if (DF.SMFlag == Rise && (VoutTemp < (CtrValue.Vout_ref >> 1))) 
    {
        V_Max_Limit = CtrValue.Vout_SSref; 
    }

    // 统一的电压参考值累加与限幅逻辑（消除原代码 if-else 块中 80% 的重复代码）
    CtrValue.Vout_ref += i0;
    CVCC_Mode = CC; // 默认由于电流环介入，处于恒流/限制状态

    if (CtrValue.Vout_ref > V_Max_Limit) 
    {
        CtrValue.Vout_ref = V_Max_Limit;
        CVCC_Mode = CV; // 触顶限幅，意味着电压外环掌握控制权，进入恒压
    }
    else if (CtrValue.Vout_ref < 0) 
    {
        CtrValue.Vout_ref = 0;
    }


    VErr0 = CtrValue.Vout_ref - VoutTemp; // 计算电压误差量，当参考电压大于输出电压，占空比增加，输出量增加

    // 当模式切换时，降低占空比，确保模式切换不过冲
    // BBModeChange为模式切换为，不同模式切换时，该位会被置1
    if (DF.BBModeChange)
    {
        u1 = 0;
        I_Integral = 0;
        i0 = 0;
        DF.BBModeChange = 0;
    }

    // 判断工作模式，BUCK，BOOST，BUCK-BOOST
    switch (DF.BBFlag)
    {
    case NA: // 初始阶段
    {
        VErr0 = 0;
        VErr1 = 0;
        VErr2 = 0;
        u0 = 0;
        u1 = 0;
        i0 = 0;
        I_Integral = 0;
        IErr0 = 0;
        IErr1 = 0;
        break;
    }
    case Buck: // BUCK模式
    {
        u0 = u1 + VErr0 * BUCKPIDb0 + VErr1 * BUCKPIDb1 + VErr2 * BUCKPIDb2; // 计算电压环输出
        // 历史数据幅值
        VErr2 = VErr1;
        VErr1 = VErr0;
        u1 = u0;

        // 环路输出赋值
        CtrValue.BoostDuty = MIN_BOOST_DUTY1; // BOOST上管固定占空比94%，下管6%，给mos驱动自举充电
        CtrValue.BuckDuty = (u0 >> 8) * 3;    // 电压环占空比输出

        // 环路输出最大最小占空比限制
        if (CtrValue.BuckDuty > CtrValue.BUCKMaxDuty)
            CtrValue.BuckDuty = CtrValue.BUCKMaxDuty;
        if (CtrValue.BuckDuty < MIN_BUKC_DUTY)
            CtrValue.BuckDuty = MIN_BUKC_DUTY;
        break;
    }
    case Boost: // Boost模式
    {
        // 调用PID环路计算公式（参照PID环路计算文档）
        u0 = u1 + VErr0 * BOOSTPIDb0 + VErr1 * BOOSTPIDb1 + VErr2 * BOOSTPIDb2;
        // 历史数据幅值
        VErr2 = VErr1;
        VErr1 = VErr0;
        u1 = u0;

        // 环路输出赋值
        CtrValue.BuckDuty = MAX_BUCK_DUTY;  // BUCK上管固定占空比94%
        CtrValue.BoostDuty = (u0 >> 8) * 3; // 电压环占空比输出

        // 环路输出最大最小占空比限制
        if (CtrValue.BoostDuty > CtrValue.BoostMaxDuty)
            CtrValue.BoostDuty = CtrValue.BoostMaxDuty;
        if (CtrValue.BoostDuty < MIN_BOOST_DUTY)
            CtrValue.BoostDuty = MIN_BOOST_DUTY;
        break;
    }
    case Mix: // Mix模式
    {
        // 调用PID环路计算公式
        u0 = u1 + VErr0 * BOOSTPIDb0 + VErr1 * BOOSTPIDb1 + VErr2 * BOOSTPIDb2;
        // 历史数据幅值
        VErr2 = VErr1;
        VErr1 = VErr0;
        u1 = u0;
        IErr1 = IErr0;

        // 环路输出赋值
        CtrValue.BuckDuty = MAX_BUCK_DUTY1; // BUCK上管固定占空比80%
        CtrValue.BoostDuty = (u0 >> 8) * 3; // 电压环占空比输出

        // 环路输出最大最小占空比限制
        if (CtrValue.BoostDuty > CtrValue.BoostMaxDuty)
            CtrValue.BoostDuty = CtrValue.BoostMaxDuty;
        if (CtrValue.BoostDuty < MIN_BOOST_DUTY)
            CtrValue.BoostDuty = MIN_BOOST_DUTY;
        break;
    }
    }

    // PWMENFlag是PWM开启标志位，当该位为0时，执行“清零+安全寄存器写入”
    if (DF.PWMENFlag == 0)
    {
        // 1) 数据拦截
        CtrValue.BuckDuty = 0;
        CtrValue.BoostDuty = 0;

        // 2) 算法拦截，避免重启瞬间带历史量冲击
        u1 = 0;
        u0 = 0;
        I_Integral = 0;
        VErr1 = 0;
        VErr2 = 0;
        IErr1 = 0;

        // 3) 安全寄存器写入（不在这里反复开关硬件输出）
        __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_1, PERIOD);
        __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_3, PERIOD >> 1);
        __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, HRTIM_COMPAREUNIT_1, 0);
        return;
    }

    // 更新对应寄存器
    // buck占空比
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_1, PERIOD - CtrValue.BuckDuty);
    // ADC触发采样点，buck占空比的一半，右移1位为除以2
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_3, __HAL_HRTIM_GETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_1) >> 1);
    // Boost占空比
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, HRTIM_COMPAREUNIT_1, CtrValue.BoostDuty);
}
