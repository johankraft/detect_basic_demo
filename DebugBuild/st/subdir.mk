################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../st/startup_stm32l475xx.s 

C_SRCS += \
../st/cmsis_os.c \
../st/flash_l4.c \
../st/stm32l4xx_hal_msp.c \
../st/stm32l4xx_hal_timebase_TIM.c \
../st/stm32l4xx_it.c \
../st/syscalls.c 

OBJS += \
./st/cmsis_os.o \
./st/flash_l4.o \
./st/startup_stm32l475xx.o \
./st/stm32l4xx_hal_msp.o \
./st/stm32l4xx_hal_timebase_TIM.o \
./st/stm32l4xx_it.o \
./st/syscalls.o 

S_DEPS += \
./st/startup_stm32l475xx.d 

C_DEPS += \
./st/cmsis_os.d \
./st/flash_l4.d \
./st/stm32l4xx_hal_msp.d \
./st/stm32l4xx_hal_timebase_TIM.d \
./st/stm32l4xx_it.d \
./st/syscalls.d 


# Each subdirectory must supply rules for building sources it contributes
st/%.o st/%.su st/%.cyclo: ../st/%.c st/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 '-DMBEDTLS_CONFIG_FILE="aws_mbedtls_config.h"' -DDEMO_CFG_SERIAL_UPLOAD_ONLY=1 -DCONFIG_MEDTLS_USE_AFR_MEMORY -DUSE_HAL_DRIVER -DSTM32L475xx '-DMQTTCLIENT_PLATFORM_HEADER=MQTTCMSIS.h' -DENABLE_IOT_INFO -DENABLE_IOT_ERROR -DSENSOR -DRFU -DUSE_OFFLOAD_SSL -c -IC:/src/github-repos/detect_basic_demo/freertos_kernel/include -I"C:/src/github-repos/detect_basic_demo/config_files" -IC:/src/github-repos/detect_basic_demo/freertos_kernel/portable/GCC/ARM_CM4F -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/B-L475E-IOT01 -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/Components/Common -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/Components/es_wifi -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/Components/hts221 -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/Components/lis3mdl -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/Components/lps22hb -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/Components/lsm6dsl -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/Components/mx25r6435f -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/BSP/Components/vl53l0x -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/CMSIS/Include -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/CMSIS/Device/ST/STM32L4xx/Include -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/STM32L4xx_HAL_Driver/Inc -IC:/src/github-repos/detect_basic_demo/st/stm32l475_discovery/STM32L4xx_HAL_Driver/Inc/Legacy -IC:/src/github-repos/detect_basic_demo -IC:/src/github-repos/detect_basic_demo/st -IC:/src/github-repos/detect_basic_demo/libraries/c_sdk/standard/common/include/private -IC:/src/github-repos/detect_basic_demo/libraries/c_sdk/standard/common/include -IC:/src/github-repos/detect_basic_demo/libraries/abstractions/platform/include -IC:/src/github-repos/detect_basic_demo/libraries/abstractions/platform/freertos/include -IC:/src/github-repos/detect_basic_demo/libraries/abstractions/platform/include/platform -IC:/src/github-repos/detect_basic_demo/libraries/freertos_plus/standard/tls/include -IC:/src/github-repos/detect_basic_demo/libraries/freertos_plus/standard/crypto/include -IC:/src/github-repos/detect_basic_demo/libraries/abstractions/pkcs11/corePKCS11/source/include -IC:/src/github-repos/detect_basic_demo/libraries/freertos_plus/standard/utils/include -IC:/src/github-repos/detect_basic_demo/libraries/logging/include -IC:/src/github-repos/detect_basic_demo/libraries/dev_mode_key_provisioning/include -IC:/src/github-repos/detect_basic_demo/libraries/coreMQTT/source/include -IC:/src/github-repos/detect_basic_demo/libraries/coreMQTT/source/interface -IC:/src/github-repos/detect_basic_demo/libraries/abstractions/backoff_algorithm/source/include -IC:/src/github-repos/detect_basic_demo/libraries/abstractions/transport/secure_sockets -IC:/src/github-repos/detect_basic_demo/libraries/abstractions/aws_iot_includes -IC:/src/github-repos/detect_basic_demo/libraries/coreJSON/source/include -IC:/src/github-repos/detect_basic_demo/libraries/device_shadow_for_aws/source/include -IC:/src/github-repos/detect_basic_demo/libraries/3rdparty/pkcs11 -IC:/src/github-repos/detect_basic_demo/libraries/3rdparty/mbedtls/include -IC:/src/github-repos/detect_basic_demo/libraries/3rdparty/mbedtls_config -IC:/src/github-repos/detect_basic_demo/libraries/3rdparty/mbedtls/include/mbedtls -IC:/src/github-repos/detect_basic_demo/libraries/3rdparty/mbedtls_utils -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/CrashCatcher/include -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/CrashCatcher/Core/src -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/DFM/include -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/DFM/config -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/DFM/kernelports/FreeRTOS/include -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/DFM/cloudports/Serial/include -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/DFM/cloudports/Serial/config -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/DFM/storageports/FLASH/include -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/DFM/storageports/FLASH/config -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/TraceRecorder/include -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/TraceRecorder/config -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/TraceRecorder/streamports/RingBuffer/include -IC:/src/github-repos/detect_basic_demo/DevAlertDemo/TraceRecorder/streamports/RingBuffer/config -O1 -ffunction-sections -Wall -fstack-protector-strong -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
st/%.o: ../st/%.s st/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -g3 -c -I"C:/src/github-repos/detect_basic_demo/config_files" -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-st

clean-st:
	-$(RM) ./st/cmsis_os.cyclo ./st/cmsis_os.d ./st/cmsis_os.o ./st/cmsis_os.su ./st/flash_l4.cyclo ./st/flash_l4.d ./st/flash_l4.o ./st/flash_l4.su ./st/startup_stm32l475xx.d ./st/startup_stm32l475xx.o ./st/stm32l4xx_hal_msp.cyclo ./st/stm32l4xx_hal_msp.d ./st/stm32l4xx_hal_msp.o ./st/stm32l4xx_hal_msp.su ./st/stm32l4xx_hal_timebase_TIM.cyclo ./st/stm32l4xx_hal_timebase_TIM.d ./st/stm32l4xx_hal_timebase_TIM.o ./st/stm32l4xx_hal_timebase_TIM.su ./st/stm32l4xx_it.cyclo ./st/stm32l4xx_it.d ./st/stm32l4xx_it.o ./st/stm32l4xx_it.su ./st/syscalls.cyclo ./st/syscalls.d ./st/syscalls.o ./st/syscalls.su

.PHONY: clean-st

