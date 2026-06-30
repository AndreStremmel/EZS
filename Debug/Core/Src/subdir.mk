################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Core/Src/pendsv.s 

C_SRCS += \
../Core/Src/app_resources.c \
../Core/Src/hc_sr04.c \
../Core/Src/main.c \
../Core/Src/os_mutex.c \
../Core/Src/os_queue.c \
../Core/Src/os_semaphore.c \
../Core/Src/scheduler.c \
../Core/Src/stack.c \
../Core/Src/stm32l4xx_hal_msp.c \
../Core/Src/stm32l4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32l4xx.c \
../Core/Src/tasks.c \
../Core/Src/uart_driver.c 

OBJS += \
./Core/Src/app_resources.o \
./Core/Src/hc_sr04.o \
./Core/Src/main.o \
./Core/Src/os_mutex.o \
./Core/Src/os_queue.o \
./Core/Src/os_semaphore.o \
./Core/Src/pendsv.o \
./Core/Src/scheduler.o \
./Core/Src/stack.o \
./Core/Src/stm32l4xx_hal_msp.o \
./Core/Src/stm32l4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32l4xx.o \
./Core/Src/tasks.o \
./Core/Src/uart_driver.o 

S_DEPS += \
./Core/Src/pendsv.d 

C_DEPS += \
./Core/Src/app_resources.d \
./Core/Src/hc_sr04.d \
./Core/Src/main.d \
./Core/Src/os_mutex.d \
./Core/Src/os_queue.d \
./Core/Src/os_semaphore.d \
./Core/Src/scheduler.d \
./Core/Src/stack.d \
./Core/Src/stm32l4xx_hal_msp.d \
./Core/Src/stm32l4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32l4xx.d \
./Core/Src/tasks.d \
./Core/Src/uart_driver.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L475xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/%.o: ../Core/Src/%.s Core/Src/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -g3 -DDEBUG -c -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/app_resources.cyclo ./Core/Src/app_resources.d ./Core/Src/app_resources.o ./Core/Src/app_resources.su ./Core/Src/hc_sr04.cyclo ./Core/Src/hc_sr04.d ./Core/Src/hc_sr04.o ./Core/Src/hc_sr04.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/os_mutex.cyclo ./Core/Src/os_mutex.d ./Core/Src/os_mutex.o ./Core/Src/os_mutex.su ./Core/Src/os_queue.cyclo ./Core/Src/os_queue.d ./Core/Src/os_queue.o ./Core/Src/os_queue.su ./Core/Src/os_semaphore.cyclo ./Core/Src/os_semaphore.d ./Core/Src/os_semaphore.o ./Core/Src/os_semaphore.su ./Core/Src/pendsv.d ./Core/Src/pendsv.o ./Core/Src/scheduler.cyclo ./Core/Src/scheduler.d ./Core/Src/scheduler.o ./Core/Src/scheduler.su ./Core/Src/stack.cyclo ./Core/Src/stack.d ./Core/Src/stack.o ./Core/Src/stack.su ./Core/Src/stm32l4xx_hal_msp.cyclo ./Core/Src/stm32l4xx_hal_msp.d ./Core/Src/stm32l4xx_hal_msp.o ./Core/Src/stm32l4xx_hal_msp.su ./Core/Src/stm32l4xx_it.cyclo ./Core/Src/stm32l4xx_it.d ./Core/Src/stm32l4xx_it.o ./Core/Src/stm32l4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32l4xx.cyclo ./Core/Src/system_stm32l4xx.d ./Core/Src/system_stm32l4xx.o ./Core/Src/system_stm32l4xx.su ./Core/Src/tasks.cyclo ./Core/Src/tasks.d ./Core/Src/tasks.o ./Core/Src/tasks.su ./Core/Src/uart_driver.cyclo ./Core/Src/uart_driver.d ./Core/Src/uart_driver.o ./Core/Src/uart_driver.su

.PHONY: clean-Core-2f-Src

