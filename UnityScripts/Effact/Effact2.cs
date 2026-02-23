using UnityEngine;
using System.Collections;

public class Effect2 : MonoBehaviour
{
    public float duration = 0.5f;

    [SerializeField] private Renderer[] fills; // SpriteRenderer 또는 MeshRenderer(Material)

    private Vector2 startInnerSize = new Vector2(1.0f, 1.0f);
    private Vector2 endInnerSize = new Vector2(0.5f, 0.5f);

    private void Start()
    {
        if (fills != null && fills.Length > 0)
            StartCoroutine(ScaleRoutine());

        Destroy(gameObject, duration);
    }

    IEnumerator ScaleRoutine()
    {
        float time = 0f;

        while (time < duration)
        {
            float t = time / duration;
            Vector2 currentSize = Vector2.Lerp(startInnerSize, endInnerSize, t);

            foreach (var fill in fills)
            {
                // Shader에서 _InnerSize 속성을 조절
                fill.material.SetVector("_InnerSize", currentSize);
            }

            time += Time.deltaTime;
            yield return null;
        }

        foreach (var fill in fills)
        {
            fill.material.SetVector("_InnerSize", endInnerSize);
        }
    }
}
