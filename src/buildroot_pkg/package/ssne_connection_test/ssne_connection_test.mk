################################################################################
#
# ssne_connection_test — A1 ↔ STM32 连接测试
#
# Source: /app/src/a1_connection_test  (project repo src/, mounted in container)
# Builds: ssne_connection_test CMake target
#
################################################################################

SSNE_CONNECTION_TEST_VERSION     =
SSNE_CONNECTION_TEST_SITE        = /app/src/a1_connection_test
SSNE_CONNECTION_TEST_SITE_METHOD = local

# Pass SDK paths
SSNE_CONNECTION_TEST_CONF_OPTS += \
	-DM1_SDK_INC_DIR=$(BASE_DIR)/opt/m1_sdk/usr/include \
	-DM1_SDK_LIB_DIR=$(BASE_DIR)/opt/m1_sdk/usr/lib

# Build only the ssne_connection_test CMake target
define SSNE_CONNECTION_TEST_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" -C $(@D) ssne_connection_test
endef

# Install the binary and scripts to the board target rootfs
define SSNE_CONNECTION_TEST_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/app_demo
	$(INSTALL) -D -m 0755 $(@D)/ssne_connection_test $(TARGET_DIR)/app_demo/
	cp -rn $(@D)/scripts/. $(TARGET_DIR)/app_demo/scripts/ 2>/dev/null || true
endef

$(eval $(cmake-package))
