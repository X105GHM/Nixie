#include "CustomWiFiManager.h"
#include <esp_system.h>
#include <algorithm>
#include <WiFiClient.h>

namespace
{
    uint32_t s_apGraceMs = 15000;
    uint32_t s_apGraceUntil = 0;
}

using namespace ewm;

// ---------------- Util: CRC32 (IEEE 802.3)
static uint32_t crc32_update(uint32_t crc, uint8_t data)
{
    crc = crc ^ data;
    for (int i = 0; i < 8; ++i)
    {
        uint32_t mask = -(crc & 1u);
        crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
    return crc;
}

uint32_t EasyWiFiManager::crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = crc32_update(crc, data[i]);
    return ~crc;
}

EasyWiFiManager &EasyWiFiManager::instance()
{
    static EasyWiFiManager inst;
    return inst;
}

EasyWiFiManager::EasyWiFiManager()
{
    hdr_.magic = MAGIC;
    hdr_.version = 0x0001;
    hdr_.count = 0;
}

void EasyWiFiManager::setHostname(const String &name) { hostname_ = name; }
void EasyWiFiManager::setAPCredentials(const String &s, const String &p)
{
    apSsid_ = s;
    apPass_ = p;
}

void EasyWiFiManager::begin(uint32_t connectTimeoutMs, uint32_t betweenRetryMs)
{
    load();
    tryRecover();

    WiFi.mode(WIFI_STA);
    delay(50);

    WiFi.mode(WIFI_STA);
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 4
    WiFi.setHostname(hostname_.c_str());
#else
    // älterer Core: setHostname ggf. über tcpip_adapter
#endif

    if (!tryConnectAll(connectTimeoutMs, betweenRetryMs))
    {
        startConfigPortal();
    }

    ensureAPState();

    if (monitorEnabled_ && !monitorTask_)
    {
        xTaskCreatePinnedToCore(monitorTaskThunk, "ewm_mon", 4096, this, 1, &monitorTask_, ARDUINO_RUNNING_CORE);
    }
}

bool EasyWiFiManager::tryConnectAll(uint32_t connectTimeoutMs, uint32_t betweenRetryMs)
{
    const auto visible = scanVisible_();

    std::vector<Credential> order;
    order.reserve(hdr_.count);

    for (size_t i = 0; i < hdr_.count && i < creds_.size(); ++i)
    {
        const auto &c = creds_[i];
        bool vis = false;
        for (auto &v : visible)
        {
            if (v == c.ssid)
            {
                vis = true;
                break;
            }
        }
        if (vis)
            order.push_back(c);
    }

    if (order.empty())
    {
        for (size_t i = 0; i < hdr_.count && i < creds_.size(); ++i)
        {
            order.push_back(creds_[i]);
        }
    }

    std::sort(order.begin(), order.end(), [](const Credential &a, const Credential &b)
              {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.last_ok > b.last_ok; });

    for (const auto &c : order)
    {
        if (tryConnectOneWithInternet_(c, connectTimeoutMs, internetTimeoutMs_))
        {
            for (size_t i = 0; i < hdr_.count; ++i)
            {
                if (strncmp(creds_[i].ssid, c.ssid, sizeof(c.ssid)) == 0)
                {
                    creds_[i].last_ok = (uint32_t)time(nullptr);
                    save();
                    break;
                }
            }

            if (onConnect_)
                onConnect_(WiFi.localIP());
            ensureAPState();
            return true;
        }

        delay(betweenRetryMs);
    }

    return false;
}

void EasyWiFiManager::startConfigPortal()
{
    s_apGraceUntil = 0;
    startAP();
    setupWeb();
    portalRunning_ = true;

    while (portalRunning_)
    {
        dns_.processNextRequest();
        server_.handleClient();
        delay(4);

        if (WiFi.status() == WL_CONNECTED)
        {
            if (s_apGraceUntil == 0)
            {
                if (connectRequested_)
                {
                    addCredential(pendingSsid_, pendingPass_, pendingPrio_);
                    connectRequested_ = false;
                }
                s_apGraceUntil = millis() + s_apGraceMs;
            }

            if ((int32_t)(millis() - s_apGraceUntil) >= 0)
                portalRunning_ = false;
        }
    }

    stopAP();
}

void EasyWiFiManager::startAP()
{
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid_.c_str(), (apPass_.length() == 0 ? nullptr : apPass_.c_str()));
    delay(100);

    dns_.start(53, "*", WiFi.softAPIP());
}

void EasyWiFiManager::stopAP()
{
    dns_.stop();
    WiFi.softAPdisconnect(true);
}

static String html_escape(const String &s)
{
    String r;
    r.reserve(s.length());
    for (char c : s)
    {
        switch (c)
        {
        case '&':
            r += "&amp;";
            break;
        case '<':
            r += "&lt;";
            break;
        case '>':
            r += "&gt;";
            break;
        case '"':
            r += "&quot;";
            break;
        case '\'':
            r += "&#39;";
            break;
        default:
            r += c;
            break;
        }
    }
    return r;
}

void EasyWiFiManager::setupWeb()
{
    auto json_escape = [](const String &s) -> String
    {
        String r;
        r.reserve(s.length() + 8);
        for (size_t i = 0; i < s.length(); ++i)
        {
            char c = s[i];
            switch (c)
            {
            case '\"':
                r += "\\\"";
                break;
            case '\\':
                r += "\\\\";
                break;
            case '\b':
                r += "\\b";
                break;
            case '\f':
                r += "\\f";
                break;
            case '\n':
                r += "\\n";
                break;
            case '\r':
                r += "\\r";
                break;
            case '\t':
                r += "\\t";
                break;
            default:
                if ((uint8_t)c < 0x20)
                {
                    char buf[7];
                    sprintf(buf, "\\u%04x", (uint8_t)c);
                    r += buf;
                }
                else
                    r += c;
            }
        }
        return r;
    };

    auto serveProbeOk = [this]()
    {
        server_.sendHeader("Connection", "close");
        server_.send(200, "text/html",
                     "<!doctype html><meta charset='utf-8'>"
                     "<meta http-equiv='refresh' content='0;url=/'/>"
                     "<title>Captive Portal</title>"
                     "<p>Weiter zur Konfigurationsseite…</p>");
    };

    auto handleRoot = [this, &json_escape]()
    {
        String list;
        for (size_t i = 0; i < hdr_.count; ++i)
        {
            list += "<li class=\"cred\" data-idx=\"" + String(i) + "\">";
            list += "<b>" + html_escape(creds_[i].ssid) + "</b> <span class=\"pri\">Prio: " + String(creds_[i].priority) + "</span>";
            list += "<button class=\"up\">▲</button><button class=\"down\">▼</button><button class=\"del danger\">Löschen</button>";
            list += "</li>";
        }

        String options = "<option value=\"\">(lade…)</option>";

        String page = R"HTML((
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
.cred .pri{color:var(--muted)}
.buttons{display:flex;gap:8px}
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
      Tipp: Die Reihenfolge entspricht der Verbindungsreihenfolge (oben = höhere Priorität). Mit ▲/▼ umsortieren.
    </div>

    <footer>CustomWiFiManager · X105GHM · © 2025</footer>
  </div>
</div>
<script>
const $ = (q)=>document.querySelector(q);
const listEl = $('#list');
const ssidSel = $('#ssid');
const btnAdd = $('#add');
const btnConn = $('#connect');

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
const cdLine = modal.querySelector('#cdLine');
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
        if(secs === 0){

        }
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
    console.log(e);
    ssidSel.innerHTML = '<option value="">(Scan fehlgeschlagen)</option>';
  }finally{
    setLoading(ssidSel, false);
  }
}

listEl.addEventListener('click', (e)=>{
  const li = e.target.closest('.cred');
  if(!li) return;
  if(e.target.classList.contains('del')) li.remove();
  if(e.target.classList.contains('up') && li.previousElementSibling) li.parentNode.insertBefore(li, li.previousElementSibling);
  if(e.target.classList.contains('down') && li.nextElementSibling) li.parentNode.insertBefore(li.nextElementSibling, li);
});

btnAdd.onclick = async ()=>{
  const ssid = ssidSel.value.trim();
  const pw = $('#pw').value;
  const pr = parseInt($('#prio').value)||100;
  if(!ssid){ alert('Bitte SSID wählen.'); return; }
  try{
    setLoading(btnAdd, true);
    const r = await fetch('/add', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid, password: pw, priority: pr})});
    const t = await r.text(); alert(t);
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
    const r = await fetch('/connect', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid, password: pw, priority: pr})});
    const t = await r.text(); console.log(t);
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
))HTML";

        page.replace("__OPTIONS__", options);
        page.replace("__LIST__", list);

        server_.sendHeader("Connection", "close");
        server_.send(200, "text/html; charset=utf-8", page);
    };

    server_.on("/", HTTP_GET, handleRoot);

    server_.on("/scan", HTTP_GET, [this, &json_escape]()
               {
        WiFi.enableSTA(true);
        WiFi.disconnect(false,false);
        delay(50);
        int n = WiFi.scanNetworks(false, true);
        String json = "[";
        for (int i=0;i<n;++i){
            if (i) json += ',';
            json += "{\"ssid\":\"" + json_escape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        }
        json += "]";
        WiFi.scanDelete();

        server_.sendHeader("Cache-Control","no-store");
        server_.sendHeader("Connection","close");
        server_.send(200, "application/json", json); });

    server_.on("/status", HTTP_GET, [this]()
               {
        bool conn = (WiFi.status() == WL_CONNECTED);
        String ip = conn ? WiFi.localIP().toString() : "";
        int rssi  = conn ? WiFi.RSSI() : 0;
        uint32_t apOffIn = 0;

        if (conn && s_apGraceUntil > millis()) apOffIn = s_apGraceUntil - millis();

        String json = "{";
            json += "\"connected\":"; json += (conn ? "true":"false"); json += ",";
            json += "\"ip\":\""; json += ip; json += "\",";
            json += "\"rssi\":"; json += String(rssi); json += ",";
            json += "\"ap_off_in\":"; json += String(apOffIn);
            json += "}";

        server_.sendHeader("Cache-Control","no-store");
        server_.sendHeader("Connection","close");
        server_.send(200, "application/json", json); });

    server_.on("/add", HTTP_POST, [this]
               {
        String body = server_.arg("plain");
        auto get = [&](const char* key)->String
        {
            int k = body.indexOf(String('"')+key+'"');
            if (k<0) return ""; k = body.indexOf(':',k); if (k<0) return ""; int s = body.indexOf('"',k); int e = body.indexOf('"',s+1); if(s<0||e<0) return ""; return body.substring(s+1,e);
        };

        String ssid = get("ssid");
        String pw   = get("password");
        int pr      = body.indexOf("priority")>=0 ? body.substring(body.indexOf("priority")).toInt() : 100;

        if (addCredential(ssid, pw, (uint8_t)constrain(pr,0,254)))
        {
            server_.send(200, "text/plain", "Hinzugefügt.");
        }
        else
        {
            server_.send(400, "text/plain", "Fehler (max. 10 oder ungültig). ");
        } });

    server_.on("/reorder", HTTP_POST, [this]
               {
        String body = server_.arg("plain");

        std::vector<String> order;
        for (size_t i = 0; i < hdr_.count; ++i)
        {
            if (String(creds_[i].ssid).length() && body.indexOf('"' + String(creds_[i].ssid) + '"') >= 0)
            {
                order.push_back(String(creds_[i].ssid));
            }
        }

        uint8_t p = 0;
        for (const auto& ss : order)
        {
            for (size_t i = 0; i < hdr_.count; ++i)
            {
                if (ss == creds_[i].ssid) creds_[i].priority = p++;
            }
        }
        save();
        server_.send(200, "text/plain", "Gespeichert."); });

    server_.on("/erase", HTTP_POST, [this]
               {
        eraseAll();
        server_.send(200, "text/plain", "Alle Einträge gelöscht."); });

    server_.on("/ap_off", HTTP_POST, [this]()
               {
        s_apGraceUntil = millis();
        portalRunning_ = false;   
        server_.send(200, "application/json", "{\"ok\":true}"); });

    server_.on("/connect", HTTP_POST, [this]
               {
        String body = server_.arg("plain");
        auto get = [&](const char* key)->String{
            int k = body.indexOf(String('"')+key+'"');
            if (k<0) return "";
            k = body.indexOf(':',k); if (k<0) return "";
            int s = body.indexOf('"',k); int e = body.indexOf('"',s+1);
            if(s<0||e<0) return "";
            return body.substring(s+1,e);
            };

        String ssid = get("ssid");
        String pw   = get("password");
        int pr      = body.indexOf("priority")>=0 ? body.substring(body.indexOf("priority")).toInt() : 100;
        pr = constrain(pr,0,254);

        WiFi.disconnect(true);
        WiFi.begin(ssid.c_str(), pw.c_str());

        pendingSsid_ = ssid;
        pendingPass_ = pw;
        pendingPrio_ = (uint8_t)pr;
        connectRequested_ = true;

        server_.send(202, "application/json", "{\"ok\":true,\"msg\":\"Verbinde...\"}"); });

    server_.on("/generate_204", HTTP_ANY, serveProbeOk);
    server_.on("/gen_204", HTTP_ANY, serveProbeOk);
    server_.on("/hotspot-detect.html", HTTP_ANY, serveProbeOk);
    server_.on("/library/test/success.html", HTTP_ANY, serveProbeOk);
    server_.on("/ncsi.txt", HTTP_ANY, serveProbeOk);
    server_.on("/connecttest.txt", HTTP_ANY, serveProbeOk);
    server_.on("/success.txt", HTTP_ANY, serveProbeOk);
    server_.on("/favicon.ico", HTTP_ANY, []() { /* 204 */ });

    server_.onNotFound(handleRoot);

    server_.begin();
}

bool EasyWiFiManager::addCredential(const String &ssid, const String &password, uint8_t priority)
{
    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 64)
        return false;

    for (size_t i = 0; i < hdr_.count; ++i)
    {
        if (ssid == creds_[i].ssid)
        {
            strncpy(creds_[i].password, password.c_str(), sizeof(creds_[i].password) - 1);
            creds_[i].priority = priority;
            save();
            return true;
        }
    }

    if (hdr_.count >= creds_.size())
        return false;
    auto &c = creds_[hdr_.count++];
    strncpy(c.ssid, ssid.c_str(), sizeof(c.ssid) - 1);
    strncpy(c.password, password.c_str(), sizeof(c.password) - 1);
    c.priority = priority;
    c.last_ok = 0;
    return save();
}

bool EasyWiFiManager::eraseAll()
{
    hdr_.count = 0;
    creds_ = {};
    return save();
}

bool EasyWiFiManager::removeCredential(const String &ssid)
{
    for (size_t i = 0; i < hdr_.count; ++i)
    {
        if (ssid == creds_[i].ssid)
        {
            for (size_t j = i + 1; j < hdr_.count; ++j)
            {
                creds_[j - 1] = creds_[j];
            }

            creds_[hdr_.count - 1] = Credential{};
            hdr_.count--;
            return save();
        }
    }
    return false;
}

bool EasyWiFiManager::safeSwitchMode_(wifi_mode_t targetMode)
{
    if (WiFi.getMode() == targetMode)
        return true;

    if (WiFi.mode(targetMode))
    {
        delay(30);
        return true;
    }

    WiFi.disconnect(true, true);
    delay(50);
    if (WiFi.mode(targetMode))
    {
        delay(30);
        return true;
    }

    delay(50);
    return WiFi.getMode() == targetMode;
}

std::vector<Credential> EasyWiFiManager::listCredentials() const
{
    std::vector<Credential> v;
    for (size_t i = 0; i < hdr_.count; ++i)
        v.push_back(creds_[i]);
    return v;
}

bool EasyWiFiManager::saveToNamespace(const char *ns, const StorageHeader &hdr, const std::array<Credential, 10> &data)
{
    Preferences p;
    if (!p.begin(ns, false))
        return false;
    bool ok = true;
    ok &= p.putBytes("hdr", &hdr, sizeof(hdr)) == sizeof(hdr);
    ok &= p.putBytes("data", data.data(), sizeof(Credential) * data.size()) == sizeof(Credential) * data.size();
    p.end();
    return ok;
}

bool EasyWiFiManager::loadFromNamespace(const char *ns, StorageHeader &hdr, std::array<Credential, 10> &data)
{
    Preferences p;
    if (!p.begin(ns, true))
        return false;
    size_t gotH = p.getBytesLength("hdr");
    size_t gotD = p.getBytesLength("data");
    if (gotH != sizeof(StorageHeader) || gotD != sizeof(Credential) * data.size())
    {
        p.end();
        return false;
    }
    p.getBytes("hdr", &hdr, sizeof(hdr));
    p.getBytes("data", data.data(), sizeof(Credential) * data.size());
    p.end();

    if (hdr.magic != MAGIC || hdr.version != 0x0001)
        return false;

    StorageHeader tmp = hdr;
    tmp.crc32 = 0;
    uint32_t crc = crc32(reinterpret_cast<const uint8_t *>(&tmp), sizeof(tmp)) ^
                   crc32(reinterpret_cast<const uint8_t *>(data.data()), sizeof(Credential) * data.size());
    if (crc != hdr.crc32)
        return false;

    return true;
}

bool EasyWiFiManager::save()
{
    StorageHeader h = hdr_;
    if (h.count > creds_.size())
        h.count = creds_.size();
    StorageHeader tmp = h;
    tmp.crc32 = 0;
    uint32_t crc = crc32(reinterpret_cast<const uint8_t *>(&tmp), sizeof(tmp)) ^
                   crc32(reinterpret_cast<const uint8_t *>(creds_.data()), sizeof(Credential) * creds_.size());
    h.crc32 = crc;

    bool a = saveToNamespace(NS_PRIMARY, h, creds_);
    bool b = saveToNamespace(NS_BACKUP, h, creds_);
    return a && b;
}

bool EasyWiFiManager::load()
{
    StorageHeader hp, hb;
    std::array<Credential, 10> dp, db;
    bool okp = loadFromNamespace(NS_PRIMARY, hp, dp);
    bool okb = loadFromNamespace(NS_BACKUP, hb, db);

    if (okp && (!okb || hp.crc32 == hb.crc32))
    {
        hdr_ = hp;
        creds_ = dp;
        return true;
    }

    if (okb && !okp)
    {
        hdr_ = hb;
        creds_ = db;
        return true;
    }

    if (okp && okb)
    {
        auto score = [](const StorageHeader &h, const std::array<Credential, 10> &d)
        {
            uint32_t s = h.count;
            for (size_t i = 0; i < h.count; ++i)
                s += d[i].last_ok;
            return s;
        };
        if (score(hp, dp) >= score(hb, db))
        {
            hdr_ = hp;
            creds_ = dp;
            return true;
        }
        hdr_ = hb;
        creds_ = db;
        return true;
    }
    hdr_.count = 0;
    creds_ = {};
    return false;
}

void EasyWiFiManager::tryRecover()
{
    save();
}

void EasyWiFiManager::setBackgroundAP(bool enabled)
{
    backgroundAP_ = enabled;
    ensureAPState();
}

void EasyWiFiManager::ensureAPState()
{
    if (backgroundAP_)
    {
        if (WiFi.getMode() != WIFI_AP_STA)
            WiFi.mode(WIFI_AP_STA);

        if (WiFi.softAPSSID() != apSsid_)
        {
            WiFi.softAP(apSsid_.c_str(), (apPass_.length() == 0 ? nullptr : apPass_.c_str()));
        }
    }
    else
    {
        if (!portalRunning_)
        {
            WiFi.softAPdisconnect(true);
            if (WiFi.getMode() == WIFI_AP_STA)
                WiFi.mode(WIFI_STA);
        }
    }
}

void EasyWiFiManager::setConnectivityMonitor(bool enabled, uint32_t checkIntervalMs, uint32_t internetTimeoutMs)
{
    monitorEnabled_ = enabled;
    checkIntervalMs_ = checkIntervalMs;
    internetTimeoutMs_ = internetTimeoutMs;
    if (enabled && !monitorTask_)
    {
        xTaskCreatePinnedToCore(monitorTaskThunk, "ewm_mon", 4096, this, 1, &monitorTask_, ARDUINO_RUNNING_CORE);
    }
}

void EasyWiFiManager::setInternetProbe(const char *host, uint16_t port)
{
    probeHost_ = host;
    probePort_ = port;
}

void EasyWiFiManager::onNoConnectivity(std::function<void()> cb, uint32_t delayMs)
{
    onNoConn_ = std::move(cb);
    noConnSince_ = 0;
    noConnCallbackDelayMs_ = delayMs;
}

void EasyWiFiManager::monitorTaskThunk(void *arg)
{
    static_cast<EasyWiFiManager *>(arg)->monitorTaskLoop();
}

bool EasyWiFiManager::hasInternet(uint32_t timeoutMs)
{
    if (WiFi.status() != WL_CONNECTED)
        return false;
    WiFiClient client;
    client.setTimeout(timeoutMs / 1000 + 1);
    if (client.connect(probeHost_, probePort_))
    {
        client.stop();
        return true;
    }
    return false;
}

std::vector<String> EasyWiFiManager::scanVisible_()
{
    if (WiFi.getMode() == WIFI_OFF)
    {
        WiFi.mode(WIFI_STA);
        delay(30);
    }
    else if (WiFi.getMode() == WIFI_AP)
    {
        WiFi.mode(WIFI_AP_STA);
        delay(30);
    }
    else if (WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_STA)
    {
        // ok
    }
    else
    {
        WiFi.mode(WIFI_STA);
        delay(30);
    }

    WiFi.disconnect(false, false);
    delay(30);

    int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
    std::vector<String> result;
    result.reserve(n > 0 ? n : 0);

    for (int i = 0; i < n; ++i)
    {
        String s = WiFi.SSID(i);
        if (s.length())
        {
            bool seen = false;
            for (auto &e : result)
                if (e == s)
                {
                    seen = true;
                    break;
                }
            if (!seen)
                result.push_back(s);
        }
    }
    WiFi.scanDelete();
    return result;
}

bool EasyWiFiManager::waitForConnectedOrFail_(uint32_t timeoutMs)
{
    const uint32_t start = millis();
    for (;;)
    {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED)
            return true;

        if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL || st == WL_CONNECTION_LOST)
        {
            return false;
        }
        if ((int32_t)(millis() - start) >= (int32_t)timeoutMs)
        {
            return false;
        }
        delay(100);
    }
}

bool EasyWiFiManager::tryConnectOneWithInternet_(const Credential &c, uint32_t connectTimeoutMs, uint32_t internetTimeoutMs)
{
    if (c.ssid[0] == '\0')
        return false;

    if (!safeSwitchMode_(WIFI_STA))
        return false;

    WiFi.disconnect(true, true);
    delay(30);
    WiFi.begin(c.ssid, c.password);

    if (!waitForConnectedOrFail_(connectTimeoutMs))
    {
        WiFi.disconnect(true, true);
        return false;
    }

    if (!hasInternet(internetTimeoutMs))
    {
        WiFi.disconnect(true, true);
        return false;
    }
    return true;
}

void EasyWiFiManager::roamTryAll()
{
    tryConnectAll(8000, 500);
    ensureAPState();
}

void EasyWiFiManager::monitorTaskLoop()
{
    for (;;)
    {
        bool ok = hasInternet(internetTimeoutMs_);
        if (!ok)
        {
            if (noConnSince_ == 0)
                noConnSince_ = millis();
            roamTryAll();
            ensureAPState();
            if (onNoConn_ && (millis() - noConnSince_ >= noConnCallbackDelayMs_))
            {
                onNoConn_();
                noConnSince_ = millis();
            }
        }
        else
        {
            noConnSince_ = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(checkIntervalMs_));
    }
}