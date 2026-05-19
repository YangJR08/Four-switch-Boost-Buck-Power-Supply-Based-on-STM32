#ifndef __PID_H
#define __PID_H

#include "main.h"

//一个开关周期数字量 
#define PERIOD 30000


#define I_INTEGRAL_MAX  8190   // 电流积分正向最大值
#define I_INTEGRAL_MIN -8190   // 电流积分负向最大值（防止短路时向下滚雪球）

void PID_Init(void);
void BuckBoostVILoopCtlPID(void);

#endif
