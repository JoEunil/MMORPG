#!/usr/bin/env python3
"""Build the public portfolio pages from the approved Markdown copy."""
from html import escape
from pathlib import Path
import re
import shutil
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


def image_size_attributes(url):
    # Reserve each capture's real aspect ratio so lazy loading cannot move history anchors.
    path = SITE / unquote(url[len(BASE):])
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
    heading_index = 0

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
            out.append(f'<figure><a href="{src}" target="_blank" rel="noopener"><img src="{src}" alt="{escape(alt, quote=True)}" {image_size_attributes(src)} loading="lazy"></a><figcaption>{escape(alt)}</figcaption></figure>')
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
    intro = next(b for b in section["blocks"] if isinstance(b, str) and b and not b.startswith("- "))
    return f'''<section class="workflow" id="workflow"><div class="section-wrap">
<div class="section-title"><h2>{escape(section['title'])}</h2><p class="diagram-intro">{escape(intro)}</p></div>
<div class="agent-diagram"><ol class="agent-chain" aria-label="사용자에서 총괄 Agent로 이어지는 역할 연결">{chain}</ol>
<div class="agent-branches"><p class="branch-label">작업에 필요한 전문 Agent를 선택해 배정</p><ul class="agent-roles" aria-label="총괄 Agent가 배정하는 전문 역할">{roles}</ul></div></div></div></section>'''


def render_execution_flow(section):
    groups = diagram_items(section)
    steps = ''.join(f'<li>{diagram_node(item)}</li>' for item in groups["작업 흐름"])
    feedback = ''.join(f'<li>{diagram_node(item)}</li>' for item in groups["피드백"])
    intro = next(b for b in section["blocks"] if isinstance(b, str) and b and not b.startswith("- "))
    example_start = next((i for i, block in enumerate(section["blocks"])
                          if block == ("heading", "실제 비교·선택 예시")), None)
    example = render_blocks(section["blocks"][example_start:], HOME_SOURCE) if example_start is not None else ""
    return f'''<section class="execution-flow section-wrap" id="execution-flow"><div class="section-title"><h2>{escape(section['title'])}</h2><p class="diagram-intro">{escape(intro)}</p></div>
<ol class="workflow-steps" aria-label="기능 구현의 진행 순서">{steps}</ol><ul class="workflow-feedback">{feedback}</ul>{example}</section>'''



def render_agent_usage(section):
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
    return f'''<section class="section-wrap agent-usage" id="agent-usage"><div class="section-title"><h2>{escape(section['title'])}</h2></div>
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
    return f'<section class="section-wrap decisions" id="decisions"><div class="section-title"><h2>{escape(section["title"])}</h2></div><ol class="decision-track">{"".join(entries)}</ol></section>'


def document(title, description, content, references, site_title):
    ref_footer = "".join(f'<a href="{escape(url, quote=True)}">{escape(label)} ↗</a>' for label, url in references)
    browser_title = title if title == site_title else f"{title} · {site_title}"
    return f'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{escape(description, quote=True)}"><title>{escape(browser_title)}</title>
<link rel="stylesheet" href="{BASE}style.css"></head><body>
<a class="skip-link" href="#top">본문으로 바로가기</a>
<header class="site-header"><a class="brand" href="{BASE}">{escape(site_title)}</a><nav aria-label="주 메뉴"><a href="{BASE}#workflow">AI 워크플로우</a><a href="{BASE}#work">작업 결과</a><a href="{BASE}#legacy">기존 Unity</a><a href="{BASE}#decisions">지침 변경</a><a href="{BASE}#agent-usage">활용 지표</a></nav></header>
{content}<footer class="site-footer"><a href="{BASE}">{escape(site_title)}</a>{ref_footer}</footer>
</body></html>'''


def work_navigation(works, current_key=None):
    label = "다른 작업 둘러보기" if current_key else "기능별 둘러보기"
    heading = f'<p>{label}</p>' if current_key else ''
    links = ''.join(f'<a href="{BASE}work/{slug(work["source"].stem)}/">{escape(work["title"])}</a>'
                    for work in works if slug(work["source"].stem) != current_key)
    return f'<nav class="work-navigation" aria-label="{label}">{heading}<div>{links}</div></nav>'


def build_work(work, references, site_title, works):
    key = slug(work["source"].stem)
    detail_url = f"{BASE}work/{key}/index.html"
    history_url = f"{BASE}work/{key}/history.html"
    sections = []
    history = []
    dates = []
    seen_dates = set()
    for section in work["sections"]:
        if section["title"] == "대표 화면":
            continue
        if section["title"] == "화면 기록":
            heading_index = 0
            for block in section["blocks"]:
                if isinstance(block, tuple):
                    heading_index += 1
                    date = re.match(r"\d{4}-\d{2}-\d{2}", block[1])
                    label = date[0] if date else "이전 기록"
                    if label not in seen_dates:
                        dates.append(f'<a href="#record-{heading_index}">{escape(label)}</a>')
                        seen_dates.add(label)
            history.append(render_blocks(section["blocks"], work["source"], "record"))
        else:
            anchor = "features" if section["title"] == "구현한 기능" else "scope" if section["title"] == "현재 확인한 범위" else slug(section["title"])
            rendered = render_blocks(section["blocks"], work["source"])
            sections.append(f'<section class="content-section" id="{anchor}"><h2>{escape(section["title"])}</h2>{rendered}</section>')
    preview_section = next(s for s in work["sections"] if s["title"] == "대표 화면")
    preview_html = render_blocks(preview_section["blocks"], work["source"])
    other_work = work_navigation(works, key)
    page_content = f'''<main class="page narrow" id="top" tabindex="-1"><a class="back-link" href="{BASE}#{key}">← 작업 목록</a>
<p class="work-status">{escape(work['status'])}</p><h1>{escape(work['title'])}</h1><p class="lead">{escape(work['summary'])}</p>
<nav class="page-actions" aria-label="작업 상세 탐색"><a href="#features">구현한 기능</a><a href="#scope">현재 확인한 범위</a><a href="{history_url}">화면 이력 →</a></nav>
<div class="detail-preview">{preview_html}</div>
{''.join(sections)}{other_work}</main>'''
    history_content = f'''<main class="page narrow" id="top" tabindex="-1"><a class="back-link" href="{detail_url}">← {escape(work['title'])} 상세</a>
<p class="work-status">화면 기록</p><h1>{escape(work['title'])}</h1><p class="lead">촬영일 순으로 보존한 실제 게임 화면·에디터 예시·도구 출력입니다. 현재 구현과 확인한 범위는 작업 상세에서 볼 수 있습니다.</p>
<nav class="page-actions" aria-label="작업과 화면 이력 탐색"><a href="{detail_url}">현재 구현</a><a href="{detail_url}#scope">확인한 범위</a><a href="{BASE}#work">다른 작업</a></nav>
<nav class="history-dates" aria-label="촬영 날짜별 화면 기록">{''.join(dates)}</nav>
<section class="content-section history-images">{''.join(history)}</section><a class="back-link" href="#top">↑ 날짜 목록으로</a>{other_work}</main>'''
    folder = SITE / "work" / key
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "index.html").write_text(document(work["title"], work["summary"], page_content, references, site_title), encoding="utf-8", newline="\n")
    (folder / "history.html").write_text(document(work["title"] + " 화면 기록", work["summary"], history_content, references, site_title), encoding="utf-8", newline="\n")
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


def main():
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
    cards = []
    for work in works:
        key, detail, history = build_work(work, references, title, works)
        card_image = find_image(work)
        preview_section = next(s for s in work["sections"] if s["title"] == "대표 화면")
        preview_notes = [b for b in preview_section["blocks"] if not (isinstance(b, str) and IMAGE.fullmatch(b))]
        media = f'<figure class="work-media"><a class="card-image" href="{detail}"><img src="{card_image[1]}" alt="{escape(card_image[0], quote=True)}" {image_size_attributes(card_image[1])} loading="lazy"></a><figcaption>{escape(card_image[0])}</figcaption></figure>'
        cards.append(f'<article class="work-card" id="{key}">{media}<div class="work-copy"><p class="work-status">{escape(work["status"])}</p><h3><a href="{detail}">{escape(work["title"])}</a></h3><p>{escape(work["summary"])}</p><div class="preview-note">{render_blocks(preview_notes, work["source"])}</div><div class="card-links"><a href="{detail}">기능 보기 →</a><a href="{detail}#scope">확인한 범위</a><a href="{history}">화면 이력</a></div></div></article>')
    sections_by_title = {section["title"]: section for section in home_sections}
    process_html = render_workflow(sections_by_title["AI Agent 워크플로우"])
    execution_html = render_execution_flow(sections_by_title["하나의 기능이 만들어지는 흐름"])
    usage_html = render_agent_usage(sections_by_title["Agent 활용 지표"])
    work_intro = next(block for block in sections_by_title["작업 둘러보기"]["blocks"] if isinstance(block, str) and block and not block.startswith("- "))
    decisions_html = render_decisions(sections_by_title["지침 변경 이력"])
    legacy_section = sections_by_title["기존 Unity 구현"]
    legacy_copy = render_blocks(legacy_section["blocks"], HOME_SOURCE)
    legacy_html = f'<section class="legacy-section section-wrap" id="legacy"><div class="section-title"><h2>{escape(legacy_section["title"])}</h2></div><div class="legacy-body">{legacy_copy}</div></section>'
    hero = f'''<main id="top" tabindex="-1"><section class="home-hero"><div class="home-copy"><h1>{escape(title)}</h1><p class="lead">{inline(intro)}</p><div class="hero-actions"><a class="button" href="#work">작업 결과 보기</a><a class="text-link" href="#workflow">AI Agent 워크플로우</a><a class="text-link" href="#legacy">기존 Unity 구현</a></div></div></section>
{process_html}
<section class="section-wrap work-section" id="work"><div class="section-title work-heading"><h2>작업과 결과</h2><p>{inline(work_intro)}</p></div>{work_navigation(works)}<div class="work-grid">{''.join(cards)}</div></section>
{legacy_html}
{execution_html}
{usage_html}
{decisions_html}</main>'''
    (SITE / "index.html").write_text(document(title, intro, hero, references, title), encoding="utf-8", newline="\n")
    for old_key in legacy_market_pages:
        folder = SITE / "work" / old_key
        folder.mkdir(parents=True, exist_ok=True)
        for page in ("index.html", "history.html"):
            target = f"{BASE}work/market/{page}"
            content = f'<main class="page narrow" id="top" tabindex="-1"><h1>거래소</h1><p>거래소 작업과 이전 화면을 한곳에 모았습니다.</p><a class="button" href="{target}">거래소 보기</a></main>'
            html = document("거래소", "통합된 거래소 작업으로 이동합니다.", content, references, title)
            html = html.replace("</head>", f'<link rel="canonical" href="{target}"><meta http-equiv="refresh" content="0;url={target}"></head>')
            (folder / page).write_text(html, encoding="utf-8", newline="\n")
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
