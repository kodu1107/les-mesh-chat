# 인터넷이 없는 OpenMANET 환경 오프라인 배포 가이드

본 문서는 외부 인터넷이 연결되지 않은 격리된 **OpenMANET / 802.11ah HaLow 메시 필드**에서 **LES Mesh Chat**을 설치하고 업데이트하는 2가지 방법을 설명합니다.

---

## 🎯 배포 시나리오 개요

```text
[인터넷 가능한 관리자 노트북] (GitHub Release에서 도구 다운로드)
        │
        ├── [방법 1: 직접 설치] ──► (SSH) ──► [단일 OpenMANET 노드]
        │
        └── [방법 2: MeshGate 피드] ──► (SSH) ──► [MeshGate 노드 (:8088)]
                                                          │
                                               (HaLow Mesh / LAN)
                                                          │
                                          ┌───────────────┴───────────────┐
                                          ▼                               ▼
                                   [Point 노드 A]                  [Point 노드 B]
```

릴리스의 `les-chat-windows-tools-<version>.zip` 파일에 포함된 도구를 사용합니다.

---

## 방법 1: Windows 도구를 이용한 개별 노드 직접 설치 (SSH)

관리자 노트북과 OpenMANET 장비가 이더넷 케이블 또는 로컬 Wi-Fi로 1:1 연결되어 있을 때 적합합니다.

### 1. 실행 방법

1. `les-chat-windows-tools-<version>.zip`의 압축을 풉니다.
2. `Install-LES-Chat.cmd`를 더블클릭합니다.
3. 프롬프트에 대상 장비 정보를 입력합니다:
   * **장비 IP 주소:** (예: `10.41.0.225`)
   * **호출 부호(닉네임):** (예: `Alpha`, `Bolt`)

### 2. PowerShell 고급 실행 옵션
SSH 키 인증이나 사용자 지정 파라미터를 사용하는 경우 PowerShell에서 직접 실행할 수 있습니다:

```powershell
.\Install-LES-Chat.ps1 `
  -NodeAddress 10.41.0.225 `
  -Callsign Bolt `
  -IdentityFile $HOME\.ssh\id_ed25519
```

### 3. 동작 원리
1. SSH를 통해 원격 장비의 하드웨어(Pi 4 vs Pi 5) 및 OpenWrt 버전을 자동 판별합니다.
2. 일치하는 오프라인 아카이브(`les-chat-offline-*.tar.gz`)를 노트북이 다운로드하여 장비의 `/tmp`로 전송합니다.
3. 장비 내부에서 SHA-256 체크섬을 검증한 뒤, 의존성 라이브러리와 `les-chatd`, `luci-app-les-chat`를 자동 설치합니다.

---

## 방법 2: 공용 MeshGate를 로컬 패키지 서버로 구축 (권장)

필드에 여러 대의 Point 노드가 존재하고 인터넷이 차단된 경우, 메시 네트워크의 중심이 되는 **MeshGate 노드**를 내부 opkg 패키지 서버로 활용합니다.

### 1단계: 관리자 노트북에서 MeshGate 동기화

`Sync-LES-Chat-MeshGate.cmd`를 더블클릭하고 MeshGate의 IP를 입력합니다. (또는 PowerShell 실행):

```powershell
.\Sync-LES-Chat-MeshGate.ps1 `
  -MeshGateAddress 10.41.0.1 `
  -FirewallZone ahwlan `
  -Port 8088
```

이 스크립트는 다음 작업을 자동으로 처리합니다:
* Pi 4와 Pi 5 패키지가 모두 포함된 서명 피드를 MeshGate로 전송
* MeshGate에서 TCP `8088` 포트로 내부 opkg HTTP 서비스(`/etc/init.d/les-chat-feed`) 시작
* 방화벽 `ahwlan` zone에 `8088` 포트 허용
* MeshGate를 **내부 시간 권위 노드(`authority`)**로 설정하여 메시 전체 시계 동기화 지원
* LAN/HaLow 중복 대역에서의 아웃바운드 라우팅 자동 구성 (`/etc/init.d/les-chat-routing`)

---

### 2단계: 각 Point 노드에서 설치 및 업데이트

각 Point 노드(Kilo, Echo 등)에 SSH로 접속하여 MeshGate 내부 피드 주소로 설치 명령을 실행합니다:

```sh
# MeshGate IP가 10.41.0.1인 경우
wget -qO- http://10.41.0.1:8088/install.sh | \
    sh -s -- http://10.41.0.1:8088
```

* 노드는 외부 인터넷이나 외부 피드 접속 없이 MeshGate 로컬 피드로부터 데몬과 UI를 즉시 설치/업그레이드합니다.
* 차후 새로운 릴리스가 나오면 MeshGate 동기화(1단계) 후 각 노드에서 위 `wget` 명령만 다시 실행하면 일괄 업그레이드가 완료됩니다.

---

## 🔒 데이터 및 설정 보존 정책

새 버전의 패키지로 업그레이드하더라도 기존 사용자 데이터는 안전하게 보존됩니다:

* **Node ID & Callsign:** `/etc/les-chat/node-id`, `/etc/les-chat/callsign`에 영구 보존되어 업그레이드 후에도 재설정할 필요가 없습니다.
* **메시지 기록 DB:** `/overlay/les-chat/messages.db` (또는 지정 경로)에 안전하게 유지됩니다.
* **데몬 제어:** LuCI 관리자 화면에서 데몬이 중단되었을 경우 상단의 **Start service** 또는 **Reconnect** 버튼으로 손쉽게 재가동할 수 있습니다.

---

## 📦 오프라인 번들 패키지 구성 및 의존성

오프라인 아카이브에 포함된 패키지:
* **핵심 애플리케이션:** `les-chatd`, `luci-app-les-chat`
* **필수 런타임 라이브러리:** `libevent2-7`, `libjson-c5`, `libsqlite3-0`, `libstdcpp6`, `libgcc1`, `zlib`

> ⚠️ **펌웨어 호환성:**  
> OpenWrt `24.10.2` (리비전 `r28739-d9340319c6`) 기반의 OpenMANET 1.7.0 펌웨어를 지원합니다. 펌웨어 기본 내장 패키지(`libc`, `luci-base`)는 변경하지 않고 애플리케이션 전용 라이브러리만 안전하게 추가됩니다.

