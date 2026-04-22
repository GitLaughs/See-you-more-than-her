M1_SDK_LIB_VERSION =
M1_SDK_LIB_SITE = $(TOPDIR)/output/opt/m1_sdk
M1_SDK_LIB_SITE_METHOD = local

export EXPORT_LIB_M1_SDK_ROOT_PATH = $(call qstrip,$(BR2_M1_SDK_ROOT_PATH))


define M1_SDK_LIB_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/usr/share
	mkdir -p $(TARGET_DIR)/usr/include/smartsoc
	mkdir -p $(TARGET_DIR)/lib/modules/$(call qstrip,$(BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE))/extra/

	cp -r $(@D)/usr/lib/* $(TARGET_DIR)/usr/lib
	cp -r $(@D)/usr/share/* $(TARGET_DIR)/usr/share
	cp -r $(@D)/usr/include/smartsoc/* $(TARGET_DIR)/usr/include/smartsoc/
	cp -r $(@D)/extra/* $(TARGET_DIR)/lib/modules/$(call qstrip,$(BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE))/extra/
endef

$(eval $(generic-package))