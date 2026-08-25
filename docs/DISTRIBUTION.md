# GitHub와 opkg 피드 배포

이 저장소는 `v*` 태그가 GitHub에 올라오면 Raspberry Pi 4와 Pi 5용 IPK를
각각 빌드하고, GitHub Release와 서명된 GitHub Pages opkg 피드를 함께
갱신하도록 구성되어 있습니다. 릴리스에는 인터넷이 없는 장비를 위한 아키텍처별
오프라인 묶음, 공용 MeshGate 피드 묶음, Windows 원클릭 도구도 포함됩니다.

## 1. GitHub 저장소 준비

GitHub에서 빈 저장소를 만든 뒤 로컬 저장소의 원격 주소로 등록합니다. 아래
주소는 본인의 계정과 저장소 이름으로 바꿉니다.

```bash
git remote add origin git@github.com:YOUR_ACCOUNT/les-mesh-chat.git
```

공개 저장소가 설치와 소스 검토에는 가장 간단합니다. 비공개 저장소도 가능하지만
GitHub Pages 공개 범위와 설치 시 인증 방법을 별도로 설계해야 합니다.

## 2. opkg 서명 키 생성

둘 중 한 OpenWrt SDK의 `usign`으로 키 쌍을 한 번만 생성합니다. 개인 키는
분실하면 같은 피드의 업데이트를 서명할 수 없으므로 안전한 별도 저장소에
백업하고 Git에는 절대 추가하지 않습니다.

```bash
mkdir -p .secrets

SDK="$HOME/sdk/pi5-bcm2712/openwrt-sdk-24.10.2-bcm27xx-bcm2712_gcc-13.3.0_musl.Linux-x86_64"

"$SDK/staging_dir/host/bin/usign" -G \
    -s .secrets/opkg.sec \
    -p .secrets/opkg.pub \
    -c "LES Mesh Chat package feed"
```

`.secrets/`는 `.gitignore`에 포함되어 있습니다. 그래도 `git status`에서 키가
추적되지 않는지 반드시 확인합니다.

## 3. GitHub Actions 시크릿 등록

GitHub CLI에 로그인했다면 저장소 루트에서 다음과 같이 등록할 수 있습니다.

```bash
gh secret set OPKG_SIGNING_KEY < .secrets/opkg.sec
gh secret set OPKG_SIGNING_PUBLIC_KEY < .secrets/opkg.pub
```

웹에서는 **Settings → Secrets and variables → Actions**에서 같은 두 이름으로
등록합니다. `OPKG_SIGNING_KEY`는 개인 키이고,
`OPKG_SIGNING_PUBLIC_KEY`는 장비가 패키지 인덱스를 검증하는 공개 키입니다.

## 4. GitHub Pages 설정

저장소의 **Settings → Pages → Build and deployment → Source**를
**GitHub Actions**로 선택합니다. 배포 워크플로는 별도의 Pages 브랜치를 만들지
않고 Actions가 생성한 사이트를 바로 배포합니다.

## 5. 첫 릴리스

릴리스 태그를 만들기 전에 다음 세 버전이 일치하는지 확인합니다.

- `CMakeLists.txt`의 프로젝트 버전
- `include/leschat/protocol.hpp`의 `app_version`
- 태그 이름에서 `v`를 뺀 버전

같은 프로그램 버전의 패키지만 다시 만들 때는
`openwrt/package/les-chatd/Makefile`의 `LESCHAT_RELEASE`를 올립니다. 프로그램
버전을 올리면 일반적으로 패키지 release를 `1`로 되돌립니다.

검증 후 변경사항을 커밋하고 원격에 올리는 작업은 직접 승인한 시점에 수행합니다.
그 다음 첫 태그의 예시는 다음과 같습니다.

```bash
git tag -a v0.1.3 -m "LES Mesh Chat v0.1.3"
git push origin main
git push origin v0.1.3
```

태그 워크플로가 끝나면 **Actions**, **Releases**, **Pages**에서 각각 결과를
확인합니다.

## 6. OpenWrt 장비에서 한 번에 설치

GitHub Pages 주소의 계정과 저장소 이름을 바꿔 실행합니다.

```bash
wget -qO- https://YOUR_ACCOUNT.github.io/les-mesh-chat/install.sh | sh
```

이 설치기는 OpenWrt `24.10.2`와 Pi 4/Pi 5 패키지 아키텍처를 확인하고,
공개 키와 feed를 등록한 후 `les-chatd`를 설치 또는 업그레이드합니다. 이후에는
다음 명령만으로 배포된 최신 패키지를 적용할 수 있습니다.

```bash
opkg update
opkg upgrade les-chatd
```

설치 후 장비별 호출 부호와 메시 네트워크 방화벽 zone 설정은
[`openwrt/README.md`](../openwrt/README.md)를 따릅니다.

외부 인터넷이 없는 OpenMANET 장비에 Windows 노트북이나 공용 MeshGate를 통해
배포하는 절차는 [`OFFLINE_DISTRIBUTION.md`](OFFLINE_DISTRIBUTION.md)를
따릅니다.
