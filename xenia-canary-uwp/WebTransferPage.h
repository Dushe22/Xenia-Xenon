#pragma once

// Browser UI served by WebTransferServer at "/". Kept self-contained (no CDN)
// so it works on a LAN without internet access; the font and logo come from
// the package through /assets/. Split into several raw string pieces to stay
// under MSVC's per-literal size limit.

#include <string>

namespace UWP {

inline const std::string& GetWebTransferPage() {
  static const std::string page = std::string(R"XT(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Xenia Xenon Transfer</title>
<link rel="icon" href="/assets/logo.png">
<style>
@font-face{font-family:Xenon;src:url(/assets/font-regular.ttf);font-weight:400}
@font-face{font-family:Xenon;src:url(/assets/font-medium.ttf);font-weight:500}
@font-face{font-family:Xenon;src:url(/assets/font-bold.ttf);font-weight:700}
:root{--bg:#0a0c1a;--side:#0c0e20;--row:rgba(255,255,255,.04);--row2:rgba(255,255,255,.08);--line:rgba(255,255,255,.08);--text:#eef1ff;--dim:#a0a8cd;--faint:#6c7399;--v:#8460ff;--b:#4696ff;--grad:linear-gradient(90deg,#8460ff,#4696ff);--danger:#ff6b81;--ok:#5ee0a0}
*{box-sizing:border-box}
html,body{height:100%}
body{margin:0;color:var(--text);font:15px/1.45 Xenon,system-ui,Segoe UI,Roboto,sans-serif;background:var(--bg);
  background-image:radial-gradient(900px 600px at 85% -10%,rgba(132,96,255,.22),transparent 60%),radial-gradient(700px 500px at 100% 110%,rgba(70,150,255,.14),transparent 60%);background-attachment:fixed}
.app{display:flex;min-height:100%}
aside{width:300px;flex:none;background:var(--side);border-right:1px solid var(--line);padding:40px 22px;display:flex;flex-direction:column;gap:6px;position:sticky;top:0;height:100vh}
.brand{display:flex;align-items:center;gap:14px;margin:0 6px 40px}
.brand img{width:58px;height:58px}
.brand b{display:block;font-size:26px;font-weight:700;letter-spacing:.32em}
.brand small{display:block;font-size:11px;letter-spacing:.32em;color:var(--dim);margin-top:2px}
.nav{all:unset;cursor:pointer;display:flex;align-items:center;gap:12px;padding:16px 22px;border-radius:999px;color:var(--dim);font-size:18px;font-weight:500;transition:background .2s,color .2s,box-shadow .2s}
.nav::before{content:'';width:6px;height:6px;border-radius:50%;background:currentColor;opacity:.6}
.nav:hover{background:var(--row);color:var(--text)}
.nav.on{background:var(--grad);color:#fff;box-shadow:0 8px 28px rgba(132,96,255,.35)}
.nav.on::before{opacity:1}
.nav span{display:flex;flex-direction:column;line-height:1.2}
.nav small{font-size:12px;font-weight:400;opacity:.75}
.foot{margin-top:auto;color:var(--faint);font-size:12px;padding:0 6px}
main{flex:1;min-width:0;padding:40px 56px 56px}
.top{display:flex;justify-content:flex-end;align-items:center;gap:22px;margin-bottom:34px}
.chip{display:flex;align-items:center;gap:8px;padding:7px 16px;border-radius:999px;border:1px solid rgba(122,140,255,.55);background:rgba(122,140,255,.15);font-size:14px}
.chip::before{content:'';width:8px;height:8px;border-radius:50%;background:#4fb0ff}
#clock{font-size:26px;font-weight:500}
h1{font-size:40px;font-weight:700;margin:0;line-height:1.1}
.head{display:flex;justify-content:space-between;align-items:baseline;gap:16px}
.count{color:var(--faint);font-size:13px;letter-spacing:.08em;text-transform:uppercase;white-space:nowrap}
.desc{color:var(--dim);margin:8px 0 22px;max-width:760px}
.bar{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.btn{font:inherit;font-weight:500;border:1px solid var(--line);background:var(--row);color:var(--text);padding:10px 20px;border-radius:999px;cursor:pointer;transition:background .2s,border-color .2s}
.btn:hover{background:var(--row2);border-color:rgba(132,96,255,.6)}
.btn.p{background:var(--grad);border-color:transparent;color:#fff}
.btn.p:hover{filter:brightness(1.1)}
.btn.d{color:var(--danger)}
.btn.s{padding:6px 14px;font-size:13px}
.crumbs{margin:22px 0 12px;font-size:15px;color:var(--faint);word-break:break-all}
.crumbs a{color:var(--dim);cursor:pointer}
.crumbs a:last-child{color:var(--text)}
#drop{border:1.5px dashed rgba(132,96,255,.45);border-radius:16px;padding:26px;text-align:center;color:var(--dim);margin-bottom:16px;background:rgba(132,96,255,.04);transition:.2s}
#drop.over{border-color:var(--b);color:var(--text);background:rgba(70,150,255,.1)}
.card{background:var(--row);border:1px solid var(--line);border-radius:16px;padding:16px 18px;margin-bottom:16px}
#sum{font-size:13px;color:var(--dim);margin-bottom:8px}
.qi{padding:8px 0;border-top:1px solid var(--line);font-size:13px}
.qi:first-child{border-top:0}
.qn{display:flex;justify-content:space-between;gap:8px}
.qn span:first-child{word-break:break-all}
.pb{height:5px;background:var(--line);border-radius:3px;overflow:hidden;margin-top:6px}
.pb i{display:block;height:100%;width:0;background:var(--grad)}
.rows{display:flex;flex-direction:column;gap:8px}
.r{display:flex;align-items:center;gap:14px;padding:14px 20px;border-radius:14px;background:var(--row);border:1px solid transparent;transition:background .15s,border-color .15s}
.r:hover{background:var(--row2);border-color:rgba(132,96,255,.35)}
.ic{width:34px;height:34px;flex:none;border-radius:10px;display:grid;place-items:center;font-size:12px;font-weight:700;color:#fff;background:rgba(255,255,255,.08)}
.r.dir .ic{background:var(--grad)}
.nm{flex:1;min-width:0;word-break:break-all;font-weight:500}
.r.dir .nm{cursor:pointer}
.sz{color:var(--dim);font-size:13px;white-space:nowrap;font-variant-numeric:tabular-nums}
.ac{display:flex;gap:6px;white-space:nowrap}
.empty{color:var(--dim);padding:18px 20px}
.err{color:var(--danger)}
.ok{color:var(--ok)}
@media (max-width:860px){.app{flex-direction:column}aside{width:auto;height:auto;position:static;padding:20px 16px;flex-direction:row;flex-wrap:wrap;border-right:0;border-bottom:1px solid var(--line)}
  .brand{width:100%;margin:0 0 10px}.nav{padding:10px 16px;font-size:15px}.foot{display:none}main{padding:20px 16px 40px}.top{margin-bottom:16px}h1{font-size:30px}.sz{display:none}.r{flex-wrap:wrap;padding:12px 14px}.nm{flex:1 1 60%}.ac{width:100%;justify-content:flex-end}}
</style></head><body><div class="app">
<aside><div class="brand"><img src="/assets/logo.png" alt=""><div><b>XENON</b><small>WEB TRANSFER</small></div></div>
<div id="tabs" style="display:contents"></div>
<div class="foot">Keep Xenon open on the console while transferring.</div></aside>
<main>
<div class="top"><div class="chip">Transfer on</div><div id="clock"></div></div>
<div class="head"><h1 id="title"></h1><div class="count" id="count"></div></div>
<p class="desc" id="rootdesc"></p>
<div class="bar">
<button class="btn p" id="bf">Upload files</button>
<button class="btn p" id="bd">Upload folder</button>
<button class="btn" id="bn">New folder</button>
<button class="btn" id="br">Refresh</button>
</div>
<div class="crumbs" id="crumbs"></div>
<div id="drop">Drop files or whole game folders here</div>
<div class="card" id="qcard" style="display:none">
<div id="sum"></div><div id="queue"></div>
<div class="bar" style="margin-top:10px"><button class="btn s d" id="bc">Cancel remaining</button></div>
</div>
<div class="rows" id="list"></div>
<input type="file" id="fi" multiple hidden>
<input type="file" id="di" webkitdirectory multiple hidden>
</main></div>
)XT") + std::string(R"XT(<script>
let roots=[],root='',cwd='',queue=[],busy=false,cancel=false,xhr=null,doneBytes=0,totalBytes=0,startT=0;
const $=id=>document.getElementById(id);
const fmt=n=>{if(n<1024)return n+' B';const u=['KB','MB','GB','TB'];let i=-1;do{n/=1024;i++}while(n>=1024&&i<3);return n.toFixed(n<10?2:1)+' '+u[i]};
const esc=s=>s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const join=(a,b)=>a?(b?a+'/'+b:a):b;
const ext=n=>{const i=n.lastIndexOf('.');return i>0?n.slice(i+1,i+4).toUpperCase():'FILE'};
function clock(){const d=new Date();$('clock').textContent=String(d.getHours()).padStart(2,'0')+':'+String(d.getMinutes()).padStart(2,'0')}
function qs(o){return Object.entries(o).map(([k,v])=>encodeURIComponent(k)+'='+encodeURIComponent(v)).join('&')}
async function api(path,params,method){
  const r=await fetch('/api/'+path+'?'+qs(params),{method:method||'GET'});
  const j=await r.json().catch(()=>({ok:false,error:'HTTP '+r.status}));
  if(!j.ok)throw new Error(j.error||('HTTP '+r.status));
  return j;
}
async function loadInfo(){
  const j=await api('info',{});roots=j.roots;
  if(!root&&roots.length)root=roots[0].name;
  $('tabs').innerHTML=roots.map(r=>'<button class="nav'+(r.name===root?' on':'')+'" data-r="'+esc(r.name)+'"><span>'+esc(r.name)+'<small>'+fmt(r.free)+' free</small></span></button>').join('');
  document.querySelectorAll('.nav').forEach(b=>b.onclick=()=>{root=b.dataset.r;cwd='';loadInfo();list()});
  const cur=roots.find(r=>r.name===root);$('title').textContent=root;$('rootdesc').textContent=cur?cur.description:'';
}
function row(e){
  const n=esc(e.name);
  return '<div class="r'+(e.dir?' dir':'')+'"><div class="ic">'+(e.dir?'DIR':esc(ext(e.name)))+'</div>'+
    '<div class="nm"'+(e.dir?' data-d="'+n+'"':'')+'>'+n+'</div><div class="sz">'+(e.dir?'':fmt(e.size))+'</div><div class="ac">'+
    (e.dir?'':'<button class="btn s" data-dl="'+n+'">Download</button>')+
    '<button class="btn s" data-rn="'+n+'">Rename</button><button class="btn s d" data-rm="'+n+'">Delete</button></div></div>';
}
async function list(){
  const parts=cwd?cwd.split('/'):[];
  let c='<a data-p="">'+esc(root)+'</a>';
  parts.forEach((p,i)=>{c+=' / <a data-p="'+esc(parts.slice(0,i+1).join('/'))+'">'+esc(p)+'</a>'});
  $('crumbs').innerHTML=c;
  document.querySelectorAll('#crumbs a').forEach(a=>a.onclick=()=>{cwd=a.dataset.p;list()});
  let j;
  try{j=await api('list',{root,path:cwd})}catch(e){$('count').textContent='';$('list').innerHTML='<div class="empty err">'+esc(e.message)+'</div>';return}
  $('count').textContent=j.entries.length+' items';
  if(!j.entries.length){$('list').innerHTML='<div class="empty">This folder is empty.</div>';return}
  $('list').innerHTML=j.entries.map(row).join('');
  document.querySelectorAll('[data-d]').forEach(a=>a.onclick=()=>{cwd=join(cwd,a.dataset.d);list()});
  document.querySelectorAll('[data-dl]').forEach(b=>b.onclick=()=>{location.href='/api/download?'+qs({root,path:join(cwd,b.dataset.dl)})});
  document.querySelectorAll('[data-rn]').forEach(b=>b.onclick=async()=>{const n=prompt('New name',b.dataset.rn);if(!n||n===b.dataset.rn)return;try{await api('rename',{root,path:join(cwd,b.dataset.rn),to:n},'POST')}catch(e){alert(e.message)}list()});
  document.querySelectorAll('[data-rm]').forEach(b=>b.onclick=async()=>{if(!confirm('Delete "'+b.dataset.rm+'" from the console? This cannot be undone.'))return;try{await api('delete',{root,path:join(cwd,b.dataset.rm)},'POST')}catch(e){alert(e.message)}loadInfo();list()});
}
function enqueue(items){
  if(!items.length)return;
  if(!busy){doneBytes=0;totalBytes=0;$('queue').innerHTML=''}
  items.forEach(it=>{const el=document.createElement('div');el.className='qi';
    el.innerHTML='<div class="qn"><span>'+esc(it.rel)+'</span><span class="st">'+fmt(it.file.size)+'</span></div><div class="pb"><i></i></div>';
    $('queue').appendChild(el);it.el=el;it.root=root;it.base=cwd;queue.push(it);totalBytes+=it.file.size});
  $('qcard').style.display='';cancel=false;if(!busy)next();
}
function summary(){const t=(Date.now()-startT)/1000||1;const left=queue.length+(busy?1:0);
  $('sum').textContent=(left?left+' file(s) left · ':'All done · ')+fmt(doneBytes)+' of '+fmt(totalBytes)+' · '+fmt(doneBytes/t)+'/s'}
)XT") + std::string(R"XT(
function next(){
  if(cancel){queue.forEach(it=>{it.el.querySelector('.st').innerHTML='<span class="err">cancelled</span>'});totalBytes-=queue.reduce((a,it)=>a+it.file.size,0);queue=[]}
  const it=queue.shift();
  if(!it){busy=false;summary();loadInfo();list();return}
  if(!busy){startT=Date.now()}
  busy=true;const st=it.el.querySelector('.st'),bar=it.el.querySelector('i');
  xhr=new XMLHttpRequest();
  xhr.open('PUT','/api/upload?'+qs({root:it.root,path:join(it.base,it.rel)}));
  let last=0;
  xhr.upload.onprogress=e=>{doneBytes+=e.loaded-last;last=e.loaded;bar.style.width=(e.loaded/it.file.size*100)+'%';st.textContent=fmt(e.loaded)+' / '+fmt(it.file.size);summary()};
  xhr.onload=()=>{let ok=xhr.status>=200&&xhr.status<300;let msg='';try{const j=JSON.parse(xhr.responseText);ok=ok&&j.ok;msg=j.error||''}catch(e){}
    doneBytes+=it.file.size-last;
    if(ok){bar.style.width='100%';st.innerHTML='<span class="ok">done</span>'}else{st.innerHTML='<span class="err">'+esc(msg||('HTTP '+xhr.status))+'</span>'}next()};
  xhr.onerror=xhr.onabort=()=>{doneBytes+=it.file.size-last;st.innerHTML='<span class="err">failed</span>';next()};
  xhr.send(it.file);
}
async function walk(entry,prefix,out){
  if(entry.isFile){await new Promise(res=>entry.file(f=>{out.push({file:f,rel:prefix+f.name});res()},()=>res()));return}
  if(entry.isDirectory){const rd=entry.createReader();let batch;
    do{batch=await new Promise(res=>rd.readEntries(res,()=>res([])));for(const e of batch)await walk(e,prefix+entry.name+'/',out)}while(batch.length)}
}
const drop=$('drop');
['dragenter','dragover'].forEach(ev=>document.addEventListener(ev,e=>{e.preventDefault();drop.classList.add('over')}));
['dragleave','drop'].forEach(ev=>document.addEventListener(ev,e=>{e.preventDefault();if(ev==='drop'||e.target===drop)drop.classList.remove('over')}));
document.addEventListener('drop',async e=>{
  const out=[];const items=[...(e.dataTransfer.items||[])];
  if(items.length&&items[0].webkitGetAsEntry){const entries=items.map(i=>i.webkitGetAsEntry()).filter(Boolean);for(const en of entries)await walk(en,'',out)}
  else for(const f of e.dataTransfer.files)out.push({file:f,rel:f.name});
  enqueue(out);
});
$('bf').onclick=()=>$('fi').click();
$('bd').onclick=()=>$('di').click();
$('fi').onchange=e=>{enqueue([...e.target.files].map(f=>({file:f,rel:f.name})));e.target.value=''};
$('di').onchange=e=>{enqueue([...e.target.files].map(f=>({file:f,rel:f.webkitRelativePath||f.name})));e.target.value=''};
$('bn').onclick=async()=>{const n=prompt('Folder name');if(!n)return;try{await api('mkdir',{root,path:join(cwd,n)},'POST')}catch(e){alert(e.message)}list()};
$('br').onclick=()=>{loadInfo();list()};
$('bc').onclick=()=>{cancel=true;if(xhr)xhr.abort()};
window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue=''}});
clock();setInterval(clock,10000);
loadInfo().then(list).catch(e=>{$('list').innerHTML='<div class="empty err">'+esc(e.message)+'</div>'});
</script></body></html>
)XT");
  return page;
}

}  // namespace UWP
