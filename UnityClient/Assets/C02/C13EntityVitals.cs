using UnityEngine;
using UnityEngine.UI;

namespace C02
{
    public sealed class C13EntityVitals : MonoBehaviour
    {
        private RectTransform barTransform;
        private RectTransform fillTransform;
        private Transform viewCamera;

        private void Awake()
        {
            var canvasObject = new GameObject("MonsterVitals", typeof(RectTransform), typeof(Canvas));
            canvasObject.transform.SetParent(transform, false);
            var canvas = canvasObject.GetComponent<Canvas>();
            canvas.renderMode = RenderMode.WorldSpace;
            canvasObject.transform.localScale = Vector3.one * 0.01f;
            barTransform = canvasObject.GetComponent<RectTransform>();
            barTransform.sizeDelta = new Vector2(100, 10);

            var renderers = GetComponentsInChildren<Renderer>();
            var headPosition = transform.position + Vector3.up * 2f;
            if (renderers.Length != 0)
            {
                var headY = float.NegativeInfinity;
                foreach (var renderer in renderers) headY = Mathf.Max(headY, renderer.bounds.max.y);
                headPosition = new Vector3(transform.position.x, headY + 0.25f, transform.position.z);
            }
            barTransform.localPosition = transform.InverseTransformPoint(headPosition);

            var background = new GameObject("Background", typeof(RectTransform), typeof(Image));
            background.transform.SetParent(canvasObject.transform, false);
            var backgroundRect = background.GetComponent<RectTransform>();
            backgroundRect.anchorMin = Vector2.zero;
            backgroundRect.anchorMax = Vector2.one;
            backgroundRect.offsetMin = Vector2.zero;
            backgroundRect.offsetMax = Vector2.zero;
            var backgroundImage = background.GetComponent<Image>();
            backgroundImage.type = Image.Type.Simple;
            backgroundImage.color = new Color(0.12f, 0.08f, 0.08f, 0.9f);
            backgroundImage.raycastTarget = false;

            var fill = new GameObject("Fill", typeof(RectTransform), typeof(Image));
            fill.transform.SetParent(background.transform, false);
            fillTransform = fill.GetComponent<RectTransform>();
            fillTransform.anchorMin = Vector2.zero;
            fillTransform.anchorMax = Vector2.one;
            fillTransform.offsetMin = new Vector2(0, 1);
            fillTransform.offsetMax = new Vector2(0, -1);
            var fillImage = fill.GetComponent<Image>();
            fillImage.type = Image.Type.Simple;
            fillImage.color = new Color(0.35f, 0.85f, 0.32f, 1f);
            fillImage.raycastTarget = false;
            canvasObject.transform.localRotation = Quaternion.identity;
        }

        public void SetHealth(int hp, int maxHp)
        {
            fillTransform.anchorMax = new Vector2(Mathf.Clamp01((float)hp / maxHp), 1f);
        }

        public void SetCamera(Transform cameraTransform)
        {
            viewCamera = cameraTransform;
        }

        private void LateUpdate()
        {
            if (viewCamera != null) barTransform.rotation = viewCamera.rotation;
        }
    }
}
