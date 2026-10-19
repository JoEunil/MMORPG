#!/usr/bin/env python3
"""Render the deliberately small Markdown subset used by SHOWCASE.md."""
from html import escape
from pathlib import Path
import re
from urllib.parse import urljoin

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "docs/unity-client/SHOWCASE.md"
OUTPUT = ROOT / "site/index.html"


def inline(text):
    parts = re.split(r"(`[^`]+`|\*\*[^*]+\*\*|\[[^\]]+\]\([^)]+\))", text)
    rendered = []
    for part in parts:
        if part.startswith("**") and part.endswith("**"):
            rendered.append("<strong>" + escape(part[2:-2]) + "</strong>")
        elif part.startswith("`") and part.endswith("`"):
            rendered.append("<code>" + escape(part[1:-1]) + "</code>")
        elif part.startswith("["):
            match = re.fullmatch(r"\[([^\]]+)\]\(([^)]+)\)", part)
            if not match:
                raise ValueError(f"Unsupported link syntax: {part}")
            label, href = match.groups()
            if href.startswith("../"):
                href = urljoin("https://github.com/JoEunil/MMORPG/blob/master/docs/unity-client/SHOWCASE.md", href)
            elif not (href.startswith("https://") or href.startswith("http://") or href.startswith("#")):
                raise ValueError(f"Links must be absolute web URLs, anchors, or document-relative links: {href}")
            rendered.append(f'<a href="{escape(href, quote=True)}">{escape(label)}</a>')
        else:
            rendered.append(escape(part))
    return "".join(rendered)


def slug(text):
    value = re.sub(r"[^\w가-힣 -]", "", text.lower()).strip().replace(" ", "-")
    return value or "section"


def nav_label(title):
    labels = {
        "왜 시작했나": "소개",
        "에이전트가 협업하는 방식": "협업",
        "서버 기능을 Unity 화면으로": "구현",
        "실제 플레이 화면": "화면",
        "대표 화면": "화면",
        "검증 결과와 현재 범위": "검증",
        "다음에 이어갈 일": "다음",
        "소스와 진행 기록": "자료",
    }
    return labels.get(title, title)


def parse(source):
    lines = source.splitlines()
    title = None
    intro = None
    status = None
    sections = []
    current = None
    for line in lines:
        line = line.strip()
        if not line:
            if current is not None:
                current["blocks"].append("")
            continue
        if line == "---":
            continue
        if line.startswith("# "):
            title = line[2:].strip()
        elif line.startswith("## "):
            current = {"title": line[3:].strip(), "blocks": []}
            sections.append(current)
        elif current is None:
            if line.startswith("**현재 상태:**"):
                status = line[len("**현재 상태:**"):].strip()
            elif intro is None:
                intro = line
            else:
                raise ValueError(f"Unexpected text before first section: {line}")
        elif line.startswith("### "):
            current["blocks"].append(("h3", line[4:].strip()))
        else:
            current["blocks"].append(line)
    if not title or not intro or not status or not sections:
        raise ValueError("SHOWCASE.md needs a title, intro, current status, and H2 sections")
    return title, intro, status, sections


def render_blocks(lines, section_title, extract_hero=False):
    out = []
    paragraph = []
    list_kind = None
    list_items = []
    image = None

    def flush_paragraph():
        if paragraph:
            out.append("<p>" + inline(" ".join(paragraph)) + "</p>")
            paragraph.clear()

    def flush_list():
        nonlocal list_kind, list_items
        if list_items:
            tag = "ol" if list_kind == "ol" else "ul"
            klass = ' class="role-list"' if "협업" in section_title and tag == "ul" else ""
            out.append(f"<{tag}{klass}>" + "".join(f"<li>{inline(item)}</li>" for item in list_items) + f"</{tag}>")
            list_items = []
            list_kind = None

    for line in lines:
        if line == "":
            flush_paragraph()
            flush_list()
            continue
        if isinstance(line, tuple) and line[0] == "h3":
            flush_paragraph()
            flush_list()
            out.append("<h3>" + escape(line[1]) + "</h3>")
            continue
        match = re.fullmatch(r"!\[([^\]]+)\]\(([^)]+)\)", line)
        if match:
            flush_paragraph()
            flush_list()
            alt, src = match.groups()
            if src.startswith(("/", "http:", "https:", "..")):
                raise ValueError(f"Showcase images must use site-relative paths: {src}")
            if not src.startswith(("/", "http:", "https:", "..")):
                image_path = OUTPUT.parent / src
                if not image_path.is_file():
                    raise ValueError(f"Referenced image does not exist: site/{src}")
            if image is None:
                image = (alt, src)
                if extract_hero:
                    continue
            out.append(f'<figure class="showcase-image"><img src="{escape(src, quote=True)}" alt="{escape(alt, quote=True)}" loading="lazy"><figcaption>{escape(alt)}</figcaption></figure>')
            continue
        ordered = re.match(r"^\d+\.\s+(.*)$", line)
        bullet = re.match(r"^-\s+(.*)$", line)
        if ordered or bullet:
            flush_paragraph()
            kind = "ol" if ordered else "ul"
            if list_kind and list_kind != kind:
                flush_list()
            list_kind = kind
            list_items.append((ordered or bullet).group(1))
            continue
        flush_list()
        paragraph.append(line)
    flush_paragraph()
    flush_list()
    return "\n".join(out), image


def main():
    source = SOURCE.read_text(encoding="utf-8")
    title, intro, status, sections = parse(source)
    hero = None
    rendered_sections = []
    for section in sections:
        is_hero_section = section["title"] in ("실제 플레이 화면", "대표 화면")
        content, image = render_blocks(section["blocks"], section["title"], is_hero_section)
        if is_hero_section and image:
            hero = image
        if content:
            sid = slug(section["title"])
            rendered_sections.append(f'<section class="chapter" id="{sid}"><div class="chapter-heading"><span class="chapter-kicker">PROJECT NOTES</span><h2>{escape(section["title"])}</h2></div><div class="chapter-body">{content}</div></section>')
    if not hero:
        raise ValueError("SHOWCASE.md must include a representative image under '대표 화면'")
    alt, src = hero
    image_path = OUTPUT.parent / src
    if not image_path.is_file():
        raise ValueError(f"Referenced image does not exist: site/{src}")
    nav = "".join(f'<a href="#{slug(s["title"])}">{escape(nav_label(s["title"]))}</a>' for s in sections)
    first_section_id = slug(sections[0]["title"])
    html = f'''<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="{escape(intro, quote=True)}">
  <title>{escape(title)} · AI-AGENT</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <header class="topbar"><a class="wordmark" href="#top"><span class="sigil">A</span><span>AI-AGENT<small>PROJECT FIELD GUIDE</small></span></a><nav aria-label="페이지 목차">{nav}</nav><a class="source-link" href="https://github.com/JoEunil/MMORPG">GitHub <span aria-hidden="true">↗</span></a></header>
  <main id="top">
    <section class="hero" aria-labelledby="hero-title">
      <div class="hero-copy"><p class="eyebrow"><span class="live-dot"></span> UNITY · SERVER · AI WORKFLOW</p><h1 id="hero-title">{escape(title)}</h1><p class="dek">{inline(intro)}</p><p class="status-pill">{inline(status)}</p><a class="scroll-cue" href="#{first_section_id}">프로젝트 둘러보기 <span aria-hidden="true">↓</span></a><p class="hero-index">FIELD GUIDE <b>01 / {len(sections):02d}</b></p></div>
      <figure class="hero-art"><img src="{escape(src, quote=True)}" alt="{escape(alt, quote=True)}"><figcaption><span>IN THE FIELD</span> {escape(alt)}</figcaption><span class="frame-mark mark-tl"></span><span class="frame-mark mark-br"></span></figure>
      <div class="hero-rail"><span>BUILDING A LIVING WORLD</span><span>SEOUL · 2026</span></div>
    </section>
    <div class="content-shell">{''.join(rendered_sections)}</div>
  </main>
  <footer><span class="wordmark"><span class="sigil">A</span><span>AI-AGENT<small>PROJECT FIELD GUIDE</small></span></span><p>검증된 작업과 현재 범위를 공개합니다.</p><a href="https://github.com/JoEunil/MMORPG">저장소 열기 ↗</a></footer>
</body>
</html>
'''
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(html, encoding="utf-8", newline="\n")
    print(f"Rendered {OUTPUT.relative_to(ROOT)} ({len(rendered_sections)} sections)")


if __name__ == "__main__":
    main()
