# LES Mesh Chat OpenWrt IPK 패키지 가이드

본 디렉터리는 OpenWrt / OpenMANET 장비에 **LES Mesh Chat 백엔드 데몬(`les-chatd`)**과 **LuCI 웹 인터페이스(`luci-app-les-chat`)**를 빌드 및 설치하고 운용하기 위한 패키지 파일과 스크립트를 포함합니다.

---

## 🛠️ IPK 패키지 빌드

### 1. 자동화 빌드 스크립트 사용 (권장)
저장소에 포함된 도우미 스크립트를 사용하면 패키지 복사, 의존성 설치, 빌드 및 체크섬 생성을 한 번에 수행합니다:

```bash
./tools/build_openwrt_ipk.sh \
    /path/to/openwrt-sdk \
    dist/pi4
```

> 📖 **참고 문서:**
> * GitHub Actions 자동 릴리스 및 opkg 피드 관리: [`docs/DISTRIBUTION.md`](../docs/DISTRIBUTION.md)
> * 인터넷이 없는 장비에 Windows 도구 또는 MeshGate로 배포: [`docs/OFFLINE_DISTRIBUTION.md`](../docs/OFFLINE_DISTRIBUTION.md)

---

### 2. OpenWrt SDK 수동 빌드

OpenWrt SDK 또는 Buildroot 환경에서 직접 빌드할 경우 아래 순서대로 진행합니다.

#### (1) 피드 업데이트 및 빌드 의존성 준비
공식 SDK에는 개발 헤더가 기본으로 staging되어 있지 않으므로 `base`와 `packages` 피드에서 의존성을 먼저 컴파일합니다:

```bash
./scripts/feeds update base packages
./scripts/feeds install -p base libevent2 libjson-c
./scripts/feeds install -p packages libsqlite3

make package/feeds/base/libevent2/compile V=s
make package/feeds/base/libjson-c/compile V=s
make package/feeds/packages/sqlite3/compile V=s
```

> 💡 **Tip:** 불필요한 `libedit` 및 `ncurses` 의존성을 방지하기 위해 `sqlite3` CLI 대신 라이브러리인 `libsqlite3`만 선택합니다.

#### (2) 패키지 소스 복사 및 컴파일
```bash
# 패키지 디렉터리 복사
cp -a /path/to/les-mesh-chat/openwrt/package/les-chatd/. package/les-chatd/
cp -a /path/to/les-mesh-chat/openwrt/package/luci-app-les-chat/. package/luci-app-les-chat/

# 빌드 실행
export LESCHAT_SOURCE_DIR=/path/to/les-mesh-chat

make package/les-chatd/clean
make package/les-chatd/compile LESCHAT_SOURCE_DIR="$LESCHAT_SOURCE_DIR" V=s

make package/luci-app-les-chat/clean
make package/luci-app-les-chat/compile LESCHAT_SOURCE_DIR="$LESCHAT_SOURCE_DIR" V=s
```

빌드된 IPK는 SDK의 `bin/packages/<architecture>/` 경로에서 확인할 수 있습니다:
* **Raspberry Pi 4:** `aarch64_cortex-a72`
* **Raspberry Pi 5:** `aarch64_cortex-a76`
* **LuCI App:** `all` (공통)

---

## 📦 장비 설치 및 실행

완성된 IPK를 장비의 `/tmp` 디렉터리로 복사한 후 설치합니다:

```bash
opkg install /tmp/les-chatd_0.1.18-*_*.ipk
opkg install /tmp/luci-app-les-chat_0.1.18-*_all.ipk

# 서비스 활성화 및 시작
/etc/init.d/les-chatd enable
/etc/init.d/les-chatd start
```

### 설치되는 파일 목록

| 설치 경로 | 설명 |
|---|---|
| `/usr/sbin/les-chatd` | C++20 백엔드 데몬 바이너리 |
| `/usr/share/les-chat/web/` | 독립 웹 UI 정적 리소스 (HTML/JS/CSS) |
| `/etc/config/les-chat` | UCI 서비스 설정 파일 |
| `/etc/init.d/les-chatd` | OpenWrt procd 서비스 init 스크립트 |
| `/etc/init.d/les-chat-routing` | 중복 LAN/HaLow 대역용 source-policy route 서비스 |
| `/etc/uci-defaults/99-les-chat` | 초기화 스크립트 (Node ID 생성 및 기본값 적용) |
| `/usr/libexec/rpcd/luci.leschat` | LuCI 통신용 rpcd 플러그인 |
| `/usr/share/luci/menu.d/luci-app-les-chat.json` | LuCI 메뉴 등록 파일 |
| `/www/luci-static/resources/view/les_chat/` | LuCI JavaScript 뷰 (Chat, Peers, Settings, Status) |

---

## ⚙️ 설정 가이드 (UCI)

### 1. 기본 설정
```bash
uci set les-chat.main.callsign='Bolt'
uci set les-chat.main.bind='0.0.0.0'
uci set les-chat.main.discovery_interface='br-ahwlan'
uci set les-chat.main.database='/overlay/les-chat/messages.db'
uci commit les-chat
/etc/init.d/les-chatd restart
```

### 2. 시간 동기화 설정 (Time Sync Mode)
외부 인터넷(NTP)이 없는 환경에서는 MeshGate 노드를 시간 권위 노드로 지정하여 메시 전체의 메시지 타임스탬프를 일치시킬 수 있습니다.

* **MeshGate 노드 설정:**
  ```bash
  uci set les-chat.main.time_sync_mode='authority'
  uci commit les-chat
  /etc/init.d/les-chatd restart
  ```
* **일반 Point 노드 설정:**
  ```bash
  uci set les-chat.main.time_sync_mode='client'
  uci set les-chat.main.time_authority_id='node-bolt'
  uci commit les-chat
  /etc/init.d/les-chatd restart
  ```

### 3. 노드 식별자 및 닉네임 자동 복구
* `node_id 'auto'`는 최초 부팅 시 `/etc/les-chat/node-id`에 고유 UUID를 생성합니다.
* LuCI Settings에서 `Node ID`를 `auto`로 유지하거나, 장비마다 고유한 수동 ID(영문·숫자·`.`, `_`, `-`)를 입력할 수 있습니다.
* Node ID를 변경하면 해당 장비가 새로운 메시지 origin으로 인식됩니다.
* `/etc/les-chat/node-id`와 `/etc/les-chat/callsign`은 패키지 업데이트 및 sysupgrade 시에도 유지됩니다.
* LuCI에서는 Settings 탭에서 `Nickname (required)`를 입력해야 하며, 미설정 시 경고 배너가 표시됩니다.

### 4. 네트워크 인터페이스 자동 감지
* `discovery_interface 'auto'`는 OpenMANET의 `br-ahwlan`이 존재하면 UDP announce 및 아웃바운드 HTTP 복제 트래픽의 송신 인터페이스로 사용합니다.
* 이를 통해 LAN(`br-lan`)과 HaLow(`br-ahwlan`)의 IP 대역이 겹치더라도 양방향 채팅 통신이 정상 유지됩니다.

---

## 🛡️ 방화벽 및 중복 대역 라우팅

`les-chatd`를 시작하면 `ahwlan` zone에 TCP/UDP 7777 규칙을 자동으로
설치합니다. WAN에는 규칙을 열지 않습니다. 이미 설치된 장비에서 규칙을
즉시 복구하려면 다음 명령을 실행할 수 있습니다:

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

MeshGate처럼 `br-lan`과 `br-ahwlan`이 같은 `10.41.0.0/16` 대역을 동시에
사용하는 장비에는 `les-chat-routing` 서비스가 함께 설치됩니다. 이 서비스는
HaLow 인터페이스의 실제 주소가 올라온 뒤 source-policy route를 구성하고,
인터페이스가 재연결될 때 자동으로 다시 적용합니다. 수동 확인은 다음과
같습니다:

```sh
/etc/init.d/les-chat-routing enable
/etc/init.d/les-chat-routing restart
ip rule show
ip route show table 1000
```

---

## 🔍 서비스 상태 확인 및 진단

```bash
# 데몬 상태 확인
/etc/init.d/les-chatd status

# 데몬 로그 확인
logread -e les-chatd

# HTTP API 응답 테스트
curl -s http://127.0.0.1:7777/healthz
curl -s http://127.0.0.1:7777/api/v1/peers
```

* **LuCI 접속:** OpenWrt 웹 관리 화면의 **Services → LES Mesh Chat** 메뉴에서 Chat, Peers, Status, Settings 화면을 이용할 수 있습니다.
* **단독 웹 UI:** `http://<node-address>:7777/`로 직접 접속하여 채팅을 이용할 수 있습니다.
