#include "CustomWiFiManager.h"
#include <esp_system.h>
#include <algorithm>
#include <WiFiClient.h>

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
    std::vector<Credential> order;
    for (size_t i = 0; i < hdr_.count && i < creds_.size(); ++i)
        order.push_back(creds_[i]);
    std::sort(order.begin(), order.end(), [](const Credential &a, const Credential &b)
              {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.last_ok > b.last_ok; });

    for (const auto &c : order)
    {
        if (tryConnectOne(c, connectTimeoutMs))
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

bool EasyWiFiManager::tryConnectOne(const Credential &c, uint32_t connectTimeoutMs)
{
    if (c.ssid[0] == '\0')
        return false;
        
    WiFi.disconnect(true);
    WiFi.begin(c.ssid, c.password);

    uint32_t start = millis();
    while (millis() - start < connectTimeoutMs)
    {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED)
        {
            return true;
        }
        delay(100);
    }
    return false;
}

void EasyWiFiManager::startConfigPortal()
{
    startAP();
    setupWeb();
    portalRunning_ = true;
    uint32_t last = millis();

    while (portalRunning_)
    {
        dns_.processNextRequest();
        server_.handleClient();
        delay(4);

        if (WiFi.status() == WL_CONNECTED)
        {
            portalRunning_ = false;
        }

        if (millis() - last > 5000)
            last = millis();
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
    server_.on("/", HTTP_GET, [this]
    {
        WiFi.enableSTA(true);
        WiFi.disconnect(false, false);
        delay(50);

        int n = WiFi.scanNetworks();

        String options;
        for (int i = 0; i < n; ++i) 
        {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            options += "<option value=\"" + html_escape(ssid) + "\">" + html_escape(ssid) + " (" + String(rssi) + " dBm)</option>";
        }

        WiFi.scanDelete();

        String list;
        for (size_t i = 0; i < hdr_.count; ++i) 
        {
            list += "<li class=\"cred\" data-idx=\"" + String(i) + "\">";
            list += "<b>" + html_escape(creds_[i].ssid) + "</b> <span class=\"pri\">Prio: " + String(creds_[i].priority) + "</span>";
            list += "<button class=\"up\">▲</button><button class=\"down\">▼</button><button class=\"del danger\">Löschen</button>";
            list += "</li>";
        }

        String page = R"HTML((
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>ESP32 WLAN-Einrichtung</title>
<style>
:root{--bg:#0b1220;--card:#121a2b;--muted:#9fb0d0;--acc:#62d2a2;--danger:#e25b5b;--txt:#f5f7fb}
*{box-sizing:border-box} body{margin:0;font:16px/1.45 system-ui;background:var(--bg);color:var(--txt)}
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
        <input id="pw" type="password" placeholder="WLAN-Passwort" />
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
const listEl = document.getElementById('list');
listEl.addEventListener('click', (e)=>{
  const li = e.target.closest('.cred');
  if(!li) return;
  if(e.target.classList.contains('del')) li.remove();
  if(e.target.classList.contains('up') && li.previousElementSibling) li.parentNode.insertBefore(li, li.previousElementSibling);
  if(e.target.classList.contains('down') && li.nextElementSibling) li.parentNode.insertBefore(li.nextElementSibling, li);
});

document.getElementById('add').onclick = async ()=>{
  const ssid = document.getElementById('ssid').value.trim();
  const pw = document.getElementById('pw').value;
  const pr = parseInt(document.getElementById('prio').value)||100;
  if(!ssid){ alert('Bitte SSID wählen.'); return; }
  const r = await fetch('/add', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid, password: pw, priority: pr})});
  const t = await r.text(); alert(t);
  location.reload();
};

document.getElementById('connect').onclick = async ()=>{
  const ssid = document.getElementById('ssid').value.trim();
  const pw = document.getElementById('pw').value;
  const pr = parseInt(document.getElementById('prio').value)||100;
  const r = await fetch('/connect', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid, password: pw, priority: pr})});
  const t = await r.text(); alert(t);
};

document.getElementById('save').onclick = async ()=>{
  const items=[...document.querySelectorAll('.cred')].map((li,idx)=>({
    ssid: li.querySelector('b').textContent,
    priority: idx
  }));
  const r = await fetch('/reorder', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(items)});
  const t = await r.text(); alert(t);
};

document.getElementById('erase').onclick = async ()=>{
  if(!confirm('Wirklich alle gespeicherten Netzwerke löschen?')) return;
  const r = await fetch('/erase', {method:'POST'});
  const t = await r.text(); alert(t); location.reload();
};
</script>
</body>
</html>
))HTML";

        page.replace("__OPTIONS__", options);
        page.replace("__LIST__", list);
        server_.send(200, "text/html; charset=utf-8", page); });

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
        }
    });

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
        server_.send(200, "text/plain", "Gespeichert.");
    });

    server_.on("/erase", HTTP_POST, [this]
    {
        eraseAll();
        server_.send(200, "text/plain", "Alle Einträge gelöscht.");
    });

    server_.on("/connect", HTTP_POST, [this]
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

        Credential c; strncpy(c.ssid, ssid.c_str(), sizeof(c.ssid)-1); strncpy(c.password, pw.c_str(), sizeof(c.password)-1); c.priority = (uint8_t)constrain(pr,0,254);

        bool ok = tryConnectOne(c, 12000);

        server_.send(200, "text/plain", ok?"Verbunden (wenn Passwort korrekt).":"Konnte nicht verbinden.");

        if (ok) 
        {
            addCredential(ssid, pw, (uint8_t)pr);
        }
    });

    server_.begin();
}

void EasyWiFiManager::loopWeb()
{
    if (!portalRunning_)
        return;

    dns_.processNextRequest();
    server_.handleClient();
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
    uint32_t crc = crc32(reinterpret_cast<const uint8_t *>(&tmp), sizeof(tmp)) ^ crc32(reinterpret_cast<const uint8_t *>(data.data()), sizeof(Credential) * data.size());
    
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