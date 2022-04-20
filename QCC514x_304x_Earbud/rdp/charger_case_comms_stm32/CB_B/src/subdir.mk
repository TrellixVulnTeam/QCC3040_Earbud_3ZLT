################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/adc.c \
../src/auth.c \
../src/battery.c \
../src/case.c \
../src/case_charger.c \
../src/ccp.c \
../src/charger.c \
../src/charger_comms.c \
../src/charger_comms_device.c \
../src/charger_detect.c \
../src/cli.c \
../src/cli_parse.c \
../src/cli_txf.c \
../src/clock.c \
../src/config.c \
../src/crc.c \
../src/current_senses.c \
../src/debug.c \
../src/dfu.c \
../src/earbud.c \
../src/fake_earbud.c \
../src/flash.c \
../src/gpio.c \
../src/interrupt.c \
../src/led.c \
../src/main.c \
../src/memory.c \
../src/pfn.c \
../src/power.c \
../src/rtc.c \
../src/timer.c \
../src/uart.c \
../src/usb.c \
../src/vreg.c \
../src/wdog.c \
../src/wire.c 

OBJS += \
./src/adc.o \
./src/auth.o \
./src/battery.o \
./src/case.o \
./src/case_charger.o \
./src/ccp.o \
./src/charger.o \
./src/charger_comms.o \
./src/charger_comms_device.o \
./src/charger_detect.o \
./src/cli.o \
./src/cli_parse.o \
./src/cli_txf.o \
./src/clock.o \
./src/config.o \
./src/crc.o \
./src/current_senses.o \
./src/debug.o \
./src/dfu.o \
./src/earbud.o \
./src/fake_earbud.o \
./src/flash.o \
./src/gpio.o \
./src/interrupt.o \
./src/led.o \
./src/main.o \
./src/memory.o \
./src/pfn.o \
./src/power.o \
./src/rtc.o \
./src/timer.o \
./src/uart.o \
./src/usb.o \
./src/vreg.o \
./src/wdog.o \
./src/wire.o 

C_DEPS += \
./src/adc.d \
./src/auth.d \
./src/battery.d \
./src/case.d \
./src/case_charger.d \
./src/ccp.d \
./src/charger.d \
./src/charger_comms.d \
./src/charger_comms_device.d \
./src/charger_detect.d \
./src/cli.d \
./src/cli_parse.d \
./src/cli_txf.d \
./src/clock.d \
./src/config.d \
./src/crc.d \
./src/current_senses.d \
./src/debug.d \
./src/dfu.d \
./src/earbud.d \
./src/fake_earbud.d \
./src/flash.d \
./src/gpio.d \
./src/interrupt.d \
./src/led.d \
./src/main.d \
./src/memory.d \
./src/pfn.d \
./src/power.d \
./src/rtc.d \
./src/timer.d \
./src/uart.d \
./src/usb.d \
./src/vreg.d \
./src/wdog.d \
./src/wire.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM GNU C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -fno-move-loop-invariants -Wall -Wextra  -g3 -DDEBUG -DUSE_FULL_ASSERT -DUSE_STDPERIPH_DRIVER -DHSE_VALUE=16000000 -DSTM32F070xB -DVARIANT_CB -I../include/override -I"../include" -I"../usb/include" -I"../system/include" -I"../system/include/cmsis" -I"../system/include/stm32f0-stdperiph" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


