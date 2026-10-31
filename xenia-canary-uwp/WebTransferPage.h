#pragma once

// Browser UI served by WebTransferServer at "/". Kept fully self-contained
// (no CDN) so it works on a LAN without internet access. Split into several
// raw string pieces to stay under MSVC's per-literal size limit.

#include <string>

namespace UWP {

inline const std::string& GetWebTransferPage() {
  static const std::string page = std::string(R"XT(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Xenia UWP Transfer</title>
<style>
:root{--bg:#0f1210;--panel:#181d19;--line:#2a322c;--text:#e8efe9;--dim:#93a196;--accent:#6cc04a;--accent2:#4e9a32;--danger:#e0574f}
@media (prefers-color-scheme: light){:root{--bg:#f3f6f3;--panel:#fff;--line:#d9e0da;--text:#17201a;--dim:#5d6b61;--accent:#3f8f22;--accent2:#2f6d18;--danger:#c23b33}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{padding:18px 16px 8px;max-width:1000px;margin:0 auto}
h1{font-size:20px;margin:0 0 4px}
.sub{color:var(--dim);font-size:13px}
main{max-width:1000px;margin:0 auto;padding:0 16px 40px}
.tabs{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0}
.tab{border:1px solid var(--line);background:var(--panel);color:var(--text);padding:8px 14px;border-radius:999px;cursor:pointer;font:inherit}
.tab.on{background:var(--accent);border-color:var(--accent);color:#fff}
.tab small{opacity:.8;margin-left:6px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px;margin-bottom:14px}
.desc{color:var(--dim);font-size:13px;margin:0 0 10px}
.bar{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
button.b{font:inherit;border:1px solid var(--line);background:transparent;color:var(--text);padding:7px 12px;border-radius:8px;cursor:pointer}
button.b.p{background:var(--accent);border-color:var(--accent);color:#fff}
button.b:hover{border-color:var(--accent)}
button.b.d{color:var(--danger)}
.crumbs{margin:10px 0 6px;font-size:14px;word-break:break-all}
.crumbs a{color:var(--accent);cursor:pointer;text-decoration:none}
#drop{border:2px dashed var(--line);border-radius:12px;padding:22px;text-align:center;color:var(--dim);margin-bottom:14px;transition:.15s}
#drop.over{border-color:var(--accent);color:var(--text);background:rgba(108,192,74,.08)}
table{width:100%;border-collapse:collapse}
td{padding:8px 6px;border-top:1px solid var(--line);vertical-align:middle}
td.n{word-break:break-all}
td.n a{color:var(--text);cursor:pointer;font-weight:600;text-decoration:none}
td.s{color:var(--dim);white-space:nowrap;text-align:right;font-variant-numeric:tabular-nums}
td.a{white-space:nowrap;text-align:right}
td.a button{padding:4px 8px;font-size:13px}
.empty{color:var(--dim);padding:14px 6px}
.q{font-size:13px}
.qi{padding:6px 0;border-top:1px solid var(--line)}
.qi:first-child{border-top:0}
.qn{display:flex;justify-content:space-between;gap:8px}
.qn span:first-child{word-break:break-all}
.pb{height:6px;background:var(--line);border-radius:3px;overflow:hidden;margin-top:4px}
.pb i{display:block;height:100%;width:0;background:var(--accent)}
.err{color:var(--danger)}
.ok{color:var(--accent)}
#sum{font-size:13px;color:var(--dim);margin-bottom:6px}
@media (max-width:600px){td.s{display:none}td.a button{padding:4px 6px}}
</style></head><body>
<header><h1>Xenia UWP &middot; File Transfer</h1>
<div class="sub">Copy games and content into the console's internal storage. Keep the emulator open on the console while transferring.</div></header>
<main>
<div class="tabs" id="tabs"></div>
<div class="card">
<p class="desc" id="rootdesc"></p>
<div class="bar">
<button class="b p" id="bf">Upload files</button>
<button class="b p" id="bd">Upload folder</button>
<button class="b" id="bn">New folder</button>
<button class="b" id="br">Refresh</button>
</div>
<div class="crumbs" id="crumbs"></div>
</div>
<div id="drop">Drop files or whole game folders here</div>
<div class="card" id="qcard" style="display:none">
<div id="sum"></div><div class="q" id="queue"></div>
<div class="bar" style="margin-top:8px"><button class="b d" id="bc">Cancel remaining</button></div>
</div>
<div class="card"><table><tbody id="list"></tbody></table></div>
<input type="file" id="fi" multiple hidden>
<input type="file" id="di" webkitdirectory multiple hidden>
</main>
)XT") + std::string(R"XT(<script>
let roots=[],root='',cwd='',queue=[],busy=false,cancel=false,xhr=null,doneBytes=0,totalBytes=0,startT=0;
const $=id=>document.getElementById(id);
const fmt=n=>{if(n<1024)return n+' B';const u=['KB','MB','GB','TB'];let i=-1;do{n/=1024;i++}while(n>=1024&&i<3);return n.toFixed(n<10?2:1)+' '+u[i]};
const esc=s=>s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const join=(a,b)=>a?(b?a+'/'+b:a):b;
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
  $('tabs').innerHTML=roots.map(r=>'<button class="tab'+(r.name===root?' on':'')+'" data-r="'+esc(r.name)+'">'+esc(r.name)+'<small>'+fmt(r.free)+' free</small></button>').join('');
  document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>{root=b.dataset.r;cwd='';loadInfo();list()});
  const cur=roots.find(r=>r.name===root);$('rootdesc').textContent=cur?cur.description:'';
}
async function list(){
  const parts=cwd?cwd.split('/'):[];
  let c='<a data-p="">'+esc(root)+'</a>';
  parts.forEach((p,i)=>{c+=' / <a data-p="'+esc(parts.slice(0,i+1).join('/'))+'">'+esc(p)+'</a>'});
  $('crumbs').innerHTML=c;
  document.querySelectorAll('#crumbs a').forEach(a=>a.onclick=()=>{cwd=a.dataset.p;list()});
  let j;
  try{j=await api('list',{root,path:cwd})}catch(e){$('list').innerHTML='<tr><td class="empty err">'+esc(e.message)+'</td></tr>';return}
  if(!j.entries.length){$('list').innerHTML='<tr><td class="empty">This folder is empty.</td></tr>';return}
  $('list').innerHTML=j.entries.map(e=>'<tr><td class="n">'+(e.dir?'&#128193; <a data-d="'+esc(e.name)+'">'+esc(e.name)+'</a>':'&#128196; '+esc(e.name))+'</td><td class="s">'+(e.dir?'':fmt(e.size))+'</td><td class="a">'+
    (e.dir?'':'<button class="b" data-dl="'+esc(e.name)+'">Download</button> ')+
    '<button class="b" data-rn="'+esc(e.name)+'">Rename</button> <button class="b d" data-rm="'+esc(e.name)+'">Delete</button></td></tr>').join('');
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
)XT") + std::string(R"XT(
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
loadInfo().then(list).catch(e=>{$('list').innerHTML='<tr><td class="empty err">'+esc(e.message)+'</td></tr>'});
</script></body></html>
)XT");
  return page;
}

}  // namespace UWP
