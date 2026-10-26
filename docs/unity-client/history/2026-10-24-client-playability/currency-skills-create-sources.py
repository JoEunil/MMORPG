from pathlib import Path

folder = Path(__file__).resolve().parent
edge = '#1c2825'
rim = '#e1d8bd'

def path(d, fill='none', stroke='none', width=1):
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'

def circle(cx, cy, r, fill, stroke='none', width=1):
    k = r * .5522847498
    return path(f'M{cx+r} {cy} C{cx+r} {cy+k} {cx+k} {cy+r} {cx} {cy+r} '
                f'C{cx-k} {cy+r} {cx-r} {cy+k} {cx-r} {cy} '
                f'C{cx-r} {cy-k} {cx-k} {cy-r} {cx} {cy-r} '
                f'C{cx+k} {cy-r} {cx+r} {cy-k} {cx+r} {cy} Z',fill,stroke,width)

def svg(name, shapes):
    text = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">\n' \
        '  <g stroke-linecap="round" stroke-linejoin="round">\n' + \
        '\n'.join('    '+shape for shape in shapes) + '\n  </g>\n</svg>\n'
    (folder / (name+'.svg')).write_text(text,encoding='utf-8')

svg('gold-currency-icon-v1',[
    circle(22,38,17.5,'#a1864e',edge,2),
    circle(22,36.5,15.1,'#c4a366',rim,1.4),
    path('M10 39 Q22 44 34 39 Q29 51 19 50 Q12 48 10 39 Z','#ae8d51'),
    circle(41,25,17.2,'#a1864e',edge,2),
    circle(41,23.5,14.9,'#d1b77c',rim,1.4),
    path('M28 27 Q40 32 54 26 Q51 37 41 38 Q31 37 28 27 Z','#b99a60'),
    path('M37 15 Q41 13 45 15 L46 25 Q41 29 36 25 Z','#ddc68e'),
])

svg('diamond-currency-icon-v1',[
    path('M14 12 Q32 8 50 12 L59 28 L32 57 L5 28 Z','#4c9eaf',edge,2),
    path('M14 12 Q32 8 50 12 L59 28 L5 28 Z','#a1d6db'),
    path('M5 28 L32 28 L32 57 Z','#6bb8c8'),
    path('M32 28 L59 28 L32 57 Z','#3d859b'),
    path('M14 12 Q32 8 50 12 L59 28 L32 57 L5 28 Z','none','#d3e7df',1.1),
    path('M5 28 L59 28','none','#c1dfdf',1),
])

strike_disc='#293630'
svg('skill-strike-icon-v2',[
    circle(32,32,28,strike_disc),
    path('M12 33 C17 18 29 11 43 15 C29 16 20 24 17 35 Z','#8eaa9b'),
    path('M24 42 C31 34 41 22 50 13 C46 28 37 40 28 46 Z','#d9dfd1',edge,1.5),
    path('M28 44 C36 36 43 25 50 13 C44 31 37 41 28 46 Z','#a6bbb5'),
    path('M23 45 L27 48 L20 54 Q17 55 15 51 Z','#ab9270',edge,1.3),
    path('M18 40 Q24 38 31 44 L28 48 Q22 43 18 44 Z','#d0bf97',edge,1),
    path('M25 41 C32 33 42 21 49 15','none','#edf0df',1),
])

ring_disc='#353329'
svg('skill-ring-icon-v2',[
    circle(32,32,28,ring_disc),
    circle(32,32,20.2,'#b8a16c',edge,1.6),
    circle(32,32,14.5,ring_disc),
    path('M52 17 L51 32 L39 23 Z','#ddc68e',edge,1.1),
    path('M12 47 L13 32 L25 41 Z','#d2b77d',edge,1.1),
    path('M19 20 C24 15 32 13 39 16','none','#e4d4a3',1.1),
    path('M45 44 C40 49 32 51 25 48','none','#d6bd83',1.1),
])

line_disc='#293640'
svg('skill-line-icon-v2',[
    circle(32,32,28,line_disc),
    path('M30 9 Q32 7 34 9 L34 17 L30 17 Z','#9bafba',edge,1.2),
    path('M28 18 L36 18 L36 36 L32 45 L28 36 Z','#e0e5db',edge,1.5),
    path('M32 19 L36 18 L36 36 L32 45 Z','#9dbac6'),
    path('M23 15 Q32 18 41 15 L41 20 Q32 22 23 20 Z','#b9cbd1',edge,1.2),
    path('M12 49 Q22 44 27 47 L32 42 L37 47 Q44 44 52 49 L47 52 Q40 49 36 51 L32 47 L28 51 Q20 49 17 52 Z','#89adbe'),
    path('M16 37 L23 44 L19 44 Z','#b4cbd0'),
    path('M48 37 L41 44 L45 44 Z','#b4cbd0'),
])
