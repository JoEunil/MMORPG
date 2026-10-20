#!/usr/bin/env python3
"""Build the public portfolio pages from the approved Markdown copy."""
from html import escape
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
CONTENT = ROOT / "docs/unity-client/portfolio"
HOME_SOURCE = ROOT / "docs/unity-client/SHOWCASE.md"
BASE = "/MMORPG/"
IMAGE = re.compile(r"!\[([^\]]*)\]\(([^)]+)\)")
LINK = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
INTERNAL_CODES = re.compile(r"\b(?:C0[1-9]|PR\s*\d+|[0-9]+/[0-9]+|[0-9a-f]{7,40})\b", re.I)


def slug(text):
    value = re.sub(r"[^\w가-힣-]+", "-", text.lower()).strip("-")
    return value or "work"


def inline(text):
    def render_link(match):
        label, href = match.groups()
        if not href.startswith(("https://", BASE, "#")):
            raise ValueError(f"Unsupported public link: {href}")
        return f'<a href="{escape(href, quote=True)}">{escape(label)}</a>'

    parts = LINK.split(text)
    rendered = [escape(parts[0])]
    for index in range(1, len(parts), 3):
        rendered.append(render_link(LINK.fullmatch(f"[{parts[index]}]({parts[index + 1]})")))
        rendered.append(escape(parts[index + 2]) if index + 2 < len(parts) else "")
    return "".join(rendered)


def local_image_path(source, relative):
    source_path = (ROOT / source).resolve()
    target = (source_path.parent / relative).resolve()
    if not target.is_relative_to(SITE.resolve()) or not target.is_file():
        raise ValueError(f"Missing or out-of-site image in {source}: {relative}")
    return BASE + target.relative_to(SITE).as_posix()


def parse_work(path):
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    if not lines or not lines[0].startswith("# "):
        raise ValueError(f"{path.relative_to(ROOT)} must start with a title")
    title = lines[0][2:].strip()
    summary = status = ""
    sections = []
    section = None
    intro = []
    for raw in lines[1:]:
        line = raw.strip()
        if line.startswith("요약:"):
            summary = line.removeprefix("요약:").strip()
        elif line.startswith("상태:"):
            status = line.removeprefix("상태:").strip()
        elif line.startswith("## "):
            section = {"title": line[3:].strip(), "blocks": []}
            sections.append(section)
        elif line.startswith("### "):
            if section is None:
                raise ValueError(f"Heading without section in {path}")
            section["blocks"].append(("heading", line[4:].strip()))
        elif line:
            (section["blocks"] if section else intro).append(line)
        elif section and section["blocks"] and section["blocks"][-1] != "":
            section["blocks"].append("")
    if not title or not summary or status not in ("검증된 결과", "중간 결과"):
        raise ValueError(f"Incomplete portfolio metadata in {path.relative_to(ROOT)}")
    if INTERNAL_CODES.search(text):
        raise ValueError(f"Internal work identifiers or test counts in public copy: {path.relative_to(ROOT)}")
    if not any(s["title"] == "화면 기록" for s in sections):
        raise ValueError(f"Missing screen history section: {path.relative_to(ROOT)}")
    return {"title": title, "summary": summary, "status": status, "intro": intro, "sections": sections, "source": path}


def parse_showcase():
    lines = HOME_SOURCE.read_text(encoding="utf-8").splitlines()
    if not lines or not lines[0].startswith("# "):
        raise ValueError("SHOWCASE.md must start with a public portfolio title")
    title = lines[0][2:].strip()
    intro = []
    sections = []
    section = None
    for raw in lines[1:]:
        line = raw.strip()
        if line.startswith("## "):
            section = {"title": line[3:].strip(), "blocks": []}
            sections.append(section)
        elif line.startswith("### "):
            if section is None:
                raise ValueError("SHOWCASE heading appears before its section")
            section["blocks"].append(("heading", line[4:].strip()))
        elif line:
            (section["blocks"] if section else intro).append(line)
        elif section and section["blocks"] and section["blocks"][-1] != "":
            section["blocks"].append("")
    if not intro:
        raise ValueError("SHOWCASE.md needs a short public introduction")
    if INTERNAL_CODES.search("\n".join([title, *intro, *(b for s in sections for b in s["blocks"] if isinstance(b, str))])):
        raise ValueError("SHOWCASE.md contains an internal identifier or test count")
    hero_section = next((s for s in sections if s["title"] == "대표 화면"), None)
    if not hero_section:
        raise ValueError("SHOWCASE.md must explicitly select an image under '대표 화면'")
    hero = next((IMAGE.fullmatch(b) for b in hero_section["blocks"] if isinstance(b, str) and IMAGE.fullmatch(b)), None)
    if not hero:
        raise ValueError("SHOWCASE.md '대표 화면' section needs one selected image")
    hero_alt, hero_ref = hero.groups()
    hero_url = local_image_path(HOME_SOURCE.relative_to(ROOT), hero_ref)
    references = []
    ref_section = next((s for s in sections if s["title"] == "참고 링크"), None)
    if ref_section:
        for block in ref_section["blocks"]:
            if isinstance(block, str):
                bullet = re.match(r"^-\s+(.*)$", block)
                if bullet:
                    match = LINK.fullmatch(bullet.group(1))
                    if not match:
                        raise ValueError("Reference links must use [title](https://...) syntax")
                    label, href = match.groups()
                    if not href.startswith("https://"):
                        raise ValueError(f"Reference links must use HTTPS: {href}")
                    references.append((label, href))
    return title, " ".join(intro), sections, (hero_alt, hero_url), references


def render_blocks(blocks, source):
    out = []
    paragraph = []
    list_items = []

    def flush_paragraph():
        if paragraph:
            out.append("<p>" + inline(" ".join(paragraph)) + "</p>")
            paragraph.clear()

    def flush_list():
        if list_items:
            out.append("<ul>" + "".join(f"<li>{inline(item)}</li>" for item in list_items) + "</ul>")
            list_items.clear()

    for block in blocks:
        if block == "":
            flush_paragraph(); flush_list()
            continue
        if isinstance(block, tuple):
            flush_paragraph(); flush_list()
            out.append("<h3>" + escape(block[1]) + "</h3>")
            continue
        image = IMAGE.fullmatch(block)
        bullet = re.match(r"^-\s+(.*)$", block)
        if image:
            flush_paragraph(); flush_list()
            alt, ref = image.groups()
            src = local_image_path(source.relative_to(ROOT), ref)
            out.append(f'<figure><a href="{src}" target="_blank" rel="noopener"><img src="{src}" alt="{escape(alt, quote=True)}" loading="lazy"></a><figcaption>{escape(alt)}</figcaption></figure>')
        elif bullet:
            flush_paragraph()
            list_items.append(bullet.group(1))
        else:
            flush_list()
            paragraph.append(block)
    flush_paragraph(); flush_list()
    return "\n".join(out)


def diagram_items(section):
    groups = {"": []}
    current = groups[""]
    for block in section["blocks"]:
        if isinstance(block, tuple):
            current = groups.setdefault(block[1], [])
        elif isinstance(block, str) and block.startswith("- "):
            current.append(block[2:])
    return groups


def diagram_node(item):
    label, _, detail = item.partition(" — ")
    return f'<strong>{escape(label)}</strong><span>{escape(detail)}</span>'


def render_workflow(section):
    groups = diagram_items(section)
    agents = groups["에이전트 구성"]
    chain = ''.join(f'<li>{diagram_node(item)}</li>' for item in agents[:2])
    roles = ''.join(f'<li>{diagram_node(item)}</li>' for item in agents[2:])
    steps = ''.join(f'<li>{diagram_node(item)}</li>' for item in groups["작업 흐름"])
    feedback = ''.join(f'<li>{diagram_node(item)}</li>' for item in groups["피드백"])
    return f'''<section class="workflow" id="workflow"><div class="section-wrap">
<div class="section-title"><div><p class="eyebrow">사람이 방향을 정하고, AI가 실행합니다</p><h2>{escape(section['title'])}</h2></div><a class="text-link" href="#work">실제 결과 보기 ↗</a></div>
<div class="workflow-layout"><div class="agent-diagram"><h3>누가 어떤 일을 하나요?</h3><ol class="agent-chain" aria-label="사람의 판단에서 총괄 에이전트의 배정으로">{chain}</ol>
<div class="agent-branches"><p class="branch-label">필요한 전문 역할을 배정</p><ul class="agent-roles">{roles}</ul></div></div>
<div class="flow-diagram"><h3>하나의 기능이 만들어지는 흐름</h3><ol class="workflow-steps">{steps}</ol><ul class="workflow-feedback">{feedback}</ul></div></div></div></section>'''


def render_journey(section):
    items = diagram_items(section)[""]
    stages = []
    for item in items:
        label, _, detail = item.partition(" — ")
        status, _, description = detail.partition(" · ")
        stages.append(f'<li><span class="stage-state">{escape(status)}</span><h3>{escape(label)}</h3><p>{escape(description)}</p></li>')
    return f'''<section class="section-wrap journey" id="journey"><div class="section-title"><div><p class="eyebrow">기능 연결에서 플레이 경험으로</p><h2>{escape(section['title'])}</h2></div></div><ol class="journey-track">{''.join(stages)}</ol></section>'''


def render_changes(section):
    groups = diagram_items(section)
    columns = []
    for title, items in groups.items():
        if not title:
            continue
        entries = ''.join(f'<li>{diagram_node(item)}</li>' for item in items)
        columns.append(f'<div class="change-group"><h3>{escape(title)}</h3><ul>{entries}</ul></div>')
    return f'<section class="section-wrap changes" id="changes"><div class="section-title"><h2>{escape(section["title"])}</h2><a class="text-link" href="#work">기능별 화면 보기 ↗</a></div><div class="change-columns">{"".join(columns)}</div></section>'


def render_decisions(section):
    entries = []
    for item in diagram_items(section)[""]:
        label, _, detail = item.partition(" — ")
        before, separator, after = label.partition("에서 ")
        if separator:
            after = after.removesuffix("으로") if after.endswith("으로") else after.removesuffix("로")
            heading = f'<h3 class="decision-shift"><span class="before">{escape(before)}</span><span class="change-arrow" aria-hidden="true">→</span><span class="after">{escape(after)}</span></h3>'
        else:
            heading = f'<h3 class="decision-shift">{escape(label)}</h3>'
        entries.append(f'<li>{heading}<span>{escape(detail)}</span></li>')
    return f'<section class="section-wrap decisions" id="decisions"><div class="section-title"><div><p class="eyebrow">개발하며 바뀐 선택</p><h2>{escape(section["title"])}</h2></div></div><ol class="decision-track">{"".join(entries)}</ol></section>'


def document(title, description, content, references, site_title):
    ref_footer = "".join(f'<a href="{escape(url, quote=True)}">{escape(label)} ↗</a>' for label, url in references)
    browser_title = title if title == site_title else f"{title} · {site_title}"
    return f'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{escape(description, quote=True)}"><title>{escape(browser_title)}</title>
<link rel="stylesheet" href="{BASE}style.css"></head><body>
<header class="site-header"><a class="brand" href="{BASE}">{escape(site_title)}</a><nav aria-label="주 메뉴"><a href="{BASE}#workflow">AI 워크플로우</a><a href="{BASE}#journey">목표와 진행</a><a href="{BASE}#work">작업 결과</a><a href="{BASE}#legacy">기존 Unity</a></nav></header>
{content}<footer class="site-footer"><a href="{BASE}">{escape(site_title)}</a>{ref_footer}</footer>
</body></html>'''


def build_work(work, references, site_title):
    key = slug(work["source"].stem)
    detail_url = f"{BASE}work/{key}/index.html"
    history_url = f"{BASE}work/{key}/history.html"
    sections = []
    history = []
    for section in work["sections"]:
        rendered = render_blocks(section["blocks"], work["source"])
        if section["title"] == "화면 기록":
            history.append(rendered)
        else:
            sections.append(f'<section class="content-section"><h2>{escape(section["title"])}</h2>{rendered}</section>')
    preview = find_image(work)
    preview_html = f'<figure class="detail-preview"><a href="{preview[1]}" target="_blank" rel="noopener"><img src="{preview[1]}" alt="{escape(preview[0], quote=True)}"></a><figcaption>{escape(preview[0])}</figcaption></figure>' if preview else ""
    page_content = f'''<main class="page narrow"><a class="back-link" href="{BASE}#work">← 작업 목록</a>
<p class="eyebrow">{escape(work['status'])}</p><h1>{escape(work['title'])}</h1><p class="lead">{escape(work['summary'])}</p>
<div class="page-actions"><a class="button" href="{history_url}">화면 변화 보기</a></div>
{preview_html}
{''.join(sections)}</main>'''
    history_content = f'''<main class="page narrow"><a class="back-link" href="{detail_url}">← 작업으로 돌아가기</a>
<p class="eyebrow">화면 기록</p><h1>{escape(work['title'])}</h1><p class="lead">작업 과정에서 남긴 실제 화면과 중간 결과입니다.</p><section class="content-section history-images">{''.join(history)}</section></main>'''
    folder = SITE / "work" / key
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "index.html").write_text(document(work["title"], work["summary"], page_content, references, site_title), encoding="utf-8", newline="\n")
    (folder / "history.html").write_text(document(work["title"] + " 화면 기록", work["summary"], history_content, references, site_title), encoding="utf-8", newline="\n")
    return key, detail_url, history_url


def find_image(work):
    for section in work["sections"]:
        for block in section["blocks"]:
            match = IMAGE.fullmatch(block) if isinstance(block, str) else None
            if match:
                alt, ref = match.groups()
                return alt, local_image_path(work["source"].relative_to(ROOT), ref)
    return None


def main():
    if not CONTENT.is_dir():
        raise SystemExit(f"Portfolio content directory not found: {CONTENT.relative_to(ROOT)}")
    works = [parse_work(path) for path in sorted(CONTENT.glob("*.md"))]
    if not works:
        raise SystemExit("Add at least one reviewed project page under docs/unity-client/portfolio/")
    title, intro, home_sections, hero_image, references = parse_showcase()
    cards = []
    for work in works:
        key, detail, history = build_work(work, references, title)
        card_image = find_image(work)
        media = f'<a class="card-image" href="{card_image[1]}" target="_blank" rel="noopener"><img src="{card_image[1]}" alt="{escape(card_image[0], quote=True)}" loading="lazy"></a>' if card_image else ""
        cards.append(f'<article class="work-card">{media}<div class="work-copy"><p class="eyebrow">{escape(work["status"])}</p><h3><a href="{detail}">{escape(work["title"])}</a></h3><p>{escape(work["summary"])}</p><div class="card-links"><a href="{detail}">작업 보기 →</a><a href="{history}">화면 이력</a></div></div></article>')
    sections_by_title = {section["title"]: section for section in home_sections}
    process_html = render_workflow(sections_by_title["AI Agent 워크플로우"])
    journey_html = render_journey(sections_by_title["큰 목표와 진행 흐름"])
    changes_html = render_changes(sections_by_title["달라진 구현"])
    decisions_html = render_decisions(sections_by_title["방향 변경 이력"])
    legacy_section = sections_by_title["기존 Unity 구현"]
    legacy_copy = render_blocks(legacy_section["blocks"], HOME_SOURCE)
    legacy_html = f'<section class="legacy-section section-wrap" id="legacy"><div class="section-title"><div><p class="eyebrow">개발의 출발점</p><h2>{escape(legacy_section["title"])}</h2></div><a class="text-link" href="#changes">현재 달라진 점 ↓</a></div><div class="legacy-body">{legacy_copy}</div></section>'
    hero = f'''<main id="top"><section class="home-hero"><div class="home-copy"><p class="eyebrow">MMORPG / UNITY / AI AGENTS</p><h1>{escape(title)}</h1><p class="lead">{inline(intro)}</p><div class="hero-actions"><a class="button" href="#workflow">어떻게 만들고 있나요? ↓</a><a class="text-link" href="#work">작업 결과 둘러보기 ↗</a></div></div><figure class="home-image"><a href="{hero_image[1]}" target="_blank" rel="noopener"><img src="{hero_image[1]}" alt="{escape(hero_image[0], quote=True)}"></a><figcaption>{escape(hero_image[0])}</figcaption></figure></section>
{process_html}
{journey_html}
{legacy_html}
{changes_html}
<section class="section-wrap work-section" id="work"><div class="section-title"><div><p class="eyebrow">기능별 실제 화면과 변화</p><h2>작업과 결과</h2></div><a class="text-link" href="#decisions">방향이 바뀐 과정 ↓</a></div><div class="work-grid">{''.join(cards)}</div></section>
{decisions_html}</main>'''
    (SITE / "index.html").write_text(document(title, intro, hero, references, title), encoding="utf-8", newline="\n")
    check_site()
    print(f"Built home and {len(works)} project pages with screen histories")


def check_site():
    html_files = sorted(SITE.rglob("*.html"))
    if not html_files:
        raise ValueError("No generated HTML found")
    for path in html_files:
        content = path.read_text(encoding="utf-8")
        if '<html lang="ko">' not in content:
            raise ValueError(f"Missing Korean document language: {path}")
        for ref in re.findall(r'(?:src|href)="([^"]+)"', content):
            parsed = urlsplit(ref)
            if parsed.scheme in ("https", "http", "mailto") or ref.startswith("//"):
                continue
            if parsed.path.startswith(BASE):
                relative = unquote(parsed.path[len(BASE):]) or "index.html"
                target = (SITE / relative).resolve()
            elif parsed.path.startswith("/"):
                raise ValueError(f"Unexpected root-absolute URL outside Pages base: {ref}")
            else:
                relative = unquote(parsed.path)
                target = path.resolve() if not relative else (path.parent / relative).resolve()
            if target.is_dir():
                target = target / "index.html"
            if not target.is_relative_to(SITE.resolve()):
                raise ValueError(f"Local URL escapes site output: {ref}")
            if not target.is_file():
                raise ValueError(f"Broken local link or asset: {path.relative_to(ROOT)} -> {ref}")
            if parsed.fragment and target.suffix.lower() == ".html":
                ids = set(re.findall(r'\bid="([^"]+)"', target.read_text(encoding="utf-8")))
                if unquote(parsed.fragment) not in ids:
                    raise ValueError(f"Broken local anchor: {path.relative_to(ROOT)} -> {ref}")
    if any("/docs/" in str(p) for p in html_files):
        raise ValueError("Internal documents must not be included in public output")


if __name__ == "__main__":
    main()
