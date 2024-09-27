################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include st/stm32l475_discovery/ports/common_io/subdir.mk
-include st/stm32l475_discovery/STM32L4xx_HAL_Driver/subdir.mk
-include st/stm32l475_discovery/CMSIS/subdir.mk
-include st/stm32l475_discovery/BSP/Components/vl53l0x/subdir.mk
-include st/stm32l475_discovery/BSP/Components/lsm6dsl/subdir.mk
-include st/stm32l475_discovery/BSP/Components/lps22hb/subdir.mk
-include st/stm32l475_discovery/BSP/Components/lis3mdl/subdir.mk
-include st/stm32l475_discovery/BSP/Components/hts221/subdir.mk
-include st/stm32l475_discovery/BSP/Components/es_wifi/subdir.mk
-include st/stm32l475_discovery/BSP/B-L475E-IOT01/subdir.mk
-include st/subdir.mk
-include freertos_kernel/portable/MemMang/subdir.mk
-include freertos_kernel/portable/GCC/ARM_CM4F/subdir.mk
-include freertos_kernel/subdir.mk
-include DevAlertDemo/TraceRecorder/streamports/RingBuffer/subdir.mk
-include DevAlertDemo/TraceRecorder/config/subdir.mk
-include DevAlertDemo/TraceRecorder/subdir.mk
-include DevAlertDemo/DFM/storageports/FLASH/subdir.mk
-include DevAlertDemo/DFM/kernelports/Generic/subdir.mk
-include DevAlertDemo/DFM/kernelports/FreeRTOS/subdir.mk
-include DevAlertDemo/DFM/cloudports/Serial/subdir.mk
-include DevAlertDemo/DFM/subdir.mk
-include DevAlertDemo/CrashCatcher/Core/src/subdir.mk
-include DevAlertDemo/CrashCatcher/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := aws_demos
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
aws_demos.elf \

MAP_FILES += \
aws_demos.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
aws_demos.list \


# All Target
all:
	+@$(MAKE) --no-print-directory main-build && $(MAKE) --no-print-directory post-build

# Main-build Target
main-build: aws_demos.elf secondary-outputs

# Tool invocations
aws_demos.elf aws_demos.map: $(OBJS) $(USER_OBJS) C:/src/github-repos/detect_basic_demo/STM32L475VGTx_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "aws_demos.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m4 -T"C:/src/github-repos/detect_basic_demo/STM32L475VGTx_FLASH.ld" --specs=nosys.specs -Wl,-Map="aws_demos.map" -Wl,--gc-sections -static -z muldefs -Wl,--build-id  -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

aws_demos.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "aws_demos.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) aws_demos.elf aws_demos.list aws_demos.map default.size.stdout
	-@echo ' '

post-build:
	arm-none-eabi-objcopy -O binary "aws_demos.elf" "aws_demos.bin"
	arm-none-eabi-objcopy -O ihex "aws_demos.elf" "aws_demos.hex"
	arm-none-eabi-size "aws_demos.elf"
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified post-build

-include ../makefile.targets
