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


def document(title, description, content, references):
    ref_nav = "".join(f'<a href="{escape(url, quote=True)}">{escape(label)}</a>' for label, url in references)
    ref_footer = "".join(f'<a href="{escape(url, quote=True)}">{escape(label)} ↗</a>' for label, url in references)
    return f'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{escape(description, quote=True)}"><title>{escape(title)} · AI 게임 개발 포트폴리오</title>
<link rel="stylesheet" href="{BASE}style.css"></head><body>
<header class="site-header"><a class="brand" href="{BASE}">AI 게임 개발 포트폴리오</a><nav aria-label="주 메뉴"><a href="{BASE}#work">작업 결과</a>{ref_nav}</nav></header>
{content}<footer class="site-footer"><a href="{BASE}">프로젝트 홈</a><span>Unity MMORPG</span>{ref_footer}</footer>
</body></html>'''


def build_work(work, references):
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
    (folder / "index.html").write_text(document(work["title"], work["summary"], page_content, references), encoding="utf-8", newline="\n")
    (folder / "history.html").write_text(document(work["title"] + " 화면 기록", work["summary"], history_content, references), encoding="utf-8", newline="\n")
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
        key, detail, history = build_work(work, references)
        card_image = find_image(work)
        media = f'<a class="card-image" href="{card_image[1]}" target="_blank" rel="noopener"><img src="{card_image[1]}" alt="{escape(card_image[0], quote=True)}" loading="lazy"></a>' if card_image else ""
        cards.append(f'<article class="work-card">{media}<div class="work-copy"><p class="eyebrow">{escape(work["status"])}</p><h3><a href="{detail}">{escape(work["title"])}</a></h3><p>{escape(work["summary"])}</p><div class="card-links"><a href="{detail}">작업 보기 →</a><a href="{history}">화면 이력</a></div></div></article>')
    project_section = next((s for s in home_sections if s["title"] == "프로젝트"), None)
    process_section = next((s for s in home_sections if s["title"] == "함께 만든 방식"), None)
    experience_section = next((s for s in home_sections if s["title"] == "직접 확인하고 바꾼 점"), None)
    if not project_section:
        raise ValueError("SHOWCASE.md needs a concise '프로젝트' section")
    project_copy = render_blocks(project_section["blocks"], HOME_SOURCE)
    process_copy = render_blocks(process_section["blocks"], HOME_SOURCE) if process_section else ""
    experience_copy = render_blocks(experience_section["blocks"], HOME_SOURCE) if experience_section else ""
    process_html = f'<section class="about section-wrap"><div><p class="eyebrow">개발 경험</p><h2>{escape(process_section["title"])}</h2></div><div>{process_copy}</div></section>' if process_copy else ""
    experience_html = f'<section class="experience section-wrap"><h2>{escape(experience_section["title"])}</h2>{experience_copy}</section>' if experience_copy else ""
    hero = f'''<main><section class="home-hero"><div class="home-copy"><h1>{escape(title)}</h1><p class="lead">{inline(intro)}</p><a class="button" href="#work">작업 결과 둘러보기</a></div><figure class="home-image"><a href="{hero_image[1]}" target="_blank" rel="noopener"><img src="{hero_image[1]}" alt="{escape(hero_image[0], quote=True)}"></a><figcaption>{escape(hero_image[0])}</figcaption></figure></section>
<section class="section-wrap project-intro"><h2>{escape(project_section["title"])}</h2>{project_copy}</section>
<section class="section-wrap" id="work"><div class="section-title"><div><p class="eyebrow">작업 사례</p><h2>작업과 결과</h2></div></div><div class="work-grid">{''.join(cards)}</div></section>
{process_html}{experience_html}</main>'''
    (SITE / "index.html").write_text(document(title, intro, hero, references), encoding="utf-8", newline="\n")
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
