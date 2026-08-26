# Grok 인수인계: LES Mesh Chat LuCI UI 디자인 개선

> 이 문서는 v0.1.8 당시의 UI 디자인 인수인계 기록입니다. 현재 공개 버전은
> v0.1.15이며, Chat/Peers/Status/Settings는 LuCI native 화면으로 구현되어
> 있습니다. 아래의 iframe 설명과 디자인 목표는 변경 이력과 향후 개선 아이디어를
> 구분해서 읽어야 하며, 현재 설치·배포 절차는 `README.md`와 `docs/`를 기준으로
> 합니다.

## 1. 목표

OpenMANET LuCI 안에서 보이는 LES Mesh Chat 화면의 사용성과 시각 디자인을 개선한다.
현재 기능은 정상 동작하므로, 네트워크 프로토콜과 메시지 저장 기능을 변경하지 않고 UI/UX를 개선하는 것이 목표다.

이 문서가 작성될 당시 `Chat` 메뉴가 기존 채팅 웹 UI를 LuCI 페이지 안의
`iframe`으로 표시해서 화면이 어색하고 LuCI와 분리된 느낌이 났다. 현재는
native LuCI 화면으로 교체되었으므로, 이후 개선에서는 이 통합 구조를 유지한다.

## 2. 당시 구조와 현재 구조

- 저장소: `kodu1107/les-mesh-chat`
- 당시 기준 버전: `v0.1.8` (현재 공개 버전: `v0.1.15`)
- OpenWrt 대상: Raspberry Pi 4/5 OpenMANET
- LuCI 패키지: `openwrt/package/luci-app-les-chat/`
- 서비스 데몬: `les-chatd`
- 기본 HTTP 포트: `7777`
- LuCI 자체 주소: `http://<node-ip>/cgi-bin/luci`
- 채팅 API/웹 UI: `http://<node-ip>:7777/`

### LuCI 화면 파일

- `files/www/luci-static/resources/view/les_chat/chat.js`
- `files/www/luci-static/resources/view/les_chat/peers.js`
- `files/www/luci-static/resources/view/les_chat/status.js`
- `files/www/luci-static/resources/view/les_chat/settings.js`
- `files/usr/share/luci/menu.d/luci-app-les-chat.json`
- `files/usr/libexec/rpcd/luci.leschat`

v0.1.8 당시 `chat.js`는 대략 다음 방식이었다.

```javascript
E('iframe', {
    'src': url,
    'title': 'LES Mesh Chat',
    'style': 'width:100%;height:70vh;border:1px solid #555;background:#000'
})
```

## 3. 반드시 유지해야 하는 기능

1. `les-chatd` 실행 방식과 UCI 설정 형식을 깨뜨리지 않는다.
2. 기존 채팅 웹 UI의 API 호환성을 유지한다.
3. 다음 API를 사용하는 현재 기능을 유지한다.
   - `GET /healthz`
   - `GET /api/v1/status`
   - `GET /api/v1/peers`
   - `GET /api/v1/messages`
   - 기존 메시지 전송 API
4. 피어 자동 발견, 메시지 복제, SQLite 저장, 10MB 저장 정책을 변경하지 않는다.
5. `node_id`와 `callsign` 설정은 반드시 유지한다.
6. 모바일 화면과 작은 OpenWrt 화면에서도 사용 가능해야 한다.
7. 외부 CDN, 인터넷 폰트, 대형 프론트엔드 프레임워크 의존성을 추가하지 않는다.
8. LuCI 기본 테마와 충돌하지 않도록 CSS를 범위 제한한다.
9. 저사양 Raspberry Pi에서 빠르게 렌더링되어야 한다.
10. 기존 패키지 설치 방식과 `opkg` feed 배포를 깨뜨리지 않는다.

## 4. 원하는 디자인 방향

- 무전기/메시 네트워크 도구처럼 신뢰감 있고 명확한 인터페이스
- 현재 연결 상태, 내 호출부호, 피어 수를 한눈에 표시
- 메시지 목록은 긴 메시지 때문에 전체 창이 계속 늘어나지 않고 내부 스크롤 사용
- 송신자와 수신자 메시지를 명확히 구분
- 피어가 없는 경우, 데몬이 꺼진 경우, 네트워크가 끊긴 경우의 빈 상태 화면 제공
- `Bolt`, `Kilo` 같은 callsign은 눈에 잘 띄되 `node_id`는 보조 정보로 표시
- 위험한 설정(`bind`, 포트, discovery interface)은 일반 사용자에게 과도하게 노출하지 않거나 고급 설정으로 분리
- 설정 저장 중/재시작 중/실패 시 상태를 명확히 표시
- 색상만으로 상태를 구분하지 말고 텍스트와 아이콘을 함께 사용

## 5. 권장 화면 구조

### Chat

- 상단: 서비스 상태, 내 callsign, 연결된 피어 수
- 중앙: 고정 높이 메시지 스크롤 영역
- 하단: 메시지 입력창과 전송 버튼
- 새 메시지 도착 시 하단 고정 또는 “새 메시지” 표시
- 긴 텍스트와 한글 줄바꿈 지원

### Peers

- 피어별 callsign, node ID, IP, 마지막 확인 시간, 연결 상태
- 피어가 없을 때 안내 문구와 점검 명령 표시

### Status

- 서비스 상태
- Node ID
- Callsign
- HTTP 포트
- discovery interface/address/port
- 데이터베이스 경로 및 저장 상태

### Settings

- 기본: Enabled, Node ID, Callsign
- 네트워크 고급 설정: bind, HTTP port, discovery address/interface/port
- 저장소 고급 설정: database path
- 저장 전 확인 및 저장 후 재시작 상태 표시

## 6. Grok에게 요청할 작업

다음 중 하나를 명확히 선택해서 작업한다.

### A. 디자인 제안만 하는 경우

- 화면별 와이어프레임 또는 목업
- 색상/간격/타이포그래피/상태 표현 규칙
- 현재 iframe을 유지할지, LuCI 네이티브 화면으로 교체할지 근거
- 구현에 필요한 파일 목록과 변경 계획
- 데스크톱/모바일 화면 예시

### B. 실제 코드까지 수정하는 경우

- 변경된 파일 전체 또는 unified diff 제공
- LuCI JS/CSS 변경
- API 계약 변경이 있다면 이유와 호환성 설명
- 빌드/설치/검증 명령 제공
- 기존 채팅 기능을 보존했는지 확인
- `node_id`/`callsign` 설정 동작을 직접 검증

가능하면 `iframe`을 단순히 꾸미는 수준보다, 기존 API를 호출하는 LuCI 네이티브 Chat 화면을 우선 검토한다. 단, 현재 채팅 웹 UI의 기능을 빠뜨리지 않는 경우에만 교체한다.

## 7. Grok에 보낼 프롬프트

아래 내용을 그대로 전달한다.

```text
너는 OpenWrt LuCI와 저사양 임베디드 장치 UI를 설계하는 시니어 프론트엔드 디자이너/개발자다.

GitHub 저장소 kodu1107/les-mesh-chat의 v0.1.15 LuCI 앱을 개선해줘.
현재 Chat 메뉴는 les-chatd API를 호출하는 native LuCI 화면이다. 과거에는
http://<node-ip>:7777/ 채팅 웹 UI를 LuCI 페이지 안 iframe으로 표시했으며,
그 구조가 LuCI와 분리되어 보였기 때문에 native 통합 화면으로 교체되었다.

목표:
1. Chat, Peers, Status, Settings 화면을 일관된 디자인으로 만든다.
2. 메시지 영역은 내부 스크롤을 사용하고 긴 메시지 때문에 페이지 전체가 늘어나지 않게 한다.
3. 내 callsign, node_id, 데몬 상태, 피어 수를 명확히 표시한다.
4. 모바일/작은 화면과 저사양 Raspberry Pi에서 사용 가능하게 한다.
5. 기존 API와 기능을 유지한다. API는 /healthz, /api/v1/status, /api/v1/peers,
   /api/v1/messages 및 기존 메시지 전송 API다.
6. UCI 설정 les-chat.main의 enabled, node_id, callsign, bind, port,
   discovery_address, discovery_interface, discovery_port, database를 유지한다.
7. 외부 CDN과 대형 프레임워크를 사용하지 않는다. LuCI 기본 컴포넌트와 순수 JS/CSS를 우선 사용한다.

먼저 다음을 제시해줘:
- iframe을 유지하는 안과 LuCI 네이티브 화면으로 교체하는 안의 장단점
- 추천안
- 화면별 와이어프레임/상태 정의
- 변경 파일 목록

그 다음 실제 구현이 가능하도록 다음 중 하나를 제공해줘:
- 디자인만이면 상세 목업/스타일 규칙과 구현 명세
- 코드까지면 각 파일의 unified diff 또는 교체 가능한 전체 파일

반드시 다음도 포함해줘:
- 데스크톱과 모바일 레이아웃
- 피어 없음/서비스 중지/오류/재연결 상태
- 긴 메시지와 한글 줄바꿈 처리
- 저장 중 및 les-chatd 재시작 중 표시
- 기존 기능을 깨뜨리지 않는 검증 체크리스트
```

## 8. Grok 결과를 받은 뒤 Codex에서 할 일

Grok이 결과를 주면 이 저장소에서 다음 순서로 진행한다.

1. Grok 결과를 새 파일이나 패치 형태로 저장한다.
2. Codex에게 다음을 함께 전달한다.
   - Grok의 디자인 설명
   - 변경 파일 또는 diff
   - 사용한 스크린샷/목업
   - iframe 유지 여부
3. Codex가 코드 리뷰 후 저장소에 적용한다.
4. CMake 빌드와 CTest를 실행한다.
5. LuCI 패키지와 `les-chatd` IPK를 다시 빌드한다.
6. MeshGate feed에 패키지를 반영한다.
7. Bolt에서 feed를 동기화하고, Kilo에서 설치/업데이트한다.
8. 두 노드에서 다음을 확인한다.

```sh
/etc/init.d/les-chatd status
wget -qO- http://127.0.0.1:7777/healthz
wget -qO- http://127.0.0.1:7777/api/v1/status
wget -qO- http://127.0.0.1:7777/api/v1/peers
```

9. 브라우저에서 다음을 확인한다.

- `Services → LES Mesh Chat → Chat`
- `Services → LES Mesh Chat → Peers`
- `Services → LES Mesh Chat → Status`
- `Services → LES Mesh Chat → Settings`
- 긴 메시지 스크롤
- 양방향 메시지 송수신
- 새로고침 후 메시지 표시
- 설정 저장 후 데몬 재시작

10. 최종 검증이 끝난 뒤에만 커밋/태그/푸시 여부를 결정한다. 사용자 승인 없이 커밋이나 푸시는 하지 않는다.

## 9. Codex에 다시 보낼 때 사용할 문장

```text
Grok에서 LES Mesh Chat LuCI UI 디자인을 받았습니다.
첨부한 디자인 설명과 diff를 검토하고 현재 저장소에 적용해줘.
기존 API, UCI 설정, 양방향 채팅, 피어 발견, 패키지 배포를 깨뜨리지 말고,
적용 후 CMake 빌드, CTest, LuCI 패키지 빌드 검증까지 진행해줘.
변경 파일과 테스트 결과를 요약하고, 커밋/푸시는 내 승인 전에는 하지 마.
```
