# GitHub 및 opkg 피드 배포 가이드

본 문서는 **LES Mesh Chat**의 GitHub Actions CI/CD를 통해 Raspberry Pi 4/5용 OpenWrt IPK 패키지를 자동 빌드하고, 서명된 **GitHub Pages opkg 피드** 및 **GitHub Release**를 배포하는 전 과정을 다룹니다.

---

## 📌 배포 파이프라인 개요

`v*` 형식의 Git 태그가 저장소에 푸시되면 GitHub Actions 워크플로가 자동으로 실행되어 다음 작업을 수행합니다:

1. **아키텍처별 컴파일:**
   * Raspberry Pi 4 (`bcm2711`, `aarch64_cortex-a72`)
   * Raspberry Pi 5 (`bcm2712`, `aarch64_cortex-a76`)
   * LuCI Web App (`all`)
2. **패키지 인덱스 생성 및 usign 서명**
3. **GitHub Pages 배포:** 원클릭 설치 스크립트(`install.sh`) 및 opkg 피드 호스팅
4. **GitHub Release 생성:** 오프라인 설치 번들, MeshGate 번들, Windows 도구 및 `SHA256SUMS` 게시

---

## 1. 사전 준비 (Prerequisites)

### (1) GitHub 원격 저장소 연결
GitHub에 빈 저장소를 생성한 후 로컬 저장소와 연결합니다:

```bash
git remote add origin git@github.com:YOUR_ACCOUNT/les-mesh-chat.git
```

### (2) opkg 패키지 서명 키 생성 (`usign`)
OpenWrt SDK의 `usign` 도구를 사용하여 서명 키 쌍을 1회 생성합니다.

> ⚠️ **보안 주의사항**  
> 생성된 개인 키(`.secrets/opkg.sec`)는 절대로 Git에 커밋하지 마십시오. `.gitignore`에 등록되어 있는지 반드시 확인하세요.

```bash
mkdir -p .secrets

# SDK 경로 설정 (Pi 4 또는 Pi 5 SDK 중 하나 사용)
SDK="$HOME/sdk/pi5-bcm2712/openwrt-sdk-24.10.2-bcm27xx-bcm2712_gcc-13.3.0_musl.Linux-x86_64"

# usign 키 쌍 생성
"$SDK/staging_dir/host/bin/usign" -G \
    -s .secrets/opkg.sec \
    -p .secrets/opkg.pub \
    -c "LES Mesh Chat package feed"
```

---

## 2. GitHub 저장소 설정

### (1) GitHub Actions Secrets 등록
생성한 키를 GitHub Actions 시크릿에 등록합니다:

* **GitHub CLI 사용 시:**
  ```bash
  gh secret set OPKG_SIGNING_KEY < .secrets/opkg.sec
  gh secret set OPKG_SIGNING_PUBLIC_KEY < .secrets/opkg.pub
  ```
* **GitHub 웹 UI 사용 시:**  
  **Settings → Secrets and variables → Actions** 이동 후 `New repository secret` 클릭:
  * `OPKG_SIGNING_KEY`: `.secrets/opkg.sec` 파일의 전체 내용 (개인키)
  * `OPKG_SIGNING_PUBLIC_KEY`: `.secrets/opkg.pub` 파일의 전체 내용 (공개키)

### (2) GitHub Pages 설정
1. 저장소의 **Settings → Pages** 메뉴로 이동합니다.
2. **Build and deployment → Source**를 **GitHub Actions**로 선택합니다.

---

## 3. 릴리스 생성 및 배포 절차

### (1) 버전 일치 확인
릴리스 태그를 생성하기 전에 아래 3곳의 버전 정보가 일치하는지 확인합니다:

| 위치 | 파일 경로 | 확인 항목 |
|---|---|---|
| CMake | `CMakeLists.txt` | `project(... VERSION x.y.z)` |
| C++ 프로토콜 | `include/leschat/protocol.hpp` | `inline constexpr const char* app_version = "x.y.z";` |
| Git Tag | - | `vx.y.z` |

> 💡 **Tip:** 프로그램 코드는 그대로 두고 패키지 빌드 리비전만 올릴 때는 `openwrt/package/les-chatd/Makefile`의 `LESCHAT_RELEASE`를 증가시킵니다.

### (2) Git 태그 생성 및 푸시
```bash
RELEASE=0.1.18

# 로컬 커밋 및 태그 생성
git tag -a "v${RELEASE}" -m "LES Mesh Chat ${RELEASE}"

# 원격 푸시 (Actions 트리거)
git push origin main
git push origin "v${RELEASE}"
```

### (3) 배포 산출물 (Release Assets)

빌드가 완료되면 GitHub Release에 아래 파일들이 자동 생성됩니다:

| 산출물 파일명 | 설명 |
|---|---|
| `les-chatd_<ver>_aarch64_cortex-a72.ipk` | Raspberry Pi 4 백엔드 데몬 IPK |
| `les-chatd_<ver>_aarch64_cortex-a76.ipk` | Raspberry Pi 5 백엔드 데몬 IPK |
| `luci-app-les-chat_<ver>_all.ipk` | LuCI 웹 UI 통합 패키지 (공통) |
| `les-chat-offline-<ver>-aarch64_cortex-a72.tar.gz` | Pi 4용 완전 오프라인 설치 묶음 |
| `les-chat-offline-<ver>-aarch64_cortex-a76.tar.gz` | Pi 5용 완전 오프라인 설치 묶음 |
| `les-chat-meshgate-feed-<ver>.tar.gz` | MeshGate 로컬 opkg 피드 묶음 |
| `les-chat-windows-tools-<ver>.zip` | Windows 원클릭 설치/동기화 배치 도구 |
| `SHA256SUMS` | 모든 릴리스 산출물의 SHA-256 해시 목록 |

---

## 4. OpenWrt 장비에서 설치 및 업그레이드

인터넷이 연결된 OpenWrt 장비에서는 원격 피드를 통해 간편하게 설치할 수 있습니다.

### (1) 최초 원클릭 설치
```bash
wget -qO- https://YOUR_ACCOUNT.github.io/les-mesh-chat/install.sh | sh
```
이 스크립트는 장비의 아키텍처(Pi 4/5)와 OpenWrt 버전을 자동 감지하고, 공개키 등록 및 opkg 피드 추가 후 데몬과 LuCI 앱을 설치합니다.

### (2) 최신 버전 업그레이드
피드가 이미 등록되어 있다면 아래 명령어로 최신 릴리스로 업그레이드합니다:

```bash
opkg update
opkg upgrade les-chatd luci-app-les-chat
```

---

## 5. 추가 참고 문서

* [**오프라인 장비 및 MeshGate 배포 가이드**](OFFLINE_DISTRIBUTION.md): 외부 인터넷이 차단된 현장 장비 배포 방법
* [**OpenWrt 및 LuCI 상세 설정**](../openwrt/README.md): 방화벽, 인터페이스 및 LuCI 화면 설정
