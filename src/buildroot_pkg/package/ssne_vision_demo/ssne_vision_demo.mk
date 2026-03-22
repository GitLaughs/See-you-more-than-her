################################################################################
#
# ssne_vision_demo — YOLOv8 + OSD + RPLidar + Aurora TCP debug interface
#
# Source: /app/src/a1_ssne_ai_demo  (project repo src/, mounted in container)
# Builds: ssne_vision_demo CMake target
#
################################################################################

SSNE_VISION_DEMO_VERSION     =
SSNE_VISION_DEMO_SITE        = /app/src/a1_ssne_ai_demo
SSNE_VISION_DEMO_SITE_METHOD = local

# Pass SDK paths directly as CMake cache variables (using BASE_DIR from Buildroot env)
SSNE_VISION_DEMO_CONF_OPTS += \
	-DM1_SDK_INC_DIR=$(BASE_DIR)/$(call qstrip,$(BR2_M1_VISION_SDK_ROOT_PATH))/include \
	-DM1_SDK_LIB_DIR=$(BASE_DIR)/$(call qstrip,$(BR2_M1_VISION_SDK_ROOT_PATH))/lib

# Build only the ssne_vision_demo CMake target (cmake-package runs cmake configure first)
define SSNE_VISION_DEMO_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" -C $(@D) ssne_vision_demo
endef

# Install the binary and shared assets to the board target rootfs
define SSNE_VISION_DEMO_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/app_demo/app_assets/models
	$(INSTALL) -D -m 0755 $(@D)/ssne_vision_demo $(TARGET_DIR)/app_demo/
	cp -rn $(@D)/app_assets/. $(TARGET_DIR)/app_demo/app_assets/
	cp -rn $(@D)/scripts/.    $(TARGET_DIR)/app_demo/scripts/
endef

$(eval $(cmake-package))
