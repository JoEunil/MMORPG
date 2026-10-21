# 몬스터 장비 표면 대표 샘플

2026-10-20. 배정 역할: 아트. 상태: 전용 Editor helper 저작 완료, Unity 적용·컴파일·시각 비교는 Coordinator 대기. 새 생성/다운로드/구매/리깅/서버 수정 없음. 다른 작업자의 파일은 수정하지 않았다.

## 대상과 시각 방향

일반 몬스터 0 고블린의 가죽 장구와 철 단검에 기존 알베도를 재사용해 평평한 단색 면을 줄인다. 실루엣·피부·천·metallic/smoothness·공용 플레이어 재질은 유지한다. 기존 tint를 그대로 곱한 첫 후보는 지나치게 어두워 실패했으므로, 두 재질의 BaseColor만 보정하는 두 번째 후보로 조정했다. 이 두 재질은 오크 장구에도 공유되므로 오크도 함께 확인한다. 골렘·조명·스폰·속도는 제외한다.

| 기존 대상 | 기존 텍스처 | 적용 규격 |
| --- | --- | --- |
| `Assets/C02/Art/C04/ScavengedLeather.mat` | `Assets/C02/Art/Textures/oiled-leather-albedo-v1.png` | URP `_BaseMap`, scale (2,2), BaseColor(.72,.66,.59,1), 기존 offset/0 metallic/.14 smoothness 유지 |
| `Assets/C02/Art/C04/DarkForgedIron.mat` | `Assets/C02/Art/Textures/worn-steel-albedo-v1.png` | URP `_BaseMap`, scale (2,2), BaseColor(.65,.68,.67,1), 기존 offset/.45 metallic/.28 smoothness 유지 |

두 원본 이미지를 직접 확인했다. 가죽은 잔주름·입자가 있는 갈색, 철은 미세 긁힘이 있는 회색이다. 기존 어두운 tint와 곱해지므로 질감이 과도하게 어두워지는지 실제 카메라에서 확인해야 한다. 색을 보정한 새로운 원본이나 normal map으로 설명하지 않는다.

Coordinator가 렌더한 `Captures/monster-surface-before.png`와 `monster-surface-after.png`를 직접 비교했다. 첫 후보는 벨트·손목·장화·단검이 거의 검게 변했고 작은 질감도 읽히지 않아 채택하지 않았다. 두 번째 후보의 BaseColor는 기존 가죽(.20,.135,.085)와 철(.25,.27,.26)에 텍스처 색이 다시 곱해지는 중복 감쇠를 줄이기 위한 tint다. 가죽의 갈색은 원본 알베도가, 철의 회색은 원본과 약한 녹회색 tint가 유지한다. 표시된 수치는 셰이더에 넣는 값이며 최종 픽셀 밝기를 보증하지 않는다. 두 번째 후보는 아직 렌더·채택되지 않았다.

출처·사용 조건은 [기존 재질 생성 기록](CLASSIC_MATERIAL_DRAFT.md)을 따른다. 내장 OpenAI 이미지 생성 출력물이며 제3자 에셋 라이선스는 없고, CC0/독점권은 주장하지 않는다. 기존 임포트 512px/mipmap/Repeat를 재사용하며 이번 작업에서 importer를 변경하지 않는다.

## 적용과 보호

새 `Assets/C02/Editor/C04MonsterSurfaceSetup.cs`의 메뉴 `Tools/MMORPG/Art/Apply Monster Leather and Iron Surfaces`를 Coordinator가 실행한다. 모든 참조와 편집 가능 여부를 변경 전에 확인하고, Play/컴파일/임포트 중·미저장 scene·대상 material dirty 상태는 거절한다. 두 material을 Undo에 등록하고 두 asset만 명시 저장한다. 전체 `SaveAssets`, scene 재생성, prefab 재저장, mesh·새 texture·공용 player material 변경은 없다. Undo 이후 저장을 원하는 경우 대상 재질의 일반 저장을 수행한다.

## Builder 재생성 경계 제안 — 이 담당은 수정하지 않음

`C02SceneBuilder.cs`의 `C04Material`(현재 약1134줄)은 기존 material을 불러오면 색과 shading만 갱신하므로 위에 연결한 texture/scale은 기존 asset 재사용 시 이미 유지된다. 다만 material을 지운 뒤 재생성하면 단색으로 돌아간다. Coordinator가 해당 함수에만 `name == "ScavengedLeather"` / `name == "DarkForgedIron"` 두 조건으로 위 texture 경로와 scale(2,2)를 지정하는 최소 패치를 적용하면 신규 재생성도 유지된다. 다른 이름은 건드리지 않고 함수 시그니처·Prefab 흐름·mesh·scene 재생성 정책은 그대로 둔다. 범용 재질 registry나 자동 매핑은 필요 없다.

## 최소 확인과 종료 조건

Coordinator가 저작을 동결한 뒤 명시 컴파일 1회와 동일 카메라·조명·해상도의 전후 화면 비교 1회를 수행한다. 고블린 장구의 질감, 어두운 색상 구분, 오크 공유 부위, player 불변을 확인한다. 가까운 대표샘플과 실제 쿼터뷰에서 UV가 늘어나거나 타일 경계가 거슬리는지 본다. 읽기 전용 참조/diff 확인으로 scene·prefab·공용 player material 불변을 확인한다. 서버·입력 회귀·전체 로그인 반복은 이 재질 변경 때문에 추가하지 않는다. 컴파일·Unity 실행·스크린샷은 아직 미실행이며 시각 개선 성공으로 보고하지 않는다.

정적 비교 대상은 `Assets/C02/Art/C04/MonsterType0.prefab`이다. 실제 `Assets/C02/C02.unity`의 `monsterPrefabs`에 이 prefab GUID `9f89d5b40f8d855468e16ce5b8771536`이 연결돼 있다. `PreviewRenderUtility`의 별도 preview scene에 prefab 복제본 한 개를 넣고 고정 카메라·조명으로 렌더한 뒤 Cleanup하면 실제 scene에 instance 추가·저장 없이 전후 비교할 수 있다. 이 정적 비교를 실제 전투 입력 검증으로 설명하지 않는다.

대상 두 material은 ROOT와 후보의 raw hash가 다르지만 CRLF→LF 정규화 후 본문은 각각 정확히 같다. 줄바꿈 차이를 기존 WIP 내용 차이로 오인하지 않는다. 이 확인을 다른 자산으로 확대하지 않으며 후보기준의 지정 두 재질만 적용한다. 두 texture의 SHA256은 기존 출처 문서의 원본과 일치했다.

## Coordinator 실제 결과

첫 후보는 동일 뒤면 비교에서 장비가 검어져 미채택, 원래BaseMap/scale 복구. URP가 MainTex미러를 갱신하면서 미저장 상태를 표시해 두번째 적용1회가 가드에 의해 거절됨. 가드 완화 없이 원래color/null texture/scale1을 읽기 검증 후 이미 복구된 메모리 값을 두asset만 저장했다. 두번째 후보를 같은 앞면 camera/bounds/해상도960x720로 비교: 가죽표면이읽히고 첫후보검정뭉침완화. 오크공유부위도 정적렌더확인. Captures/monster-surface-front-before.png/after.png, orc-surface-front-after.png. 모두 임시 PreviewScene 정적렌더이며 실게임 전투·애니메이션·사냥품질 통과가 아니다.

최종값: leatherBaseColor(.72,.66,.59)/iron(.65,.68,.67), 둘다기존BaseMapscale2. metallic/smoothness/피부/천/mesh/prefab/Scene/서버변경0. ROOT/후보 두material 최초hash차이는 줄바꿈뿐임을 정규화equal로 확인하고 원본백업뒤 대상2개만통합. 후보diff는 BaseMap/scale/BaseColor 각3개값뿐. 새helper+Unitymeta보존. builderC04Material만 ROOT/후보함수본문동일검사·원본백업뒤 두이름조건추가하여재생성일치유지. 전체builder실행0.

명시최종compile0오류/0경고. 독립whisper_review아트읽기blocker0, 초기자산미통합warning은이후실제통합diff로해소. 자산메뉴가드실패는제품우회로숨기지않음. 공유루트전체파일복사와MainTex제거는자동승인심사에거절되어수행하지않았고, 비교·백업·현재복구값검증후목표범위만수정/저장한안전대안으로완료. 추가플레이입력/로그인/서버재시작0.

전문호출: monster_surface 신규1/재활성화1, whisper_review 아트재검토1. 누적신규13/재활성화48/활성화61회. 실제토큰N/A. 작은별도PR을생성하지않고다음관련필드·몬스터표현묶음에보존한다.

공개화면은 새 가방 결과만 갱신했다. 몬스터는 Editor 재질 비교이므로 기존 실제 플레이 캡처를 교체하지 않고 다음 사냥 묶음의 실제 쿼터뷰 결과와 함께 반영한다.
