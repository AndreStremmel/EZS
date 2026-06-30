#ifndef HC_SR04_H
#define HC_SR04_H

#include <stdint.h>

void HC_SR04_Init(void);
void HC_SR04_Trigger(void);

uint32_t HC_SR04_GetEchoPulseUs(void);

#endif