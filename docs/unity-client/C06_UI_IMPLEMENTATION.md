# C06 채팅 UI 구현

기존 C02 Canvas에 좌측 하단 Global 채팅 패널을 추가한다. `C06ChatSetup.Apply()`는 현재 C02 씬에서만 기존 Canvas/월드/로그인 객체를 검사하고 UI 자식과 C06 컴포넌트만 생성·연결한다. 전체 씬 빌더를 다시 실행하지 않는다.

`C06ChatScreen`은 월드 진입 중에만 입력을 켜고, 실제 `OnChatReceived` 이벤트로 받은 Global 메시지만 최근 12줄에 표시한다. 보낸 내용을 로컬 에코로 추가하지 않는다. 이름이 비어 있으면 sender chat ID로 대체한다. 송신은 trim, 공백 전용/NUL/줄바꿈/UTF-8 128바이트 제한 후 `Chat(message, 0, 0)`을 호출한다. 호출 직후에는 서버 echo 대기라고 표시하며 전달 완료로 간주하지 않는다. UI와 표시 본문은 TMP rich text를 끈다.

한국어 표시는 로컬 Windows의 Malgun Gothic을 TMP DynamicOS 글꼴로 만들고 이 패널에만 적용한다. OS에 해당 글꼴이 없거나 TMP가 DynamicOS를 제공하지 않는 환경에서는 Liberation Sans로 돌아가므로 한국어 글리프가 보이지 않을 수 있다. 글꼴 파일을 복사하거나 전역 TMP 설정을 바꾸지 않는다.

아직 Unity Editor 설정 실행, 컴파일, Play, 입력, 서버 송수신 검증은 하지 않았다. 기능 검증은 전체 C06 작업 단위의 리뷰가 끝난 후 Coordinator가 한 번의 배치로 수행한다.
