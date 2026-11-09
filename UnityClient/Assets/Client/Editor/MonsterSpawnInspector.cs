using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEditor.UIElements;
using UnityEngine;
using UnityEngine.UIElements;

namespace Mmorpg.Client.Editor
{
    [CustomEditor(typeof(MonsterSpawns))]
    public sealed class MonsterSpawnInspector : UnityEditor.Editor
    {
        private VisualElement grid;
        private DropdownField zonePicker;
        private Label preview;
        private Label validation;

        public override VisualElement CreateInspectorGUI()
        {
            var root = new VisualElement();
            AddNote(root, "1구역은 몬스터 없는 마을입니다. 사냥 구역은 5×5 배치 · 셀 폭 42 · 중앙은 보스 전용입니다.");
            AddNote(root, "셀별 배열을 비우면 기존 공통 개수를 사용합니다. 배열은 y × 5 + x 순서의 25개이며 중앙 값은 0입니다.");
            AddNote(root, "구역별 총수를 지정하면 가중치 비율로 정확히 배분합니다. 표는 북쪽이 위이며 배분된 일반 + 강화 개수를 표시합니다.");
            root.Add(new PropertyField(serializedObject.FindProperty("regularPerCell"), "일반 몬스터 / 셀"));
            root.Add(new PropertyField(serializedObject.FindProperty("strongPerCell"), "강한 몬스터 / 셀"));
            root.Add(new PropertyField(serializedObject.FindProperty("edgeInset"), "외곽 여백 (1–41)"));
            root.Add(new PropertyField(serializedObject.FindProperty("regularByCell"), "셀별 일반 가중치 / 기존 개수"));
            root.Add(new PropertyField(serializedObject.FindProperty("strongByCell"), "셀별 강화 가중치 / 기존 개수"));
            root.Add(new PropertyField(serializedObject.FindProperty("zoneDensityPercent"), "구역별 밀도 (%) · 마을은 0%, 사냥 구역은 50–200%"));
            root.Add(new PropertyField(serializedObject.FindProperty("regularTotals"), "구역별 일반 총수 (6개)"));
            root.Add(new PropertyField(serializedObject.FindProperty("strongTotals"), "구역별 강화 총수 (6개)"));
            root.Add(new PropertyField(serializedObject.FindProperty("bossCounts"), "구역별 보스 수 (6개 · Zone4/6만 0/1)"));
            root.Add(new PropertyField(serializedObject.FindProperty("zoneCellWeights"), "사냥터별 셀 가중치"));
            AddNote(root, "구역별 일반·강화 배열을 비우면 해당 종류는 공통 배열을 따릅니다. 0을 넣은 셀에서는 생성하지 않습니다.");
            AddNote(root, "총수 세 배열을 모두 비우면 기존 밀도를 적용합니다. 총수를 쓰면 세 배열이 모두 필요하며 밀도는 적용하지 않습니다.");
            zonePicker = new DropdownField("셀 배치 미리보기", new List<string>
                { "Zone 1", "Zone 2", "Zone 3", "Zone 4", "Zone 5", "Zone 6" }, 1);
            root.Add(zonePicker);
            grid = new VisualElement();
            grid.style.marginTop = 6;
            root.Add(grid);
            preview = new Label();
            preview.style.whiteSpace = WhiteSpace.Normal;
            preview.style.marginTop = 4;
            root.Add(preview);
            validation = new Label();
            validation.style.whiteSpace = WhiteSpace.Normal;
            root.Add(validation);
            AddNote(root, "위치 오프셋은 설정값부터 41까지 무작위입니다. offset + rand() % (42 - offset) 방식이며, 미리보기는 셀별 개수만 표시합니다.");
            AddNote(root, "내보낸 값은 후보 데이터입니다. 서버 배포는 별도 작업입니다.");
            zonePicker.RegisterValueChangedCallback(_ => RefreshPreview());
            root.TrackSerializedObjectValue(serializedObject, _ => RefreshPreview());
            RefreshPreview();
            return root;
        }

        private static void AddNote(VisualElement root, string text)
        {
            var note = new Label(text);
            note.style.whiteSpace = WhiteSpace.Normal;
            note.style.marginBottom = 4;
            root.Add(note);
        }

        private void RefreshPreview()
        {
            serializedObject.UpdateIfRequiredOrScript();
            int inset = serializedObject.FindProperty("edgeInset").intValue;
            int[][] regularByZone, strongByZone;
            int[] bosses;
            try
            {
                var content = (MonsterSpawns)target;
                MonsterSpawnExport.Validate(content);
                regularByZone = MonsterSpawnExport.EffectiveZoneCounts(content, false);
                strongByZone = MonsterSpawnExport.EffectiveZoneCounts(content, true);
                bosses = MonsterSpawnExport.EffectiveBossCounts(content);
            }
            catch (Exception error)
            {
                grid.Clear();
                preview.text = "배치 값을 수정하면 미리보기가 표시됩니다.";
                validation.text = error.Message;
                return;
            }
            grid.Clear();
            int selectedZone = zonePicker.index;
            for (int y = 4; y >= 0; y--)
            {
                var row = new VisualElement();
                row.style.flexDirection = FlexDirection.Row;
                for (int x = 0; x < 5; x++)
                {
                    bool boss = x == 2 && y == 2;
                    int cell = y * 5 + x;
                    var cellLabel = new Label(boss ? $"보스\n{bosses[selectedZone]}" :
                        $"{regularByZone[selectedZone][cell]} + {strongByZone[selectedZone][cell]}");
                    cellLabel.style.flexGrow = 1;
                    cellLabel.style.flexBasis = 0;
                    cellLabel.style.height = 36;
                    cellLabel.style.marginRight = 2;
                    cellLabel.style.marginBottom = 2;
                    cellLabel.style.unityTextAlign = TextAnchor.MiddleCenter;
                    cellLabel.style.unityFontStyleAndWeight = boss ? FontStyle.Bold : FontStyle.Normal;
                    cellLabel.style.whiteSpace = WhiteSpace.Normal;
                    cellLabel.style.backgroundColor = boss ? new Color(0.42f, 0.31f, 0.13f) : new Color(0.13f, 0.24f, 0.19f);
                    cellLabel.style.color = boss ? new Color(1f, 0.88f, 0.57f) : new Color(0.9f, 0.95f, 0.88f);
                    cellLabel.style.borderTopWidth = 1;
                    cellLabel.style.borderBottomWidth = 1;
                    cellLabel.style.borderLeftWidth = 1;
                    cellLabel.style.borderRightWidth = 1;
                    cellLabel.style.borderTopColor = new Color(0.58f, 0.67f, 0.54f);
                    cellLabel.style.borderBottomColor = new Color(0.58f, 0.67f, 0.54f);
                    cellLabel.style.borderLeftColor = new Color(0.58f, 0.67f, 0.54f);
                    cellLabel.style.borderRightColor = new Color(0.58f, 0.67f, 0.54f);
                    row.Add(cellLabel);
                }
                grid.Add(row);
            }
            var zoneTotals = new string[6];
            for (int zone = 0; zone < zoneTotals.Length; zone++)
            {
                int regularTotal = 0, strongTotal = 0;
                for (int cell = 0; cell < 25; cell++)
                {
                    regularTotal += regularByZone[zone][cell];
                    strongTotal += strongByZone[zone][cell];
                }
                zoneTotals[zone] = $"Zone {zone + 1}: 일반 {regularTotal} / 강화 {strongTotal} / 보스 {bosses[zone]}";
            }
            preview.text = $"{string.Join(" · ", zoneTotals)}\n좌표 오프셋 {inset}–41";
            validation.text = "저작 값이 유효합니다.";
        }
    }
}
