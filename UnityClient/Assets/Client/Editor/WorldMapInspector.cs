using System;
using UnityEditor;
using UnityEditor.UIElements;
using UnityEngine;
using UnityEngine.UIElements;

namespace Mmorpg.Client.Editor
{
    [CustomEditor(typeof(WorldMap))]
    public sealed class WorldMapInspector : UnityEditor.Editor
    {
        private const int MaxPreviewSide = 1024;
        private Texture2D previewTexture;
        private Image previewImage;
        private Label summary;
        private Label status;

        public override VisualElement CreateInspectorGUI()
        {
            var root = new VisualElement();
            root.Add(new Label("저장된 통행 격자"));
            root.Add(new HelpBox("미리보기는 현재 에셋의 통행 배열을 보여줍니다. 원점·크기·장애물을 편집한 뒤에는 C02 > World > Bake and Export Navigation으로 격자를 다시 생성해야 합니다. 이 화면은 서버 데이터를 자동으로 바꾸지 않습니다.", HelpBoxMessageType.Info));

            root.Add(new PropertyField(serializedObject.FindProperty("origin"), "월드 원점 (X, Z)"));
            root.Add(new PropertyField(serializedObject.FindProperty("tileSize"), "타일 크기"));
            root.Add(new PropertyField(serializedObject.FindProperty("width"), "가로 타일 수"));
            root.Add(new PropertyField(serializedObject.FindProperty("height"), "세로 타일 수"));

            summary = new Label();
            summary.style.whiteSpace = WhiteSpace.Normal;
            summary.style.marginTop = 4;
            root.Add(summary);

            status = new Label();
            status.style.whiteSpace = WhiteSpace.Normal;
            status.style.marginTop = 4;
            root.Add(status);

            previewImage = new Image { scaleMode = ScaleMode.ScaleToFit };
            previewImage.style.width = Length.Percent(100);
            previewImage.style.maxWidth = 480;
            previewImage.style.height = 480;
            previewImage.style.alignSelf = Align.Center;
            previewImage.style.marginTop = 6;
            root.Add(previewImage);

            var refresh = new Button(RefreshPreview) { text = "미리보기 새로 고침" };
            refresh.style.marginTop = 4;
            root.Add(refresh);

            root.TrackSerializedObjectValue(serializedObject, _ => RefreshPreview());
            RefreshPreview();
            return root;
        }

        private void RefreshPreview()
        {
            serializedObject.UpdateIfRequiredOrScript();
            var map = target as WorldMap;
            if (map == null || !map.IsValid)
            {
                ClearPreview();
                summary.text = "격자 정보가 유효하지 않습니다. 크기와 저장된 통행 배열을 확인하세요.";
                status.text = "미리보기를 만들 수 없습니다.";
                return;
            }

            int walkable = 0;
            for (int i = 0; i < map.walkable.Length; i++)
            {
                if (map.walkable[i] > 1)
                {
                    ClearPreview();
                    summary.text = "저장된 통행 값은 0 또는 1이어야 합니다.";
                    status.text = "미리보기를 만들 수 없습니다.";
                    return;
                }
                walkable += map.walkable[i];
            }

            int blocked = map.walkable.Length - walkable;
            double maxX = (double)map.origin.x + (double)map.width * map.tileSize;
            double maxZ = (double)map.origin.y + (double)map.height * map.tileSize;
            summary.text = $"통행 {walkable:N0} · 차단 {blocked:N0}   |   원점 ({map.origin.x:0.##}, {map.origin.y:0.##}) · 타일 {map.tileSize:0.##}\n" +
                $"범위: X [{map.origin.x:0.##}, {maxX:0.##}) · Z [{map.origin.y:0.##}, {maxZ:0.##}) — 상한은 미포함";
            if (walkable == 0)
            {
                ClearPreview();
                status.text = "통행 가능한 타일이 없습니다. 서버 데이터로 내보낼 수 없습니다.";
                return;
            }

            int scale = Math.Max(1, Math.Max((map.width + MaxPreviewSide - 1) / MaxPreviewSide,
                (map.height + MaxPreviewSide - 1) / MaxPreviewSide));
            int previewWidth = (map.width + scale - 1) / scale;
            int previewHeight = (map.height + scale - 1) / scale;
            var pixels = new Color32[previewWidth * previewHeight];
            for (int py = 0; py < previewHeight; py++)
                for (int px = 0; px < previewWidth; px++)
                {
                    int xEnd = Math.Min(map.width, (px + 1) * scale);
                    int yEnd = Math.Min(map.height, (py + 1) * scale);
                    bool pass = true;
                    for (int y = py * scale; y < yEnd && pass; y++)
                        for (int x = px * scale; x < xEnd; x++)
                            if (map.walkable[y * map.width + x] == 0) { pass = false; break; }
                    // Texture rows follow the map's bottom-to-top Z axis.
                    pixels[py * previewWidth + px] = pass
                        ? new Color32(102, 164, 101, 255)
                        : new Color32(143, 76, 61, 255);
                }

            ReleasePreviewTexture();
            previewTexture = new Texture2D(previewWidth, previewHeight, TextureFormat.RGBA32, false)
            {
                name = "C20 World Map Inspector Preview",
                hideFlags = HideFlags.HideAndDontSave,
                filterMode = FilterMode.Point,
                wrapMode = TextureWrapMode.Clamp
            };
            previewTexture.SetPixels32(pixels);
            previewTexture.Apply(false);
            previewImage.image = previewTexture;
            status.text = scale == 1
                ? $"초록: 통행 · 적갈색: 차단 · 1픽셀 = 1타일 · 아래쪽이 낮은 Z · {previewWidth} × {previewHeight}"
                : $"초록: 통행 · 적갈색: 차단 · 약 {scale}×{scale} 타일을 1픽셀로 축소 (모든 타일이 통행 가능해야 초록) · 아래쪽이 낮은 Z · {previewWidth} × {previewHeight}";
        }

        private void ClearPreview()
        {
            if (previewImage != null) previewImage.image = null;
            ReleasePreviewTexture();
        }

        private void ReleasePreviewTexture()
        {
            if (previewTexture == null) return;
            UnityEngine.Object.DestroyImmediate(previewTexture);
            previewTexture = null;
        }

        private void OnDisable() => ClearPreview();
    }
}
