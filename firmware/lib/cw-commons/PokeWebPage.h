#pragma once

// Phone remote + front-camera gesture control (motion on device, no CDN).

const char POKE_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"/>
<meta name="theme-color" content="#e8f0f4"/>
<title>遥控 · Godwise</title>
<style>
:root{
  --bg:#e8f0f4;--ink:#1a2430;--muted:#5a6b7a;--card:#fff;--line:#d5e0e8;
  --accent:#2a7d8c;--accent-soft:#d4eef2;--ok:#2f7a4e;--ok-bg:#d8f0e2;
  --warn:#8a6a18;--warn-bg:#f5ebc8;--soft:#5c4d8a;--soft-bg:#ebe6f5;
  --love:#9a3d5c;--love-bg:#f5dce6;--cool:#2c5f9e;--cool-bg:#dce9f7;
  --day:#9a6b12;--day-bg:#f7e8b8;--night:#3d4f8a;--night-bg:#d8dff5;--r:16px;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{
  margin:0;min-height:100dvh;color:var(--ink);
  font:15px/1.45 "Segoe UI","PingFang SC","Hiragino Sans GB","Noto Sans SC",sans-serif;
  background:
    radial-gradient(900px 420px at 10% -10%,#cfe8ef 0%,transparent 55%),
    radial-gradient(700px 380px at 100% 0%,#f0e6d8 0%,transparent 50%),
    var(--bg);
  padding:18px 16px 36px;max-width:440px;margin-inline:auto;
}
.top{display:flex;align-items:flex-end;justify-content:space-between;gap:12px;margin-bottom:16px}
.brand{display:flex;flex-direction:column;gap:2px}
.brand b{font-size:22px;letter-spacing:.02em;font-weight:700}
.brand span{font-size:12px;color:var(--muted)}
.top a{font-size:13px;color:var(--accent);text-decoration:none;padding:8px 12px;border-radius:999px;background:var(--accent-soft);font-weight:600}
.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:14px;margin-bottom:12px}
.card h2{margin:0 0 10px;font-size:12px;font-weight:700;letter-spacing:.08em;text-transform:uppercase;color:var(--muted)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.grid .span{grid-column:1/-1}
button{appearance:none;border:0;border-radius:12px;padding:13px 10px;font:inherit;font-size:14px;font-weight:650;cursor:pointer;background:#eef3f6;color:var(--ink)}
button:active{transform:scale(.97);filter:brightness(.96)}
.b-accent{background:var(--accent-soft);color:var(--accent)}
.b-ok{background:var(--ok-bg);color:var(--ok)}
.b-warn{background:var(--warn-bg);color:var(--warn)}
.b-soft{background:var(--soft-bg);color:var(--soft)}
.b-love{background:var(--love-bg);color:var(--love)}
.b-cool{background:var(--cool-bg);color:var(--cool)}
.b-day{background:var(--day-bg);color:var(--day)}
.b-night{background:var(--night-bg);color:var(--night)}
.b-mute{background:#eef3f6;color:var(--muted)}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:8px}
.chip{border:1px solid var(--line);border-radius:999px;padding:8px 12px;background:#fff;color:var(--muted);font-size:13px;font-weight:600}
.chip.on{border-color:var(--accent);background:var(--accent-soft);color:var(--accent)}
.row{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:8px}
input{width:100%;border:1px solid var(--line);border-radius:12px;background:#f7fafc;color:var(--ink);padding:12px 14px;font:inherit;margin:0 0 8px}
input::placeholder{color:#9aabba}
.cam-wrap{position:relative;border-radius:12px;overflow:hidden;background:#0f1419;aspect-ratio:4/3;margin-bottom:10px}
.cam-wrap video{width:100%;height:100%;object-fit:cover;transform:scaleX(-1);display:block}
.cam-wrap.off video{display:none}
.cam-ph{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;color:#8a9aab;font-size:13px;padding:16px;text-align:center}
.cam-wrap:not(.off) .cam-ph{display:none}
.hint{font-size:12px;color:var(--muted);margin:8px 0 0;line-height:1.5}
.hint b{color:var(--accent);font-weight:650}
#gst{min-height:18px;font-size:13px;font-weight:650;color:var(--accent);margin-top:8px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%) translateY(20px);background:var(--ink);color:#fff;font-size:13px;font-weight:600;padding:10px 16px;border-radius:999px;opacity:0;pointer-events:none;transition:opacity .2s,transform .2s;z-index:9;white-space:nowrap}
#toast.show{opacity:1;transform:translateX(-50%) translateY(0)}
</style>
</head>
<body>
  <header class="top">
    <div class="brand">
      <b>遥控</b>
      <span>Godwise · 手机控制台</span>
    </div>
    <a href="/">设置</a>
  </header>

  <section class="card">
    <h2>摄像头手势（可选）</h2>
    <div class="cam-wrap off" id="camBox">
      <video id="vid" playsinline muted autoplay></video>
      <div class="cam-ph">摄像头打不开也没关系<br/>下面按钮全部可用</div>
    </div>
    <div class="grid">
      <button type="button" class="b-ok" id="camBtn" onclick="toggleCam()">开启摄像头</button>
      <button type="button" class="b-mute" onclick="camOff()">关闭</button>
    </div>
    <div id="gst">手势待机 · 或直接用下方按钮</div>
    <p class="hint">
      <b>左右挥手</b> → 眨眼 · <b>上下点头</b> → 比心 · <b>快速晃动</b> → 换背景<br/>
      固定入口：<b>http://clockwise.local/poke</b><br/><br/>
      <b>打不开摄像头时：</b>这是正常现象（页面是 HTTP，浏览器会拦）。<br/>
      · <b>Android Chrome</b>：地址栏输入 <code>chrome://flags</code> → 搜
      <code>Insecure origins treated as secure</code> → 填入
      <code>http://clockwise.local</code> → 重启 Chrome → 再开本页点「开启摄像头」<br/>
      · <b>iPhone</b>：Safari 基本开不了，请直接用下方按钮遥控<br/>
      · 不折腾也完全够用：点按钮即可控制时钟
    </p>
  </section>

  <section class="card">
    <h2>白天 / 黑夜</h2>
    <div class="grid">
      <button type="button" class="b-day" onclick="dn('day')">白天</button>
      <button type="button" class="b-night" onclick="dn('night')">黑夜</button>
      <button type="button" class="b-mute span" onclick="dn('auto')">跟随时间</button>
    </div>
  </section>

  <section class="card">
    <h2>背景收藏</h2>
    <div class="grid">
      <button type="button" class="b-love" onclick="fav('toggle')">收藏当前</button>
      <button type="button" class="b-cool" onclick="fav('next')">下一张收藏</button>
      <button type="button" class="b-ok" onclick="fav('only')">只播收藏</button>
      <button type="button" class="b-mute" onclick="fav('all')">播放全部</button>
    </div>
  </section>

  <section class="card">
    <h2>人物进出</h2>
    <div class="grid">
      <button type="button" class="b-mute" onclick="bye('right')">请出去 →</button>
      <button type="button" class="b-mute" onclick="bye('left')">← 请出去</button>
      <button type="button" class="b-ok" onclick="poke('back')">请回来</button>
      <button type="button" class="b-warn" onclick="poke('call')">呼唤</button>
    </div>
  </section>

  <section class="card">
    <h2>表盘</h2>
    <div class="grid">
      <button type="button" class="b-cool" onclick="sec('on')">显示秒针</button>
      <button type="button" class="b-mute" onclick="sec('off')">隐藏秒针</button>
    </div>
  </section>

  <section class="card">
    <h2>自动状态</h2>
    <div class="chips" id="mins">
      <button type="button" class="chip on" data-m="5">5分</button>
      <button type="button" class="chip" data-m="10">10分</button>
      <button type="button" class="chip" data-m="30">30分</button>
      <button type="button" class="chip" data-m="60">60分</button>
    </div>
    <div class="chips" id="iv">
      <button type="button" class="chip" data-i="15">每15秒</button>
      <button type="button" class="chip on" data-i="30">每30秒</button>
      <button type="button" class="chip" data-i="60">每60秒</button>
    </div>
    <div class="grid">
      <button type="button" class="b-accent" onclick="lock('sleep')">睡觉</button>
      <button type="button" class="b-love" onclick="lock('shy')">害羞</button>
      <button type="button" class="b-ok" onclick="lock('heart')">比心</button>
      <button type="button" class="b-soft" onclick="lock('peek')">偷看</button>
      <button type="button" class="b-mute span" onclick="lock('none')">取消自动</button>
    </div>
  </section>

  <section class="card">
    <h2>瞬间</h2>
    <div class="grid">
      <button type="button" onclick="poke('blink')">眨眼</button>
      <button type="button" class="b-soft" onclick="poke('wink')">单眼眨</button>
      <button type="button" class="b-soft" onclick="poke('peek')">偷看</button>
      <button type="button" class="b-love" onclick="poke('shy')">害羞</button>
      <button type="button" class="b-ok" onclick="poke('heart')">比心</button>
      <button type="button" class="b-accent" onclick="poke('sleep')">瞌睡</button>
      <button type="button" class="b-warn" onclick="poke('surprise')">小惊喜</button>
      <button type="button" class="b-cool" onclick="poke('next')">换背景</button>
    </div>
  </section>

  <section class="card">
    <h2>气泡</h2>
    <div class="row">
      <button type="button" class="chip" onclick="say('Hi!')">Hi!</button>
      <button type="button" class="chip" onclick="say('Love')">Love</button>
      <button type="button" class="chip" onclick="say('Night')">Night</button>
    </div>
    <input id="msg" maxlength="16" placeholder="英文短句，最多16字" autocomplete="off"/>
    <button type="button" class="b-ok span" style="width:100%" onclick="say(document.getElementById('msg').value||'Hi!')">发送气泡</button>
  </section>

  <canvas id="cv" width="48" height="36" style="display:none"></canvas>
  <div id="toast"></div>
<script>
let mins=5,iv=30,stream=null,raf=0,prev=null,coolUntil=0;
const W=48,H=36,xs=[],ys=[],ms=[];
const vid=document.getElementById('vid');
const cv=document.getElementById('cv');
const ctx=cv.getContext('2d',{willReadFrequently:true});
function pick(sel,cls,set){
  document.querySelectorAll(sel).forEach(b=>{
    b.onclick=()=>{set(b);document.querySelectorAll(sel).forEach(x=>x.classList.remove(cls));b.classList.add(cls);};
  });
}
pick('#mins .chip','on',b=>{mins=+b.dataset.m});
pick('#iv .chip','on',b=>{iv=+b.dataset.i});
function toast(t){
  const el=document.getElementById('toast');
  el.textContent=t;el.classList.add('show');
  clearTimeout(toast._t);toast._t=setTimeout(()=>el.classList.remove('show'),900);
}
function gst(t){document.getElementById('gst').textContent=t;}
async function send(a,t){
  let u='/poke?a='+encodeURIComponent(a);
  if(t!=null)u+='&t='+encodeURIComponent(t);
  try{await fetch(u,{cache:'no-store'});toast('已发送');}catch(e){toast('发送失败');}
}
function poke(a){send(a);}
function bye(d){send('bye',d);}
function fav(m){send('fav',m);}
function say(t){send('say',t);}
function dn(m){send('dn',m);}
function sec(m){send('sec',m);}
function lock(m){if(m==='none')send('lock','none');else send('lock',m+','+mins+','+iv);}

function fireGesture(name,action){
  const now=performance.now();
  if(now<coolUntil)return;
  coolUntil=now+1600;
  gst('识别到：'+name);
  poke(action);
  setTimeout(()=>{if(stream)gst('手势待机 · 继续挥手/点头');},1200);
}

function push(arr,v,n){arr.push(v);if(arr.length>n)arr.shift();}
function flips(arr){
  let c=0;
  for(let i=2;i<arr.length;i++){
    const a=arr[i-1]-arr[i-2],b=arr[i]-arr[i-1];
    if(a*b<0&&Math.abs(a)>0.8&&Math.abs(b)>0.8)c++;
  }
  return c;
}
function analyze(){
  if(!stream||vid.readyState<2)return;
  ctx.drawImage(vid,0,0,W,H);
  const im=ctx.getImageData(0,0,W,H).data;
  const gray=new Float32Array(W*H);
  for(let i=0,p=0;i<im.length;i+=4,p++)gray[p]=0.299*im[i]+0.587*im[i+1]+0.114*im[i+2];
  if(!prev){prev=gray;return;}
  let sum=0,cx=0,cy=0,n=0;
  for(let y=0;y<H;y++){
    for(let x=0;x<W;x++){
      const i=y*W+x,d=Math.abs(gray[i]-prev[i]);
      if(d>18){sum+=d;cx+=x*d;cy+=y*d;n++;}
    }
  }
  prev=gray;
  const motion=sum/(W*H);
  const mx=n?cx/sum:W/2,my=n?cy/sum:H/2;
  push(xs,mx,14);push(ys,my,14);push(ms,motion,10);
  const avgM=ms.reduce((a,b)=>a+b,0)/ms.length;
  if(avgM>55){fireGesture('快速晃动','next');xs.length=0;ys.length=0;return;}
  if(flips(xs)>=3&&avgM>8){fireGesture('左右挥手','blink');xs.length=0;ys.length=0;return;}
  if(flips(ys)>=2&&avgM>7){fireGesture('上下点头','heart');xs.length=0;ys.length=0;return;}
}
function loop(){analyze();raf=requestAnimationFrame(loop);}
async function camOn(){
  if(!navigator.mediaDevices||!navigator.mediaDevices.getUserMedia){
    gst('此浏览器无摄像头接口 · 请用下方按钮');return;
  }
  try{
    stream=await navigator.mediaDevices.getUserMedia({
      audio:false,
      video:{facingMode:'user',width:{ideal:320},height:{ideal:240}}
    });
    vid.srcObject=stream;
    await vid.play();
    document.getElementById('camBox').classList.remove('off');
    document.getElementById('camBtn').textContent='已开启';
    prev=null;xs.length=0;ys.length=0;ms.length=0;
    cancelAnimationFrame(raf);loop();
    gst('手势待机 · 挥手或点头');
  }catch(e){
    const n=e&&e.name?e.name:'';
    if(!window.isSecureContext||n==='NotAllowedError'||n==='SecurityError'){
      gst('被浏览器拦截 · 看下方 Android Chrome 设置，或直接用按钮');
    }else{
      gst('打开失败 · 直接用下方按钮即可');
    }
  }
}
function camOff(){
  cancelAnimationFrame(raf);raf=0;
  if(stream){stream.getTracks().forEach(t=>t.stop());stream=null;}
  vid.srcObject=null;
  document.getElementById('camBox').classList.add('off');
  document.getElementById('camBtn').textContent='开启摄像头';
  gst('手势已关闭');
}
function toggleCam(){if(stream)camOff();else camOn();}
</script>
</body>
</html>
)HTML";
