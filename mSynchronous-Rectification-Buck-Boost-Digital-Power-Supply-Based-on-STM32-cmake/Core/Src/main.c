/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "hrtim.h"
#include "i2c.h"
#include "iwdg.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include "function.h"
#include "param_store.h"
#include "ui.h"
#include "Key.h"
#include "PID.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// #define REF_3V3 3.2993 // VREF参考电压
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 毫秒计时变量
volatile uint16_t ms_cnt_1 = 0; // 计时变量1
volatile uint16_t ms_cnt_2 = 0; // 计时变量2
volatile uint16_t ms_cnt_3 = 0; // 计时变量3
volatile uint16_t ms_cnt_4 = 0; // 计时变量4
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_I2C3_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_USART1_UART_Init();
  MX_ADC5_Init();
  MX_HRTIM1_Init();
  MX_TIM3_Init();
  MX_IWDG_Init();
  MX_TIM4_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */
  DF.SMFlag = Init;                         // 初始化状态机
  OLED_Init();                              // OLED初始化
  OLED_Clear();                             // 清除OLED屏显示缓冲区
  OLED_ShowChinese(40, 24, "启动中");       // 在屏幕中间显示 启动中
  OLED_Update();                            // 更新OLED显示内容
  HAL_TIM_Base_Start_IT(&htim2);            // 启动定时器2和定时器中断，1kHz
  HAL_TIM_Base_Start_IT(&htim3);            // 启动定时器3和定时器中断，200Hz
  HAL_TIM_Base_Start_IT(&htim4);            // 启动定时器4和定时器中断，100Hz
  Key_Init();                               // 按键状态机初始化
  PID_Init();                               // PID初始化
  ParamStore_Init();                        // 读取参数存储
  Power_ApplySetpoints();                   // 同步设置值到控制参考
  UI_Init();                                // UI状态机初始化

  HAL_Delay(200);                                        // 延时100ms，等待供电稳定
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED); // 校准ADC1
  HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED); // 校准ADC2
  HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED); // 校准ADC5
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)ADC1_RESULT, 4); // 启动ADC1采样和DMA数据传送,采样输入输出电压电流
  HAL_ADC_Start(&hadc2);                                 // 启动ADC2采样，采样NTC温度
  HAL_ADC_Start(&hadc5);                                 // 启动ADC5采样，采样单片机CPU温度

  //__HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_1, 30000 - 18000); // 设置HRTIM定时器D的比较单元1的值（设置PWM占空比）
  //__HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_3, 15000);         // 设置HRTIM定时器D的比较单元3的值（设置触发ADC采样的比较值）
  //__HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, HRTIM_COMPAREUNIT_1, 1800);          // 设置HRTIM定时器F的比较单元1的值（设置PWM占空比）
  // HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1 | HRTIM_OUTPUT_TD2);                   // 开启HRTIM的PWM输出
  // HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TF1 | HRTIM_OUTPUT_TF2);                   // 开启HRTIM的PWM输出

  HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_TIMER_D);                     // 开启HRTIM波形计数器
  HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_TIMER_F);                     // 开启HRTIM波形计数器
  __HAL_HRTIM_TIMER_ENABLE_IT(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_TIM_IT_REP); // 开启HRTIM定时器D的中断



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    {
      KeyEvent event;
      while (Key_PollEvent(&event))
      {
        UI_HandleEvent(&event);
      }
    }

    if (ms_cnt_3 >= 10) // 判断是否计时到10ms
    {
      ms_cnt_3 = 0;    // 计时清零
      ADC_calculate(); // ADC采样结果计算
    }

    if (ms_cnt_4 >= 50) // 判断是否计时到50ms
    {
      ms_cnt_4 = 0;    // 计时清零


      if (IOUT >= 0.1)
      {
        powerEfficiency = (VOUT * IOUT) / (VIN * IIN) * 100.0; // 计算电源转换效率
      }
      else
      {
        powerEfficiency = 0;
      }

      USART1_Printf("%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%d\n", VIN, IIN, VOUT, IOUT, MainBoard_TEMP, CPU_TEMP, powerEfficiency, CVCC_Mode); // 串口发送数据
    }

    if (ms_cnt_2 >= 100) // 判断是否计时到100ms
    {
      ms_cnt_2 = 0;   // 计时清零
      UI_Render();       // 刷新OLED屏显示内容
      ParamStore_Tick(); // 参数存储延时提交
    }

    if (ms_cnt_1 >= 500) // 判断是否计时到500ms
    {
      ms_cnt_1 = 0;                                   // 计时清零
      
      
    }

    HAL_IWDG_Refresh(&hiwdg); // 喂狗
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */

/**
 * @brief HAL_TIM_PeriodElapsedCallback函数,中断回调函数
 *
 * 当定时器周期结束时，该函数将被调用。
 *
 * @param htim TIM句柄指针
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2) // 定时器TIM2，中断时间1ms
  {
    ms_cnt_1++;
    ms_cnt_2++;
    ms_cnt_3++;
    ms_cnt_4++;
  }
  if (htim->Instance == TIM3) // 定时器TIM3，中断时间5ms
  {
    ADCSample(); // ADC采样滤波函数
    ShortOff();  // 短路保护
    OTP();       // 过温保护
    OVP();       // 输出过压保护
    OCP();       // 输出过流保护
    StateM();    // 电源状态机函数
    BBMode();    // 运行模式判断
  }
  if (htim->Instance == TIM4) // 定时器TIM4，中断时间10ms
  {
    KEY_Scan(1, KEY1);        // 按键1扫描
    KEY_Scan(2, KEY2);        // 按键2扫描
    KEY_Scan(3, KEY3);        // 按键3扫描
    KEY_Scan(4, KEY4);        // 按键4扫描
    Key_Tick10ms();           // 生成按键事件
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
