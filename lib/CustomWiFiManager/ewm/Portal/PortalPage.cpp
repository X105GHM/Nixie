#include "ewm/Portal/PortalPage.hpp"

namespace ewm::portal
{

    const char *kPortalPage = R"HTML(
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no" />
<title>ESP32 WLAN-Einrichtung</title>
<style>
:root{--bg:#0b1220;--card:#121a2b;--muted:#9fb0d0;--acc:#62d2a2;--danger:#e25b5b;--txt:#f5f7fb}
*{box-sizing:border-box}
html { -webkit-text-size-adjust:100%; }
input,select,textarea,button { font-size:16px; line-height:1.2; }
a,button { touch-action:manipulation; }
body{margin:0;font:16px/1.45 system-ui;background:var(--bg);color:var(--txt)}
.container{max-width:880px;margin:0 auto;padding:24px}
.card{background:var(--card);padding:20px;border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,.25)}
h1{font-size:1.6rem;margin:0 0 12px}
p.muted{color:var(--muted)}
.row{display:grid;gap:12px}
@media(min-width:720px){.row{grid-template-columns:1fr 1fr}}
label{display:block;font-weight:600;margin:8px 0 4px}
input,select{width:100%;padding:12px 14px;border-radius:12px;border:1px solid #2a3550;background:#0e1626;color:var(--txt)}
button{border:0;border-radius:12px;padding:10px 14px;font-weight:700;cursor:pointer}
button.primary{background:var(--acc);color:#0a0f14}
button.danger{background:var(--danger);color:#fff}
.cred-list{list-style:none;padding:0;margin:10px 0 0}
.cred{display:flex;align-items:center;gap:8px;justify-content:space-between;padding:10px;background:#0e1626;border-radius:10px;margin:8px 0}
.cred b{font-weight:700}
.cred .pri{color:var(--muted);margin-left:8px}
.buttons{display:flex;gap:8px;align-items:center}
footer{opacity:.7;margin-top:16px}
hr{border:none;border-top:1px solid #2a3550;margin:16px 0}
.notice{background:#0e1626;border-left:4px solid #62d2a2;padding:10px;border-radius:10px}
button.loading { position:relative; }
button.loading::after{
  content:""; position:absolute; right:10px; top:50%; width:14px; height:14px; margin-top:-7px;
  border:2px solid rgba(255,255,255,.6); border-top-color:transparent; border-radius:50%;
  animation:spin 0.8s linear infinite;
}
@keyframes spin { to { transform: rotate(360deg); } }
.modal {
  position: fixed; inset: 0; display: none; place-items: center;
  background: rgba(0,0,0,.5); z-index: 9999;
}
.modal .box{
  background:#121a2b; padding:18px; border-radius:14px; max-width:480px; width:calc(100% - 40px);
  box-shadow:0 10px 30px rgba(0,0,0,.35)
}
.modal .rowbtns{ display:flex; gap:8px; margin-top:12px; flex-wrap:wrap }
.modal.show{ display:grid }
</style>
</head>
<body>
<div class="container">
  <div class="card">
    <h1>WLAN konfigurieren</h1>
    <p class="muted">Verbinde den ESP32 mit einem bekannten Netzwerk oder verwalte gespeicherte Zugangsdaten.</p>

    <div class="row">
      <div>
        <label>Gefundene Netzwerke</label>
        <select id="ssid">__OPTIONS__</select>
      </div>
      <div>
        <label>Passwort</label>
        <input id="pw" type="password" placeholder="WLAN-Passwort"
               autocomplete="current-password" autocapitalize="off"
               autocorrect="off" spellcheck="false" />
      </div>
    </div>
    <div class="row">
      <div>
        <label>Priorität (0 = höchste)</label>
        <input id="prio" type="number" min="0" max="254" value="1" />
      </div>
      <div style="display:flex;align-items:flex-end;gap:8px">
        <button id="add" class="primary">Hinzufügen</button>
        <button id="connect" class="primary">Verbinden</button>
      </div>
    </div>

    <hr/>
    <h3>Gespeicherte Netzwerke</h3>
    <ul class="cred-list" id="list">__LIST__</ul>
    <div class="buttons" style="margin-top:10px">
      <button id="save" class="primary">Speichern</button>
      <button id="erase" class="danger">Alles löschen</button>
    </div>

    <div class="notice" style="margin-top:14px">
      Tipp: Reihenfolge = Verbindungsreihenfolge (oben = höhere Priorität). Mit ▲/▼ umsortieren und „Speichern“ klicken.
    </div>

    <footer>CustomWiFiManager · © 2025</footer>
  </div>
</div>
<script>
const $ = (q)=>document.querySelector(q);
const listEl = $('#list');
const ssidSel = $('#ssid');
const btnAdd = $('#add');
const btnConn = $('#connect');
const btnSave = $('#save');
const btnErase = $('#erase');

const modal = document.createElement('div');
modal.className = 'modal';
modal.innerHTML = `
  <div class="box">
    <h3>Verbunden</h3>
    <p>Der ESP32 ist jetzt im WLAN.</p>
    <p><b>IP:</b> <span id="ipVal"></span></p>
    <p id="cdLine">Setup-WLAN wird in <b><span id="cd">..</span>s</b> beendet.</p>
    <div class="rowbtns">
      <button id="openIp" class="primary">IP öffnen</button>
      <button id="copyIp">IP kopieren</button>
      <button id="endAp" class="danger">AP jetzt beenden</button>
    </div>
  </div>`;
document.body.appendChild(modal);

const ipSpan = modal.querySelector('#ipVal');
const cdSpan = modal.querySelector('#cd');
modal.querySelector('#copyIp').onclick = async ()=> {
  try { await navigator.clipboard.writeText(ipSpan.textContent); alert('IP kopiert'); } catch(e){}
};
modal.querySelector('#openIp').onclick = ()=> {
  const ip = ipSpan.textContent.trim();
  if (ip) location.href = 'http://' + ip + '/';
};
modal.querySelector('#endAp').onclick = async ()=> {
  try { await fetch('/ap_off', {method:'POST'}); } catch(e){}
};

let showedModal = false;

async function pollStatusUntilConnected(){
  for(;;){
    await new Promise(r=>setTimeout(r, 1000));
    try{
      const r = await fetch('/status',{cache:'no-store'});
      const s = await r.json();
      if(s.connected){
        if(!showedModal){
          showedModal = true;
          ipSpan.textContent = s.ip || '';
          modal.classList.add('show');
        }
        const secs = Math.max(0, Math.floor((s.ap_off_in||0)/1000));
        cdSpan.textContent = secs;
      }
    }catch(e){
      return;
    }
  }
}

function setLoading(el, on){
  el.classList.toggle('loading', !!on);
  el.disabled = !!on;
}

async function loadScan(){
  try{
    setLoading(ssidSel, true);
    const r = await fetch('/scan',{cache:'no-store'});
    const arr = await r.json();
    ssidSel.innerHTML = arr.map(x =>
        `<option value="${x.ssid.replace(/"/g,'&quot;')}">${x.ssid} (${x.rssi} dBm)</option>`
    ).join('') || '<option value="">(keine gefunden)</option>';
  }catch(e){
    ssidSel.innerHTML = '<option value="">(Scan fehlgeschlagen)</option>';
  }finally{
    setLoading(ssidSel, false);
  }
}

listEl.addEventListener('click', async (e)=>{
  const li = e.target.closest('.cred');
  if(!li) return;

  if(e.target.classList.contains('del')){
    const ssid = li.dataset.ssid || li.querySelector('b')?.textContent || '';
    if(!ssid) return;
    if(!confirm('WLAN wirklich löschen?')) return;
    try{
      await fetch('/del', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid})});
      li.remove();
    }catch(err){}
    return;
  }
  if(e.target.classList.contains('up') && li.previousElementSibling) li.parentNode.insertBefore(li, li.previousElementSibling);
  if(e.target.classList.contains('down') && li.nextElementSibling) li.parentNode.insertBefore(li.nextElementSibling, li);
});

btnSave.onclick = async ()=>{
  const ssids = [...listEl.querySelectorAll('.cred')].map(li => li.dataset.ssid || li.querySelector('b')?.textContent || '').filter(Boolean);
  try{
    setLoading(btnSave, true);
    const r = await fetch('/reorder', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({order:ssids})});
    alert(await r.text());
    location.reload();
  }finally{ setLoading(btnSave, false); }
};

btnErase.onclick = async ()=>{
  if(!confirm('Wirklich alle WLANs löschen?')) return;
  try{
    setLoading(btnErase, true);
    const r = await fetch('/erase', {method:'POST'});
    alert(await r.text());
    location.reload();
  }finally{ setLoading(btnErase, false); }
};

btnAdd.onclick = async ()=>{
  const ssid = ssidSel.value.trim();
  const pw = $('#pw').value;
  const pr = parseInt($('#prio').value)||100;
  if(!ssid){ alert('Bitte SSID wählen.'); return; }
  try{
    setLoading(btnAdd, true);
    const r = await fetch('/add', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid, password: pw, priority: pr})});
    alert(await r.text());
    location.reload();
  }finally{ setLoading(btnAdd, false); }
};

btnConn.onclick = async ()=>{
  const ssid = ssidSel.value.trim();
  const pw = $('#pw').value;
  const pr = parseInt($('#prio').value)||100;
  if(!ssid){ alert('Bitte SSID wählen.'); return; }
  try{
    setLoading(btnConn, true);
    await fetch('/connect', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid, password: pw, priority: pr})});
    pollStatusUntilConnected();
  }catch(e){
    alert('Fehler beim Starten der Verbindung.');
  }finally{
    setLoading(btnConn, false);
  }
};

document.addEventListener('DOMContentLoaded', loadScan);
</script>
</body>
</html>
)HTML";
}
