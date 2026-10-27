from pathlib import Path
import json
import re
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent
project = root.parents[1]
previous = root.parent / 'CurrencySkillIcons'
# Reuse the existing native SVG path rasterizer, without its file-output loop.
renderer = {'__file__': str(previous / 'rasterize-icons.py')}
exec((previous / 'rasterize-icons.py').read_text().split('for source in sorted(root.glob')[0], renderer)
renderer['scale'] = 16

def flattened(data):
    # Same viewBox and placement as the approved board: x=3,y=14,w=50,h=46.
    s = 46 / 52
    tx, ty = 3 + (50 - 56*s)/2 - 4*s, 14 - 5*s
    def transform(match):
        values = list(map(float, re.findall(r'-?\d+(?:\.\d+)?', match.group(2))))
        numbers = [v*s + (tx if i % 2 == 0 else ty) for i,v in enumerate(values)]
        return match.group(1) + ' ' + ' '.join(f'{v:.6f}' for v in numbers)
    return re.sub(r'([MLCQ])([\s\d.\-]+)', transform, data)

for name, dark, light in [('strike', '#266b59', '#68be8f'), ('ring', '#805a27', '#dfb75e'), ('line', '#265d79', '#67b7ce')]:
    source = ET.parse(previous / f'skill-{name}-icon-v2.svg').getroot()
    glyphs = list(source.iter('{http://www.w3.org/2000/svg}path'))[1:]
    body = f'''<path d="M13 1 L43 1 L55 12 L55 48 L43 61 L13 61 L1 48 L1 12 Z" fill="{dark}" stroke="#10252a" stroke-width="3"/>
<path d="M3 40 Q20 53 53 22 L53 47 L42 59 L14 59 L3 47 Z" fill="{light}" fill-opacity="0.65"/>
<path d="M13 1 L43 1 L55 12 L55 48 L43 61 L13 61 L1 48 L1 12 Z" fill="none" stroke="#ead5a1" stroke-width="1.3"/>
<path d="M5 15 L15 5 L41 5" fill="none" stroke="{light}" stroke-width="2"/>'''
    for glyph in glyphs:
        glyph.set('d', flattened(glyph.get('d')))
        if glyph.get('fill') == '#353329': glyph.set('fill', '#805a27')
        glyph.set('stroke-width', str(float(glyph.get('stroke-width', 1)) * 46/52))
        body += ET.tostring(glyph, encoding='unicode')
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 56 62"><g stroke-linecap="round" stroke-linejoin="round">{body}</g></svg>'
    (root / f'skill-{name}-medallion-v3.svg').write_text(svg, encoding='utf-8')
    renderer['canvas'] = Image.new('RGBA', (56*16, 62*16))
    renderer['render'](ET.fromstring(svg))
    output = renderer['canvas'].resize((224, 248), Image.Resampling.LANCZOS)
    output.save(project / f'Assets/C02/Art/Skills/skill-{name}-medallion-v3.png')
    assert output.getpixel((112,124))[3] == 255

# Identical track/fill silhouettes: every visible track pixel is covered at 100%.
scale = 16
face = Image.new('RGBA', (274*scale,21*scale))
mask = Image.new('L', face.size)
ImageDraw.Draw(mask).rounded_rectangle((0,0,274*scale-1,21*scale-1), radius=5*scale, fill=255)
draw = ImageDraw.Draw(face)
draw.rectangle((0,0,274*scale,21*scale), fill=(225,225,225,255))
draw.rectangle((0,0,274*scale,7*scale), fill=(255,255,255,255))
draw.rectangle((0,19*scale,274*scale,21*scale), fill=(185,185,185,255))
face.putalpha(mask)
face = face.resize((274,21), Image.Resampling.LANCZOS)
face.save(project / 'Assets/C02/Art/CitadelEnvironment/vitals-track-face-v1.png')
(root / 'vitals-track-face-v1.svg').write_text('''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 274 21"><defs><clipPath id="track"><rect width="274" height="21" rx="5"/></clipPath></defs><g clip-path="url(#track)"><rect width="274" height="21" fill="#e1e1e1"/><rect width="274" height="7" fill="#ffffff"/><rect y="19" width="274" height="2" fill="#b9b9b9"/></g></svg>''', encoding='utf-8')
(root / 'provenance.json').write_text(json.dumps({
    'decision':'User selected minimap B / skills B / vitals A.',
    'sources':['../HudOptions/build-board.py','../HudOptions/art-options.json','../HudOptions/options-skills-vitals.png','../HudOptions/options-combined.png','../CurrencySkillIcons/skill-*-icon-v2.svg','../CurrencySkillIcons/rasterize-icons.py'],
    'rights':'Original project-authored SVG shapes and approved comparison palette. No external artwork, image generation, purchases or new third-party licence.',
    'skill':'56x62 native layout rendered at 224x248. Keys remain live uGUI text; opaque centre supports existing radial cooldown mask.',
    'vitals':'274x21 grayscale face with radius5 corners, top-third light band and 2px lower shade; shared by track/fill, actual fill ratios remain runtime-owned.'
}, ensure_ascii=False, indent=2), encoding='utf-8')
