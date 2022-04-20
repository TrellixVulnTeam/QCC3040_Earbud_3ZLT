################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../usb/src/stm32f0xx_hal_pcd.c \
../usb/src/stm32f0xx_hal_pcd_ex.c \
../usb/src/stm32f0xx_ll_usb.c \
../usb/src/usbd_cdc.c \
../usb/src/usbd_cdc_if.c \
../usb/src/usbd_conf.c \
../usb/src/usbd_core.c \
../usb/src/usbd_ctlreq.c \
../usb/src/usbd_desc.c \
../usb/src/usbd_ioreq.c 

OBJS += \
./usb/src/stm32f0xx_hal_pcd.o \
./usb/src/stm32f0xx_hal_pcd_ex.o \
./usb/src/stm32f0xx_ll_usb.o \
./usb/src/usbd_cdc.o \
./usb/src/usbd_cdc_if.o \
./usb/src/usbd_conf.o \
./usb/src/usbd_core.o \
./usb/src/usbd_ctlreq.o \
./usb/src/usbd_desc.o \
./usb/src/usbd_ioreq.o 

C_DEPS += \
./usb/src/stm32f0xx_hal_pcd.d \
./usb/src/stm32f0xx_hal_pcd_ex.d \
./usb/src/stm32f0xx_ll_usb.d \
./usb/src/usbd_cdc.d \
./usb/src/usbd_cdc_if.d \
./usb/src/usbd_conf.d \
./usb/src/usbd_core.d \
./usb/src/usbd_ctlreq.d \
./usb/src/usbd_desc.d \
./usb/src/usbd_ioreq.d 


# Each subdirectory must supply rules for building sources it contributes
usb/src/%.o: ../usb/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM GNU C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -fno-move-loop-invariants -Wall -Wextra  -g3 -DDEBUG -DUSE_FULL_ASSERT -DUSE_STDPERIPH_DRIVER -DHSE_VALUE=16000000 -DSTM32F070xB -DVARIANT_ST2 -I../include/override -I"../include" -I"../usb/include" -I"../system/include" -I"../system/include/cmsis" -I"../system/include/stm32f0-stdperiph" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


