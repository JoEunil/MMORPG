from pathlib import Path
import json, re, shutil

root = Path(__file__).resolve().parent
out = root.parent / 'Decision-Planning' / 'hud-options'
out.mkdir(exist_ok=True)
shutil.copy2(root/'hud-comparison-base.png', out/'base.png')
shutil.copy2(root.parent/'MinimapFeather/minimap-feather-preview.png', out/'map-source.png')
spec = json.loads((root/'art-options.json').read_text(encoding='utf-8-sig'))
glyphs=[]
for name in ['strike','ring','line']:
    svg=(root.parent/'CurrencySkillIcons'/f'skill-{name}-icon-v2.svg').read_text()
    paths=re.findall(r'<path[^>]+/>', svg)[1:]
    # Compare broad original glyphs without baking the previous dark disc into every option.
    glyphs.append(''.join(paths))
data={k:spec[k] for k in ('minimap','skills','vitals')}
data['skills']['A']['name']='큰 전투 문양'
html='''<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>미니맵 · 스킬 · HP/MP 비교</title><style>
*{box-sizing:border-box}body{margin:0;background:#f3f1eb;color:#252b2c;font:18px/1.65 system-ui,sans-serif}main{max-width:1460px;margin:auto;padding:30px 28px 60px}h1{font-size:32px;line-height:1.3;margin:8px 0 14px}h2{font-size:26px;margin:0 0 10px}h3{font-size:21px;margin:0 0 7px}p{margin:7px 0 14px}.muted{color:#536061}a{color:#24535b}nav{display:flex;gap:20px;flex-wrap:wrap;margin:20px 0}section{margin-top:44px}.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:18px}article{background:#fff;border-radius:15px;padding:20px;box-shadow:0 2px 10px #1a252b0c;border:2px solid transparent}article.selected{border-color:#275d65}.picture{width:100%;height:auto;display:block;background:#2d3834;border-radius:8px;margin:12px 0 15px}.note{font-size:16px;line-height:1.6;color:#536061;min-height:50px}button{font:inherit;border:1px solid #a3aeac;background:#fff;color:#243b40;border-radius:8px;padding:8px 16px;cursor:pointer}button:hover{background:#e7efeb}button[aria-pressed=true]{background:#275d65;color:#fff;border-color:#275d65}.choices{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.scene{background:#22342c;border-radius:12px;overflow:hidden;line-height:0}.scene svg{width:100%;height:auto;display:block}#summary{padding:14px 0;font-size:20px;font-weight:600}#state-note{margin:0 0 18px}.state{display:flex;gap:10px;margin:14px 0;align-items:center;flex-wrap:wrap}.status{background:#e4ece7;border-radius:12px;padding:18px 22px;margin:24px 0}.zoom{padding:0;background:none;border:0;text-decoration:underline;color:#31575e;margin-left:12px}dialog{border:0;border-radius:12px;max-width:min(1500px,96vw);width:96vw;padding:20px;background:#f3f1eb}dialog::backdrop{background:#121c21bb}dialog svg{display:block;width:100%;height:auto}dialog header{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-bottom:12px}svg text{font-family:Arial,sans-serif}footer{margin-top:45px;border-top:1px solid #ccd4cf;padding-top:22px}@media(max-width:850px){main{padding:22px 16px}.grid{grid-template-columns:1fr}h1{font-size:28px}.note{min-height:0}article{padding:17px}}
</style></head><body><main>
<h1>미니맵 · 스킬 · HP/MP 비교</h1>
<p>각 항목에서 A·B·C를 고르면 아래 전체 화면에 함께 반영됩니다. 서로 다른 안을 섞어서 골라도 됩니다.</p>
<p class="muted">현재 게임 장면 위에 얹은 비교용 UI 시안입니다. Unity 적용 결과는 아니며, HP·MP는 기본 100% 예시입니다.</p>
<nav><a href="#map">미니맵 3안</a><a href="#skill">스킬 3안</a><a href="#vitals">HP·MP 3안</a><a href="#combined">조합해서 보기</a></nav>
<section id="map"><h2>미니맵 — 배경과 표식을 더 또렷하게</h2><p>크기·범위·점 위치는 같습니다. 초록은 자신, 회색은 플레이어, 빨강은 몬스터입니다.</p><div class="grid" id="map-grid"></div></section>
<section id="skill"><h2>스킬 — 큰 형태, 색면, 얇은 장식</h2><p>베기·회전·내려찍기와 Z·X·C는 유지하고, 아이콘을 받쳐 주는 형태와 색을 비교합니다.</p><div class="grid" id="skill-grid"></div></section>
<section id="vitals"><h2>HP·MP — 끝까지 채워지는 게이지</h2><p>100%일 때 안쪽 트랙 전체가 채워지도록 그렸습니다. 실제 게임에서는 남은 수치에 따라 줄어듭니다.</p><div class="grid" id="vitals-grid"></div></section>
<section id="combined"><h2>고른 안을 한 화면에서</h2><div id="summary"></div><div class="state"><span>채움 상태</span><button id="full" aria-pressed="true">100% · 가득 찬 상태</button><button id="partial" aria-pressed="false">65% · 줄어든 상태</button><button id="recommended">추천 조합 보기</button><button id="fullscreen">크게 보기</button></div><p id="state-note" class="muted">수치와 준비 상태는 디자인 비교용 예시입니다.</p><div class="scene" id="combined-scene"></div><div class="status"><strong>선택 내용</strong><div id="answer"></div><p>이 조합을 채팅에 남겨 주세요. 모두 마음에 들지 않으면 바꾸고 싶은 요소만 말씀해 주셔도 됩니다.</p></div><p class="muted">추천: 미니맵 B · 스킬 B · HP·MP A. 지도는 장면과 분리하고, 스킬은 색면으로 살리며, 게이지는 간결하게 두는 조합입니다. 아직 사용자 선택으로 확정하지 않았습니다.</p></section>
<footer>현재 제품의 위치·단축키·스킬 기능·재화 처리는 변경하지 않았습니다. 선택한 방향을 기준으로 실제 Unity UI를 함께 다듬습니다. <a href="../mmorpg-reference/result.html">현재 적용 화면</a></footer>
</main><dialog id="large"><header><h3 id="large-title"></h3><button id="close-dialog">닫기</button></header><div id="large-scene"></div></dialog>
<script>
const data=__DATA__, glyphs=__GLYPHS__;
let choice={map:'B',skill:'B',vitals:'A'}, ratio=1, serial=0;
const names={map:'미니맵',skill:'스킬',vitals:'HP·MP'};
function text(x,y,s,size=16,fill='#fff4dc',extra=''){return `<text x="${x}" y="${y}" font-size="${size}" fill="${fill}" ${extra}>${s}</text>`}
function map(which,id){
 const o=data.minimap[which], p=o.palette, fade={A:.08,B:.14,C:.24}[which];
 const stops=`<stop offset="0" stop-color="black"/><stop offset="${fade}" stop-color="white"/><stop offset="${1-fade}" stop-color="white"/><stop offset="1" stop-color="black"/>`;
 const defs=`<defs><linearGradient id="mx${id}">${stops}</linearGradient><linearGradient id="my${id}" x1="0" y1="0" x2="0" y2="1">${stops}</linearGradient><mask id="edgeX${id}"><rect x="1048" y="86" width="208" height="156" fill="url(#mx${id})"/></mask><mask id="edgeY${id}"><rect x="1048" y="86" width="208" height="156" fill="url(#my${id})"/></mask><filter id="tint${id}" color-interpolation-filters="sRGB"><feColorMatrix type="saturate" values="${which==='B'?.22:.42}"/><feComponentTransfer><feFuncR type="linear" slope="${which==='B'?.62:.72}"/><feFuncG type="linear" slope="${which==='B'?.9:.84}"/><feFuncB type="linear" slope="${which==='B'?1.18:.83}"/></feComponentTransfer></filter></defs>`;
 let g=`${defs}<g mask="url(#edgeX${id})"><g mask="url(#edgeY${id})"><rect x="1048" y="86" width="208" height="156" rx="${o.radius}" fill="${p.panel}"/>`;
 g+=`<svg x="1048" y="86" width="208" height="156" viewBox="1048 86 208 156"><image href="map-source.png" width="1280" height="720" opacity="${o.terrainOpacity}" filter="url(#tint${id})"/></svg>`;
 g+=`<rect x="1048" y="86" width="208" height="156" fill="${p.panel}" opacity="${which==='A'?.38:.2}"/>`;
 for(const r of [52,104])g+=`<circle cx="1152" cy="164" r="${r}" fill="none" stroke="${p.ring}" stroke-width=".8" opacity=".7"/>`;
 g+=`<path d="M1055 164H1249M1152 93V235" stroke="${p.ring}" stroke-opacity=".22" stroke-width=".7"/>`;
 for(const [x,y,type] of [[1123,143,'monster'],[1183,117,'monster'],[1206,153,'monster'],[1178,143,'player'],[1134,177,'player']]){
  g+=`<circle cx="${x}" cy="${y}" r="4.5" fill="${data.minimap.base[type]}" stroke="#0b191e" stroke-width="1.5"/>`;
  if(which==='B')g+=`<circle cx="${x}" cy="${y}" r="5.3" fill="none" stroke="#ecf5df" stroke-opacity=".7" stroke-width=".65"/>`;
 }
 g+=`<circle cx="1152" cy="164" r="8.2" fill="#72e09a" fill-opacity=".2"/><path d="M1152 157L1159 164L1152 171L1145 164Z" fill="#70ef9c" stroke="#efffdc" stroke-width="1.2"/>`;
 g+=text(1152,102,'N',14,'#f6edd6','text-anchor="middle" font-weight="bold"')+text(1203,171,'20m',13)+text(1223,185,'40m',13);
 g+='</g></g>';
 if(which!=='C')g+=`<path d="M1049 104V87H1066M1238 87H1255V104M1049 224V241H1066M1238 241H1255V224" fill="none" stroke="${p.edge}" stroke-opacity=".8" stroke-width="1"/>`;
 return g;
}
function skills(which,id){
 let g='';const p=data.skills[which].palette;
 for(let i=0;i<3;i++){
  const x=547+i*66,y=638; let face;
  if(which==='A')face=`<path d="M5 0H51Q56 0 56 6V44Q56 53 28 62Q0 53 0 44V6Q0 0 5 0Z" fill="#16312f" stroke="#acb9a0" stroke-width="1"/><path d="M6 7H50V45Q43 51 28 56Q13 51 6 45Z" fill="#294a43"/>`;
  if(which==='B'){
    const shades=[['#266b59','#68be8f'],['#805a27','#dfb75e'],['#265d79','#67b7ce']][i];
    face=`<path d="M13 1H43L55 12V48L43 61H13L1 48V12Z" fill="${shades[0]}" stroke="#ead5a1" stroke-width="1.3"/><path d="M3 40Q20 53 53 22V47L42 59H14L3 47Z" fill="${shades[1]}" opacity=".65"/><path d="M5 15L15 5H41" fill="none" stroke="${shades[1]}" stroke-width="2"/>`;
  }
  if(which==='C')face=`<path d="M10 1Q28 5 46 1L55 10Q51 31 55 52L46 61Q28 57 10 61L1 52Q5 31 1 10Z" fill="#243b43" stroke="#dcc18b" stroke-width="1.3"/><path d="M6 12L12 6M44 6L50 12M50 50L44 56M12 56L6 50" stroke="#f2deb2" stroke-width="1.2"/><path d="M28 -3L31 1L28 5L25 1ZM28 57L31 61L28 65L25 61Z" fill="#dcc18b"/>`;
  const adjusted=glyphs[i].replaceAll('#353329',which==='B'?'#805a27':'#263d3a');
  g+=`<g transform="translate(${x},${y})"><path d="M-2 5Q-2 -2 5 -2H51Q58 -2 58 5V57Q58 64 51 64H5Q-2 64 -2 57Z" fill="#0a171c" fill-opacity=".8"/>${face}<svg x="3" y="14" width="50" height="46" viewBox="4 5 56 52">${adjusted}</svg><path d="M-2 -3H17L21 1V18H-2Z" fill="#11242a" stroke="${which==='B'?'#e3cc98':'#9eafa8'}" stroke-width=".9"/>${text(8,13,['Z','X','C'][i],19,'#fff4da','text-anchor="middle" font-weight="bold"')}</g>`;
 }
 return g;
}
function vitals(which,id,r=1){
 const p=data.vitals[which].palette;let g='';const x=974,y=643,w=274,h=21;
 if(which!=='C')g+=`<path d="M970 633Q1109 628 1252 633L1258 637V696Q1110 703 966 696V637Z" fill="${p.panel}" fill-opacity=".9"/>`;
 for(let i=0;i<2;i++){
  let yy=y+i*28, path;
  if(which==='A')path=`M${x+5} ${yy}H${x+w-5}Q${x+w} ${yy} ${x+w} ${yy+5}V${yy+h-5}Q${x+w} ${yy+h} ${x+w-5} ${yy+h}H${x+5}Q${x} ${yy+h} ${x} ${yy+h-5}V${yy+5}Q${x} ${yy} ${x+5} ${yy}Z`;
  if(which==='B')path=`M${x+8} ${yy}H${x+w-8}L${x+w} ${yy+10.5}L${x+w-8} ${yy+h}H${x+8}Q${x} ${yy+h} ${x} ${yy+10.5}Q${x} ${yy} ${x+8} ${yy}Z`;
  if(which==='C')path=`M${x+2} ${yy+2}Q${x+140} ${yy-2} ${x+w-6} ${yy+2}L${x+w} ${yy+10}L${x+w-6} ${yy+h}Q${x+140} ${yy+h-3} ${x} ${yy+h}Z`;
  const clip=`v${id}${i}`;
  g+=`<defs><clipPath id="${clip}"><path d="${path}"/></clipPath></defs><path d="${path}" fill="${p.track}" stroke="#0a191f" stroke-width="3"/><g clip-path="url(#${clip})"><rect x="${x}" y="${yy-2}" width="${w*r}" height="${h+4}" fill="${i===0?p.hp:p.mp}"/><rect x="${x}" y="${yy-2}" width="${w*r}" height="8" fill="${i===0?p.hpLight:p.mpLight}" opacity=".6"/></g><path d="${path}" fill="none" stroke="${p.rim}" stroke-width=".8"/>`;
  if(which==='B')g+=`<path d="M${x+8} ${yy-3}L${x+18} ${yy+10}L${x+8} ${yy+24}L${x-2} ${yy+10}Z" fill="${i===0?p.hp:p.mp}" stroke="${p.rim}" stroke-width="1.2"/><path d="M${x+8} ${yy}L${x+8} ${yy+18}L${x+2} ${yy+10}Z" fill="${i===0?p.hpLight:p.mpLight}"/>`;
  g+=text(x+(which==='B'?26:9),yy+15,i===0?'HP':'MP',13,'#fff6df','font-weight="bold" paint-order="stroke" stroke="#1a2830" stroke-width="2"');
  g+=text(x+161,yy+16,`${r===1?'10,000':'6,500'} / 10,000`,16,'#fff6e5','text-anchor="middle" font-weight="bold" paint-order="stroke" stroke="#1a2830" stroke-width="2"');
 }
 return g;
}
function svg(content,view='0 0 1280 720'){return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${view}" role="img"><image href="base.png" width="1280" height="720"/>${content}</svg>`}
function full(){const id=++serial;return svg(map(choice.map,id)+skills(choice.skill,id)+vitals(choice.vitals,id,ratio))}
function update(){document.querySelector('#combined-scene').innerHTML=full();document.querySelector('#summary').textContent=`미니맵 ${choice.map} · 스킬 ${choice.skill} · HP·MP ${choice.vitals}`;document.querySelector('#answer').textContent=`미니맵 ${choice.map}, 스킬 ${choice.skill}, HP·MP ${choice.vitals}`;document.querySelectorAll('[data-pick]').forEach(b=>{const [cat,key]=b.dataset.pick.split(':');const yes=choice[cat]===key;b.setAttribute('aria-pressed',yes);b.closest('article').classList.toggle('selected',yes)});document.querySelector('#full').setAttribute('aria-pressed',ratio===1);document.querySelector('#partial').setAttribute('aria-pressed',ratio!==1)}
for(const [cat,source,draw,view] of [['map','minimap',map,'1026 64 254 200'],['skill','skills',skills,'520 616 240 104'],['vitals','vitals',vitals,'950 620 330 100']]){
 for(const key of ['A','B','C']){const o=data[source][key],id=++serial;const card=document.createElement('article');card.innerHTML=`<h3>${key} · ${o.name}</h3><p>${o.effect}</p><div class="picture">${svg(draw(key,id),view)}</div><p class="note">${o.tradeoff}</p><button data-pick="${cat}:${key}">${names[cat]} ${key} 선택</button><button class="zoom" data-zoom="${cat}:${key}">전체 화면</button>`;document.querySelector('#'+cat+'-grid').append(card)}
}
document.querySelectorAll('[data-pick]').forEach(b=>b.onclick=()=>{const [cat,key]=b.dataset.pick.split(':');choice[cat]=key;update()});
function large(title,scene){document.querySelector('#large-title').textContent=title;document.querySelector('#large-scene').innerHTML=scene;document.querySelector('#large').showModal()}
document.querySelectorAll('[data-zoom]').forEach(b=>b.onclick=()=>{const [cat,key]=b.dataset.zoom.split(':');const saved={...choice};choice[cat]=key;large(`${names[cat]} ${key} · 나머지는 현재 조합`,full());choice=saved});
document.querySelector('#full').onclick=()=>{ratio=1;update()};document.querySelector('#partial').onclick=()=>{ratio=.65;update()};document.querySelector('#recommended').onclick=()=>{choice={map:'B',skill:'B',vitals:'A'};update()};document.querySelector('#fullscreen').onclick=()=>large('선택한 조합 · 비교용 시안',full());document.querySelector('#close-dialog').onclick=()=>document.querySelector('#large').close();update();
</script></body></html>'''
html=html.replace('__DATA__',json.dumps(data,ensure_ascii=False)).replace('__GLYPHS__',json.dumps(glyphs,ensure_ascii=False))
(out/'index.html').write_text(html,encoding='utf-8')
print(out/'index.html')
