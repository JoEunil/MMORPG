from pathlib import Path
import re
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

root=Path(__file__).resolve().parent
project=root.parents[1]
scale=16
def paths(data):
    tokens = re.findall(r'[MLCQZ]|-?\d+(?:\.\d+)?', data)
    i = 0
    point = (0.0, 0.0)
    start = point
    current = []
    result = []
    while i < len(tokens):
        op = tokens[i]
        i += 1
        if op == 'Z':
            current.append(start)
            result.append((current, True))
            current = []
            point = start
            continue
        count = {'M': 2, 'L': 2, 'Q': 4, 'C': 6}[op]
        values = list(map(float, tokens[i:i + count]))
        i += count
        if op == 'M':
            if current:
                result.append((current, False))
            point = start = tuple(values)
            current = [point]
        elif op == 'L':
            point = tuple(values)
            current.append(point)
        else:
            controls = [point] + list(zip(values[::2], values[1::2]))
            for step in range(1, 33):
                t = step / 32
                if op == 'C':
                    weights = [(1-t)**3, 3*(1-t)**2*t, 3*(1-t)*t*t, t**3]
                else:
                    weights = [(1-t)**2, 2*(1-t)*t, t*t]
                current.append(tuple(sum(p[axis]*w for p,w in zip(controls,weights)) for axis in (0,1)))
            point = controls[-1]
    if current:
        result.append((current, False))
    return result

def rgba(value, opacity):
    if value == 'none':
        return None
    return tuple(int(value[n:n+2], 16) for n in (1,3,5)) + (round(255 * opacity),)

def render(element, offset=(0.0, 0.0), inherited=None):
    style = dict(inherited or {})
    style.update(element.attrib)
    shift = element.get('transform', '')
    if shift:
        values = re.findall(r'-?\d+(?:\.\d+)?', shift)
        if not shift.startswith('translate(') or len(values) != 2:
            raise ValueError('Unsupported SVG transform')
        offset = (offset[0] + float(values[0]), offset[1] + float(values[1]))
    if element.tag.rsplit('}', 1)[-1] == 'path':
        layer = Image.new('RGBA', canvas.size)
        draw = ImageDraw.Draw(layer)
        fill = rgba(style.get('fill', 'none'), float(style.get('fill-opacity', 1)))
        stroke = rgba(style.get('stroke', 'none'), float(style.get('stroke-opacity', 1)))
        width = max(1, round(float(style.get('stroke-width', 1)) * scale))
        for points, closed in paths(element.get('d')):
            points = [((x + offset[0]) * scale, (y + offset[1]) * scale) for x,y in points]
            if fill is not None:
                draw.polygon(points, fill=fill)
            if stroke is not None:
                draw.line(points, fill=stroke, width=width, joint='curve')
                if not closed and style.get('stroke-linecap') == 'round':
                    radius = width / 2
                    for x,y in (points[0],points[-1]):
                        draw.ellipse((x-radius,y-radius,x+radius,y+radius),fill=stroke)
        canvas.alpha_composite(layer)
    child_style = {key: value for key,value in style.items() if key not in ('transform','d')}
    for child in element:
        render(child, offset, child_style)


for source in sorted(root.glob('*.svg')):
    canvas=Image.new('RGBA',(64*scale,64*scale))
    render(ET.parse(source).getroot())
    output=canvas.resize((256,256),Image.Resampling.LANCZOS)
    feature='Currency' if '-currency-' in source.stem else 'Skills'
    target=project/'Assets/C02/Art'/feature/(source.stem+'.png')
    target.parent.mkdir(parents=True,exist_ok=True)
    output.save(target)
    alpha=output.getchannel('A')
    visible=alpha.point(lambda a:255 if a>=16 else 0)
    print(source.stem,{'size':output.size,'alpha':alpha.getextrema(),'clear':alpha.histogram()[0],'bounds':visible.getbbox(),'centre':alpha.getpixel((128,128))})
