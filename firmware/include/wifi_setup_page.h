#pragma once
#include <pgmspace.h>
// Página del portal WiFi, servida desde la flash cuando Gimena no tiene red (http://4.3.2.1/).
static const char WIFI_SETUP_PAGE[] PROGMEM = R"GIMENAI_HTML(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Gimena - Conectar WiFi</title>
<style>
:root{--bg:#090d16;--surface:#111827;--border:#1e293b;--hover:#1f2937;--primary:#0284c7;--primary-hover:#0369a1;--text:#f8fafc;--text2:#94a3b8;--muted:#64748b;--error:#f87171;--ok:#34d399}
*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif;-webkit-tap-highlight-color:transparent}
body{background:var(--bg);color:var(--text);min-height:100vh;padding:24px 16px}
main{max-width:440px;margin:0 auto;display:flex;flex-direction:column;gap:16px}
h1{font-size:1.3rem;font-weight:700;letter-spacing:-.02em}
p.sub{color:var(--text2);font-size:.9rem;margin-top:4px;line-height:1.4}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:16px}
.head{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}
.head h2{font-size:.95rem;font-weight:600}
button{border:0;border-radius:8px;cursor:pointer;font-size:.9rem;font-weight:600}
.ghost{background:transparent;color:var(--primary);padding:6px 8px}
.ghost:disabled{color:var(--muted)}
ul{list-style:none}
li button{width:100%;display:flex;justify-content:space-between;align-items:center;gap:8px;background:transparent;color:var(--text);padding:12px 10px;border-radius:8px;text-align:left;font-weight:500}
li button:hover,li button.sel{background:var(--hover)}
li button.sel{outline:1px solid var(--primary)}
.name{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.meta{color:var(--text2);font-size:.8rem;white-space:nowrap}
.empty{color:var(--muted);font-size:.9rem;padding:8px 2px}
label{display:block;font-size:.8rem;color:var(--text2);margin:12px 0 6px}
input{width:100%;background:var(--bg);border:1px solid var(--border);color:var(--text);border-radius:8px;padding:11px 12px;font-size:1rem}
input:focus{outline:none;border-color:var(--primary)}
.show{display:flex;align-items:center;gap:6px;margin-top:8px;font-size:.85rem;color:var(--text2)}
.show input{width:auto}
.main{width:100%;margin-top:16px;background:var(--primary);color:#fff;padding:12px}
.main:hover{background:var(--primary-hover)}
.main:disabled{opacity:.5}
.msg{font-size:.9rem;line-height:1.4;margin-top:12px}
.msg.err{color:var(--error)}
.msg.ok{color:var(--ok)}
.warn{border-color:#7f1d1d;color:var(--error);font-size:.9rem}
</style>
</head>
<body>
<main>
<div>
<h1>Conectar a Gimena</h1>
<p class="sub">Elige la red WiFi de tu casa. Gimena necesita una red de 2,4 GHz.</p>
</div>
<div class="card warn" id="failed" hidden></div>
<section class="card">
<div class="head"><h2>Redes disponibles</h2><button class="ghost" id="rescan">Buscar de nuevo</button></div>
<ul id="list"><li class="empty">Buscando redes...</li></ul>
</section>
<form class="card" id="form">
<label for="ssid">Nombre de la red</label>
<input id="ssid" maxlength="32" autocomplete="off" autocapitalize="off" required>
<label for="pass">Clave</label>
<input id="pass" type="password" maxlength="64" autocomplete="off">
<label class="show"><input type="checkbox" id="showpass"> Mostrar clave</label>
<button class="main" id="go" type="submit">Conectar</button>
<p class="msg" id="msg" hidden></p>
</form>
</main>
<script>
const $=id=>document.getElementById(id);
let timer=null;
function signal(r){return r>-60?'Excelente':r>-70?'Buena':r>-80?'Regular':'Débil'}
function render(nets){
  const ul=$('list');ul.innerHTML='';
  if(!nets.length){ul.innerHTML='<li class="empty">No se encontraron redes. Pulsa Buscar de nuevo.</li>';return}
  nets.forEach(n=>{
    const li=document.createElement('li'),b=document.createElement('button');
    b.type='button';
    const name=document.createElement('span');name.className='name';name.textContent=n.ssid;
    const meta=document.createElement('span');meta.className='meta';meta.textContent=signal(n.rssi)+(n.secure?' · Protegida':' · Abierta');
    b.append(name,meta);
    b.onclick=()=>{document.querySelectorAll('li button').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');$('ssid').value=n.ssid;$('pass').value='';if(n.secure)$('pass').focus()};
    li.append(b);ul.append(li);
  });
}
async function scan(nuevo){
  clearTimeout(timer);$('rescan').disabled=true;
  if(nuevo)$('list').innerHTML='<li class="empty">Buscando redes...</li>';
  try{
    const r=await fetch('/api/wifi/scan'+(nuevo?'?nuevo=1':''));const d=await r.json();
    if(d.failed){$('failed').hidden=false;$('failed').textContent='No se pudo conectar a "'+d.failed+'". Revisa la clave o elige otra red.'}
    if(d.scanning){timer=setTimeout(()=>scan(false),1500);return}
    render(d.networks||[]);
  }catch(e){timer=setTimeout(()=>scan(false),2000);return}
  $('rescan').disabled=false;
}
$('rescan').onclick=()=>scan(true);
$('showpass').onchange=e=>{$('pass').type=e.target.checked?'text':'password'};
$('form').onsubmit=async e=>{
  e.preventDefault();
  const msg=$('msg'),ssid=$('ssid').value.trim();
  if(!ssid)return;
  $('go').disabled=true;msg.hidden=false;msg.className='msg';msg.textContent='Guardando...';
  try{
    const r=await fetch('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,password:$('pass').value})});
    const d=await r.json();
    if(!r.ok)throw new Error(d.error||'Error al guardar');
    msg.className='msg ok';
    msg.textContent='Listo. Gimena se reinicia y se conecta a "'+ssid+'". Vuelve a conectar tu celular a tu red de siempre. Si la clave era incorrecta, la red de Gimena aparecerá otra vez.';
  }catch(err){msg.className='msg err';msg.textContent=err.message;$('go').disabled=false}
};
scan(false);
</script>
</body>
</html>
)GIMENAI_HTML";
