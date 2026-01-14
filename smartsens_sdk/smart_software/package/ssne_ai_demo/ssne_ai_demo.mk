SSNE_AI_DEMO_VERSION =
SSNE_AI_DEMO_SITE = $(S1SRC)/app_demo/$(call qstrip,$(BR2_PACKAGE_SSNE_AI_DEMO_APP)/ssne_ai_demo)
SSNE_AI_DEMO_SITE_METHOD = local

export EXPORT_LIB_M1_SDK_ROOT_PATH = $(call qstrip,$(BR2_M1_SDK_ROOT_PATH))

define SSNE_AI_DEMO_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" -C $(@D) all
endef

define SSNE_AI_DEMO_INSTALL_TARGET_CMDS
	rm -rf mkdir $(TARGET_DIR)/app_demo/
	mkdir $(TARGET_DIR)/app_demo/
	$(INSTALL) -D -m 0755 $(@D)/ssne_ai_demo $(TARGET_DIR)/app_demo/
	cp -r $(@D)/app_assets/. $(TARGET_DIR)/app_demo/app_assets/
	cp -r $(@D)/scripts/. $(TARGET_DIR)/app_demo/scripts/
endef

$(eval $(cmake-package))