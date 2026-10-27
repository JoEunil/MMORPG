from pathlib import Path
from PIL import Image,ImageDraw
import hashlib,json

root=Path(__file__).resolve().parent
project=root.parents[1]
asset=project/'Assets/C02/Art/CitadelEnvironment/hud-menu-icons-v6.png'
previous=asset.with_name('hud-menu-icons-v5.png')
new=Image.open(asset).convert('RGBA')
old=Image.open(previous).convert('RGBA')
grass=Image.open(root.parent/'HudSelectedBBA/hud-selected-full.png').convert('RGBA').crop((710,280,1230,500))
sheet=Image.new('RGBA',(520,300),'#f3f1eb')
draw=ImageDraw.Draw(sheet)
draw.text((12,8),'ACTUAL PIXEL SIZE / NO UPSCALING',fill='#243737')

for x,label in ((18,'v5 @40px'),(174,'v5 @48px'),(350,'v6 @48px')): draw.text((x,30),label,fill='#243737')
sheet.alpha_composite(grass,(0,53))
draw.rectangle((0,162,520,273),fill='#132a2f')
for row,y in enumerate((72,191)):
 for image,size,start in ((old,40,18),(old,48,174),(new,48,350)):
  for cell in range(2):
   icon=image.crop((cell*512,0,(cell+1)*512,512)).resize((size,size),Image.Resampling.LANCZOS)
   sheet.alpha_composite(icon,(start+cell*(size+12),y))
draw.text((12,280),'Transparent artwork only; no button plate, text or shortcuts baked in.',fill='#243737')
sheet.save(root/'native-48px-contactsheet.png')
alpha=new.getchannel('A')
manifest={
 'output':str(asset),'size':[1024,512],'format':'RGBA PNG','mapping':{'bag':[0,0,.5,1],'market':[.5,0,.5,1]},
 'sha256':hashlib.sha256(asset.read_bytes()).hexdigest(),
 'sourceSha256':hashlib.sha256((root/'menu-icons-visible-source.svg').read_bytes()).hexdigest(),
 'preservedV5Sha256':hashlib.sha256(previous.read_bytes()).hexdigest(),
 'alphaExtrema':list(alpha.getextrema()),'fullyTransparentPixels':alpha.histogram()[0],
 'seam6pxAlphaMax':alpha.crop((509,0,515,512)).getextrema()[1],
 'visibleBoundsAlpha16':{'bag':alpha.point(lambda v:255 if v>=16 else 0).crop((0,0,512,512)).getbbox(),'market':alpha.point(lambda v:255 if v>=16 else 0).crop((512,0,1024,512)).getbbox()},
 '48pxDarkOutlineWidth':'source 2.6..2.8 units becomes 1.95..2.1 px at 48px; broad silhouette edge, no background plate',
 'nativeContactSheet':'native-48px-contactsheet.png; v5 40/48px and v6 48px on same grass and dark fields',
 'notVerified':['Unity import and Sprite mapping','actual game 48px rendering and user input','user satisfaction']}
(root/'asset-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(manifest,ensure_ascii=False))
