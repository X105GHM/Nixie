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
:root {
    --bg: #0b1220;
    --card: #121a2b;
    --muted: #9fb0d0;
    --acc: #62d2a2;
    --danger: #e25b5b;
    --txt: #f5f7fb
}

* {
    box-sizing: border-box
}

html {
    -webkit-text-size-adjust: 100%
}

input,
select,
textarea,
button {
    font-size: 16px;
    line-height: 1.2
}

a,
button {
    touch-action: manipulation
}

body {
    margin: 0;
    font: 16px/1.45 system-ui;
    background: var(--bg);
    color: var(--txt)
}

.container {
    max-width: 880px;
    margin: 0 auto;
    padding: 24px
}

.card {
    background: var(--card);
    padding: 20px;
    border-radius: 16px;
    box-shadow: 0 10px 30px rgba(0, 0, 0, .25)
}

h1 {
    font-size: 1.6rem;
    margin: 0 0 12px
}

p.muted {
    color: var(--muted)
}

.row {
    display: grid;
    gap: 12px
}

@media(min-width:720px) {
    .row {
        grid-template-columns: 1fr 1fr
    }
}

label {
    display: block;
    font-weight: 600;
    margin: 8px 0 4px
}

input,
select {
    width: 100%;
    padding: 12px 14px;
    border-radius: 12px;
    border: 1px solid #2a3550;
    background: #0e1626;
    color: var(--txt)
}

button {
    border: 0;
    border-radius: 12px;
    padding: 10px 14px;
    font-weight: 700;
    cursor: pointer
}

button.primary {
    background: var(--acc);
    color: #0a0f14
}

button.danger {
    background: var(--danger);
    color: #fff
}

.cred-list {
    list-style: none;
    padding: 0;
    margin: 10px 0 0
}

.cred {
    display: flex;
    align-items: center;
    gap: 8px;
    justify-content: space-between;
    padding: 10px;
    background: #0e1626;
    border-radius: 10px;
    margin: 8px 0;
    cursor: grab
}

.cred.dragging {
    opacity: .55
}

.cred b {
    font-weight: 700
}

.cred .pri {
    color: var(--muted);
    margin-left: 8px
}

.buttons {
    display: flex;
    gap: 8px;
    align-items: center;
    flex-shrink: 0
}

footer {
    opacity: .7;
    margin-top: 16px
}

hr {
    border: none;
    border-top: 1px solid #2a3550;
    margin: 16px 0
}

.notice {
    background: #0e1626;
    border-left: 4px solid #62d2a2;
    padding: 10px;
    border-radius: 10px
}

button.loading {
    position: relative
}

button.loading::after {
    content: "";
    position: absolute;
    right: 10px;
    top: 50%;
    width: 14px;
    height: 14px;
    margin-top: -7px;
    border: 2px solid rgba(255, 255, 255, .6);
    border-top-color: transparent;
    border-radius: 50%;
    animation: spin .8s linear infinite
}

@keyframes spin {
    to {
        transform: rotate(360deg)
    }
}

.modal {
    position: fixed;
    inset: 0;
    display: none;
    place-items: center;
    background: rgba(0, 0, 0, .5);
    z-index: 9999
}

.modal .box {
    background: #121a2b;
    padding: 18px;
    border-radius: 14px;
    max-width: 480px;
    width: calc(100% - 40px);
    box-shadow: 0 10px 30px rgba(0, 0, 0, .35)
}

.modal .rowbtns {
    display: flex;
    gap: 8px;
    margin-top: 12px;
    flex-wrap: wrap
}

.modal.show {
    display: grid
}

/* Untere Buttons (Speichern / Alles löschen) sollen nebeneinander bleiben und nicht "riesig" werden */
div.buttons {
    display: flex;
    gap: 8px;
    align-items: stretch;
    flex-wrap: nowrap;
}

div.buttons button {
    flex: 1 1 0;
    min-width: 0;
    padding: 10px 12px;
    font-size: clamp(12px, 3.2vw, 15px);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
}

@media (max-width:360px) {
    div.buttons button {
        padding: 9px 10px;
        font-size: 12px
    }
}

/* Mobile: Verbinden/Löschen in der Liste als Icons, kompakt */
@media (max-width:520px) {
    .cred {
        gap: 6px
    }

    .cred b {
        max-width: 52vw;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap
    }

    .cred .pri {
        font-size: .85rem
    }

    .cred .buttons {
        gap: 6px;
        flex-shrink: 0
    }

    .cred .buttons button {
        width: 42px;
        height: 38px;
        padding: 0;
        display: inline-flex;
        align-items: center;
        justify-content: center
    }

    .cred .buttons button.con,
    .cred .buttons button.del {
        text-indent: -9999px;
        overflow: hidden;
        position: relative
    }

    .cred .buttons button.con::before,
    .cred .buttons button.del::before {
        position: absolute;
        inset: 0;
        display: flex;
        align-items: center;
        justify-content: center;
        text-indent: 0;
        font-size: 18px;
        line-height: 1
    }

    .cred .buttons button.con::before {
        content: "🔌"
    }

    .cred .buttons button.del::before {
        content: "🗑️"
    }
}
</style>

</head>
<body>
    <div class="container">
        <div class="card">
            <h1>WLAN konfigurieren</h1>
            <p class="muted">Verbinde den ESP32 mit einem bekannten Netzwerk oder verwalte gespeicherte Zugangsdaten.
            </p>

            <div class="row">
                <div>
                    <label>Gefundene Netzwerke</label>
                    <select id="ssid">__OPTIONS__</select>
                </div>
                <div>
                    <label>Passwort</label>
                    <input id="pw" type="password" placeholder="WLAN-Passwort" autocomplete="current-password"
                        autocapitalize="off" autocorrect="off" spellcheck="false" />
                </div>
            </div>
            <div class="row">
                <div>
                    <label>Priorität (0 = höchste)</label>
                    <input id="prio" type="number" min="0" max="254" value="__DEFAULT_PRIO__" />
                </div>
                <div style="display:flex;align-items:flex-end;gap:8px">
                    <button id="add" class="primary">Hinzufügen</button>
                    <button id="connect" class="primary">Verbinden</button>
                </div>
            </div>

            <hr />
            <h3>Gespeicherte Netzwerke</h3>
            <ul class="cred-list" id="list">__LIST__</ul>
            <div class="buttons" style="margin-top:10px">
                <button id="save" class="primary">Speichern</button>
                <button id="erase" class="danger">Alles löschen</button>
            </div>

            <div class="notice" style="margin-top:14px">
                Tipp: Reihenfolge = Verbindungsreihenfolge (oben = höhere Priorität). Per Drag & Drop umsortieren und
                „Speichern“ klicken.
            </div>

            <footer>CustomWiFiManager · © 2025</footer>
        </div>
    </div>
<script>
window.EwmPortalCfg = (() => { try { return __EWM_PORTAL_CFG__; } catch (e) { return {}; } })();
window.EwmCsrfToken = "__CSRF_TOKEN__";

(() => {
    const CFG = (typeof window.EwmPortalCfg === "object" && window.EwmPortalCfg) ? window.EwmPortalCfg : {};

    const EP = Object.assign({
        scan: "/scan",
        status: "/status",
        connect: "/connect",
        add: "/add",
        del: "/del",
        reorder: "/reorder",
        erase: "/erase",
        apOff: "/ap_off"
    }, CFG.endpoints || {});

    const FIN = Object.assign({
        redirect: false,
        redirectUrl: "",
        closeTab: false,
        closeMode: "blank",
        finishDelayMs: 250,
        countdownAutoFinish: true
    }, CFG.finish || {});

    const TIM = Object.assign({
        pollMs: 800,
        connectTimeoutMs: 45000,
        perTryTimeoutMs: 1300,
        scanTimeoutMs: 12000,
        giveUpAfterFails: 18
    }, CFG.timing || {});

    const mdnsHost = (CFG.mdnsHost || "").toString().trim().replace(/^https?:\/\//, "").replace(/\/+$/, "");
    const mdnsBase = mdnsHost ? ("http://" + mdnsHost) : "";
    const CSRF = (window.EwmCsrfToken || "").toString();

    const $ = (q) => document.querySelector(q);
    const listEl = $("#list");
    const ssidSel = $("#ssid");
    const btnAdd = $("#add");
    const btnConn = $("#connect");
    const btnSave = $("#save");
    const btnErase = $("#erase");

    const hint = document.createElement("div");
    hint.className = "notice";
    hint.style.marginTop = "14px";
    hint.style.display = "none";
    $(".card").appendChild(hint);

    const showHint = (msg) => { hint.textContent = msg; hint.style.display = "block"; };
    const hideHint = () => { hint.style.display = "none"; };

    const modal = document.createElement("div");
    modal.className = "modal";
    modal.innerHTML = `
    <div class="box">
      <h3 id="mTitle">Verbinde…</h3>
      <p id="mText">Bitte warten…</p>
      <p><b>IP:</b> <span id="ipVal"></span></p>
      <p id="cdLine" style="display:none;">Setup-WLAN wird in <b><span id="cd">..</span>s</b> beendet.</p>
      <div class="rowbtns">
        <button id="openIp" class="primary">Öffnen</button>
        <button id="copyIp">Kopieren</button>
        <button id="endAp" class="danger">AP jetzt beenden</button>
      </div>
    </div>`;
    document.body.appendChild(modal);

    const titleEl = modal.querySelector("#mTitle");
    const textEl = modal.querySelector("#mText");
    const ipSpan = modal.querySelector("#ipVal");
    const cdLine = modal.querySelector("#cdLine");
    const cdSpan = modal.querySelector("#cd");

    const state = {
        stopped: false,
        finished: false,
        pollRunning: false,
        connectRequested: false,
        connectStartMs: 0,
        failCount: 0,
        staBase: "",
        activeControllers: new Set(),
        finishTimer: null
    };

    const setLoading = (el, on) => {
        if (!el) return;
        el.classList.toggle("loading", !!on);
        el.disabled = !!on;
    };

    const stopClient = () => {
        state.stopped = true;
        if (state.finishTimer) { try { clearTimeout(state.finishTimer); } catch (e) { } state.finishTimer = null; }
        for (const ac of state.activeControllers) { try { ac.abort(); } catch (e) { } }
        state.activeControllers.clear();
        btnConn && (btnConn.disabled = true);
        btnAdd && (btnAdd.disabled = true);
        btnSave && (btnSave.disabled = true);
        btnErase && (btnErase.disabled = true);
        listEl && (listEl.style.pointerEvents = "none");
        ssidSel && (ssidSel.disabled = true);
    };

    const computeTargetUrl = () => {
        const explicit = (FIN.redirectUrl || "").toString().trim();
        if (explicit) return explicit;
        if (state.staBase) return state.staBase + "/";
        if (mdnsBase) return mdnsBase + "/";
        return "";
    };

    const finish = () => {
        if (state.finished) return;
        state.finished = true;
        stopClient();

        const target = computeTargetUrl();

        if (FIN.redirect && target) {
            setTimeout(() => { try { location.replace(target); } catch (e) { } }, FIN.finishDelayMs);
            return;
        }

        if (FIN.closeTab) {
            setTimeout(() => {
                try { window.close(); } catch (e) { }
                if (FIN.closeMode === "blank") { try { location.replace("about:blank"); } catch (e) { } }
            }, FIN.finishDelayMs);
        }
    };

    const showModalConnecting = () => {
        titleEl.textContent = "Verbinde…";
        textEl.textContent = "Bitte warten…";
        ipSpan.textContent = "";
        cdLine.style.display = "none";
        modal.classList.add("show");
    };

    const armFinishTimer = (ms) => {
        if (!FIN.countdownAutoFinish) return;
        if (state.finishTimer) return;
        if (!(ms > 0)) return;
        state.finishTimer = setTimeout(() => { finish(); }, ms + 50);
    };

    const showModalConnected = (s) => {
        titleEl.textContent = "Verbunden";
        textEl.textContent = "Der ESP32 ist jetzt im WLAN.";
        if (s && s.ip) ipSpan.textContent = s.ip;
        cdLine.style.display = "block";
        const apOffIn = (s && typeof s.ap_off_in === "number") ? s.ap_off_in : 0;
        cdSpan.textContent = Math.max(0, Math.floor(apOffIn / 1000));
        modal.classList.add("show");
        armFinishTimer(apOffIn);
        if (FIN.countdownAutoFinish && apOffIn <= 250) finish();
    };

    const showModalInfo = (t, m) => {
        titleEl.textContent = t || "";
        textEl.textContent = m || "";
        cdLine.style.display = "none";
        modal.classList.add("show");
    };

    const csrfHeaders = (extra = {}) => Object.assign({ "X-EWM-CSRF": CSRF }, extra);

    const fetchJsonWithTimeout = async (url, ms, opts = {}) => {
        if (state.stopped) throw new Error("stopped");
        const ac = new AbortController();
        const timer = setTimeout(() => { try { ac.abort(); } catch (e) { } }, ms);
        state.activeControllers.add(ac);
        try {
            const r = await fetch(url, Object.assign({ cache: "no-store", signal: ac.signal }, opts));
            if (!r.ok) throw new Error("http " + r.status);
            return await r.json();
        } finally {
            try { clearTimeout(timer); } catch (e) { }
            state.activeControllers.delete(ac);
        }
    };

    const statusCandidates = () => {
        const q = "?r=" + Date.now();
        const a = [];
        a.push(EP.status + q);
        if (mdnsBase) a.push(mdnsBase + EP.status + q);
        if (state.staBase) a.unshift(state.staBase + EP.status + q);
        return a;
    };

    const postCandidates = (path) => {
        const a = [];
        a.push(path);
        if (mdnsBase) a.push(mdnsBase + path);
        if (state.staBase) a.unshift(state.staBase + path);
        return a;
    };

    const pollStatusLoop = async () => {
        if (state.pollRunning || state.stopped) return;
        state.pollRunning = true;

        while (!state.stopped) {
            await new Promise(r => setTimeout(r, TIM.pollMs));

            let s = null;
            for (const u of statusCandidates()) {
                try {
                    s = await fetchJsonWithTimeout(u, TIM.perTryTimeoutMs);
                    break;
                } catch (e) { }
            }

            if (!s) {
                state.failCount++;
                if (state.connectRequested && state.failCount > TIM.giveUpAfterFails) {
                    const target = computeTargetUrl();
                    const msg = target
                        ? "Status nicht erreichbar. Handy ist vermutlich ins Heim-WLAN gewechselt. Leite zur Nixie weiter…"
                        : "Status nicht erreichbar. Handy ist vermutlich ins Heim-WLAN gewechselt. Öffne die Ziel-URL.";
                    showHint(msg);
                    showModalInfo("Info", msg);

                    if (FIN.redirect && target && !state.finishTimer) {
                        state.finishTimer = setTimeout(() => { finish(); }, Math.max(FIN.finishDelayMs, 1000));
                    }
                    break;
                }
                continue;
            }

            state.failCount = 0;

            if (s && s.ip && !state.staBase) state.staBase = "http://" + s.ip;

            if (s && s.connected) {
                showModalConnected(s);
                continue;
            }

            if (state.connectRequested && (Date.now() - state.connectStartMs) > TIM.connectTimeoutMs) {
                showModalInfo("Fehler", "Verbindung fehlgeschlagen.");
                break;
            }
        }

        state.pollRunning = false;
    };

    const startConnectPolling = () => {
        state.connectRequested = true;
        state.connectStartMs = Date.now();
        state.failCount = 0;
        hideHint();
        showModalConnecting();
        pollStatusLoop();
    };

    const loadScan = async () => {
        try {
            setLoading(ssidSel, true);
            const r = await fetchJsonWithTimeout(EP.scan, TIM.scanTimeoutMs);
            const arr = Array.isArray(r) ? r : [];
            ssidSel.textContent = "";

            if (!arr.length) {
                const opt = document.createElement("option");
                opt.value = "";
                opt.textContent = "(keine gefunden)";
                ssidSel.appendChild(opt);
            } else {
                for (const x of arr) {
                    const ssid = String((x && x.ssid) || "");
                    const rssi = (x && typeof x.rssi !== "undefined") ? x.rssi : "";
                    const opt = document.createElement("option");
                    opt.value = ssid;
                    opt.textContent = `${ssid} (${rssi} dBm)`;
                    ssidSel.appendChild(opt);
                }
            }
        } catch (e) {
            ssidSel.textContent = "";
            const opt = document.createElement("option");
            opt.value = "";
            opt.textContent = "(Scan fehlgeschlagen)";
            ssidSel.appendChild(opt);
        } finally {
            setLoading(ssidSel, false);
        }
    };

    modal.querySelector("#copyIp").onclick = async () => {
        try { await navigator.clipboard.writeText((ipSpan.textContent || "").trim()); } catch (e) { }
    };

    modal.querySelector("#openIp").onclick = () => {
        const ip = (ipSpan.textContent || "").trim();
        const target = ip ? ("http://" + ip + "/") : computeTargetUrl();
        if (target) { try { location.href = target; } catch (e) { } }
    };

    modal.querySelector("#endAp").onclick = async () => {
        for (const u of postCandidates(EP.apOff)) {
            try { await fetchJsonWithTimeout(u, TIM.perTryTimeoutMs, { method: "POST", headers: csrfHeaders() }); break; } catch (e) { }
        }
        finish();
    };

    listEl.addEventListener("click", async (e) => {
        if (state.stopped) return;
        const li = e.target.closest(".cred");
        if (!li) return;

        if (e.target.classList.contains("con")) {
            const ssid = li.dataset.ssid || li.querySelector("b")?.textContent || "";
            if (!ssid) return;

            startConnectPolling();

            try {
                setLoading(btnConn, true);
                const r = await fetch(EP.connect, {
                    method: "POST",
                    headers: csrfHeaders({ "Content-Type": "application/json" }),
                    body: JSON.stringify({ ssid }),
                    cache: "no-store"
                });
                if (!r.ok) showModalInfo("Fehler", await r.text());
            } catch (e) {
            } finally {
                setLoading(btnConn, false);
            }
            return;
        }

        if (e.target.classList.contains("del")) {
            const ssid = li.dataset.ssid || li.querySelector("b")?.textContent || "";
            if (!ssid) return;
            if (!confirm("WLAN wirklich löschen?")) return;

            try {
                const r = await fetch(EP.del, {
                    method: "POST",
                    headers: csrfHeaders({ "Content-Type": "application/json" }),
                    body: JSON.stringify({ ssid }),
                    cache: "no-store"
                });
                if (!r.ok) { showModalInfo("Fehler", await r.text()); return; }
                window.location.replace("/");
            } catch (e) {
                showModalInfo("Fehler", "Löschen fehlgeschlagen.");
            }
        }
    });

    let dragEl = null;

    listEl.addEventListener("dragstart", (e) => {
        if (state.stopped) return;
        const li = e.target.closest(".cred");
        if (!li) return;
        dragEl = li;
        li.classList.add("dragging");
        e.dataTransfer.effectAllowed = "move";
    });

    listEl.addEventListener("dragend", () => {
        if (dragEl) dragEl.classList.remove("dragging");
        dragEl = null;
    });

    listEl.addEventListener("dragover", (e) => {
        if (state.stopped || !dragEl) return;
        e.preventDefault();
        const els = [...listEl.querySelectorAll(".cred:not(.dragging)")];
        let closest = { offset: Number.NEGATIVE_INFINITY, element: null };
        for (const el of els) {
            const box = el.getBoundingClientRect();
            const offset = e.clientY - box.top - box.height / 2;
            if (offset < 0 && offset > closest.offset) closest = { offset, element: el };
        }
        if (closest.element == null) listEl.appendChild(dragEl);
        else listEl.insertBefore(dragEl, closest.element);
    });

    btnSave.onclick = async () => {
        if (state.stopped) return;
        const ssids = [...listEl.querySelectorAll(".cred")]
            .map(li => li.dataset.ssid || li.querySelector("b")?.textContent || "")
            .filter(Boolean);

        try {
            setLoading(btnSave, true);
            const r = await fetch(EP.reorder, {
                method: "POST",
                headers: csrfHeaders({ "Content-Type": "application/json" }),
                body: JSON.stringify({ order: ssids }),
                cache: "no-store"
            });
            if (!r.ok) showModalInfo("Fehler", await r.text());
            window.location.replace("/");
        } finally {
            setLoading(btnSave, false);
        }
    };

    btnErase.onclick = async () => {
        if (state.stopped) return;
        if (!confirm("Wirklich alle WLANs löschen?")) return;

        try {
            setLoading(btnErase, true);
            const r = await fetch(EP.erase, { method: "POST", headers: csrfHeaders(), cache: "no-store" });
            if (!r.ok) showModalInfo("Fehler", await r.text());
            window.location.replace("/");
        } finally {
            setLoading(btnErase, false);
        }
    };

    btnConn.onclick = async () => {
        if (state.stopped) return;
        const ssid = (ssidSel.value || "").trim();
        const pw = ($("#pw").value || "");
        const pr = parseInt(($("#prio").value || "0"), 10) || 0;
        if (!ssid) return;

        startConnectPolling();

        try {
            setLoading(btnConn, true);
            const r = await fetch(EP.connect, {
                method: "POST",
                headers: csrfHeaders({ "Content-Type": "application/json" }),
                body: JSON.stringify({ ssid, password: pw, priority: pr }),
                cache: "no-store",
                keepalive: true
            });
            if (!r.ok) showModalInfo("Fehler", await r.text());
        } catch (e) {
        } finally {
            setLoading(btnConn, false);
        }
    };

    btnAdd.onclick = async () => {
        if (state.stopped) return;
        const ssid = (ssidSel.value || "").trim();
        const pw = ($("#pw").value || "");
        const pr = parseInt(($("#prio").value || "0"), 10) || 0;
        if (!ssid) return;

        try {
            setLoading(btnAdd, true);
            const r = await fetch(EP.add, {
                method: "POST",
                headers: csrfHeaders({ "Content-Type": "application/json" }),
                body: JSON.stringify({ ssid, password: pw, priority: pr }),
                cache: "no-store"
            });
            if (!r.ok) { showModalInfo("Fehler", await r.text()); return; }
            window.location.replace("/");
        } catch (e) {
            showModalInfo("Fehler", "Hinzufügen fehlgeschlagen.");
        } finally {
            setLoading(btnAdd, false);
        }
    };

    document.addEventListener("DOMContentLoaded", () => {
        loadScan();
        pollStatusLoop();
    });
})();
</script>
</body>
</html>
)HTML";
}