################################################################################
#
# ssne_ai_demo — 人脸检测 + OSD标签 + STM32 AKM 底盘控制
#
# Source: /app/src/app_demo/face_detection/ssne_ai_demo
#         (symlinked from SDK smart_software/src/app_demo)
#
################################################################################

SSNE_AI_DEMO_VERSION     =
SSNE_AI_DEMO_SITE        = /app/src/app_demo/face_detection/ssne_ai_demo
SSNE_AI_DEMO_SITE_METHOD = local

# Pass SDK paths
SSNE_AI_DEMO_CONF_OPTS += \
	-DM1_SDK_INC_DIR=$(BASE_DIR)/opt/m1_sdk/usr/include \
	-DM1_SDK_LIB_DIR=$(BASE_DIR)/opt/m1_sdk/usr/lib

# Build only the ssne_ai_demo CMake target
define SSNE_AI_DEMO_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" -C $(@D) ssne_ai_demo
endef

# Install the binary and shared assets to the board target rootfs
define SSNE_AI_DEMO_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/app_demo/app_assets/models
	$(INSTALL) -D -m 0755 $(@D)/ssne_ai_demo $(TARGET_DIR)/app_demo/
	cp -rn $(@D)/app_assets/. $(TARGET_DIR)/app_demo/app_assets/
	cp -rn $(@D)/scripts/.    $(TARGET_DIR)/app_demo/scripts/
endef

$(eval $(cmake-package))
