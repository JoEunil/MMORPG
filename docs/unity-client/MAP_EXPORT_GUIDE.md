## 현재 경로 구분 — 2026-10-28

맵·구역·충돌과 스폰 배치는 아래 Unity→서버 경로를 유지한다. 스킬·몬스터 능력치는 서버 C++ Data→공통 JSON→ServerContentImport로 전환했고 옛 ServerContentExport는 제거했다. 두 소유권을 구분하며 [데이터 공유 경로](CONTENT_PIPELINE.md)를 따른다. 아래 과거 상태/검증 기록은 당시 근거다.

## 현재 Unity 경로 — 2026-10-27

현재 저작 컴포넌트는 Mmorpg.Client.MapAuthoring이며 씬은 Assets/Client/Scenes/World.unity이다. 데이터는 Assets/Client/Content, Exporter는 Assets/Client/Editor/WorldMapExport.cs·WorldLayoutExport.cs·MonsterSpawnExport.cs에 있다. 아래 과거 기록의 단계 번호 타입·경로는 당시 이름이며, 현재 구현은 기능명 경로를 사용한다. 좌표·출력 스키마·서버 소비 계약은 이번 코드 구조 정리로 변경하지 않는다.

## 최신 — 사용자 서버 AI 변경과 내보내기 구분 / 2026-10-23

사용자가 A* 없이 상태 머신 추상화·몬스터 Cell 간 이동·목표 도착 후 10~30틱 배회 대기·이동한 위치의 스킬 phase 시전을 변경했다고 알렸다. 배회 대기는 Wandering의 wanderPauseTicks이며 별도 WanderPhase enum은 없다. 아래 Cell 제한과 A* 후속 계획은 이전 Exporter 후보의 기록으로만 읽는다. 통행 데이터·AOI Cell·Zone 소유권은 계속 구분하며 사용자 변경을 덮어쓰지 않는다. 새 서버 소스 통합 및 실행 검증은 이번 지침 수정과 별개다.

## 최신 후보 — 서버 공간 설정도 Export에서 직접 사용 / 2026-10-22

통행 비트에 더해 Config의 구역 수와 Cell의 구역 크기·행열·중첩·AOI Cell 크기를 WorldMap.inc에서 파생한다. ZoneInit과 소유권 전환 경계는 exported Zones를 사용한다. Cell.h는 AOI 실행 컨테이너와 인접 송신 처리이므로 유지한다. nav tile과 AOI Cell은 별개다.

map-only public Export는 metadata 없는 파일을 쓰기 전에 예외로 중단한다. Export(map, author, outputDirectory) 또는 BakeAndExport(author, outputDirectory)를 사용해 완전한 snapshot 하나를 내보낸다. 산출물을 수작업으로 고치지 않는다. 내보낸 체크아웃에서 서버 재빌드·정상 전환이 필요하며 서버 hot reload는 없다.

현재 실행 콘텐츠는 6구역이다. 기존 몬스터 배치의 6구역 밀도 guard는 유지해 9구역 콘텐츠 활성화를 완료한 것으로 주장하지 않는다. 사용자 AI/A*와 Cell 이동 제한은 수정하지 않는다. 새 후보 서버 빌드와 실제 Unity 진입, 전 구역/AOI 일치 fixture 통과. FEEL02_RESULTS.txt에 근거를 기록했다.

## 최신 — Unity 입력 경로도 확인 / 2026-10-22

한 번의 실제 접속에서 InputSystem 방향키로 1→2→1 전환과 첫 상태 후 조작 복귀·벽 앞 정지를 확인했다. 실제 마우스로 로그인/선택/진입/퇴장했고 I/B/C 입력과 실제 서버 상태도 확인했다. MAP_UNITY_INPUT_RESULTS.txt 참조. 이전 입력 보류/미실행 설명은 과거 기록이다. 이웃 Zone 합동 AOI·모든 코너·지연별 시각 보정과 몬스터 AI의 검증으로 확대하지 않는다.

## 최신 실행 결과 — 2026-10-22 서버 통합 완료

실행 중인6구역 서버에서 내보낸 장애물의 통행 차단과 Zone1→2→1 자동 소유권 전환을 공개 ClientCore 요청으로 확인했다. 각 전환의 증가 epoch와 첫 Player/Monster Full 쌍을 확인했고 정상퇴장 후 저장도 확인했다. 상세는 MAP_SERVER_COMPLETION_RESULTS.txt. 서버/wire 추가변경은 없고 C02 로컬 경계 입력 목표만 보완했다.

이 서버 probe 이후 Unity 입력 배치도 완료했다(MAP_UNITY_INPUT_RESULTS.txt). 두 결과는 별도 근거이며 서버 probe를 화면 검증으로 대신하지 않는다. AI/A*와 몬스터 Cell 이동 제한은 사용자 후속 작업이다. 활성6구역과9구역 내보내기 지원, owner 전환과 이웃 Zone 합동 AOI를 구분한다.

## 마을·사망 귀환 실행 계약 — 2026-10-22

현재 6구역 중 Zone 1은 마을이며 MonsterSpawns.inc의 마을 밀도는 반드시 0이다. 일반·강화·보스 생성 없이 시작하고 나머지 구역은 기존 배치를 유지한다. 사망자가 명시적으로 귀환하면 서버가 Zone 1 전역 좌표 (10,10)의 통행 가능 여부와 HP0·소유권을 확인하고 HP/MP를 회복한다. 새 opcode 39 때문에 서버/DLL을 함께 전환해야 한다. STOP01 승인 후 새 마을 서버로 정상 전환했고 실제 HP0 캐릭터가 사냥터에서 마을로 귀환해 HP/MP 최대·EXP 보존·관측 몬스터0을 확인했다. TOWN_RESPAWN_LIVE_RESULTS.txt를 따른다. 몬스터 AI/A*나 전체 생존 경계 이동 검증은 별개다.

## PR45 리뷰 보완 — Dummy 도구의 동일 wire 사용

DummyClients도 ACTION26/ownershipEpoch와 확장 구역 응답·첫 Full 쌍을 사용한다. 서버/ClientCore 규약을 낮추지 않으며 예전18바이트 Dummy 행동 패킷은 현재 후보에서 지원하지 않는다. 안전한 네트워크 없는 검증은 DummyClients의 --protocol-self-test 모드다. 인자가 없는 기존5000클라이언트 부하 모드는 이번에 실행하지 않았다. 실제 처리량은 별도 서버 지표와 함께 확인해야 한다. PR45_REVIEW_RESULTS.txt 참조.

## 실행 상태 — 2026-10-22

호환되는 MainServer와 ClientCore로 실제 접속·사망 귀환·HP/MP 회복·정상 퇴장 및 저장을 확인했다(TOWN_RESPAWN_LIVE_RESULTS.txt). 이후 생존 벽 충돌과 자동 경계 왕복도 확인해 Map Exporter·서버 연결 묶음을 완료했다(MAP_UNITY_INPUT_RESULTS.txt). FEEL02_RESULTS.txt는 후속 이동·모션 후보의 별도 관찰이다. 이웃 Zone 합동 AOI·Scene streaming·사용자 A*는 별도 범위다.

# Unity 맵 내보내기와 서버 연결

이 문서는 현재 구현과 서버 변경 후보를 구분한다. 몬스터 A* 및 Cell 이동 제한 변경은 포함하지 않는다.

## 현재 코드에서 확인한 구조

- 활성 월드는 3열×2행의 6구역이다. 과거 3×3 기억과 다르다. 구역 번호는 아래 행 1·2·3, 위 행 4·5·6이다.
- 이 3D 클라이언트의 이전 구역 이동 버튼 요청을 제거했고 서버가 검증한 이동으로 구역 전환을 결정한다. 현재 활성 클라이언트에는 구역 소유권을 바꾸는 Trigger나 방향 전환 버튼이 없다.
- AOI는 구역당 5×5, 한 Cell은 42단위다. 1단위 NavigationGrid는 이미 별도로 존재하며 이번에도 재사용한다.
- 현재 게임은 하나의 persistent Unity Scene과 글로벌 X/Z 좌표를 쓴다. Additive Scene 스트리밍이나 이웃 Zone 소유 엔티티의 합동 AOI를 이미 구현한 것으로 설명하지 않는다.

## 좌표와 중첩

Unity X/Z는 서버 X/Y에 대응한다. 기존 글로벌 wire 좌표를 일괄 로컬 좌표로 바꾸지 않고, Export 데이터에 명시적인 구역 로컬 변환을 추가했다. 로컬 원점은 중첩 영역까지 포함한 좌측 하단이다.

| 항목 | 현재 값 |
| --- | --- |
| 구역 core 크기 | 200×200 |
| 구역 전체 크기 | 210×210 |
| 구역 원점 간격 | 200 |
| 인접 구역 중첩 폭 | 10 (양쪽 여유 5) |
| 구역 1의 월드 원점 | (-5, -5) |
| 구역 2의 월드 원점 | (195, -5) |
| 9구역 예시의 구역 9 원점 | (395, 395) |
| 로컬 좌표 → 월드 좌표 | ZoneOrigin + LocalPosition |
| 월드 좌표 → 로컬 좌표 | WorldPosition - ZoneOrigin |

사각형은 `[최솟값, 최댓값)`이다. OwnershipCore는 명목상 200×200 구간이고 ExclusiveBounds는 이웃과 공유하지 않는 영역이다. 실제 소유권은 현재 owner의 중첩 끝까지 유지하는 hysteresis 규칙으로 정한다. 이 세 경계를 같은 것으로 해석하지 않는다.

AOI Cell(0,0)은 구역 로컬 좌하단이다. 25개 사각형은 중첩을 포함한 210을 5등분하며, Cell(4,4)의 로컬 시작은 (168,168)이다. 길찾기에 이 25개 Cell을 사용하지 않는다.

## 에디터 사용

1. C02 Scene에서 C02World/C20FixedWorldMap의 C20MapAuthoring을 선택한다. 실제 게임 입력이나 Play는 필요 없다. 메뉴와 Inspector 모두 해당 오브젝트가 속한 Scene을 저장해야 내보낼 수 있다. 다른 Scene이 활성화돼 있어도 검사 대상은 저작 Scene이다. NavigationGrid도 프로젝트에 저장된 에셋이어야 한다.
2. Ground와 Obstacles에 내보낼 정적 BoxCollider만 명시한다. 장식 풀과 이동 엔티티는 제외한다. Actor Clearance는 반지름에 대한 보수적 여유다.
3. Unity 저작값인 Zone Columns/Rows, Core Size, Transition Margin, Content Version을 확인한다. 서버 공간 값은 이 출력에서 파생한다. Preview Zone Id는 검토할 구역을 고른다. 출력에서는 다른 구역을 제거하지 않고 전체 레이아웃을 하나의 일관된 snapshot으로 저장한다.
4. Scene Gizmo: 파랑은 구역 전체, 초록은 전용 영역, 주황은 중첩, 흰색은 선택 구역의 AOI25, 빨강은 로컬 원점이다. 저장된 NavigationGrid 보기 버튼은 통행 배열 미리보기로 이동한다. 씬을 바꾼 뒤에는 이전 bake와 다를 수 있다.
5. Scene을 저장하고 전체 맵 검증 및 내보내기 버튼 또는 C02 > World > Bake and Export Navigation 메뉴를 실행한다.

출력은 `CoreLib/Generated/WorldMap.inc` 한 파일이다. nav와 layout을 따로 덮어쓰지 않는다. 전 데이터를 준비한 뒤 임시 파일을 교체하며 실패하면 이전 통행 배열/dirty 및 기존 출력이 유지된다. 프로세스 중단과 Unity 자산 저장까지 하나의 트랜잭션으로 보장하는 것은 아니다.

3×3 예시를 만들려면 Columns=3, Rows=3으로 지정하고 바닥/NavigationGrid도 610×610으로 맞춘다. 현재 서버는 여전히 6구역이므로 예시 출력만으로 9구역 서버가 활성화되지는 않는다.

## 출력과 서버 사용

FormatVersion과 ContentVersion, ZoneColumns/Rows, ZoneCoreSize, TransitionMargin, 각 구역의 origin/local bounds/core/exclusive bounds/8방향 이웃·중첩 사각형/AOI25, ActorClearance, 정적 장애물 AABB 모서리, 기존 packed WalkableBits를 포함한다. 콘텐츠 변경을 배포할 때 Content Version도 함께 올린다. ZoneEpoch는 네트워크 소유권 세대이며 콘텐츠 버전과 별개다.

`neighborIds`는 공간적으로 인접한 구역이다. `directCrossings`는 현재 owner의 해당 경계·모서리를 직접 통과할 수 있는지 구분한다. 모서리만 막혀 있어도 기하학 이웃은 남으며 다른 직교 구역을 경유할 수 있다. 직교 이웃 사이 중첩을 가로지르는 통로가 전혀 없으면 내보내기를 거부한다. 연결 검사는 Export 때의 작은 BFS이며 몬스터 AI가 아니다.

서버는 Unity 없이 C++ header로 읽는다. 기존 `WorldMap::IsWalkable` 및 `CanTraverse`는 글로벌 좌표를 받는다. A*가 구역 로컬 좌표를 쓰면 해당 구역 origin을 더한 뒤 조회한다. 내비게이션의 원점/타일 크기/가로·세로와 bit 인덱스는 AOI Cell과 독립적이다.

장애물 bake는 작은 장애물도 타일 사각형과 교차하면 막고, actor clearance를 더한다. 회전한 BoxCollider는 월드 AABB로 보수적으로 막으므로 실제 외형보다 넓게 차단할 수 있다. 원형 캐릭터의 정확한 연속 충돌 메시나 동적 장애물 처리라고 주장하지 않는다.

## 생성한 데이터와 검증 상태

현재 6구역의 실제 정적 맵 클론에서 출력했고 기존 nav 비트와 일치함을 확인했다. 별도의 9구역 예시는 복제 바닥을 610×610으로 늘려 생성했으며 기존 벽18개만 포함한다. 새 3번째 행의 몬스터 배치/실행 서버는 만들지 않았다.

독립 리뷰와 직접 영향 검증에서 정상 출력/좌표·AOI·장애물, readonly 실패 보존, 중첩 전체 막힘 거부·대각 직접 통행 구분을 확인했다. 최초 출력 FormatException과 std::array 오류는 수정·실패 기록을 보존했고 6/9 출력 모두 C++ 컴파일·실행을 통과했다. 해당 exporter 코드를 변경하지 않은 후속 작업에서 이 검증을 반복하지 않는다.

서버 자동 전환·첫 Full 동기화도 구현하고 직접 영향 자동 검증을 통과했다(WORLD_EXPORT_TRANSITION_RESULTS.txt). 생성물을 사용하는 서버에서 실제 귀환과 벽 정지·1→2→1 이동·조작 복귀까지 확인했다(TOWN_RESPAWN_LIVE_RESULTS.txt, MAP_UNITY_INPUT_RESULTS.txt). 현재 이동 수치와 후속 실입력 결과는 FEEL02_RESULTS.txt를 따른다. 이 근거는 모든 모서리·가변 지연·새 몬스터 교전의 검증을 뜻하지 않는다.

## 몬스터 AI 인계 — 내보내기 당시 기록

Map Exporter와 이번 마무리는 AI 상태 머신을 변경하지 않는다. 앞선 마을 작업에서 `ZoneState_Monsters.cpp`의 초기 스폰만 마을0으로 제한한 것과 AI 개편을 구분한다. 기존 추적은 현재 Cell의 area를 넘으면 aggro를 풀고, 배회도 해당 Cell과 spawn 반경 안으로 제한한다. 내비게이션 선분 검사는 이미 사용 중이지만 A* 탐색은 없다.

통행 데이터 조회에는 WorldMap의 통행 bits/IsWalkable/CanTraverse를 사용하고, 구역 로컬 경로를 쓰면 ZoneOrigin을 더해 글로벌 좌표로 조회한다. Cell 밖으로 이동시키려면 좌표뿐 아니라 m_cells의 monsterIndexes 및 monster.cellX/cellY 소속을 함께 갱신해야 한다. AOI Cell을 A* 노드로 사용하지 않는다.

ActorClearance는 현재 한 가지 값으로 전체 통행 격자에 적용한다. 몬스터마다 다른 반지름을 지원하는 다중 통행 격자를 이번 작업에서 추가한 것은 아니다. 큰 모델을 위한 여유는 후속 AI/콘텐츠 작업에서 명시적으로 결정한다.

서버 Config.h의 ZONE_COUNT와 Cell.h의 core 크기·행열·중첩·AOI Cell 크기는 생성 데이터에서 직접 파생한다. 별도 서버 공간 상수를 수작업으로 맞추지 않는다. Format 1의 AOI 5×5 컨테이너와 콘텐츠 검증은 유지한다. 현재 몬스터 밀도 데이터와 ZoneState_Monsters.cpp의 guard는 6구역용이므로 9구역 예시만 교체하면 서버 빌드가 거부된다. 9구역 활성화는 해당 콘텐츠와 소비자를 함께 준비할 별도 작업이다.

새 wire는 이전 서버와 호환되지 않아 MainServer와 ClientCore DLL을 같은 버전으로 전환했다. 맵 PR45는 master에 병합됐고 관련 마을·이동 보완은 master 기준 PR46에 모았다. 원본 Game의 별도 미완료 AI 소스는 보존한다. DB/계정/아이템 초기화나 직접 HP 복구는 하지 않았고, 실제 사망 귀환의 회복·위치 저장은 정상 게임 경로의 결과다.

이웃 Zone이 소유한 엔티티를 경계 너머로 함께 표시하는 기능은 남아 있다. 현재 owner-only AOI/클라이언트 필터/Zone-local ID를 함께 바꿔야 하므로 이웃의 Full만 추가 송신하면 안 된다. 사용자 몬스터 AI의 A* 연결과도 구분해 후속 작업으로 다룬다.
