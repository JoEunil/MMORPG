# V04 독립 정적 리뷰

2026-10-18. 판정: **코드상 차단 문제 없음**. V04_TASK의 시각 저작 범위에서 Builder의 ApplyV04, 저장 Scene, V04 메시·텍스처·재질과 동결 기록을 파일로 검토했다. 리뷰어는 이 문서만 작성했으며 Unity/Editor/렌더/입력/테스트/서버/DB를 실행하거나 변경하지 않았다. 다른 작업자의 변경을 보존했다.

## 동결 기준

`UnityClient/Captures/v04-final-freeze.txt`의 19개 항목을 실제 파일 SHA256과 대조하여 불일치 0을 확인했다.

| 파일 | SHA256 |
| --- | --- |
| Assets/C02/Editor/C02SceneBuilder.cs | `551837349C673C2F42BB5C4A7B2957D474A7324C86B65A9848D422B6F0B24C31` |
| Assets/C02/C02.unity | `E2D2E68CE072EBE4E7921422706700B8539B80EDEDE36ADDEED48526A3F93475` |
| Assets/C02/C02WorldScreen.cs | `96C857D1DCB8EE4302494578EE5BBD477E7973EE79E212742752C2652E45EDD1` |
| Assets/C02/C04ObservedWorld.cs | `D7742F8CA56BFC819AFEB256FF3E3AC3001E97B730EFA0B0975B5211F7C5F5EC` |
| Assets/C02/C05CombatScreen.cs | `270EFD4E83EBA5901A15CE4AF2723DDD72CC6A2BFF77B9447A474753F653B98F` |
| Assets/Plugins/ClientCore/ClientCore.dll | `1D10D5AEF0D81622F29BE55E0796A1E27B679980EE664BADAD20BFA5A726063E` |

## 정적 확인

- ApplyV04는 C02 Scene/Edit Mode를 요구하며 기존 V04 root만 교체한다. V03 root와 기존 도로변 반복 식생을 비활성 보존하고 원본 메시를 재사용한다. V04 재질은 전용 경로에 새로 만들거나 재사용하며, 원본 재질 복제본의 tint를 변경하므로 원본 sharedMaterial의 값을 훼손하는 경로는 없다. 고정 random seed와 고정 곡선/노이즈 설정으로 배치를 저작한다. runtime Manager·가짜 서버 데이터·NPC·기능 UI는 추가하지 않았다.
- V04 Scene subtree를 직접 추적하여 Transform1515, MeshRenderer1297, Collider0, MonoBehaviour0을 확인했다. 새 장식은 시각 객체다. 새 이동 장애물이나 추가 raycast 입력 경로가 없다. C02World는 저장 상태 inactive, 원점·단위 scale이며 C04Observations는 같은 월드 아래 원점·단위 scale로 유지된다.
- ContinuousGround는 4정점/2삼각형의 평면, 중심(50,-.035,50)/extents(120,0,120)이다. 240m 지면 위의 색·길·식생 저작이 서버 좌표·Zone/AOI·이동 판정을 변경하지 않는다. 원 FieldPlane은 position(0,0,0), scale(6,1,6), 기존 enabled/nontrigger MeshCollider와 기존 mesh 참조를 유지한다. C02 fieldPlane 참조도 같은 Transform이다.
- 전역 1024² RGB24 텍스처는 sRGB/clamp/mipmap/trilinear, 국소 128² RGB24 grain은 linear/repeat/mipmap/trilinear로 저장돼 Builder 설정과 일치한다. 재질의 BaseMap/detail GUID, detail scale15·strength.85·`_DETAIL_SCALED`가 대응한다. 지면 UV 0..1에 전역 색/굽은 길을 얹고 국소 grain을 분리한다. 새 외부 모델·이미지 서비스·런타임 생성 의존성은 없다.
- C02 login/selection/enter/disconnect/world/own/camera 참조, C04 login/world/displayRoot 및 player+monster3 prefab 참조, C05 login/world/skillButtons3/vitals/status/aim 참조를 확인했다. 세 runtime component의 Scene 내부 참조는 모두 기존 local object로 해석된다. C04/C05 런타임 및 Core DLL 해시는 보존 기준과 일치한다.
- 선택 content는 topstretch anchorMin(0,1)/anchorMax(1,1), pivot(.5,1), sizeDeltaX0과 원본 10행을 유지한다. C05 패널의 기존 raycastTarget도 유지되며 V04 subtree에는 UI나 입력 handler가 없다. V04 변경에서 새 입력 가림 근거를 찾지 못했다.
- ApplyV04는 기존 V01/V03 저작 결과를 사용하는 단계형 메서드다. 기존 V04 root 교체와 asset 재사용 경로는 일치하지만, `Build()` 단독 호출로 최종 V04까지 전부 재생성된다는 의미는 아니다. 실제 재적용/저장 결과는 저작 담당 기록 근거이며 리뷰어가 실행하지 않았다.

## 검증 범위와 한계

`v04-static-check.txt` 및 V04_TASK는 저작 담당의 컴파일 completed/errors=[]·Play/compiling/dirty/worldActive=false, Edit Mode 정적 렌더와 초기 timeout 후 실제 저장 확인을 기록한다. Coordinator는 최종 진입/중앙 PNG를 직접 검수하고 최소 지면 디테일 보정을 수용했다고 전달했다. 이는 담당자의 정적 실행 근거이며 리뷰어의 독립 렌더 결과는 아니다.

실제 이동/카메라 추적·실 몬스터 가독성·애니메이션/VFX·GPU/프레임 성능은 미검증이다. Renderer1297의 비용이나 플레이 중 시야 품질을 정적 리뷰로 통과 처리하지 않는다. 낮은 디테일의 평면 필드 프로토타입 한계를 유지하며, 이번 판정은 코드·참조·충돌 경계의 V04 변경에 한정한다. R01의 저장/배포 차단 판정과도 별개다.
