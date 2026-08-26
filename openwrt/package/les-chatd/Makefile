include $(TOPDIR)/rules.mk

PKG_NAME:=les-chatd
LESCHAT_VERSION?=0.1.11
LESCHAT_RELEASE?=1
PKG_VERSION:=$(LESCHAT_VERSION)
PKG_RELEASE:=$(LESCHAT_RELEASE)
PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
LESCHAT_SOURCE_DIR?=$(TOPDIR)/../les-mesh-chat
PKG_BUILD_DEPENDS:=libevent2 libjson-c sqlite3

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/les-chatd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=LES Mesh Chat daemon
  DEPENDS:=+libstdcpp +libevent2 +libjson-c +libsqlite3
endef

define Package/les-chatd/description
  Offline-first distributed mesh chat service for OpenMANET nodes.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(LESCHAT_SOURCE_DIR)/CMakeLists.txt $(PKG_BUILD_DIR)/
	$(CP) $(LESCHAT_SOURCE_DIR)/LICENSE $(PKG_BUILD_DIR)/
	$(CP) $(LESCHAT_SOURCE_DIR)/THIRD_PARTY_NOTICES.md $(PKG_BUILD_DIR)/
	$(CP) $(LESCHAT_SOURCE_DIR)/include $(PKG_BUILD_DIR)/
	$(CP) $(LESCHAT_SOURCE_DIR)/src $(PKG_BUILD_DIR)/
	$(CP) $(LESCHAT_SOURCE_DIR)/tests $(PKG_BUILD_DIR)/
	$(CP) $(LESCHAT_SOURCE_DIR)/web $(PKG_BUILD_DIR)/
endef

CMAKE_OPTIONS += \
	-DCMAKE_BUILD_TYPE=Release \
	-DLESCHAT_WEB_ROOT=/usr/share/les-chat/web

define Package/les-chatd/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/les-chatd $(1)/usr/sbin/les-chatd
	$(INSTALL_DIR) $(1)/usr/share/les-chat/web
	$(CP) $(PKG_BUILD_DIR)/web/. $(1)/usr/share/les-chat/web/
	$(INSTALL_DIR) $(1)/usr/share/licenses/les-chatd
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/LICENSE $(1)/usr/share/licenses/les-chatd/LICENSE
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/THIRD_PARTY_NOTICES.md $(1)/usr/share/licenses/les-chatd/THIRD_PARTY_NOTICES.md
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/les-chat $(1)/etc/config/les-chat
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/les-chatd $(1)/etc/init.d/les-chatd
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/etc/uci-defaults/99-les-chat $(1)/etc/uci-defaults/99-les-chat
endef

$(eval $(call BuildPackage,les-chatd))
