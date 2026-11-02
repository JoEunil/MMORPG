#!/usr/bin/env python3
"""Build the public portfolio pages from the approved Markdown copy."""
from html import escape
from pathlib import Path
import argparse
import re
import shutil
import xml.etree.ElementTree as ET
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
CONTENT = ROOT / "docs/unity-client/portfolio"
HOME_SOURCE = ROOT / "docs/unity-client/SHOWCASE.md"
TOUR_SOURCE = ROOT / "docs/unity-client/CONTENT_TOUR.md"
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
        return escape(label)

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


def image_size_attributes(url):
    # Reserve each capture's real aspect ratio so lazy loading cannot move history anchors.
    path = SITE / unquote(url[len(BASE):])
    if path.suffix.lower() == ".svg":
        svg = ET.parse(path).getroot()
        viewbox = svg.get("viewBox", "").split()
        if len(viewbox) == 4:
            width, height = float(viewbox[2]), float(viewbox[3])
        else:
            width = float(svg.get("width", "0").removesuffix("px"))
            height = float(svg.get("height", "0").removesuffix("px"))
        if width <= 0 or height <= 0:
            raise ValueError(f"Invalid SVG dimensions: {path}")
        return f'width="{round(width)}" height="{round(height)}"'
    with path.open("rb") as image:
        header = image.read(24)
        if header.startswith(b"\x89PNG\r\n\x1a\n"):
            width, height = int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")
        elif header.startswith((b"GIF87a", b"GIF89a")):
            width, height = int.from_bytes(header[6:8], "little"), int.from_bytes(header[8:10], "little")
        elif header.startswith(b"\xff\xd8"):
            image.seek(2)
            while True:
                if image.read(1) != b"\xff":
                    raise ValueError(f"Invalid JPEG capture: {path}")
                marker = image.read(1)
                while marker == b"\xff":
                    marker = image.read(1)
                length = int.from_bytes(image.read(2), "big")
                if marker in (b"\xc0", b"\xc1", b"\xc2", b"\xc3", b"\xc5", b"\xc6", b"\xc7", b"\xc9", b"\xca", b"\xcb", b"\xcd", b"\xce", b"\xcf"):
                    size = image.read(5)
                    height, width = int.from_bytes(size[1:3], "big"), int.from_bytes(size[3:5], "big")
                    break
                if length < 2 or marker in (b"", b"\xda", b"\xd9"):
                    raise ValueError(f"Missing JPEG capture dimensions: {path}")
                image.seek(length - 2, 1)
        else:
            raise ValueError(f"Unsupported capture format: {path}")
    if not width or not height:
        raise ValueError(f"Invalid capture dimensions: {path}")
    return f'width="{width}" height="{height}"'


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


def parse_showcase(source=HOME_SOURCE):
    lines = source.read_text(encoding="utf-8").splitlines()
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
    return title, " ".join(intro), sections, references


def render_blocks(blocks, source, heading_prefix=None):
    out = []
    paragraph = []
    list_items = []
    table_rows = []
    heading_index = 0

    def flush_paragraph():
        if paragraph:
            out.append("<p>" + inline(" ".join(paragraph)) + "</p>")
            paragraph.clear()

    def flush_list():
        if list_items:
            out.append("<ul>" + "".join(f"<li>{inline(item)}</li>" for item in list_items) + "</ul>")
            list_items.clear()

    def flush_table():
        if not table_rows:
            return
        if len(table_rows) < 2 or not all(re.fullmatch(r":?-{3,}:?", cell) for cell in table_rows[1]):
            raise ValueError(f"A public Markdown table needs a header separator: {source}")
        columns = len(table_rows[0])
        if any(len(row) != columns for row in table_rows):
            raise ValueError(f"Inconsistent public table columns: {source}")
        head = ''.join(f'<th scope="col">{inline(cell)}</th>' for cell in table_rows[0])
        rows = ''.join('<tr>' + ''.join(f'<td>{inline(cell)}</td>' for cell in row) + '</tr>' for row in table_rows[2:])
        out.append(f'<div class="content-table"><table><thead><tr>{head}</tr></thead><tbody>{rows}</tbody></table></div>')
        table_rows.clear()

    for block in blocks:
        if isinstance(block, str) and LINK.fullmatch(block):
            # Standalone navigation copy remains in Markdown, not in detail-page flows.
            continue
        if isinstance(block, str) and block.startswith("|") and block.endswith("|"):
            flush_paragraph(); flush_list()
            table_rows.append([cell.strip() for cell in block[1:-1].split("|")])
            continue
        flush_table()
        if block == "":
            flush_paragraph(); flush_list()
            continue
        if isinstance(block, tuple):
            flush_paragraph(); flush_list()
            heading_index += 1
            anchor = f' id="{heading_prefix}-{heading_index}"' if heading_prefix else ""
            out.append(f"<h3{anchor}>" + escape(block[1]) + "</h3>")
            continue
        image = IMAGE.fullmatch(block)
        bullet = re.match(r"^-\s+(.*)$", block)
        if image:
            flush_paragraph(); flush_list()
            alt, ref = image.groups()
            src = local_image_path(source.relative_to(ROOT), ref)
            out.append(f'<figure><img src="{src}" alt="{escape(alt, quote=True)}" {image_size_attributes(src)} loading="lazy"><figcaption>{escape(alt)}</figcaption></figure>')
        elif bullet:
            flush_paragraph()
            list_items.append(bullet.group(1))
        else:
            flush_list()
            paragraph.append(block)
    flush_paragraph(); flush_list(); flush_table()
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
    return f'<strong>{escape(label)}</strong>' + (f'<span>{escape(detail)}</span>' if detail else "")


def render_workflow(section):
    desktop = BASE + "media/diagrams/agent-workflow-desktop.svg"
    mobile = BASE + "media/diagrams/agent-workflow-mobile.svg"
    # Validate both responsive sources, including their intrinsic aspect ratios.
    desktop_size = image_size_attributes(desktop)
    mobile_size = image_size_attributes(mobile)
    intro = next(b for b in section["blocks"] if isinstance(b, str) and b and not b.startswith("- "))
    return f'''<section class="workflow" id="workflow"><div class="section-wrap"><div class="section-title"><h2>Agent 워크플로우</h2><p>{inline(intro)}</p></div><picture class="workflow-picture"><source media="(max-width: 760px)" srcset="{mobile}" {mobile_size}><img src="{desktop}" {desktop_size} alt="사용자와 총괄 Agent, 필요한 전문 역할의 배정·검수·재작업·기록 흐름" loading="lazy"></picture></div></section>'''


def render_agent_usage(section, case=""):
    # Publish the reconciled role totals, including explicitly unassigned historical activations.
    ledger = (ROOT / "docs/unity-client/AGENT_USAGE.md").read_text(encoding="utf-8")
    summary = ledger.split("## 공개 집계\n", 1)[1].split("\n## ", 1)[0]
    date = re.search(r"^기준일: (.+)$", summary, re.M).group(1)
    scope = re.search(r"^상세 범위: (.+)$", summary, re.M).group(1)
    new = int(re.search(r"^누적 신규: (\d+)$", summary, re.M).group(1))
    resumed = int(re.search(r"^누적 재활성화: (\d+)$", summary, re.M).group(1))
    agents = []
    for line in summary.splitlines():
        match = re.fullmatch(r"\| ([^|]+) \| ([^|]+) \| (\d+) \| (\d+) \|", line)
        if match:
            agents.append((match[2].strip(), int(match[3]), int(match[4])))
    if not agents or sum(a[1] for a in agents) != new or sum(a[2] for a in agents) != resumed:
        raise ValueError("Public role counts must equal the recorded cumulative totals")
    role_new = sum(a[1] for a in agents)
    role_resumed = sum(a[2] for a in agents)
    peak = max(a[1] + a[2] for a in agents)
    rows = ''.join(f'<tr><th scope="row">{escape(name)}<span class="usage-bar" style="--fill:{(created + restarted) / peak * 100:.1f}%" aria-hidden="true"></span></th><td>{created}</td><td>{restarted}</td><td>{created + restarted}</td></tr>' for name, created, restarted in agents)
    copy = render_blocks(section["blocks"], HOME_SOURCE)
    return f'''<section class="section-wrap agent-usage" id="agent-usage"><div class="section-title"><h2>활용 기록</h2></div>{case}
<div class="usage-copy">{copy}</div><dl class="usage-totals"><div><dt>전체 실행</dt><dd>{new + resumed}<span>회</span></dd></div><div><dt>새 Agent 실행</dt><dd>{new}<span>회</span></dd></div><div><dt>후속 작업 실행</dt><dd>{resumed}<span>회</span></dd></div></dl>
<h3>역할별 실행 횟수</h3><p class="usage-scope">{escape(scope)} · {escape(date)} 기준</p>
<div class="usage-table-wrap"><table class="usage-table"><caption class="sr-only">기록 시작 이후 역할별 새 Agent 실행·후속 작업 실행·전체 실행 횟수</caption><thead><tr><th scope="col">역할</th><th scope="col">새 Agent 실행</th><th scope="col">후속 작업 실행</th><th scope="col">전체 실행</th></tr></thead><tbody>{rows}</tbody><tfoot><tr><th scope="row">전체 합계</th><td>{role_new}</td><td>{role_resumed}</td><td>{role_new + role_resumed}</td></tr></tfoot></table></div></section>'''


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
    return f'<section class="section-wrap decisions" id="decisions"><div class="section-title"><h2>지침 변경</h2></div><ol class="decision-track">{"".join(entries)}</ol></section>'


def render_comparison_case(section):
    opening, examples = [], []
    for block in section["blocks"]:
        if isinstance(block, str) and IMAGE.fullmatch(block):
            examples.append([block])
        elif examples:
            examples[-1].append(block)
        else:
            opening.append(block)
    gallery = ''.join(f'<article class="case-example">{render_blocks(blocks, HOME_SOURCE)}</article>' for blocks in examples)
    return render_blocks(opening, HOME_SOURCE) + f'<div class="case-examples">{gallery}</div>'


def document(title, description, content, references, site_title):
    browser_title = title if title == site_title else f"{title} · {site_title}"
    return f'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{escape(description, quote=True)}"><title>{escape(browser_title)}</title>
<link rel="stylesheet" href="{BASE}style.css"></head><body>
<a class="skip-link" href="#top">본문으로 바로가기</a>
<header class="site-header"><a class="brand" href="{BASE}">{escape(site_title)}</a><nav aria-label="주 메뉴"><a href="{BASE}play/">콘텐츠 둘러보기</a><a href="{BASE}">작업과 결과</a><a href="{BASE}experience/">경험과 개선</a><a href="{BASE}process/">Agent 작업 방식</a></nav></header>
{content}<footer class="site-footer">{escape(site_title)}</footer>
</body></html>'''


def build_tour(references, site_title, require_media=False):
    title, intro, sections, _ = parse_showcase(TOUR_SOURCE)
    if require_media:
        clips = [IMAGE.fullmatch(b.removeprefix("영상: ")) for s in sections for b in s["blocks"] if isinstance(b, str) and b.startswith("영상: ")]
        expected = {"skill-strike.gif", "skill-sweep.gif", "skill-cleave.gif", "market-flow.gif", "market-return.gif", "chat-global-zone.gif", "chat-zone.gif", "chat-whisper.gif"}
        if len(clips) != 8 or any(c is None for c in clips) or {Path(c[2]).name for c in clips} != expected:
            raise ValueError("The completed tour must reference all eight required recordings")
        for clip in clips:
            image_size_attributes(local_image_path(TOUR_SOURCE.relative_to(ROOT), clip[2]))
            poster_ref = str(Path(clip[2]).with_name(Path(clip[2]).stem + "-poster.png"))
            image_size_attributes(local_image_path(TOUR_SOURCE.relative_to(ROOT), poster_ref))
    rendered = []
    first_clip = True
    for section in sections:
        blocks = []
        body = []
        for block in section["blocks"]:
            # Optional recordings live in Markdown; absent clips leave no fake previews.
            clip = IMAGE.fullmatch(block.removeprefix("영상: ")) if isinstance(block, str) and block.startswith("영상: ") else None
            if clip:
                body.append(render_blocks(blocks, TOUR_SOURCE))
                blocks.clear()
                alt, ref = clip.groups()
                target = (TOUR_SOURCE.parent / ref).resolve()
                if not target.is_relative_to(SITE.resolve()):
                    raise ValueError(f"Out-of-site tour clip: {ref}")
                if target.is_file():
                    src = local_image_path(TOUR_SOURCE.relative_to(ROOT), ref)
                    poster = target.with_name(target.stem + "-poster.png")
                    preview = ""
                    if poster.is_file():
                        poster_url = BASE + poster.relative_to(SITE.resolve()).as_posix()
                        loading = 'fetchpriority="high"' if first_clip else 'loading="lazy"'
                        preview = f'<img class="tour-poster" src="{poster_url}" alt="{escape(alt, quote=True)} 정지 화면" {image_size_attributes(poster_url)} {loading}>'
                    clip_class = "tour-clip tour-chat" if target.stem.startswith("chat-") else "tour-clip"
                    body.append(f'<figure class="{clip_class}"><details><summary><span>{escape(alt)} · GIF 펼쳐 보기</span>{preview}</summary><img src="{src}" alt="{escape(alt, quote=True)}" {image_size_attributes(src)} loading="lazy"></details><figcaption>{escape(alt)}</figcaption></figure>')
                    first_clip = False
            else:
                blocks.append(block)
        body.append(render_blocks(blocks, TOUR_SOURCE))
        rendered.append(f'<section class="content-section" id="{slug(section["title"])}"><h2>{escape(section["title"])}</h2>{"".join(body)}</section>')
    content = f'''<main class="page tour-page" id="top" tabindex="-1"><h1>{escape(title)}</h1><p class="lead">{inline(intro)}</p>{"".join(rendered)}</main>'''
    folder = SITE / "play"
    folder.mkdir(exist_ok=True)
    (folder / "index.html").write_text(document(title, intro, content, references, site_title), encoding="utf-8", newline="\n")



def render_history(section, source):
    records = []
    heading, blocks = None, []
    def flush():
        if heading is not None or blocks:
            index = len(records) + 1
            label = heading or "이전 화면"
            records.append(f'<details class="history-record" id="record-{index}"><summary>{escape(label)}</summary><div class="history-record-body">{render_blocks(blocks, source)}</div></details>')
    for block in section["blocks"]:
        if isinstance(block, tuple):
            flush()
            heading, blocks = block[1], []
        else:
            blocks.append(block)
    flush()
    return ''.join(records)


def build_work(work, references, site_title, works):
    key = slug(work["source"].stem)
    detail_url = f"{BASE}work/{key}/index.html"
    history_url = detail_url + "#history"
    sections = []
    history = ""
    for section in work["sections"]:
        if section["title"] == "대표 화면":
            continue
        if section["title"] == "화면 기록":
            history = render_history(section, work["source"])
        else:
            anchor = "features" if section["title"] == "구현한 기능" else "scope" if section["title"] == "현재 확인한 범위" else slug(section["title"])
            rendered = render_blocks(section["blocks"], work["source"])
            sections.append(f'<section class="content-section" id="{anchor}"><h2>{escape(section["title"])}</h2>{rendered}</section>')
    preview_section = next(s for s in work["sections"] if s["title"] == "대표 화면")
    preview_html = render_blocks(preview_section["blocks"], work["source"])
    page_content = f'''<main class="page narrow work-detail" id="top" tabindex="-1">
<p class="work-status">{escape(work['status'])}</p><h1>{escape(work['title'])}</h1><p class="lead">{escape(work['summary'])}</p>
<div class="detail-preview" id="preview">{preview_html}</div>
{''.join(sections)}<section class="content-section history-images" id="history"><h2>화면 이력</h2>{history}</section></main>'''
    folder = SITE / "work" / key
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "index.html").write_text(document(work["title"], work["summary"], page_content, references, site_title), encoding="utf-8", newline="\n")
    # Keep old bookmarks as direct entries into the integrated detail page.
    redirect = document(work["title"] + " 화면 이력", work["summary"], f'<main class="page narrow" id="top"><a href="{history_url}">화면 이력 보기</a></main>', references, site_title)
    redirect = redirect.replace("</head>", f'<link rel="canonical" href="{history_url}"><meta http-equiv="refresh" content="0;url={history_url}"><script>location.replace("{detail_url}" + (/^#record-\\d+$/.test(location.hash) ? location.hash : "#history"));</script></head>')
    (folder / "history.html").write_text(redirect, encoding="utf-8", newline="\n")
    return key, detail_url, history_url


def find_image(work):
    section = next((s for s in work["sections"] if s["title"] == "대표 화면"), None)
    images = [IMAGE.fullmatch(b) for b in section["blocks"] if isinstance(b, str) and IMAGE.fullmatch(b)] if section else []
    if len(images) != 1:
        raise ValueError(f"Select exactly one representative image in {work['source']}")
    alt, ref = images[0].groups()
    return alt, local_image_path(work["source"].relative_to(ROOT), ref)


def sync_readme_preview(home_sections):
    section = next(s for s in home_sections if s["title"] == "README 대표 이미지")
    images = [IMAGE.fullmatch(b) for b in section["blocks"] if isinstance(b, str) and IMAGE.fullmatch(b)]
    if len(images) != 1:
        raise ValueError("Select exactly one README image in SHOWCASE.md")
    url = local_image_path(HOME_SOURCE.relative_to(ROOT), images[0][2])
    source = SITE / unquote(url[len(BASE):])
    target = SITE / "media/ai-agent-preview.png"
    if source == target or source.suffix.lower() != ".png":
        raise ValueError("README preview must select an original PNG, not its fixed-path copy")
    if (ROOT / "README.md").read_text(encoding="utf-8").count('src="site/media/ai-agent-preview.png"') != 1:
        raise ValueError("README must display the fixed preview path exactly once")
    shutil.copyfile(source, target)


def main(require_tour_media=False):
    if not CONTENT.is_dir():
        raise SystemExit(f"Portfolio content directory not found: {CONTENT.relative_to(ROOT)}")
    # Earlier stages now share the market's detail and screen history.
    legacy_market_pages = ("bazaar-search", "my-listings", "item-market")
    work_order = ("field-travel", "player-hud", "inventory", "market", "server-content")
    paths = [path for path in CONTENT.glob("*.md") if path.stem not in legacy_market_pages]
    paths.sort(key=lambda path: (work_order.index(path.stem) if path.stem in work_order else len(work_order), path.stem))
    works = [parse_work(path) for path in paths]
    if not works:
        raise SystemExit("Add at least one reviewed project page under docs/unity-client/portfolio/")
    title, intro, home_sections, references = parse_showcase()
    sync_readme_preview(home_sections)
    cards = {}
    for work in works:
        key, detail, history = build_work(work, references, title, works)
        card_image = find_image(work)
        media = f'<figure class="work-media"><div class="card-image"><img src="{card_image[1]}" alt="{escape(card_image[0], quote=True)}" {image_size_attributes(card_image[1])} loading="lazy"></div><figcaption>{escape(card_image[0])}</figcaption></figure>'
        cards[key] = f'<article class="featured-work" id="featured-{key}"><a class="feature-entry" href="{detail}">{media}<div class="work-copy"><h3>{escape(work["title"])}</h3><p>{escape(work["summary"])}</p></div></a></article>'

    sections_by_title = {section["title"]: section for section in home_sections}
    process_html = render_workflow(sections_by_title["AI Agent 워크플로우"])
    decisions_html = render_decisions(sections_by_title["지침 변경 이력"])
    legacy_section = sections_by_title["먼저 만든 기반: 기존 2D Unity"]
    legacy_copy = render_blocks(legacy_section["blocks"], HOME_SOURCE)
    case = render_comparison_case(sections_by_title["선택한 방향을 결과로"])
    usage_html = render_agent_usage(sections_by_title["Agent 활용 지표"], case)
    featured = ''.join(cards[key] for key in ("player-hud", "field-travel", "inventory", "market", "server-content"))
    hero = f'''<main id="top" tabindex="-1"><section class="home-hero"><h1>{escape(title)}</h1><p class="lead">{inline(intro)}</p></section>
<section class="section-wrap home-actions"><h2><a href="{BASE}process/">Agent 작업 방식과 기록 →</a></h2><a class="button" href="{BASE}play/">클라이언트 콘텐츠 둘러보기</a></section>
<section class="section-wrap origin-overview" id="legacy"><div class="section-title"><h2>기존 2D 클라이언트</h2></div><div class="legacy-visible">{legacy_copy}</div></section>
<section class="section-wrap work-section" id="work"><div class="section-title"><h2>기능별 결과</h2></div><div class="featured-grid">{featured}</div></section></main>'''
    (SITE / "index.html").write_text(document(title, intro, hero, references, title), encoding="utf-8", newline="\n")
    process_content = f'''<main class="process-page" id="top" tabindex="-1"><header class="page narrow"><h1>Agent 작업 방식과 기록</h1></header>{process_html}{usage_html}{decisions_html}</main>'''
    process_folder = SITE / "process"
    process_folder.mkdir(exist_ok=True)
    (process_folder / "index.html").write_text(document("Agent 작업 방식과 기록", "사람의 판단, Agent 역할과 작업 과정", process_content, references, title), encoding="utf-8", newline="\n")
    experience_source = ROOT / "docs/unity-client/EXPERIENCE.md"
    experience_title, experience_intro, experience_sections, _ = parse_showcase(experience_source)
    experience_body = ''.join(f'<section class="content-section" id="{slug(s["title"])}"><h2>{escape(s["title"])}</h2>{render_blocks(s["blocks"], experience_source)}</section>' for s in experience_sections)
    experience_content = f'''<main class="page narrow experience-page" id="top" tabindex="-1"><h1>{escape(experience_title)}</h1><p class="lead">{inline(experience_intro)}</p>{experience_body}</main>'''
    experience_folder = SITE / "experience"
    experience_folder.mkdir(exist_ok=True)
    (experience_folder / "index.html").write_text(document(experience_title, experience_intro, experience_content, references, title), encoding="utf-8", newline="\n")
    for old_key in legacy_market_pages:
        folder = SITE / "work" / old_key
        folder.mkdir(parents=True, exist_ok=True)
        for page in ("index.html", "history.html"):
            target = f"{BASE}work/market/index.html" + ("#history" if page == "history.html" else "")
            content = f'<main class="page narrow" id="top" tabindex="-1"><h1>거래소</h1><p>거래소 작업과 이전 화면을 한곳에 모았습니다.</p><a class="button" href="{target}">거래소 보기</a></main>'
            html = document("거래소", "통합된 거래소 작업으로 이동합니다.", content, references, title)
            html = html.replace("</head>", f'<link rel="canonical" href="{target}"><meta http-equiv="refresh" content="0;url={target}"></head>')
            (folder / page).write_text(html, encoding="utf-8", newline="\n")
    build_tour(references, title, require_tour_media)
    check_site()
    check_work_navigation(works)
    print(f"Built home and {len(works)} project pages with screen histories")


def check_work_navigation(works):
    # Canonical content pages are reached directly from home/global navigation.
    home = (SITE / "index.html").read_text(encoding="utf-8")
    for work in works:
        key = slug(work["source"].stem)
        route = f"{BASE}work/{key}/index.html"
        if f'href="{route}"' not in home or f'id="featured-{key}"' not in home:
            raise ValueError(f"Missing direct home feature card: {key}")
        detail = (SITE / "work" / key / "index.html").read_text(encoding="utf-8")
        if 'id="history"' not in detail:
            raise ValueError(f"History must stay in the feature page: {key}")
    canonical = [SITE / "index.html", *(SITE / "work" / slug(w["source"].stem) / "index.html" for w in works), *(SITE / name / "index.html" for name in ("play", "process", "experience"))]
    for page in canonical:
        html = page.read_text(encoding="utf-8")
        body = html.split("<main", 1)[1].split("</main>", 1)[0]
        for href in re.findall(r'<a\b[^>]*href="([^"]+)"', body):
            if page != SITE / "index.html":
                raise ValueError(f"Content pages must not add another navigation depth: {page} -> {href}")
            if not href.startswith((BASE + "work/", BASE + "play/", BASE + "process/")) or "#" in href:
                raise ValueError(f"Unexpected repeated home navigation: {href}")
        if re.search(r'<a\b[^>]*>\s*<(?:img|picture)\b', body) and page != SITE / "index.html":
            raise ValueError(f"Original images must not be linked: {page}")


def check_site():
    html_files = sorted(SITE.rglob("*.html"))
    if not html_files:
        raise ValueError("No generated HTML found")
    for path in html_files:
        content = path.read_text(encoding="utf-8")
        if '<html lang="ko">' not in content:
            raise ValueError(f"Missing Korean document language: {path}")
        for ref in re.findall(r'(?:src|href|srcset)="([^"]+)"', content):
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--require-tour-media", action="store_true", help="Require all eight content tour GIFs before delivery")
    main(parser.parse_args().require_tour_media)
