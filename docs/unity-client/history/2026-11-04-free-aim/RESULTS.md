# 자유각도 조준 통합 결과 — 2026-11-04

기준 origin/master239a01f, 후보 codex/client-clock-compat. 사용자가 서버 변경을 허용해 후보에서 작업했으며 원본 Game 폴더·사용자 몬스터 AI·DB 스키마는 변경하지 않았다.

## 변경

- WASD/방향키 이동·Q/E/R 기존3스킬, Shift/F 미배정. 마우스 월드 평면 투영으로 시전 각도 결정.
- aimAngle은 +Y=0°,시계방향[0,360), finite/범위 검사. 시전 시 고정하며 phase 위치는 caster 현재 위치의 기존 규칙 유지.
- 플레이어 Rectangle 자유 회전, 로컬/원격 플레이어 모션·예고·VFX 같은 각도 사용. 몬스터는 기존 dir 판정과 AI 유지.
- UI 위 키 입력은 차단, 명시 스킬 버튼은 최근 유효 월드 조준각 사용. 이력이 없으면 안내하고 요청하지 않음.
- 유저 속도9→7.65 units/s(0.85배), 서버 틱 이동 예산0.3825, 클라 요청/표시 선행거리 같은 비율로 축소. 몬스터 공통 배율 불변.
- 요청 프레임26→30, 행동 결과 필드21→25. 서버·ClientCore·Unity DLL·DummyClients 동시 적용 필요. 이전 빌드 혼용 불가. 이동/DB byte dir 유지.

## 빌드·직접 검사

MainServer·ClientCore·DummyClients·Unity 컴파일 성공. 새 작업 폴더의 ClientCore assets 누락은 기존 NuGet 복원으로 해결했다. UnitTests 초기 빌드는 기존 NonZoneThreadPool의 Skill2인자 호출1곳 C2660으로 실패했고 같은 제작자가 당시dir에 맞는90°를 명시한 후 재빌드 성공했다. 실패 로그를 보존한다.

직접 자동 검사: 서버2(회전 범위/잘못된 각도, 실제charge→hit의 고정각·현재위치·범위 내외 피해), Dummy protocol self-test1, Unity4(포인터투영/원자적각도,25바이트결과,로컬차징/타격,원격표현) 모두 통과. 새 wire와 서버/로컬/원격의 독립 경로 때문에 기본2~3개 예산을 확대했으며 무관한 기존 통과 검사는 반복하지 않았다.

증거: Builds/ClientGrowthHUD/verification/aim360의 server-build.log, server-tests-rebuild.log, server-aim-tests.xml/log, clientcore-final-build.log, dummy-build.log, dummy-protocol-result.log와 unity-compile-result/projection-result/wire-result/effects-result/remote-result.json. 후보와 실제 Editor의 변경Unity21파일 해시 일치.

## 실서버 배치

새 MainServer10099와 기존 Login 앱을 함께 실행하고 보호test1 정보를 native UI에 입력해 캐릭터11/Zone2에 진입했다. 비밀은 메모리 입력 외 출력·문서·인자에 남기지 않았다.

1. Native 월드 포인터 선택 후 R 버튼 클릭: angle358.409882가 charge/hit 동일, MP9109→9089.
2. 포인터가 R UI 위일 때 합성R을 정상 InputSystem/PlayerLoop로 전달: frame66133→68552 동안 응답2개 유지/MP9089 유지. 쿨다운4초 경과 후 검사했다.
3. Native 월드 포인터 선택 후 합성R: charge frame76374/hit76434, angle31.7326088 동일. charge 수신 후 합성 포인터를 다른 위치로 옮겼지만 기존 각도 유지. MP9069, 실제 화면 EXP190→200 및 HP포션1개 획득 안내.

이 배치는 native 클릭+합성키/포인터이며 물리 키보드 평가가 아니다. 제품 핸들러/강제 InputSystem.Update로 성공을 만들지 않았다. 실제 타격 캡처와 통합fixture의 범위 내외HP 판정은 별도 근거다. 실서버 몬스터별 HP 전후를 이번 관찰기로 따로 기록하지 않았으며 개별HP 숫자를 만들어 보고하지 않는다.

live/actions.jsonl, pointer-moved-after-charge.txt, after-button/ui-key-input/after-ui-key/world-key-input/final-live-state.json, cast-3-phase-0.png/cast-4-phase-1.png가 근거다. charge 첫 프레임 캡처는 영역이 희미해 예고 가독성 전체의 증명으로 쓰지 않는다. 관찰 도구를 잘못 eval_file로 실행한 컴파일 오류는 제품 오류가 아니며 run_script로 해결했다.

## 검수·종료·제한

독립 코드 리뷰 차단0. 종합 검수는 자동검사 이후 실서버 관찰 요구를 남겼고, 같은 담당자가 실서버 응답·캡처를 검토해 범위 통과/필수0으로 최종 판정했다. 물리키 조작감·사용자 만족·다중 클라이언트 실플레이는 평가하지 않았다.

임시관찰기 해제/합성키release 후 q 정상 종료exit0, Unity authfalse/Ready frame95266 확인. Playoff/backgroundfalse 복원, Login helper31908은 LOGIN_STOPPED/프로세스종료 확인. 강제 종료·DB 초기화 없음.

Agent 실제 추가: aim_protocol_server 구현 신규1/후속1, commerce_final_display 구현후속1, aim360_contract_review 리뷰신규1, commerce_quality 종합검수후속2. 총6회, 누적604(신규230/후속374). 총괄 직접 작업·검사·메시지는 제외. 토큰/비용N/A.
