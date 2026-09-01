#pragma once

const char POKE_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>逗她</title>
<style>
  body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:#12141a;color:#eef1f6;padding:20px;max-width:560px}
  h1{font-size:22px;margin:0 0 14px}
  h2{font-size:15px;margin:18px 0 8px;color:#c9d0dc}
  .grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
  button{
    display:block;width:100%;border:0;border-radius:12px;padding:14px 12px;
    font-size:15px;font-weight:700;color:#151820;background:#ffb4d0;cursor:pointer
  }
  button.alt{background:#9ecbff} button.warn{background:#ffe08a}
  button.ok{background:#b8f5c8} button.love{background:#ff8fb8}
  button.soft{background:#d7c4ff} button.mint{background:#9ef0d2}
  button.cream{background:#ffe6c7} button.unlock{background:#c5cad6}
  button.day{background:#ffd56a} button.night{background:#8aa4ff}
  .box{margin-top:4px;padding:14px;border-radius:14px;background:#1a1e28;border:1px solid #2a3140}
  .row{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}
  .chip{border:0;border-radius:999px;padding:9px 12px;background:#2a3140;color:#eef1f6;font-size:13px;cursor:pointer}
  .chip.on{background:#ffe08a;color:#151820}
  input{width:100%;box-sizing:border-box;border-radius:10px;border:1px solid #3a4254;background:#12151c;color:#fff;padding:12px;font-size:15px;margin:8px 0}
  a.set{color:#9ecbff;font-size:14px}
  #toast{min-height:18px;color:#9ef0c2;font-size:13px;margin-top:10px}
</style>
</head>
<body>
  <h1>逗她</h1>

  <div class="box">
    <h2 style="margin-top:0">白天 / 黑夜</h2>
    <div class="grid">
      <button type="button" class="day" onclick="dn('day')">白天</button>
      <button type="button" class="night" onclick="dn('night')">黑夜</button>
      <button type="button" class="unlock" style="grid-column:1/-1" onclick="dn('auto')">跟随时间</button>
    </div>
  </div>

  <div class="box" style="margin-top:14px">
    <h2 style="margin-top:0">表盘</h2>
    <div class="grid">
      <button type="button" class="alt" onclick="sec('on')">显示秒针</button>
      <button type="button" class="unlock" onclick="sec('off')">隐藏秒针</button>
    </div>
  </div>

  <div class="box" style="margin-top:14px">
    <h2 style="margin-top:0">自动状态</h2>
    <div class="row" id="mins">
      <button type="button" class="chip on" data-m="5">5分</button>
      <button type="button" class="chip" data-m="10">10分</button>
      <button type="button" class="chip" data-m="30">30分</button>
      <button type="button" class="chip" data-m="60">60分</button>
    </div>
    <div class="row" id="iv" style="margin-top:8px">
      <button type="button" class="chip" data-i="15">每15秒</button>
      <button type="button" class="chip on" data-i="30">每30秒</button>
      <button type="button" class="chip" data-i="60">每60秒</button>
    </div>
    <div class="grid" style="margin-top:10px">
      <button type="button" class="mint" onclick="lock('sleep')">睡觉</button>
      <button type="button" class="love" onclick="lock('shy')">害羞</button>
      <button type="button" class="ok" onclick="lock('heart')">比心</button>
      <button type="button" class="cream" onclick="lock('peek')">偷看</button>
      <button type="button" class="unlock" style="grid-column:1/-1" onclick="lock('none')">取消</button>
    </div>
  </div>

  <h2>瞬间</h2>
  <div class="grid">
    <button type="button" onclick="poke('blink')">眨眼</button>
    <button type="button" class="soft" onclick="poke('wink')">单眼眨</button>
    <button type="button" class="cream" onclick="poke('peek')">偷看</button>
    <button type="button" class="love" onclick="poke('shy')">害羞</button>
    <button type="button" class="ok" onclick="poke('heart')">比心</button>
    <button type="button" class="mint" onclick="poke('sleep')">瞌睡</button>
    <button type="button" class="warn" onclick="poke('surprise')">小惊喜</button>
    <button type="button" class="alt" onclick="poke('next')">换背景</button>
    <button type="button" class="unlock" onclick="poke('bye')">请出去</button>
    <button type="button" class="mint" onclick="poke('back')">请回来</button>
  </div>

  <h2>气泡</h2>
  <div class="row">
    <button type="button" class="chip" onclick="say('Hi!')">Hi!</button>
    <button type="button" class="chip" onclick="say('Love')">Love</button>
    <button type="button" class="chip" onclick="say('Night')">Night</button>
  </div>
  <input id="msg" maxlength="16" placeholder="英文"/>
  <button type="button" class="ok" style="width:100%;margin-top:6px" onclick="say(document.getElementById('msg').value||'Hi!')">发送</button>
  <p style="margin-top:16px"><a class="set" href="/">设置</a></p>
  <div id="toast"></div>
<script>
let mins=5, iv=30;
document.querySelectorAll('#mins .chip').forEach(b=>{
  b.onclick=()=>{
    mins=+b.dataset.m;
    document.querySelectorAll('#mins .chip').forEach(x=>x.classList.remove('on'));
    b.classList.add('on');
  };
});
document.querySelectorAll('#iv .chip').forEach(b=>{
  b.onclick=()=>{
    iv=+b.dataset.i;
    document.querySelectorAll('#iv .chip').forEach(x=>x.classList.remove('on'));
    b.classList.add('on');
  };
});
async function send(a,t){
  let u='/poke?a='+encodeURIComponent(a);
  if(t!=null) u+='&t='+encodeURIComponent(t);
  await fetch(u,{cache:'no-store'});
  document.getElementById('toast').textContent='ok';
}
function poke(a){ send(a); }
function say(t){ send('say', t); }
function dn(m){ send('dn', m); }
function sec(m){ send('sec', m); }
function lock(m){
  if(m==='none') send('lock','none');
  else send('lock', m+','+mins+','+iv);
}
</script>
</body>
</html>
)HTML";
