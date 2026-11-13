using System;
using System.Collections.Generic;
using System.Text;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace Mmorpg.Client
{
    internal sealed class MonsterInspectionPanel
    {
        private static readonly Color Ink = new Color32(237, 234, 222, 255);
        private static readonly Color Gold = new Color32(231, 205, 147, 255);
        private const float Width = 304;
        private const float PatternWidth = Width - 28;

        private readonly RectTransform root;
        private readonly TMP_Text title;
        private readonly TMP_Text health;
        private readonly TMP_Text attack;
        private readonly TMP_Text pattern;
        private readonly RectTransform healthFill;
        private int lastHp = -1;
        private int lastMaxHp = -1;

        internal MonsterInspectionPanel(Transform canvas, TMP_FontAsset font, Action close)
        {
            var background = Surface(canvas, "Monster Inspection", new Color32(17, 24, 27, 247));
            root = background.rectTransform;
            root.anchorMin = root.anchorMax = root.pivot = Vector2.one;
            root.anchoredPosition = new Vector2(-24, -252);
            background.raycastTarget = true;

            title = Label("Name", 18, Gold, font, 14, -8, Width - 62, 28);
            title.fontStyle = FontStyles.Bold;
            var closeFace = Surface(root, "Close", new Color32(46, 57, 58, 255));
            Place(closeFace.rectTransform, Width - 38, -8, 28, 28);
            closeFace.raycastTarget = true;
            var button = closeFace.gameObject.AddComponent<Button>();
            button.targetGraphic = closeFace;
            button.onClick.AddListener(() => close());
            var closeText = Label("Close Symbol", 20, Ink, font, Width - 38, -7, 28, 28);
            closeText.text = "×";
            closeText.alignment = TextAlignmentOptions.Center;

            health = Label("Health", 15, Ink, font, 14, -41, 168, 22);
            attack = Label("Attack", 15, Ink, font, 180, -41, 110, 22);
            attack.alignment = TextAlignmentOptions.MidlineRight;
            var healthTrack = Surface(root, "Health Track", new Color32(55, 47, 46, 255)).rectTransform;
            Place(healthTrack, 14, -67, PatternWidth, 5);
            healthFill = Surface(healthTrack, "Health Fill", new Color32(200, 83, 73, 255)).rectTransform;
            healthFill.anchorMin = Vector2.zero;
            healthFill.anchorMax = Vector2.one;
            healthFill.offsetMin = healthFill.offsetMax = Vector2.zero;

            var heading = Label("Pattern Heading", 15, Gold, font, 14, -81, PatternWidth, 22);
            heading.text = "공격 패턴";
            pattern = Label("Pattern", 14, Ink, font, 14, -108, PatternWidth, 42);
            pattern.alignment = TextAlignmentOptions.TopLeft;
            pattern.textWrappingMode = TextWrappingModes.Normal;
            pattern.lineSpacing = 5;
            Hide();
        }

        internal void Show(ServerContent.MonsterSpecies species, ServerContent.RingSkill[] skills, int hp, int maxHp)
        {
            title.text = species?.name ?? "몬스터";
            attack.text = species == null ? "공격력 —" : $"공격력 {species.attack:N0}";
            pattern.text = DescribePattern(species?.phases, skills);
            float height = Mathf.Ceil(pattern.GetPreferredValues(pattern.text, PatternWidth, 0).y);
            pattern.rectTransform.sizeDelta = new Vector2(PatternWidth, height);
            root.sizeDelta = new Vector2(Width, 122 + height);
            SetHealth(hp, maxHp);
            root.SetAsLastSibling();
            root.gameObject.SetActive(true);
        }

        internal void SetHealth(int hp, int maxHp)
        {
            if (hp == lastHp && maxHp == lastMaxHp) return;
            lastHp = hp;
            lastMaxHp = maxHp;
            health.text = $"HP {hp:N0} / {maxHp:N0}";
            healthFill.anchorMax = new Vector2(Mathf.Clamp01((float)hp / maxHp), 1);
        }

        internal void Hide()
        {
            if (root != null) root.gameObject.SetActive(false);
        }

        internal void Destroy()
        {
            if (root == null) return;
            if (Application.isPlaying) UnityEngine.Object.Destroy(root.gameObject);
            else UnityEngine.Object.DestroyImmediate(root.gameObject);
        }

        private static string DescribePattern(ServerContent.MonsterPhase[] phases, ServerContent.RingSkill[] skills)
        {
            if (phases == null || phases.Length == 0) return "확인된 패턴 정보 없음";

            var description = new StringBuilder();
            var specials = new List<int>();
            for (int i = 0; i < phases.Length; i++)
            {
                var steps = phases[i]?.steps;
                if (steps == null || steps.Length == 0) continue;
                if (description.Length > 0) description.AppendLine();
                if (phases.Length > 1) description.Append(i + 1).Append("단계  ");
                for (int j = 0; j < steps.Length;)
                {
                    int id = steps[j].skillID;
                    if (j > 0) description.Append(" → ");
                    description.Append(SkillName(id));
                    if (id != 0 && !specials.Contains(id)) specials.Add(id);
                    int end = j + 1;
                    while (end < steps.Length && steps[end].skillID == id) end++;
                    if (end - j > 1) description.Append(" ×").Append(end - j);
                    j = end;
                }
            }

            if (description.Length == 0) return "확인된 패턴 정보 없음";
            foreach (int id in specials)
            {
                var skill = skills == null ? null : Array.Find(skills, candidate => candidate != null && candidate.skillID == id);
                var hit = skill?.phases == null ? null : Array.Find(skill.phases, phase => phase != null && phase.action == SkillAction.Hit);
                if (hit == null) continue;
                string area = hit.range switch
                {
                    SkillRange.Circle => $"반경 {hit.range1:0.#}m",
                    SkillRange.Rectangle => $"길이 {hit.range1:0.#}m · 폭 {hit.range2:0.#}m",
                    SkillRange.Boss1_1 => "부채꼴이 번갈아 타격",
                    SkillRange.Boss2 => "사각 띠가 바깥으로 확장",
                    _ => null
                };
                if (area == null) continue;
                description.AppendLine().Append(SkillName(id)).Append("  ").Append(area);
                if (hit.waitTicks > 0 && (hit.range == SkillRange.Circle || hit.range == SkillRange.Rectangle))
                    description.Append($" · 예고 {hit.waitTicks * .05f:0.##}초");
            }
            return description.ToString();
        }

        private static string SkillName(int id) => id switch
        {
            0 => "기본 공격",
            1 => "원형 강타",
            2 => "전방 내려찍기",
            3 => "교차 부채꼴",
            4 => "확장 충격파",
            5 => "숲의 휩쓸기",
            6 => "넓은 가르기",
            7 => "제단 폭발",
            _ => "미확인 공격"
        };

        private TMP_Text Label(string name, float size, Color color, TMP_FontAsset font,
            float x, float y, float width, float height)
        {
            var label = new GameObject(name, typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TextMeshProUGUI>();
            label.rectTransform.SetParent(root, false);
            Place(label.rectTransform, x, y, width, height);
            label.font = font != null ? font : TMP_Settings.defaultFontAsset;
            label.fontSize = size;
            label.color = color;
            label.alignment = TextAlignmentOptions.MidlineLeft;
            label.textWrappingMode = TextWrappingModes.NoWrap;
            label.richText = false;
            label.raycastTarget = false;
            return label;
        }

        private static Image Surface(Transform parent, string name, Color color)
        {
            var image = new GameObject(name, typeof(RectTransform), typeof(Image)).GetComponent<Image>();
            image.rectTransform.SetParent(parent, false);
            image.color = color;
            image.raycastTarget = false;
            return image;
        }

        private static void Place(RectTransform rect, float x, float y, float width, float height)
        {
            rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
            rect.anchoredPosition = new Vector2(x, y);
            rect.sizeDelta = new Vector2(width, height);
        }
    }
}
