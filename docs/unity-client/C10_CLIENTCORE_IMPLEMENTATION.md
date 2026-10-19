# C10 ClientCore 후보 구현

2026-10-19. 격리 전달 후보 `UnityClient/Builds/C10-Delivery/source`, 기준 `0e2adcf`. 제품 및 fixture **컴파일 완료**, 기능 시험·Unity 실행·독립 리뷰는 아직 미실행이다. 원본 C09 작업 폴더는 수정하지 않았다.

바꿀 동작은 현재 field 세션의 내 매물 읽기 요청과 응답 수명이다. 최소 수정 위치는 기존 Interfaces → NetworkService → Packet/Handler/TCPSocket → ViewModel 경계다. ViewData의 공유 목록 및 UI 이벤트를 재사용한다. 새로운 서비스·Manager·인터페이스 타입·타이머는 추가하지 않았다.

## 변경

- `ClientCore/Interfaces.cs`: UI `RequestBazaarMyList()`, network `RequestBazaarMyList(uint)`, 수신 `BazaarMyListReceived(uint, byte, BazaarListing[])`를 기존 인터페이스에 추가했다.
- `ClientCore/Services/NetworkService.cs`, `PacketHelper/Packet.cs`: TCP28 요청은 header8 + requestID u32 = 12바이트이고 소유자 ID 입력은 없다. TCP29 응답은 15 + 39×count, count0~30, flags0, requestID 비영, status0/1이며 실패는 count0이다. row 상태는 0/1/2를 허용하고 ID·수량 비영/매물 ID 중복/정확한 길이·header 일치를 검사한다. Search26/27의 상태0 제한은 유지한다.
- `ClientCore/Network/Handler.cs`, `TCPSocket.cs`: 29를 simulation 경로 전에 파싱하고 잘못된 패킷은 기존 socket disconnect 경계에 전달한다. 불완전 TCP frame 대기 및 합쳐진 frame 분리는 기존 경로를 재사용한다. ReceiveTick 변경은 필요하지 않았다.
- `ClientCore/ViewModel.cs`: Search/MyList가 단일 pending ID와 요청 종류를 함께 검사한다. ID는 취소 후에도 증가하며 uint 최대치에서 재사용하지 않는다. 수신 배열을 dispatcher 전에 복사하고 메인 스레드 반영 전 수명·world·종류·ID를 다시 확인한다. 기존 `CancelBazaar`, ZoneChange, Error/disconnect, Dispose 경계를 재사용한다. close/tab/timeout은 UI에서 `CancelBazaar`를 호출한다. 기존 `C09BazaarScreen.RequestTimeoutSeconds = 8f`를 확인했으며 별도 timeout은 만들지 않았다.
- `UnityClient/Assets/Tests/C02/C10BazaarMyListTests.cs`와 `.meta`: 새 fixture 11개 case를 작성했다. 0/1/30행·실패/빈 성공·엄격한 envelope/row·Handler routing·종류/ID 혼합·배열 snapshot·취소/Zone/disconnect/dispose·send 실패·ID exhaustion을 포함한다. SOLD/CLAIMED로 정산 가능성을 합성하지 않는다.
- 기존 C02EntryState/C04Observation/C05Combat/C06ChatPacket/C07Inventory/C07InventoryUi/C08Zone/C08ZoneTransition/C09Bazaar/C09BazaarUi fixture 10개의 INetworkService fake에 새 메서드 빈 stub만 추가했다. 기존 assertion과 시험 동작은 변경하지 않았다.

## 검증 상태

`dotnet build ClientCore/ClientCore.csproj --configuration Release --nologo` 성공, 오류0. 최초 `--no-restore`는 격리 후보의 assets 파일이 없어 NETSDK1004로 종료됐고 정상 restore/build로 해결했다. NU1900은 NuGet 취약성 데이터 endpoint 접근 실패이며 코드 컴파일 경고는 없다.

ignored 경로 `ClientCore/obj/C10Compile/C10Compile.csproj`에서 동일 ClientCore 소스와 실제 NUnit DLL, C08TestSession/C09BazaarTests/C10BazaarMyListTests를 **컴파일만** 수행해 오류0으로 성공했다. Unity DLL 교체나 Editor 조작은 하지 않았다. build summary는 `ClientCore/obj/C10Compile/build-summary.txt`에 있다. `git diff --check` 공백 오류0.

제품 출력 `ClientCore/bin/Release/netstandard2.0/ClientCore.dll` SHA256: `CCD88B33B6A9A4923AD48FE816A62B4688F38018CA7AB7A2BE21C9490623F473`. Unity Assets/Plugins DLL은 이 담당이 교체하지 않았다.

## 배치 인계

포함할 검증은 새 fixture 11 case와 기존 Search·세션 회귀, TCP 분할/합쳐진 frame 및 malformed disconnect, 실제 MARKET→내 매물→새로고침→판매 검색→닫기다. 기대 결과는 실제 서버 데이터와 빈 성공/실패 구분, 다른 종류·취소된 ID 응답 무시다. fixture 수초·실입력 수분 예상이며 전체 구현 동결과 독립 리뷰 뒤 총괄 배치에서 한 번 실행한다. 통과 뒤 새 변경·실패·미해결 위험이 없으면 반복하지 않는다.

실서버·DB·실입력·기능 fixture 실행0, 서버/서비스 조작0, Git commit/push/PR0, Slack 발신0이다. 서버 wire28/29 후보 및 UI 연결과 함께 독립 리뷰·배치 후 최종 결과를 판단한다. 복구는 기존 C09 서버/DLL 쌍으로 정상 교체하며 DB rollback은 필요하지 않다.
