
# dir to s1 tiny bootrom
export MYBOOTDIR=$(BR2_EXTERNAL_SMART_SOFTWARE_PATH)/package/boot

# root dir to user space and kernel space code
export S1SRC=$(BR2_EXTERNAL_SMART_SOFTWARE_PATH)/src

# include makefile to build packages
include $(sort $(wildcard $(BR2_EXTERNAL_SMART_SOFTWARE_PATH)/package/boot/*/*.mk))
include $(sort $(wildcard $(BR2_EXTERNAL_SMART_SOFTWARE_PATH)/package/*/*.mk))

