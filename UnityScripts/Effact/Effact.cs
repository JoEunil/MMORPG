using UnityEngine;
using System.Collections;

public class Effect : MonoBehaviour
{
    public float duration = 0.5f;

    [SerializeField] private Transform[] fills;

    private float startScale = 0f;
    private float endScaleX = 1f;
    private float endScaleY = 1f;

    private void Start()
    {
        if (fills != null)
        {
            StartCoroutine(ScaleRoutine());
        }

        Destroy(gameObject, duration);
    }

    IEnumerator ScaleRoutine()
    {
        float time = 0;
        Vector3 start = new Vector3(startScale, startScale, 1f);
        Vector3 end = new Vector3(endScaleX, endScaleY, 1f);

        while (time < duration)
        {
            float t = Mathf.Clamp01(time / duration);
            foreach (var fill in fills)
                fill.localScale = Vector3.Lerp(start, end, t);
            time += Time.deltaTime;
            yield return null;
        }

        foreach (var fill in fills)
            fill.localScale = end;
    }
}
