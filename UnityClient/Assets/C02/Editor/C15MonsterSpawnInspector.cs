using System;
using UnityEditor;
using UnityEditor.UIElements;
using UnityEngine;
using UnityEngine.UIElements;

namespace C02.Editor
{
    [CustomEditor(typeof(C15MonsterSpawns))]
    public sealed class C15MonsterSpawnInspector : UnityEditor.Editor
    {
        private VisualElement grid;
        private Label preview;
        private Label validation;

        public override VisualElement CreateInspectorGUI()
        {
            var root = new VisualElement();
            AddNote(root, "고정 5×5 배치 · 셀 폭 42 · 중앙 (2,2) 셀은 보스 1마리");
            AddNote(root, "셀별 배열을 비우면 기존 공통 개수를 사용합니다. 배열은 y × 5 + x 순서의 25개이며 중앙 값은 0입니다.");
            AddNote(root, "셀 표기 순서: 일반 + 강한 몬스터");
            root.Add(new PropertyField(serializedObject.FindProperty("regularPerCell"), "일반 몬스터 / 셀"));
            root.Add(new PropertyField(serializedObject.FindProperty("strongPerCell"), "강한 몬스터 / 셀"));
            root.Add(new PropertyField(serializedObject.FindProperty("edgeInset"), "외곽 여백 (1–41)"));
            root.Add(new PropertyField(serializedObject.FindProperty("regularByCell"), "셀별 일반 개수 (선택)"));
            root.Add(new PropertyField(serializedObject.FindProperty("strongByCell"), "셀별 강한 개수 (선택)"));
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
            int regular = serializedObject.FindProperty("regularPerCell").intValue;
            int strong = serializedObject.FindProperty("strongPerCell").intValue;
            int inset = serializedObject.FindProperty("edgeInset").intValue;
            int[] regularCounts, strongCounts;
            try
            {
                var content = (C15MonsterSpawns)target;
                C15MonsterSpawnExport.Validate(content);
                regularCounts = C15MonsterSpawnExport.EffectiveCounts(content.regularByCell, regular, "regular");
                strongCounts = C15MonsterSpawnExport.EffectiveCounts(content.strongByCell, strong, "strong");
            }
            catch (Exception error)
            {
                grid.Clear();
                preview.text = "배치 값을 수정하면 미리보기가 표시됩니다.";
                validation.text = error.Message;
                return;
            }
            grid.Clear();
            for (int y = 0; y < 5; y++)
            {
                var row = new VisualElement();
                row.style.flexDirection = FlexDirection.Row;
                for (int x = 0; x < 5; x++)
                {
                    bool boss = x == 2 && y == 2;
                    var cellLabel = new Label(boss ? "Boss\n1" : $"{regularCounts[y * 5 + x]} + {strongCounts[y * 5 + x]}");
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
            long total = 1;
            for (int i = 0; i < 25; i++) total += (long)regularCounts[i] + strongCounts[i];
            preview.text = $"Zone 합계 {total}마리 · 좌표 오프셋 {inset}–41";
            validation.text = "저작 값이 유효합니다.";
        }
    }
}
