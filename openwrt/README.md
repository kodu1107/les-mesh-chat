# LES Mesh Chat OpenWrt IPK

이 디렉터리는 장비에서 소스를 빌드하지 않고, PC에서 미리 만든 IPK를 장비에 설치하는 배포 구성을 담습니다.

## IPK 빌드

저장소의 빌드 도우미를 사용하면 패키지 복사, 의존성 준비, 빌드 및 체크섬
생성을 한 번에 수행합니다.

```bash
./tools/build_openwrt_ipk.sh \
    /path/to/openwrt-sdk \
    dist/pi4
```

GitHub Release와 서명된 opkg 피드를 자동 운영하는 방법은
[`docs/DISTRIBUTION.md`](../docs/DISTRIBUTION.md)에 있습니다.
인터넷이 없는 장비에 Windows CMD 또는 공용 MeshGate로 배포하는 방법은
[`docs/OFFLINE_DISTRIBUTION.md`](../docs/OFFLINE_DISTRIBUTION.md)에 있습니다.

### 수동 빌드

OpenWrt SDK 또는 Buildroot 환경에 이 패키지를 `package/les-chatd`로 복사하고 소스 저장소 경로를 지정합니다.

공식 SDK에는 feed 소스와 개발 헤더가 기본으로 staging되어 있지 않으므로, 먼저 `base`와 `packages` feed에서 빌드 의존성을 준비합니다.

```bash
./scripts/feeds update base packages
./scripts/feeds install -p base libevent2 libjson-c
./scripts/feeds install -p packages libsqlite3

make package/feeds/base/libevent2/compile V=s
make package/feeds/base/libjson-c/compile V=s
make package/feeds/packages/sqlite3/compile V=s
```

SQLite CLI는 사용하지 않으므로 `sqlite3`나 `sqlite3-cli` 대신 `libsqlite3`만 선택합니다. 그래야 불필요한 `libedit` 및 `ncurses` 의존성을 피할 수 있습니다.

그다음 패키지 디렉터리를 SDK로 복사합니다.

```bash
cp -a /path/to/les-mesh-chat/openwrt/package/les-chatd/. \
    package/les-chatd/
```

```bash
export LESCHAT_SOURCE_DIR=/path/to/les-mesh-chat
make package/les-chatd/clean
make package/les-chatd/compile \
    LESCHAT_SOURCE_DIR="$LESCHAT_SOURCE_DIR" V=s
```

생성된 IPK는 SDK의 `bin/packages/` 아래에서 찾습니다. Pi 4와 Pi 5는 각각 해당 target SDK에서 별도로 빌드합니다.

이 작업은 장비에서 실행하지 않습니다. 장비에는 완성된 IPK 파일만 전달합니다.

## 장비 설치

IPK를 장비로 복사한 뒤 다음을 실행합니다.

```bash
opkg install /tmp/les-chatd_0.1.4-r1_*.ipk
/etc/init.d/les-chatd enable
/etc/init.d/les-chatd start
```

설치된 파일:

```text
/usr/sbin/les-chatd
/usr/share/les-chat/web/
/etc/config/les-chat
/etc/init.d/les-chatd
/etc/uci-defaults/99-les-chat
```

## 설정

```bash
uci set les-chat.main.callsign='Bolt'
uci set les-chat.main.bind='0.0.0.0'
uci set les-chat.main.database='/overlay/les-chat/messages.db'
uci commit les-chat
/etc/init.d/les-chatd restart
```

`node_id 'auto'`는 최초 초기화 시 `/etc/les-chat/node-id`에 생성된 안정적인 ID를 사용합니다. 두 장비가 같은 node ID 파일을 공유하지 않도록 합니다.

## 방화벽

WAN을 열지 않고 실제 메시 네트워크 zone에만 허용합니다. 장비의 zone 이름이 `ahwlan`이 아니라면 실제 zone 이름으로 바꿉니다.

```bash
uci add firewall rule
uci set firewall.@rule[-1].name='LES Mesh Chat TCP'
uci set firewall.@rule[-1].src='ahwlan'
uci set firewall.@rule[-1].proto='tcp'
uci set firewall.@rule[-1].dest_port='7777'
uci set firewall.@rule[-1].target='ACCEPT'

uci add firewall rule
uci set firewall.@rule[-1].name='LES Mesh Chat UDP'
uci set firewall.@rule[-1].src='ahwlan'
uci set firewall.@rule[-1].proto='udp'
uci set firewall.@rule[-1].dest_port='7777'
uci set firewall.@rule[-1].target='ACCEPT'

uci commit firewall
/etc/init.d/firewall restart
```

## 설치 확인

```bash
/etc/init.d/les-chatd status
logread -e les-chatd
curl -s http://127.0.0.1:7777/healthz
curl -s http://127.0.0.1:7777/api/v1/peers
```

패키지 업데이트 시 새 IPK를 설치하면 기존 `/overlay/les-chat/messages.db`와 node ID는 유지됩니다.
