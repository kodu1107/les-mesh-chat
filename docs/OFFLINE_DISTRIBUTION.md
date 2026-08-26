# 인터넷이 없는 OpenMANET 장비에 배포하기

OpenMANET 장비는 외부 인터넷에 직접 연결하지 않아도 됩니다. 인터넷이 되는
Windows 노트북이 GitHub Release에서 서명된 묶음을 받은 뒤, SSH로 장비에
전달합니다. 노트북의 Wi-Fi 인터넷과 OpenMANET 장비로 향하는 Ethernet을 동시에
사용하며 Windows 인터넷 공유 기능은 켤 필요가 없습니다.

릴리스의 `les-chat-windows-tools-*.zip`을 내려받아 압축을 풀면 두 가지 CMD
실행 파일이 있습니다.

## 방식 2: 개인 OpenMANET 게이트에 바로 설치

`Install-LES-Chat.cmd`를 더블클릭하고 다음 값을 입력합니다.

1. OpenMANET 장비의 IP 주소(예: `10.41.0.225`)
2. 호출 부호. 자동값을 유지하려면 비워 둡니다.

스크립트가 SSH로 Pi 4/Pi 5와 OpenWrt 버전을 판별하고, 맞는 오프라인 묶음을
노트북으로 내려받아 장비에 복사합니다. 장비에서는 서명과 모든 파일의 SHA-256을
검증한 다음 앱과 실행 라이브러리를 한 번에 설치합니다.

Windows OpenSSH 클라이언트가 필요하며 장비의 SSH 로그인이 먼저 되어야 합니다.
SSH 키를 쓴다면 PowerShell에서 다음처럼 실행할 수도 있습니다.

```powershell
.\Install-LES-Chat.ps1 `
  -NodeAddress 10.41.0.225 `
  -Callsign Bolt `
  -IdentityFile $HOME\.ssh\id_ed25519
```

## 방식 3: 공용 MeshGate를 내부 패키지 서버로 사용

인터넷이 되는 관리 노트북에서 `Sync-LES-Chat-MeshGate.cmd`를 더블클릭하고
MeshGate의 IP를 입력합니다. 스크립트는 Pi 4와 Pi 5 패키지가 모두 든 서명된
피드를 받아 MeshGate에 복사하고 TCP `8088`의 로컬 피드 서비스를 시작합니다.
기본 방화벽 zone은 `ahwlan`이므로 외부 WAN에는 공개하지 않습니다.

zone이나 포트를 바꿔야 한다면 PowerShell에서 실행합니다.

```powershell
.\Sync-LES-Chat-MeshGate.ps1 `
  -MeshGateAddress 10.41.0.1 `
  -FirewallZone ahwlan `
  -Port 8088
```

완료되면 스크립트가 각 OpenMANET 노드에서 실행할 명령을 출력합니다. MeshGate는
동시에 겹치는 LAN/HaLow 대역에서 채팅 복제용 outgoing 경로도 자동 설정합니다.

```sh
wget -qO- http://10.41.0.1:8088/install.sh | \
    sh -s -- http://10.41.0.1:8088
```

이 명령은 MeshGate의 공개 키와 서명된 opkg 인덱스를 이용합니다. 노드 자체에는
인터넷이 필요하지 않으며 설치 중 OpenMANET/OpenWrt 외부 피드에도 접속하지
않습니다. 새 릴리스를 배포할 때 관리자는 MeshGate 동기화 CMD를 다시 실행하고,
각 노드에서 위 설치 명령을 다시 실행하면 업그레이드됩니다.

## 릴리스에 포함되는 파일

- `les-chat-offline-...-aarch64_cortex-a72.tar.gz`: Pi 4 직접 설치 묶음
- `les-chat-offline-...-aarch64_cortex-a76.tar.gz`: Pi 5 직접 설치 묶음
- `les-chat-meshgate-feed-....tar.gz`: 두 아키텍처용 MeshGate 피드
- `les-chat-windows-tools-....zip`: 두 CMD와 PowerShell 스크립트
- `SHA256SUMS`: 릴리스 파일 체크섬

오프라인 묶음에는 `les-chatd`, `luci-app-les-chat`, `libgcc1`, `libstdcpp6`,
`zlib`, `libevent2-7`, `libjson-c5`, `libsqlite3-0`만 포함합니다. `luci-base`와
펌웨어 핵심인 `libc`는 이미지에 이미 있으므로 교체하지 않습니다. OpenWrt
`24.10.2`와 같은 리비전(`r28739-d9340319c6`)을 `24.10`으로 표시하는
OpenMANET 1.7.0만 허용하며, 다른 빌드에는 설치를 거부합니다.
