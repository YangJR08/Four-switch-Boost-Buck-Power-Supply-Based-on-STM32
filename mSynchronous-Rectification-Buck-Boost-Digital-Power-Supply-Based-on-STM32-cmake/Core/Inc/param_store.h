#ifndef __PARAM_STORE_H
#define __PARAM_STORE_H

#include <stdint.h>

uint8_t ParamStore_HasValidData(void);
void ParamStore_Init(void);
void ParamStore_RequestCommit(void);
void ParamStore_Tick(void);

#endif
