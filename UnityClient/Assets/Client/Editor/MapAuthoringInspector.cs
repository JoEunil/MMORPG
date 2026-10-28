using System;
using System.IO;
using Mmorpg.Client;
using UnityEditor;
using UnityEditor.UIElements;
using UnityEngine.UIElements;

namespace Mmorpg.Client.Editor
{
    [CustomEditor(typeof(MapAuthoring))]
    public sealed class MapAuthoringInspector : UnityEditor.Editor
    {
        private Label summary;

        public override VisualElement CreateInspectorGUI()
        {
            var root = new VisualElement();
            root.Add(new HelpBox("Unity X/Z = 서버 X/Y. 각 구역의 로컬 (0, 0)은 중첩 영역까지 포함한 좌측 하단입니다. 전체 맵을 한 파일로 내보내며, 검토 구역 선택은 출력에서 다른 구역을 제거하지 않습니다.", HelpBoxMessageType.Info));
            InspectorElement.FillDefaultInspector(root, serializedObject, this);
            summary = new Label();
            summary.style.whiteSpace = WhiteSpace.Normal;
            summary.style.marginTop = 8;
            root.Add(summary);
            root.Add(new HelpBox("Scene: 파랑=구역 범위, 초록=전용 영역, 주황=이웃과 중첩, 흰색=선택 구역 AOI 5×5, 빨강=로컬 원점. 통행 격자는 별도 맵 에셋에서 확인합니다.", HelpBoxMessageType.Info));
            root.Add(new Button(() =>
            {
                var author = (MapAuthoring)target;
                if (author.map != null) Selection.activeObject = author.map;
            }) { text = "저장된 NavigationGrid 보기" });
            root.Add(new Button(() =>
            {
                serializedObject.ApplyModifiedProperties();
                var author = (MapAuthoring)target;
                try
                {
                    string path = Path.GetFullPath(Path.Combine(UnityEngine.Application.dataPath, "../../CoreLib/Generated"));
                    WorldMapExport.BakeAndExport(author, path);
                    summary.text = "전체 맵 내보내기 완료: " + Path.Combine(path, "WorldMap.inc");
                }
                catch (Exception exception)
                {
                    summary.text = "내보내지 못했습니다: " + exception.Message;
                }
            }) { text = "전체 맵 검증 및 내보내기" });
            root.TrackSerializedObjectValue(serializedObject, _ => RefreshSummary());
            RefreshSummary();
            return root;
        }

        private void RefreshSummary()
        {
            var author = (MapAuthoring)target;
            try
            {
                var layout = WorldLayout.Build(author.zoneColumns, author.zoneRows, author.zoneCoreSize, author.transitionMargin);
                if (author.previewZoneId < 1 || author.previewZoneId > layout.Zones.Length)
                    throw new InvalidOperationException("검토 구역 ID가 배치 범위 밖입니다.");
                var zone = layout.Zones[author.previewZoneId - 1];
                summary.text = $"전체 {layout.Columns}×{layout.Rows} / {layout.Zones.Length}구역 · 콘텐츠 버전 {author.contentVersion}\n" +
                    $"선택 구역 {zone.Id}: 월드 원점 {zone.ZoneOrigin}, 로컬 크기 {zone.Span}×{zone.Span}\n" +
                    $"AOI 5×5 / Cell {zone.AoiCellSize} · 내비게이션 타일 {author.map?.tileSize}\n" +
                    "좌표와 사각형 범위는 [최솟값, 최댓값)입니다. 저장된 통행 배열은 내보내기 전까지 이전 상태일 수 있습니다.";
                SceneView.RepaintAll();
            }
            catch (ArgumentException exception) { summary.text = "배치 확인 필요: " + exception.Message; }
            catch (InvalidOperationException exception) { summary.text = exception.Message; }
        }
    }
}
