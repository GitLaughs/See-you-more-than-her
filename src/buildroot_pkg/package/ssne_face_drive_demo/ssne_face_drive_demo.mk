################################################################################
#
# ssne_face_drive_demo — 人脸检测 + STM32 AKM 底盘控制兼容性测试
#
# Source: /app/src/a1_ssne_ai_demo  (project repo src/, mounted in container)
# Builds: ssne_face_drive_demo CMake target
#
################################################################################

SSNE_FACE_DRIVE_DEMO_VERSION     =
SSNE_FACE_DRIVE_DEMO_SITE        = /app/src/a1_ssne_ai_demo
SSNE_FACE_DRIVE_DEMO_SITE_METHOD = local

# Pass SDK paths — reuse the same SDK root as ssne_vision_demo
SSNE_FACE_DRIVE_DEMO_CONF_OPTS += \
	-DM1_SDK_INC_DIR=$(BASE_DIR)/opt/m1_sdk/usr/include \
	-DM1_SDK_LIB_DIR=$(BASE_DIR)/opt/m1_sdk/usr/lib

# Build only the ssne_face_drive_demo CMake target
define SSNE_FACE_DRIVE_DEMO_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" -C $(@D) ssne_face_drive_demo
endef

# Install the binary and shared assets to the board target rootfs
define SSNE_FACE_DRIVE_DEMO_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/app_demo/app_assets/models
	$(INSTALL) -D -m 0755 $(@D)/ssne_face_drive_demo $(TARGET_DIR)/app_demo/
	cp -rn $(@D)/app_assets/. $(TARGET_DIR)/app_demo/app_assets/
	cp -rn $(@D)/scripts/.    $(TARGET_DIR)/app_demo/scripts/
endef

$(eval $(cmake-package))
