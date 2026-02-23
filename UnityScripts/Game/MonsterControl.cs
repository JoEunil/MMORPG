using ClientCore;
using ClientCore.PacketHelper;
using System;
using System.Collections;
using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class MonsterControl : MonoBehaviour
{
    [SerializeField] private Image hpFill;
    private SpriteRenderer spriteRenderer;
    [SerializeField] private GameObject damageTextPrefab; 

    [SerializeField] private Transform damageArea;        

    private Vector3 _targetPos;

    private void Awake()
    {
        spriteRenderer = GetComponent<SpriteRenderer>();
    }
    void Start()
    {
        _targetPos = transform.position;
    }

    void Update()
    {
        if ((_targetPos - transform.position).sqrMagnitude > 0.001f)
        {
            transform.position = Vector3.MoveTowards(transform.position, _targetPos, 10f * Time.deltaTime);
        }
        else
        {
            transform.position = _targetPos;
        }
    }
    public void Teleport(float x, float y)
    {
        transform.position = new Vector3(x, y, transform.position.z);
        _targetPos = new Vector3(x, y, transform.position.z);
    }
    public void UpdatePosition(byte direction, float x, float y)
    { 
        _targetPos = new Vector3(x, y, transform.position.z);
    }

    public void UpdateHP(int current, int max)
    {
        if (hpFill != null)
            hpFill.fillAmount = Mathf.Clamp01((float)current / max);
    }
    public void ShowDamage(uint damage)
    {
        if (damage == 0)
            return;
        if (damageTextPrefab == null || damageArea == null) return;

        // DamageText 생성
        GameObject dmgObj = Instantiate(damageTextPrefab, damageArea);
        TextMeshProUGUI tmp = dmgObj.GetComponent<TextMeshProUGUI>();
        if (tmp != null)
        {
            tmp.text = damage.ToString();
        }

        // 코루틴으로 위로 이동 + 사라짐
        StartCoroutine(FadeAndMoveDamageText(dmgObj));
    }
    private IEnumerator FadeAndMoveDamageText(GameObject obj)
    {
        TextMeshProUGUI tmp = obj.GetComponent<TextMeshProUGUI>();
        Color color = tmp.color;

        float duration = 0.8f;
        float elapsed = 0f;

        Vector3 startPos = obj.GetComponent<RectTransform>().anchoredPosition;
        Vector3 endPos = startPos + new Vector3(0, 50f, 0); // 위로 50px 이동

        while (elapsed < duration)
        {
            elapsed += Time.deltaTime;
            float t = elapsed / duration;

            // 위치 이동
            obj.GetComponent<RectTransform>().anchoredPosition = Vector3.Lerp(startPos, endPos, t);
            // 투명도 감소
            tmp.color = new Color(color.r, color.g, color.b, 1f - t);

            yield return null;
        }

        Destroy(obj);
    }

    public IEnumerator DestroyMonster()
    {
        float duration = 1f; // 사라지는 시간
        float elapsed = 0f;

        Color spriteColor = spriteRenderer.color;
        Color hpColor = hpFill.color;

        while (elapsed < duration)
        {
            elapsed += Time.deltaTime;
            float t = elapsed / duration;

            // Sprite 투명화
            spriteRenderer.color = new Color(spriteColor.r, spriteColor.g, spriteColor.b, 1f - t);
            if (hpFill != null)
                hpFill.color = new Color(hpColor.r, hpColor.g, hpColor.b, 1f - t);

            yield return null;
        }

        // 삭제
        Destroy(gameObject);
    }
}
