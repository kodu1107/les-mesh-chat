include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-les-chat
LESCHAT_VERSION?=0.1.16
LESCHAT_RELEASE?=1
PKG_VERSION:=$(LESCHAT_VERSION)
PKG_RELEASE:=$(LESCHAT_RELEASE)
PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
LESCHAT_SOURCE_DIR?=$(TOPDIR)/../les-mesh-chat

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-les-chat
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LES Mesh Chat LuCI interface
  DEPENDS:=+les-chatd +luci-base
  PKGARCH:=all
endef

define Package/luci-app-les-chat/description
  LuCI menus for LES Mesh Chat status, peers, settings, and chat.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/luci-app-les-chat/install
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/usr/libexec/rpcd/luci.leschat $(1)/usr/libexec/rpcd/luci.leschat
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) ./files/usr/share/luci/menu.d/luci-app-les-chat.json \
		$(1)/usr/share/luci/menu.d/luci-app-les-chat.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./files/usr/share/rpcd/acl.d/luci-app-les-chat.json \
		$(1)/usr/share/rpcd/acl.d/luci-app-les-chat.json
	$(INSTALL_DIR) $(1)/usr/share/ucitrack
	$(INSTALL_DATA) ./files/usr/share/ucitrack/luci-app-les-chat.json \
		$(1)/usr/share/ucitrack/luci-app-les-chat.json
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/les_chat
	$(INSTALL_DATA) \
		./files/www/luci-static/resources/view/les_chat/chat.js \
		./files/www/luci-static/resources/view/les_chat/common.js \
		./files/www/luci-static/resources/view/les_chat/les_chat.css \
		./files/www/luci-static/resources/view/les_chat/peers.js \
		./files/www/luci-static/resources/view/les_chat/settings.js \
		./files/www/luci-static/resources/view/les_chat/status.js \
		$(1)/www/luci-static/resources/view/les_chat/
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/etc/uci-defaults/80-luci-app-les-chat \
		$(1)/etc/uci-defaults/80-luci-app-les-chat
	$(INSTALL_DIR) $(1)/usr/share/licenses/luci-app-les-chat
	$(INSTALL_DATA) $(LESCHAT_SOURCE_DIR)/LICENSE \
		$(1)/usr/share/licenses/luci-app-les-chat/LICENSE
endef

$(eval $(call BuildPackage,luci-app-les-chat))
