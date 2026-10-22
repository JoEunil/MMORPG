## PR45 리뷰 보완 — Dummy 도구의 동일 wire 사용

DummyClients도 ACTION26/ownershipEpoch와 확장 구역 응답·첫 Full 쌍을 사용한다. 서버/ClientCore 규약을 낮추지 않으며 예전18바이트 Dummy 행동 패킷은 현재 후보에서 지원하지 않는다. 안전한 네트워크 없는 검증은 DummyClients의 --protocol-self-test 모드다. 인자가 없는 기존5000클라이언트 부하 모드는 이번에 실행하지 않았다. 실제 처리량은 별도 서버 지표와 함께 확인해야 한다. PR45_REVIEW_RESULTS.txt 참조.

## 실행 상태 — 2026-10-22

같은후보 MainServer와ClientCore로 실제UI접속/HP0필드진입·조작차단·정상퇴장을 확인했다. HP복구는 하지 않았으며 생존이동/자동구역경계 확인은 남아 있다. 아래의 미기동/입력보류 설명은 최초검증시점 기록이며 최신결과는 MAP01_LIVE_RESULTS.txt를 따른다.

# Unity 맵 내보내기와 서버 연결

이 문서는 현재 구현과 서버 변경 후보를 구분한다. 몬스터 A* 및 Cell 이동 제한 변경은 포함하지 않는다.

## 현재 코드에서 확인한 구조

- 활성 월드는 3열×2행의 6구역이다. 과거 3×3 기억과 다르다. 구역 번호는 아래 행 1·2·3, 위 행 4·5·6이다.
- Unity는 현재 구역 이동 버튼으로 방향 op를 보냈다. 이 3D 클라이언트에 구역 전환 Trigger는 없었다. 이번 후속에서 버튼 요청을 없애고 서버가 검증한 이동으로 전환하도록 바꾸고 있다.
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
3. Zone Columns/Rows, Core Size, Transition Margin, Content Version을 확인한다. Preview Zone Id는 검토할 구역을 고른다. 출력에서는 다른 구역을 제거하지 않고 전체 레이아웃을 하나의 일관된 snapshot으로 저장한다.
4. Scene Gizmo: 파랑은 구역 전체, 초록은 전용 영역, 주황은 중첩, 흰색은 선택 구역의 AOI25, 빨강은 로컬 원점이다. 저장된 NavigationGrid 보기 버튼은 통행 배열 미리보기로 이동한다. 씬을 바꾼 뒤에는 이전 bake와 다를 수 있다.
5. Scene을 저장하고 전체 맵 검증 및 내보내기 버튼 또는 C02 > World > Bake and Export Navigation 메뉴를 실행한다.

출력은 `CoreLib/Generated/WorldMap.inc` 한 파일이다. nav와 layout을 따로 덮어쓰지 않는다. 전 데이터를 준비한 뒤 임시 파일을 교체하며 실패하면 이전 통행 배열/dirty 및 기존 출력이 유지된다. 프로세스 중단과 Unity 자산 저장까지 하나의 트랜잭션으로 보장하는 것은 아니다.

3×3 예시를 만들려면 Rows=3뿐 아니라 바닥/NavigationGrid도 610×610으로 맞춘다. 현재 서버는 여전히 6구역이므로 예시 출력만으로 9구역 서버가 활성화되지는 않는다.

## 출력과 서버 사용

FormatVersion과 ContentVersion, ZoneColumns/Rows, ZoneCoreSize, TransitionMargin, 각 구역의 origin/local bounds/core/exclusive bounds/8방향 이웃·중첩 사각형/AOI25, ActorClearance, 정적 장애물 AABB 모서리, 기존 packed WalkableBits를 포함한다. 콘텐츠 변경을 배포할 때 Content Version도 함께 올린다. ZoneEpoch는 네트워크 소유권 세대이며 콘텐츠 버전과 별개다.

`neighborIds`는 공간적으로 인접한 구역이다. `directCrossings`는 현재 owner의 해당 경계·모서리를 직접 통과할 수 있는지 구분한다. 모서리만 막혀 있어도 기하학 이웃은 남으며 다른 직교 구역을 경유할 수 있다. 직교 이웃 사이 중첩을 가로지르는 통로가 전혀 없으면 내보내기를 거부한다. 연결 검사는 Export 때의 작은 BFS이며 몬스터 AI가 아니다.

서버는 Unity 없이 C++ header로 읽는다. 기존 `WorldMap::IsWalkable` 및 `CanTraverse`는 글로벌 좌표를 받는다. A*가 구역 로컬 좌표를 쓰면 해당 구역 origin을 더한 뒤 조회한다. 내비게이션의 원점/타일 크기/가로·세로와 bit 인덱스는 AOI Cell과 독립적이다.

장애물 bake는 작은 장애물도 타일 사각형과 교차하면 막고, actor clearance를 더한다. 회전한 BoxCollider는 월드 AABB로 보수적으로 막으므로 실제 외형보다 넓게 차단할 수 있다. 원형 캐릭터의 정확한 연속 충돌 메시나 동적 장애물 처리라고 주장하지 않는다.

## 생성한 데이터와 검증 상태

현재 6구역의 실제 정적 맵 클론에서 출력했고 기존 nav 비트와 일치함을 확인했다. 별도의 9구역 예시는 복제 바닥을 610×610으로 늘려 생성했으며 기존 벽18개만 포함한다. 새 3번째 행의 몬스터 배치/실행 서버는 만들지 않았다.

독립 리뷰 후 기존 exporter fixture를 실제 레이아웃 규약에 맞춘 3×3 작은 맵으로 갱신했다. 핵심 직접 assertion 3개는 정상 출력/좌표·AOI·장애물, readonly 실패 보존, 중첩 전체 막힘 거부·대각 직접 통행 구분을 확인한다. 첫 실행의 출력 FormatException은 보존하고 문자열 처리를 수정했다. 추가 standalone C++ 컴파일에서 std::array 닫는 괄호 오류를 찾아 수정한 뒤 6/9 출력 모두 컴파일·실행 통과했다. 실제 Unity 키보드/마우스 입력 테스트는 보류다.

서버 자동 전환·첫 Full 동기화도 같은 후보에서 구현하고 직접 영향 자동 검증을 통과했다. 다만 새 서버를 기동하거나 실제 Unity 입력으로 접속한 검증은 아직 하지 않았다. 최종 결과는 WORLD_EXPORT_TRANSITION_RESULTS.txt를 따른다.

## 몬스터 AI 작업 인계

이번 작업은 `ZoneState_Monsters.cpp`, `MonsterState.h`, AI 상태 머신을 수정하지 않았다. 기존 추적은 현재 Cell의 area를 넘으면 aggro를 풀고, 배회도 해당 Cell과 spawn 반경 안으로 제한한다. 내비게이션 선분 검사는 이미 사용 중이지만 A* 탐색은 없다.

후속 A*에서는 WorldMap의 통행 bits/IsWalkable/CanTraverse를 사용하고, 구역 로컬 경로를 쓰면 ZoneOrigin을 더해 글로벌 좌표로 조회한다. Cell 밖으로 이동시키려면 좌표뿐 아니라 m_cells의 monsterIndexes 및 monster.cellX/cellY 소속을 함께 갱신해야 한다. AOI Cell을 A* 노드로 사용하지 않는다.

ActorClearance는 현재 한 가지 값으로 전체 통행 격자에 적용한다. 몬스터마다 다른 반지름을 지원하는 다중 통행 격자를 이번 작업에서 추가한 것은 아니다. 큰 모델을 위한 여유는 후속 AI/콘텐츠 작업에서 명시적으로 결정한다.

서버는 내보낸 Zone 수/행·열/core/여유/5×5 Cell이 현재 서버 공간 설정과 일치하는지 컴파일 시 검사한다. 9구역 예시를 실서버에 적용하려면 서버 Zone 수와 관련 소비자도 함께 바꿔야 하며, 예시 파일만 덮어써 잘못된 구역 배치를 조용히 실행할 수는 없다.

새 wire는 이전 실행 서버와 호환되지 않는다. 후보 MainServer와 ClientCore DLL을 같은 버전으로 전환해야 한다. DB/계정/기존 HP·아이템 초기화는 하지 않는다. 작업 브랜치는 codex/world-map-export이며, 원본 Game의 별도 미완료 AI 소스는 그대로 보존했다.

이웃 Zone이 소유한 엔티티를 경계 너머로 함께 표시하는 기능은 남아 있다. 현재 owner-only AOI/클라이언트 필터/Zone-local ID를 함께 바꿔야 하므로 이웃의 Full만 추가 송신하면 안 된다. 사용자 몬스터 AI의 A* 연결과도 구분해 후속 작업으로 다룬다.
