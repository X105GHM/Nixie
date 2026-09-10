document.addEventListener("DOMContentLoaded", () => {
  const skipSync = new Set();
  const pendingState = new Map();
  let lastData = {};
  let otaPollingActive = false;
  let infoStreamConnected = false;
  let infoEventSource = null;
  let lastStateRevision = 0;

  // Netzwerklog
  const netLogEl = document.getElementById("networkLog");
  const clearBtn = document.getElementById("clearNetworkLog");
  const trimCheckbox = document.getElementById("logTrimToggle");
  const logToUI = msg => {
    if (!netLogEl)
      return;

    const ts = new Date().toLocaleTimeString("de-DE");
    netLogEl.textContent += `[${ts}] ${msg}\n`;

    if (trimCheckbox.checked) {
      const lines = netLogEl.textContent.split("\n");
      if (lines.length > 255) {
        netLogEl.textContent = lines.slice(lines.length - 255).join("\n");
      }
    }

    netLogEl.parentElement.scrollTop = netLogEl.parentElement.scrollHeight;
  };

  clearBtn.addEventListener("click", () => {
    netLogEl.textContent = "";
  });

  // Eingabe senden
  const sendBtn = document.getElementById("sendCommand"),
        inputEl = document.getElementById("manualCommand");
  inputEl.addEventListener("keydown", e => {
    if (e.key === "Enter") {
      e.preventDefault();
      sendBtn.click();
    }
  });

  // API-Aufrufe mitloggen
  const _fetch = window.fetch;
  window.fetch = async (input, init) => {
    const url = typeof input === "string" ? input : input.url, method = init?.method || "GET",
          start = Date.now();
    logToUI(`→ ${method} ${url}`);
    try {
      const res = await _fetch(input, init), dur = Date.now() - start;
      logToUI(`← ${res.status} ${url} (${dur}ms)`);
      return res;
    } catch (err) {
      const dur = Date.now() - start;
      logToUI(`!! ${method} ${url} ERROR (${dur}ms): ${err}`);
      throw err;
    }
  };

  const API_BASE = '/api/v1';
  const apiJson = (path, method, body) => fetch(`${API_BASE}${path}`, {
    method,
    headers: {'Content-Type': 'application/json', 'X-Nixie-CSRF': '1'},
    body: JSON.stringify(body),
    cache: 'no-store'
  });
  const sendCommand = (command, fields = {}) => apiJson('/commands', 'POST', {command, ...fields});

  // Kurzbefehle
  sendBtn.addEventListener("click", () => {
    const txt = inputEl.value.trim();
    if (!txt)
      return;

    const parts = txt.split('/');
    let request = null;

    if (parts[0] === 'set') {
      switch (parts[1]) {
        case 'ON':
          request = () => sendCommand('display.set', {enabled: true});
          break;
        case 'OFF':
          request = () => sendCommand('display.set', {enabled: false});
          break;
        case 'reset':
          request = () => sendCommand('system.restart');
          break;
        case 'setOldValue':
          request = () => sendCommand('config.reload');
          break;
        case 'resetValue':
          request = () => sendCommand('config.reset');
          break;
        case 'ACP':
          request = () => sendCommand('display.acp');
          break;
        case 'tempDisplay':
          request = () => sendCommand('display.weather');
          break;
        case 'DATE':
          request = () => sendCommand('display.date');
          break;

        case 'zip':
          if (parts[2])
            request = () => sendCommand('zip.set', {zip: parts[2]});
          break;

        case 'ticker':
        case 'silentMode':
        case 'weatherUpdate':
          if (parts[2] === '0' || parts[2] === '1') {
            const commands = {
              ticker: 'ticker.set',
              silentMode: 'silent.set',
              weatherUpdate: 'weather.set'
            };
            request = () => sendCommand(commands[parts[1]], {enabled: parts[2] === '1'});
          }
          break;

        case 'timeLimit':
          if (parts[2] === '0' || parts[2] === '1') {
            const fields = {enabled: parts[2] === '1'};
            if (parts[3] && parts[4]) {
              fields.from = parts[3];
              fields.to = parts[4];
            }
            request = () => sendCommand('time_limit.set', fields);
          }
          break;

        case 'manualBrightness':
          if (parts[2]) {
            const fields = {enabled: parts[2] === 'true' || parts[2] === '1'};
            if (parts[3]) {
              fields.brightness = Number(parts[3]);
            }
            request = () => sendCommand('brightness.manual', fields);
          }
          break;

        case 'logConfig':
          if (parts[2] && /^\d+$/.test(parts[2])) {
            request = () => sendCommand('logging.set', {value: Number(parts[2])});
          }
          break;

        case 'firmware':
          if (parts[2]) {
            request = () => sendCommand('firmware.set', {target: parts[2]});
          }
          break;

        case 'ota':
          request = () => sendCommand('ota.start');
          break;

        case 'pwmPeriod':
          if (parts[2] && /^\d+$/.test(parts[2])) {
            request = () => sendCommand('pwm.period', {microseconds: Number(parts[2])});
          }
          break;

        case 'brownout':
          request = () => sendCommand('brownout.clear');
          break;
      }
    } else if (parts[0] === 'get') {
      switch (parts[1]) {
        case 'info':
          request = () => fetch(`${API_BASE}/status`, {cache: 'no-store'});
          break;

        case 'brownout':
          request = () => fetch(`${API_BASE}/brownout-log`, {cache: 'no-store'});
          break;

        case 'taskStats':
          request = () => fetch(`${API_BASE}/task-stats`, {cache: 'no-store'});
          break;
      }
    }

    if (request) {
      request()
          .then(async res => {
            const body = await res.text();
            if (body)
              logToUI(`   ▶ body: ${body}`);
          })
          .catch(err => logToUI(`!! API request ERROR: ${err}`));
      inputEl.value = '';
    } else {
      logToUI(`Ungültiger Befehl: ${txt}`);
    }
  });

  // Navigation
  const tabs = document.querySelectorAll("nav .menu li"),
        mobileTabs = document.querySelectorAll("#mobileMenu li"),
        tabContents = document.querySelectorAll(".tab-content"),
        hamburger = document.getElementById("hamburger"),
        mobileMenu = document.getElementById("mobileMenu");

  function activateTab(id) {
    tabs.forEach(t => t.classList.toggle("active", t.dataset.tab === id));
    mobileTabs.forEach(t => t.classList.toggle("active", t.dataset.tab === id));
    tabContents.forEach(
        sec => sec.id === id ? sec.classList.add("active") : sec.classList.remove("active"));
    if (id === "info")
      void loadResetDiagnostics();
  }

  tabs.forEach(t => t.addEventListener("click", () => activateTab(t.dataset.tab)));
  mobileTabs.forEach(t => t.addEventListener("click", () => {
    activateTab(t.dataset.tab);
    mobileMenu.style.display = mobileMenu.style.display === "flex" ? "none" : "flex";
  }));

  hamburger.addEventListener("click", () => {
    mobileMenu.style.display = mobileMenu.style.display === "flex" ? "none" : "flex";
  });

  activateTab("dashboard");

  // Neustartdiagnose

  const resetDiagnosticsOutput = document.getElementById("resetDiagnosticsOutput");
  const resetDiagnosticsRefresh = document.getElementById("resetDiagnosticsRefresh");

  function formatDiagnosticDuration(milliseconds) {
    const totalSeconds = Math.max(0, Math.floor(Number(milliseconds) / 1000));
    const days = Math.floor(totalSeconds / 86400);
    const hours = Math.floor((totalSeconds % 86400) / 3600);
    const minutes = Math.floor((totalSeconds % 3600) / 60);
    const seconds = totalSeconds % 60;
    const parts = [];
    if (days)
      parts.push(`${days} d`);
    if (hours || days)
      parts.push(`${hours} h`);
    if (minutes || hours || days)
      parts.push(`${minutes} min`);
    parts.push(`${seconds} s`);
    return parts.join(" ");
  }

  function resetReasonLabel(reason) {
    const labels = {
      power_on: "Einschalten / Stromversorgung",
      external: "externer Reset",
      software: "Software-Neustart",
      panic: "Panic / Ausnahme",
      interrupt_watchdog: "Interrupt-Watchdog",
      task_watchdog: "Task-Watchdog",
      watchdog: "Watchdog",
      deep_sleep: "Deep-Sleep-Ende",
      brownout: "Hardware-Brownout",
      power_glitch: "Versorgungsspannungs-Glitch",
      cpu_lockup: "CPU-Lockup",
      usb: "USB-Reset",
      jtag: "JTAG-Reset",
      efuse: "eFuse-Fehler",
      sdio: "SDIO-Reset",
      unknown: "unbekannt"
    };
    return labels[reason] || reason || "unbekannt";
  }

  async function loadResetDiagnostics() {
    if (!resetDiagnosticsOutput)
      return;
    if (resetDiagnosticsRefresh)
      resetDiagnosticsRefresh.disabled = true;
    try {
      const response = await fetch(`${API_BASE}/reset-log`, {cache: "no-store"});
      if (!response.ok)
        throw new Error(`HTTP ${response.status}`);
      const diagnostics = await response.json();
      const current = diagnostics.current || {};
      const history = current.history || {};
      const lines = [
        `Aktueller Boot #${current.bootId ?? "–"}`,
        `Resetgrund: ${resetReasonLabel(current.resetReason)} (${current.resetReasonCode ?? "–"})`,
        `Laufzeit: ${formatDiagnosticDuration(current.uptimeMs || 0)}`,
        `Interner Heap: ${Number(current.freeInternalHeap || 0).toLocaleString("de-DE")} Byte`,
        `Freier PSRAM: ${Number(current.freePsram || 0).toLocaleString("de-DE")} Byte`,
        `Chart-Historie: ${history.state || "unbekannt"}${
            history.available ? " / aktiv" : " / nicht verfügbar"}`,
        `NVS-Resetlog: ${diagnostics.nvsHealthy ? "OK" : "Fehler"}`, "",
        `Letzte Boot-Ereignisse (${diagnostics.count || 0}/${diagnostics.capacity || 0}):`
      ];

      (diagnostics.events || []).forEach(event => {
        const planned = event.plannedReason ? `, geplant: ${event.plannedReason}` : "";
        lines.push("");
        lines.push(`#${event.bootId}: ${resetReasonLabel(event.resetReason)}${planned}`);
        if (!event.previous) {
          lines.push(
              "  Vorheriger Zustand: nicht verfügbar (z. B. vollständiger Stromverlust/erste Firmware)");
          return;
        }
        const lastSeen = event.previous.lastSeen ?
            new Date(event.previous.lastSeen).toLocaleString("de-DE") :
            "keine gültige SNTP-Zeit";
        lines.push(`  Letztes Lebenszeichen: ${lastSeen}`);
        lines.push(
            `  Vorherige Laufzeit: ${formatDiagnosticDuration(event.previous.uptimeMs || 0)}`);
        lines.push(
            `  12-V-Schiene zuletzt: ${Number(event.previous.voltage12V || 0).toFixed(3)} V`);
        lines.push(`  Heap/PSRAM: ${
            Number(event.previous.freeInternalHeap || 0).toLocaleString("de-DE")} / ${
            Number(event.previous.freePsram || 0).toLocaleString("de-DE")} Byte`);
        lines.push(`  History-Zustand: ${event.previous.historyState || "unbekannt"}`);
      });
      resetDiagnosticsOutput.textContent = lines.join("\n");
    } catch (error) {
      resetDiagnosticsOutput.textContent =
          `Neustart-Diagnose konnte nicht geladen werden: ${error.message || error}`;
    } finally {
      if (resetDiagnosticsRefresh)
        resetDiagnosticsRefresh.disabled = false;
    }
  }

  resetDiagnosticsRefresh?.addEventListener("click", () => void loadResetDiagnostics());

  // Uhr und Zustandsabgleich
  const clockEl = document.getElementById("clock");
  setInterval(() => {
    clockEl.textContent = new Date().toLocaleTimeString("de-DE");
  }, 1000);

  const skipSyncTimeouts = new Map();

  function holdSkipSync(id, ms = 30000) {
    skipSync.add(id);

    if (skipSyncTimeouts.has(id)) {
      clearTimeout(skipSyncTimeouts.get(id));
    }

    const t = setTimeout(() => {
      skipSync.delete(id);
      skipSyncTimeouts.delete(id);
    }, ms);

    skipSyncTimeouts.set(id, t);
  }

  function normalizeStateValue(value, expected) {
    if (typeof expected === "boolean") {
      return value === true || value === 1 || value === "1" || value === "true";
    }
    if (typeof expected === "number") {
      const number = Number(value);
      return Number.isFinite(number) ? number : value;
    }
    return value == null ? "" : String(value);
  }

  function pendingVisualTarget(element) {
    return element?.closest?.(".switch") || element;
  }

  function clearPendingState(id) {
    const pending = pendingState.get(id);
    if (!pending)
      return;
    clearTimeout(pending.timer);
    pendingState.delete(id);
    const element = document.getElementById(id);
    element?.removeAttribute("aria-busy");
    pendingVisualTarget(element)?.classList.remove("pending-sync");
  }

  function beginPendingState(control, stateKey, expected, timeoutMs = 12000) {
    const element = typeof control === "string" ? document.getElementById(control) : control;
    if (!element)
      return;

    clearPendingState(element.id);
    const token = Symbol(element.id);
    const timer = setTimeout(() => {
      if (pendingState.get(element.id)?.token !== token)
        return;
      clearPendingState(element.id);
      console.warn(`State confirmation timed out for ${element.id}`);
      void fetchInfo();
    }, timeoutMs);

    pendingState.set(
        element.id,
        {stateKey, expected, timer, token, requestAccepted: false, minimumRevision: null});
    element.setAttribute("aria-busy", "true");
    pendingVisualTarget(element)?.classList.add("pending-sync");
    return {id: element.id, token};
  }

  function acceptPendingRequest(reference) {
    if (!reference)
      return;
    const pending = pendingState.get(reference.id);
    if (!pending || pending.token !== reference.token)
      return;
    pending.requestAccepted = true;
    pending.minimumRevision = lastStateRevision + 1;
  }

  function clearPendingReference(reference) {
    if (!reference)
      return;
    const pending = pendingState.get(reference.id);
    if (pending?.token === reference.token)
      clearPendingState(reference.id);
  }

  function applyPendingState(serverData, revision = null) {
    const effectiveData = {...serverData};
    pendingState.forEach((pending, id) => {
      const revisionConfirmed = revision == null || pending.minimumRevision == null ||
          revision >= pending.minimumRevision;
      if (pending.requestAccepted && revisionConfirmed &&
          Object.prototype.hasOwnProperty.call(serverData, pending.stateKey) &&
          normalizeStateValue(serverData[pending.stateKey], pending.expected) ===
              normalizeStateValue(pending.expected, pending.expected)) {
        clearPendingState(id);
        return;
      }
      effectiveData[pending.stateKey] = pending.expected;
    });
    return effectiveData;
  }

  async function sendStateCommand(
      controls, stateKey, expected, command, fields = {}, timeoutMs = 12000) {
    const controlList = Array.isArray(controls) ? controls : [controls];
    const pendingReferences =
        controlList.map(control => beginPendingState(control, stateKey, expected, timeoutMs))
            .filter(Boolean);
    try {
      const response = await sendCommand(command, fields);
      if (!response.ok)
        throw new Error(`HTTP ${response.status}`);
      pendingReferences.forEach(acceptPendingRequest);
      return response;
    } catch (error) {
      pendingReferences.forEach(clearPendingReference);
      void fetchInfo();
      throw error;
    }
  }

  // Laufzeitmonitor
  const statsMonitorEls = {
    barCard: document.getElementById("barCard"),
    mainBody: document.getElementById("statsMonitorMainBody"),
    coreCard: document.getElementById("statsMonitorCoreCard"),
    barCanvas: document.getElementById("barCanvas"),
    pie0: document.getElementById("pie0"),
    pie1: document.getElementById("pie1"),
    legend: document.getElementById("legend"),
    pieLegend0: document.getElementById("pieLegend0"),
    pieLegend1: document.getElementById("pieLegend1"),
    toggle: document.getElementById("toggleMonitor"),
    unitSelect: document.getElementById("unitSelect"),
    windowText: document.getElementById("windowText"),
    lastUpdateText: document.getElementById("lastUpdateText"),
    core0Total: document.getElementById("core0Total"),
    core1Total: document.getElementById("core1Total")
  };

  const statsMonitor = {enabled: true, timer: null, data: null, displayUnit: "ms"};

  function statsMonitorAvailable() {
    return !!(statsMonitorEls.barCanvas && statsMonitorEls.pie0 && statsMonitorEls.pie1);
  }

  function statsIsDark() {
    return document.body.classList.contains("dark");
  }

  function statsPalette() {
    if (statsIsDark()) {
      return {
        text: "#e0e0e0",
        muted: "rgba(224,224,224,0.75)",
        grid: "rgba(255,255,255,0.10)",
        panel: "rgba(255,255,255,0.05)",
        idle: "rgba(255,255,255,0.16)",
        donutHole: "rgba(30,30,30,0.92)",
        shadow: "rgba(0,0,0,0.30)"
      };
    }

    return {
      text: "#333",
      muted: "rgba(51,51,51,0.72)",
      grid: "rgba(0,0,0,0.10)",
      panel: "rgba(0,0,0,0.04)",
      idle: "rgba(0,0,0,0.12)",
      donutHole: "rgba(255,255,255,0.94)",
      shadow: "rgba(0,0,0,0.10)"
    };
  }

  function hashString(str) {
    let h = 0;
    for (let i = 0; i < str.length; i++) {
      h = ((h << 5) - h) + str.charCodeAt(i);
      h |= 0;
    }
    return Math.abs(h);
  }

  function statsTaskColor(name, alpha = 1) {
    const hue = hashString(name || "?") % 360;
    return `hsla(${hue}, 72%, 56%, ${alpha})`;
  }

  function fitStatsCanvas(canvas) {
    const rect = canvas.getBoundingClientRect();
    const cssW = Math.max(1, Math.floor(rect.width));
    const cssH = Math.max(1, Math.floor(rect.height));
    const dpr = Math.max(1, window.devicePixelRatio || 1);

    canvas.width = Math.floor(cssW * dpr);
    canvas.height = Math.floor(cssH * dpr);

    const ctx = canvas.getContext("2d");
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return {ctx, w: cssW, h: cssH};
  }

  function statsSum(arr) {
    return arr.reduce((a, b) => a + b, 0);
  }

  function statsUnitSymbol(u) {
    if (u === "µs")
      return "µs";
    if (u === "ns")
      return "ns";
    return "ms";
  }

  function statsConvertFromUs(us, u) {
    if (u === "ms")
      return us / 1000;
    if (u === "ns")
      return us * 1000;
    return us;
  }

  function statsFormatNumber(x) {
    const ax = Math.abs(x);
    if (ax >= 1000)
      return Math.round(x).toLocaleString("de-DE");
    if (ax >= 100)
      return (Math.round(x * 10) / 10).toString();
    return (Math.round(x * 100) / 100).toString();
  }

  function statsCompact(x) {
    const ax = Math.abs(x);
    const sign = x < 0 ? "-" : "";
    if (ax >= 1e9)
      return sign + (Math.round((ax / 1e9) * 100) / 100) + "G";
    if (ax >= 1e6)
      return sign + (Math.round((ax / 1e6) * 100) / 100) + "M";
    if (ax >= 1e3)
      return sign + (Math.round((ax / 1e3) * 100) / 100) + "k";
    if (ax >= 100)
      return sign + Math.round(ax);
    return sign + (Math.round(ax * 10) / 10);
  }

  function statsFormatPct(x) {
    const s = (Math.round((Number(x) || 0) * 10) / 10).toFixed(1);
    return s.endsWith(".0") ? s.slice(0, -2) : s;
  }

  function statsFormatValue(val) {
    return Math.abs(val) >= 1000 ? statsCompact(val) : statsFormatNumber(val);
  }

  function roundStatsRect(ctx, x, y, w, h, r) {
    const rr = Math.min(r, h / 2, w / 2);
    ctx.beginPath();
    ctx.moveTo(x + rr, y);
    ctx.arcTo(x + w, y, x + w, y + h, rr);
    ctx.arcTo(x + w, y + h, x, y + h, rr);
    ctx.arcTo(x, y + h, x, y, rr);
    ctx.arcTo(x, y, x + w, y, rr);
    ctx.closePath();
  }

  function setStatsPieLegend(el, items) {
    if (!el)
      return;
    el.innerHTML = "";
    items.forEach(it => {
      const row = document.createElement("div");
      row.className = "pie-leg-item";
      row.innerHTML = `
        <div class="pie-leg-left">
          <span class="swatch" style="background:${it.color}"></span>
          <span class="pie-leg-name">${it.name}</span>
        </div>
        <div class="pie-leg-pct">${it.pct}%</div>
      `;
      el.appendChild(row);
    });
  }

  function buildStatsLegend(tasks) {
    if (!statsMonitorEls.legend)
      return;
    statsMonitorEls.legend.innerHTML = "";
    tasks.forEach(t => {
      const item = document.createElement("div");
      item.className = "legend-item";
      item.innerHTML = `
        <span class="swatch" style="background:${statsTaskColor(t.name)}"></span>
        <span>${t.name}</span>
        <span class="mono" style="opacity:.75;">(Core ${t.core})</span>
      `;
      statsMonitorEls.legend.appendChild(item);
    });
  }

  function drawStatsBars() {
    if (!statsMonitor.data || !statsMonitorEls.barCanvas)
      return;

    const palette = statsPalette();
    const tasks = Array.isArray(statsMonitor.data.tasks) ? statsMonitor.data.tasks : [];

    const minHeight = 320;
    const perTaskHeight = 34;
    const dynamicHeight = Math.max(minHeight, 70 + tasks.length * perTaskHeight);
    statsMonitorEls.barCanvas.style.height = `${dynamicHeight}px`;

    const {ctx, w, h} = fitStatsCanvas(statsMonitorEls.barCanvas);

    ctx.clearRect(0, 0, w, h);

    if (!tasks.length) {
      ctx.fillStyle = palette.muted;
      ctx.font = "14px Arial";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText("Keine Task-Daten verfügbar", w / 2, h / 2);
      return;
    }

    const padL = (w < 420) ? 120 : 160;
    const padR = 18;
    const padT = 22;
    const padB = 18;

    const plotW = w - padL - padR;
    const plotH = h - padT - padB;

    const valuesUs = tasks.map(t => Number(t.runtime_delta_us || 0));
    const maxUs = Math.max(1, ...valuesUs);
    const maxDisp = statsConvertFromUs(maxUs, statsMonitor.displayUnit);

    const baseFont = (w < 420) ? "11px ui-monospace, Menlo, Consolas, monospace" :
                                 "12px ui-monospace, Menlo, Consolas, monospace";

    ctx.font = baseFont;

    let ticks = 5;
    while (ticks > 2) {
      const spacing = plotW / ticks;
      const testLabel = statsCompact(maxDisp);
      const labelW = ctx.measureText(testLabel).width;
      if (spacing >= labelW + 14)
        break;
      ticks--;
    }

    ctx.strokeStyle = palette.grid;
    ctx.lineWidth = 1;

    for (let i = 0; i <= ticks; i++) {
      const x = padL + (plotW * i / ticks);
      ctx.beginPath();
      ctx.moveTo(x, padT);
      ctx.lineTo(x, padT + plotH);
      ctx.stroke();
    }

    ctx.fillStyle = palette.muted;
    ctx.font = baseFont;
    ctx.textAlign = "left";
    ctx.textBaseline = "bottom";
    ctx.fillText(`Unit: ${statsUnitSymbol(statsMonitor.displayUnit)}`, padL + 12, padT - 6);

    ctx.textAlign = "center";
    for (let i = 0; i <= ticks; i++) {
      const x = padL + (plotW * i / ticks);
      const valDisp = (maxDisp * i / ticks);
      ctx.fillText(statsCompact(valDisp), x, padT - 6);
    }

    const rowH = plotH / tasks.length;
    const barH = rowH * 0.56;

    tasks.forEach((t, idx) => {
      const yCenter = padT + rowH * idx + rowH / 2;
      const y = yCenter - barH / 2;

      const valUs = Number(t.runtime_delta_us || 0);
      const barW = (valUs / maxUs) * plotW;

      ctx.textAlign = "left";
      ctx.textBaseline = "middle";
      ctx.fillStyle = palette.text;
      ctx.font = (w < 420) ? "12px Arial" : "13px Arial";
      ctx.fillText(t.name || "?", 12, yCenter);

      ctx.fillStyle = palette.muted;
      ctx.font = "11px ui-monospace, Menlo, Consolas, monospace";
      ctx.fillText(`Core ${t.core}`, 12, yCenter + 16);

      ctx.fillStyle = palette.panel;
      roundStatsRect(ctx, padL, y, plotW, barH, 10);
      ctx.fill();

      ctx.fillStyle = statsTaskColor(t.name || "?");
      roundStatsRect(ctx, padL, y, Math.max(3, barW), barH, 10);
      ctx.fill();

      const valDisp = statsConvertFromUs(valUs, statsMonitor.displayUnit);
      const txt = `${statsFormatValue(valDisp)} ${statsUnitSymbol(statsMonitor.displayUnit)} • ${
          statsFormatPct(t.pct_total)}%`;

      ctx.fillStyle = palette.text;
      ctx.font = "11px ui-monospace, Menlo, Consolas, monospace";
      ctx.textAlign = "left";
      ctx.fillText(txt, padL + Math.min(plotW - 140, barW + 8), yCenter);
    });
  }

  function drawStatsPie(canvas, coreIndex) {
    if (!statsMonitor.data || !canvas)
      return;

    const palette = statsPalette();
    const tasks = (statsMonitor.data.tasks || []).filter(t => Number(t.core) === coreIndex);
    const {ctx, w, h} = fitStatsCanvas(canvas);

    ctx.clearRect(0, 0, w, h);

    const cx = w / 2;
    const cy = h / 2;
    const r = Math.min(w, h) * 0.36;

    const busyPct = tasks.reduce((sum, t) => sum + (Number(t.pct_core) || 0), 0);
    const loadPct = Math.max(0, Math.min(100, busyPct));

    const windowUs = Number(statsMonitor.data.window_ms || 0) * 1000;
    const usedUs = Math.round(windowUs * (loadPct / 100));
    const idleUs = Math.max(0, windowUs - usedUs);

    const slices = tasks.map(t => ({
                               name: t.name || "?",
                               pct: Number(t.pct_core || 0),
                               color: statsTaskColor(t.name || "?")
                             }));

    if (100 - loadPct > 0.05) {
      slices.push({name: "Idle", pct: 100 - loadPct, color: palette.idle});
    }

    const totalPct = statsSum(slices.map(s => s.pct)) || 1;
    let a0 = -Math.PI / 2;

    ctx.beginPath();
    ctx.arc(cx, cy, r + 8, 0, Math.PI * 2);
    ctx.fillStyle = palette.shadow;
    ctx.fill();

    slices.forEach(s => {
      const a1 = a0 + (s.pct / totalPct) * Math.PI * 2;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.arc(cx, cy, r, a0, a1);
      ctx.closePath();
      ctx.fillStyle = s.color;
      ctx.fill();
      a0 = a1;
    });

    ctx.beginPath();
    ctx.arc(cx, cy, r * 0.60, 0, Math.PI * 2);
    ctx.fillStyle = palette.donutHole;
    ctx.fill();

    ctx.textAlign = "center";
    ctx.textBaseline = "middle";

    ctx.fillStyle = palette.muted;
    ctx.font = "12px ui-monospace, Menlo, Consolas, monospace";
    ctx.fillText(`Core ${coreIndex}`, cx, cy - 18);

    ctx.fillStyle = palette.text;
    ctx.font = "700 22px Arial";
    ctx.fillText(`${Math.round(loadPct)}%`, cx, cy + 6);

    const legendItems =
        slices.map(s => ({name: s.name, color: s.color, pct: statsFormatPct(s.pct)}));

    const usedDisp = statsConvertFromUs(usedUs, statsMonitor.displayUnit);
    const idleDisp = statsConvertFromUs(idleUs, statsMonitor.displayUnit);
    const uSym = statsUnitSymbol(statsMonitor.displayUnit);

    if (coreIndex === 0) {
      setStatsPieLegend(statsMonitorEls.pieLegend0, legendItems);
      if (statsMonitorEls.core0Total) {
        statsMonitorEls.core0Total.textContent = `Used: ${statsFormatNumber(usedDisp)} ${
            uSym} • Idle: ${statsFormatNumber(idleDisp)} ${uSym}`;
      }
    } else {
      setStatsPieLegend(statsMonitorEls.pieLegend1, legendItems);
      if (statsMonitorEls.core1Total) {
        statsMonitorEls.core1Total.textContent = `Used: ${statsFormatNumber(usedDisp)} ${
            uSym} • Idle: ${statsFormatNumber(idleDisp)} ${uSym}`;
      }
    }
  }

  function renderStatsMonitor() {
    if (!statsMonitorAvailable() || !statsMonitor.data)
      return;

    buildStatsLegend(statsMonitor.data.tasks || []);
    drawStatsBars();
    drawStatsPie(statsMonitorEls.pie0, 0);
    drawStatsPie(statsMonitorEls.pie1, 1);
  }

  async function fetchStatsMonitor() {
    if (!statsMonitorAvailable() || !statsMonitor.enabled || otaPollingActive)
      return;

    try {
      const res = await fetch(`${API_BASE}/task-stats`, {cache: "no-store"});
      if (!res.ok)
        throw new Error(res.status);

      const data = await res.json();
      statsMonitor.data = data;

      if (statsMonitorEls.windowText) {
        statsMonitorEls.windowText.textContent = `${data.window_ms || 0} ms`;
      }

      if (statsMonitorEls.lastUpdateText) {
        statsMonitorEls.lastUpdateText.textContent =
            data.timestamp || new Date().toLocaleTimeString("de-DE");
      }

      renderStatsMonitor();
    } catch (e) {
      console.warn("Stats monitor error:", e);
    }
  }

  function startStatsMonitor() {
    if (!statsMonitorAvailable())
      return;

    stopStatsMonitor(false);
    statsMonitor.enabled = true;

    setStatsMonitorVisibility(true);
    statsMonitorEls.barCard?.classList.remove("disabled");

    fetchStatsMonitor();
    statsMonitor.timer = setInterval(fetchStatsMonitor, 2000);
  }

  function stopStatsMonitor(markDisabled = true) {
    if (statsMonitor.timer) {
      clearInterval(statsMonitor.timer);
      statsMonitor.timer = null;
    }

    statsMonitor.enabled = false;

    setStatsMonitorVisibility(false);

    if (markDisabled) {
      statsMonitorEls.barCard?.classList.add("disabled");
    }
  }

  if (statsMonitorAvailable()) {
    setStatsMonitorVisibility(statsMonitorEls.toggle?.checked ?? true);

    statsMonitorEls.unitSelect?.addEventListener("change", () => {
      statsMonitor.displayUnit = statsMonitorEls.unitSelect.value;
      renderStatsMonitor();
    });

    statsMonitorEls.toggle?.addEventListener("change", () => {
      if (statsMonitorEls.toggle.checked) {
        startStatsMonitor();
      } else {
        stopStatsMonitor();
      }
    });

    window.addEventListener("resize", renderStatsMonitor);
  }

  function setStatsMonitorVisibility(enabled) {
    if (!statsMonitorEls.mainBody || !statsMonitorEls.coreCard)
      return;

    if (enabled) {
      statsMonitorEls.mainBody.classList.remove("monitor-hidden");
      statsMonitorEls.coreCard.classList.remove("monitor-hidden");

      requestAnimationFrame(() => {
        statsMonitorEls.mainBody.classList.remove("monitor-collapsing");
        statsMonitorEls.coreCard.classList.remove("monitor-collapsing");
      });
    } else {
      statsMonitorEls.mainBody.classList.add("monitor-hidden");
      statsMonitorEls.coreCard.classList.add("monitor-hidden");
    }
  }

  function formatHourMinute(value) {
    const n = Number(value);
    if (!Number.isFinite(n))
      return "–";
    return `${String(n).padStart(2, "0")}:00`;
  }

  // Farbschema
  const themeToggle = document.getElementById("themeToggle");
  if (localStorage.getItem("darkMode") === "true")
    document.body.classList.add("dark");
  themeToggle.checked = document.body.classList.contains("dark");
  themeToggle.addEventListener("change", () => {
    document.body.classList.toggle("dark", themeToggle.checked);
    localStorage.setItem("darkMode", themeToggle.checked);
    renderRuntimeMonitor();
  });

  // Einzelröhren
  const acpToggle = document.getElementById('acpToggle');
  const acpControls = document.getElementById('acpControls');
  const nixieContainer = document.getElementById('acpNixieContainer');
  const nixieTubes = Array.from(nixieContainer.querySelectorAll('.nixie-tube'));
  const applyBtn = document.getElementById('acpCleanButton');

  let selectedTube = null;

  // Startzustand
  acpControls.style.display = 'none';
  nixieContainer.classList.add('hidden');
  applyBtn.classList.add('hidden');

  // Bedienfeld zeigen
  acpToggle.addEventListener('change', () => {
    const on = acpToggle.checked;

    // Oberfläche
    acpControls.style.display = on ? 'block' : 'none';
    nixieContainer.classList.toggle('hidden', !on);
    applyBtn.classList.toggle('hidden', !on);
    if (on)
      initNixie();

    // Andere Aktionen sperren
    [ACPButton, DateButton, WeatherButton, CricketButton, StartValuesButton].forEach(btn => {
      btn.disabled = on;
    });

    if (!on) {
      sendCommand('display.single', {enabled: false})
          .then(res => {
            if (!res.ok)
              console.error('singleDigitControl:value=0 failed', res.status);
          })
          .catch(err => console.error('Network error (value=0):', err));
    }
  });

  function initNixie() {
    nixieTubes.forEach((tube, idx) => {
      const span = tube.querySelector('.digit');
      span.textContent = '';
      tube.classList.remove('selected');
      if (idx === 0) {
        span.textContent = '0';
        tube.classList.add('selected');
        selectedTube = tube;
        applyBtn.disabled = false;
      }
    });
  }
  initNixie();

  nixieTubes.forEach(tube => {
    tube.addEventListener('click', () => {
      nixieTubes.forEach(t => {
        if (t !== tube) {
          t.querySelector('.digit').textContent = '';
          t.classList.remove('selected');
        }
      });
      const span = tube.querySelector('.digit');
      const num = span.textContent === '' ? 0 : (parseInt(span.textContent, 10) + 1) % 10;
      span.textContent = num;
      tube.classList.add('selected');
      selectedTube = tube;
      applyBtn.disabled = false;
    });
  });

  applyBtn.addEventListener('click', () => {
    if (!selectedTube)
      return;

    const idx = Number(selectedTube.dataset.index) - 1;  // 0…5
    const digit = parseInt(selectedTube.querySelector('.digit').textContent, 10);
    const singleDigit = idx * 10 + digit;  // 0…59

    sendCommand('display.single', {enabled: true, digit: singleDigit})
        .then(res => {
          if (!res.ok) {
            console.error('singleDigitControl:value=1 failed', res.status);
          } else {
            console.log('singleDigitControl OK');
          }
        })
        .catch(err => console.error('Network error:', err));
  });

  // Diagramme
  const configs = [
    {key: "voltage_12V", canvasId: "chart_voltage_12V", label: "Spannung 12 V (V)"},
    {key: "voltage_5V", canvasId: "chart_voltage_5V", label: "Spannung 5 V (V)"},
    {key: "voltage_3V3", canvasId: "chart_voltage_3V3", label: "Spannung 3.3 V (V)"},
    {key: "voltage_18V", canvasId: "chart_voltage_18V", label: "Spannung 18 V (V)"},
    {key: "voltage_UHSS", canvasId: "chart_voltage_UHSS", label: "Spannung UHSS (V)"},
    {key: "current_mA", canvasId: "chart_current_mA", label: "Strom (mA)"},
    {key: "power_W", canvasId: "chart_power_W", label: "Leistung (W)"},
    {key: "energy_Wh", canvasId: "chart_energy_Wh", label: "Energie (Wh)"},
    {key: "temperature", canvasId: "chart_temperature", label: "Case-Temperatur (°C)"},
    {key: "freeHeap", canvasId: "chart_freeHeap", label: "FreeHeap (Bytes)"},
    {key: "totalLoad", canvasId: "chart_totalLoad", label: "CPU Auslastung (0-100%)"},
    {key: "coreLoad0", canvasId: "chart_coreLoad0", label: "Kern 0 (0-100%)"},
    {key: "coreLoad1", canvasId: "chart_coreLoad1", label: "Kern 1 (0-100%)"}
  ];
  const chartData = {}, charts = {};
  const chartRangeMilliseconds = {
    '10m': 10 * 60 * 1000,
    '1h': 60 * 60 * 1000,
    '6h': 6 * 60 * 60 * 1000,
    '12h': 12 * 60 * 60 * 1000,
    '24h': 24 * 60 * 60 * 1000,
    '7d': 7 * 24 * 60 * 60 * 1000
  };
  let activeHistoryLoad = null;

  function getColor(i) {
    return [
      "#2196F3", "#FF9800", "#4CAF50", "#E91E63", "#9C27B0", "#00BCD4", "#FFC107", "#795548",
      "#607D8B", "#F44336", "#3F51B5", "#009688", "#FF5722"
    ][i % 13];
  }

  const timeRangeSelect = document.getElementById('timeRange');

  function updateChartTimeWindow() {
    const now = Date.now();
    const spanMs = chartRangeMilliseconds[timeRangeSelect.value] || chartRangeMilliseconds['10m'];

    configs.forEach(cfg => {
      const chart = charts[cfg.key];
      const dataArr = chartData[cfg.key];
      while (dataArr.length > 0 && dataArr[0].x < now - spanMs) dataArr.shift();
      chart.options.scales.x.min = now - spanMs;
      chart.options.scales.x.max = now;
      chart.update('none');
    });
  }

  function chartValue(data, key) {
    const jsonKey =
        key === 'temperature' ? 'CaseTemperature_C' : key.charAt(0).toUpperCase() + key.slice(1);
    const value = Number(data[jsonKey]);
    return Number.isFinite(value) ? value : null;
  }

  function appendLiveChartSample(data, updateCharts = true) {
    const serverTimestamp = Number(data.Timestamp);
    const timestamp =
        Number.isFinite(serverTimestamp) && serverTimestamp > 0 ? serverTimestamp : Date.now();

    if (activeHistoryLoad) {
      activeHistoryLoad.live.push({timestamp, data});
      if (activeHistoryLoad.live.length > 30)
        activeHistoryLoad.live.shift();
      return;
    }

    const spanMs = chartRangeMilliseconds[timeRangeSelect.value] || chartRangeMilliseconds['10m'];
    const cutoff = Date.now() - spanMs;
    configs.forEach(cfg => {
      const value = chartValue(data, cfg.key);
      if (value === null)
        return;
      const points = chartData[cfg.key];
      const last = points[points.length - 1];
      if (last && last.x === timestamp) {
        last.y = value;
      } else if (!last || timestamp > last.x) {
        points.push({x: timestamp, y: value});
      }
      while (points.length > 0 && points[0].x < cutoff) points.shift();
      if (updateCharts)
        charts[cfg.key].update('none');
    });
  }

  async function loadChartHistory(rangeName) {
    const context = {rangeName, live: activeHistoryLoad ? activeHistoryLoad.live : []};
    activeHistoryLoad = context;
    try {
      const response = await fetch(`${API_BASE}/history?range=${encodeURIComponent(rangeName)}`,
                                   {cache: 'no-store'});
      if (!response.ok)
        throw new Error(`History HTTP ${response.status}`);
      const history = await response.json();
      if (activeHistoryLoad !== context)
        return;
      if (!Array.isArray(history.series) || !Array.isArray(history.timestamps) ||
          !Array.isArray(history.average)) {
        throw new Error('Invalid history response');
      }

      configs.forEach(cfg => {
        const seriesIndex = history.series.indexOf(cfg.key);
        const averages = seriesIndex >= 0 ? history.average[seriesIndex] : null;
        const points = chartData[cfg.key];
        points.length = 0;
        if (!Array.isArray(averages))
          return;
        const count = Math.min(history.timestamps.length, averages.length);
        for (let index = 0; index < count; ++index) {
          const timestamp = Number(history.timestamps[index]);
          const value = Number(averages[index]);
          if (Number.isFinite(timestamp) && Number.isFinite(value))
            points.push({x: timestamp, y: value});
        }
      });

      const bufferedLive = context.live.slice();
      activeHistoryLoad = null;
      bufferedLive.forEach(sample => appendLiveChartSample(sample.data, false));
      updateChartTimeWindow();
    } catch (error) {
      if (activeHistoryLoad !== context)
        return;
      const bufferedLive = context.live.slice();
      activeHistoryLoad = null;
      bufferedLive.forEach(sample => appendLiveChartSample(sample.data, false));
      updateChartTimeWindow();
      console.warn('Chart history unavailable, continuing with live data:', error);
    }
  }

  timeRangeSelect.addEventListener('change', () => {
    void loadChartHistory(timeRangeSelect.value);
  });

  function initCharts() {
    configs.forEach((cfg, i) => {
      chartData[cfg.key] = [];
      const ctx = document.getElementById(cfg.canvasId).getContext("2d");
      charts[cfg.key] = new Chart(ctx, {
        type: 'line',
        data: {
          datasets: [{
            label: cfg.label,
            data: chartData[cfg.key],
            borderColor: getColor(i),
            backgroundColor: 'transparent',
            tension: 0.3,
            pointRadius: 0
          }]
        },
        options: {
          animation: false,
          responsive: true,
          parsing: false,
          scales: {
            x: {
              type: 'time',
              time: {unit: 'minute', displayFormats: {minute: 'HH:mm'}},
              title: {display: true, text: 'Zeit'}
            },
            y: {title: {display: true, text: cfg.label}}
          },
          plugins: {legend: {display: false}}
        }
      });
    });
  }

  // OTA-Dialog
  const otaModal = document.getElementById("otaModal");
  const otaCloseBtn = document.getElementById("otaCloseBtn");
  const otaOkBtn = document.getElementById("otaOkBtn");
  const otaRestartBtn = document.getElementById("otaRestartBtn");

  const otaLiveArea = document.getElementById("otaLiveArea");
  const otaResultArea = document.getElementById("otaResultArea");
  const otaRebootFlag = document.getElementById("otaRebootFlag");

  const otaStageText = document.getElementById("otaStageText");
  const otaMessageText = document.getElementById("otaMessageText");

  const otaAppBlock = document.getElementById("otaAppBlock");
  const otaAppBar = document.getElementById("otaAppBar");
  const otaAppPercent = document.getElementById("otaAppPercent");
  const otaAppBytes = document.getElementById("otaAppBytes");

  const otaSpiffsBlock = document.getElementById("otaSpiffsBlock");
  const otaSpiffsBar = document.getElementById("otaSpiffsBar");
  const otaSpiffsPercent = document.getElementById("otaSpiffsPercent");
  const otaSpiffsBytes = document.getElementById("otaSpiffsBytes");

  const otaResultTitle = document.getElementById("otaResultTitle");
  const otaResultText = document.getElementById("otaResultText");
  const updateBannerBadge = document.getElementById("updateBannerBadge");

  const otaUi = {pollTimer: null, visible: false, seenRunning: false, completionHandled: false};

  function otaFormatBytes(n) {
    const x = Number(n) || 0;
    return x.toLocaleString("de-DE");
  }

  function showOtaModal() {
    otaModal.classList.remove("hidden");
    otaUi.visible = true;
  }

  function hideOtaModal() {
    otaModal.classList.add("hidden");
    otaUi.visible = false;
  }

  function otaSetLiveView() {
    otaLiveArea.classList.remove("hidden");
    otaResultArea.classList.add("hidden");
    otaOkBtn.classList.add("hidden");
  }

  function otaSetResultView() {
    otaLiveArea.classList.add("hidden");
    otaResultArea.classList.remove("hidden");
    otaOkBtn.classList.remove("hidden");
  }

  function otaResetBars() {
    otaAppBar.style.width = "0%";
    otaAppPercent.textContent = "0%";
    otaAppBytes.textContent = "0 / 0";

    otaSpiffsBar.style.width = "0%";
    otaSpiffsPercent.textContent = "0%";
    otaSpiffsBytes.textContent = "0 / 0";
  }

  function updateOtaUi(data) {
    otaStageText.textContent = data.stage || "–";
    otaMessageText.textContent = data.message || "–";

    const appPct = Number(data.appProgressPct || 0);
    const appDone = Number(data.appBytesDone || 0);
    const appTotal = Number(data.appBytesTotal || 0);

    const spiffsPct = Number(data.spiffsProgressPct || 0);
    const spiffsDone = Number(data.spiffsBytesDone || 0);
    const spiffsTotal = Number(data.spiffsBytesTotal || 0);

    otaAppBar.style.width = `${Math.max(0, Math.min(100, appPct))}%`;
    otaAppPercent.textContent = `${appPct}%`;
    otaAppBytes.textContent = `${otaFormatBytes(appDone)} / ${otaFormatBytes(appTotal)}`;

    otaSpiffsBar.style.width = `${Math.max(0, Math.min(100, spiffsPct))}%`;
    otaSpiffsPercent.textContent = `${spiffsPct}%`;
    otaSpiffsBytes.textContent = `${otaFormatBytes(spiffsDone)} / ${otaFormatBytes(spiffsTotal)}`;

    // Balken nur beim Update
    const running = !!data.running;
    otaAppBlock.classList.toggle("hidden", !running);
    otaSpiffsBlock.classList.toggle("hidden", !running);

    otaRebootFlag.classList.toggle("hidden", !data.rebootRequired);
    otaRestartBtn.classList.toggle("hidden", !data.rebootRequired);

    if (running) {
      otaSetLiveView();
      showOtaModal();
      otaUi.seenRunning = true;
      return;
    }

    // Nur laufende Updates anzeigen
    const shouldShowResult = otaUi.seenRunning || otaUi.visible;

    if (shouldShowResult && !otaUi.completionHandled) {
      otaUi.completionHandled = true;
      otaSetResultView();
      showOtaModal();

      if (data.stage === "done") {
        otaResultTitle.textContent = "OTA abgeschlossen";
        otaResultText.textContent = data.rebootRequired ?
            "Das Update wurde erfolgreich abgeschlossen." :
            "Das Update wurde erfolgreich abgeschlossen. Kein Neustart erforderlich.";
      } else if (data.stage === "error") {
        otaResultTitle.textContent = "OTA fehlgeschlagen";
        otaResultText.textContent =
            `${data.message || "Unbekannter Fehler"} (${data.lastResult || "ESP_FAIL"})`;
      }
    }
  }

  async function fetchOtaStatus() {
    const res = await fetch(`${API_BASE}/ota/status`, {cache: "no-store"});
    if (!res.ok)
      throw new Error(`OTA status ${res.status}`);
    return await res.json();
  }

  async function otaResetStatusOnController() {
    try {
      await sendCommand('ota.status.reset');
    } catch (e) {
      console.warn("OTA reset status failed", e);
    }
  }

  function stopOtaPolling() {
    if (otaUi.pollTimer) {
      clearInterval(otaUi.pollTimer);
      otaUi.pollTimer = null;
    }
  }

  function startOtaPolling() {
    stopOtaPolling();
    otaPollingActive = true;

    otaUi.completionHandled = false;

    otaUi.pollTimer = setInterval(async () => {
      try {
        const data = await fetchOtaStatus();
        updateOtaUi(data);

        if (!data.running && (data.stage === "done" || data.stage === "error")) {
          stopOtaPolling();
          otaPollingActive = false;
        }
      } catch (e) {
        console.warn("OTA polling failed", e);
      }
    }, 1000);
  }

  async function closeOtaPopup() {
    stopOtaPolling();
    hideOtaModal();
    otaPollingActive = false;
    otaUi.seenRunning = false;
    otaUi.completionHandled = false;
    otaResetBars();
    await otaResetStatusOnController();
  }

  otaCloseBtn.addEventListener("click", closeOtaPopup);
  otaOkBtn.addEventListener("click", closeOtaPopup);

  otaRestartBtn.addEventListener("click", async () => {
    try {
      await sendCommand('system.restart', {reason: 'ota'});
    } catch (e) {
      console.error(e);
      alert("Fehler beim Neustart.");
    }
  });

  // Gerätesteuerung
  const displayToggle = document.getElementById("displayToggle"),
        ResetButton = document.getElementById("resetBtn"),
        ACPButton = document.getElementById("acpBtn"),
        DateButton = document.getElementById("dateBtn"),
        WeatherButton = document.getElementById("weatherBtn"),
        StartValuesButton = document.getElementById("startValue"),
        ResetStartValuesButton = document.getElementById("rstValue"),
        tickerToggle = document.getElementById("tickerToggle"),
        timeLimitToggle = document.getElementById("timeLimitToggle"),
        silentToggle = document.getElementById("silentToggle"),
        PWMToggle = document.getElementById("PWMToggle"),
        weatherToggle = document.getElementById("weatherToggle"),
        brightnessToggle = document.getElementById("brightnessToggle"),
        brightnessSlider = document.getElementById("brightnessSlider"),
        brightnessValue = document.getElementById("brightnessValue"),
        brightnessContainer = document.querySelector('.brightness-slider-setting'),
        firmwareSelect = document.getElementById("firmwareSelect"),
        applyFirmware = document.getElementById("applyFirmware"),
        zipInput = document.getElementById("zipInput"),
        applyZip = document.getElementById("applyZip"),
        timeLimitSettings = document.getElementById("timeLimitSettings"),
        timeLimitFromInput = document.getElementById("timeLimitFromInput"),
        timeLimitToInput = document.getElementById("timeLimitToInput"),
        setOTA = document.getElementById("otaUpdateButton"),
        timezoneSelect = document.getElementById("timezoneSelect"),
        applyTimezoneButton = document.getElementById("applyTimezoneSelect"),
        timerToggle = document.getElementById("timerToggle"),
        timerTimeInput = document.getElementById("timerTimeInput"),
        timerSettings = document.getElementById("timerSettings"),
        alarmToggle = document.getElementById("alarmToggle"),
        alarmTimeInput = document.getElementById("alarmTimeInput"),
        alarmSettings = document.getElementById("alarmSettings"),
        cricketToggle = document.getElementById("cricketToggle"),
        CricketButton = document.getElementById("cricketButton"),
        CheckUpdate = document.getElementById("checkUpdate"),
        noACPatNightToggle = document.getElementById("noACPatNightToggle"),
        brightnessNightStartInput = document.getElementById("brightnessNightStartInput"),
        brightnessNightEndInput = document.getElementById("brightnessNightEndInput"),
        brightnessDimStartInput = document.getElementById("brightnessDimStartInput"),
        brightnessDimEndInput = document.getElementById("brightnessDimEndInput"),
        brightnessNightValueInput = document.getElementById("brightnessNightValueInput"),
        brightnessDimValueInput = document.getElementById("brightnessDimValueInput"),
        brightnessDayValueInput = document.getElementById("brightnessDayValueInput"),
        applyBrightnessConfig = document.getElementById("applyBrightnessConfig");

  const tzNames =
      ["CET", "EET", "WET", "UTC", "EST", "CST", "MST", "PST", "HST", "JST", "IST", "AEST", "AWST"];

  [brightnessNightStartInput, brightnessNightEndInput, brightnessDimStartInput,
   brightnessDimEndInput, brightnessNightValueInput, brightnessDimValueInput,
   brightnessDayValueInput]
      .forEach(input => {
        if (!input)
          return;

        input.addEventListener("focus", () => holdSkipSync(input.id, 30000));
        input.addEventListener("input", () => holdSkipSync(input.id, 30000));
        input.addEventListener("change", () => holdSkipSync(input.id, 30000));
      });

  // Anzeige separat behandeln
  displayToggle.addEventListener("change", async () => {
    if (displayToggle.checked && !lastData.loadDetected) {
      alert("Keine Last erkannt – Display bleibt aus.");
      displayToggle.checked = false;
      return;
    }
    const enabled = displayToggle.checked;
    try {
      await sendStateCommand(displayToggle, 'DisplayEnabled', enabled, 'display.set', {enabled});
    } catch (e) {
      console.error(e);
    }
  });

  // Normale Schalter
  function makeToggleHandler(el, stateKey, command) {
    el.addEventListener("change", async () => {
      const enabled = el.checked;
      try {
        await sendStateCommand(el, stateKey, enabled, command, {enabled});
      } catch (e) {
        console.error(e);
      }
    });
  }

  makeToggleHandler(tickerToggle, "tickerEnabled", "ticker.set");
  makeToggleHandler(timeLimitToggle, "timeLimitEnabled", "time_limit.set");
  makeToggleHandler(silentToggle, "silentModeEnabled", "silent.set");
  makeToggleHandler(noACPatNightToggle, "noACPatNight", "acp_night.set");
  makeToggleHandler(weatherToggle, "WeatherUpdateEnabled", "weather.set");
  makeToggleHandler(PWMToggle, "NixiePWM", "pwm.set");
  makeToggleHandler(cricketToggle, "cricketSoundEnabled", "cricket.set");

  // Neustart
  ResetButton.addEventListener("click", async () => {
    try {
      await sendCommand('system.restart', {reason: 'user'});
    } catch (e) {
      console.error(e);
      alert("Fehler beim Zurücksetzen des Geräts.");
    }
  });

  // Röhrenreinigung
  ACPButton.addEventListener("click", async () => {
    try {
      await sendCommand('display.acp');
    } catch (e) {
      console.error(e);
      alert("Fehler beim Auslösen des ACP-Events.");
    }
  });

  // Datum
  DateButton.addEventListener("click", async () => {
    try {
      await sendCommand('display.date');
    } catch (e) {
      console.error(e);
      alert("Fehler beim Setzen des Datums.");
    }
  });

  // Wetter
  WeatherButton.addEventListener("click", async () => {
    try {
      await sendCommand('display.weather');
    } catch (e) {
      console.error(e);
      alert("Fehler beim Abrufen der Wetterdaten.");
    }
  });

  // Grillenzirpen
  CricketButton.addEventListener("click", async () => {
    try {
      await sendCommand('sound.cricket');
    } catch (e) {
      console.error(e);
      alert("Fehler beim Setzen des Cricket-Sounds.");
    }
  });

  // Updateprüfung
  CheckUpdate.addEventListener("click", async () => {
    try {
      await sendCommand('update.check');
    } catch (e) {
      console.error(e);
      alert("Fehler beim Überprüfen auf Updates.");
    }
  });

  StartValuesButton.addEventListener("click", async () => {
    try {
      await sendCommand('config.reload');
    } catch (e) {
      console.error(e);
      alert("Fehler beim Setzen der Startwerte.");
    }
  });

  ResetStartValuesButton.addEventListener("click", async () => {
    try {
      await sendCommand('config.reset');
    } catch (e) {
      console.error(e);
      alert("Fehler beim Zurücksetzen der Startwerte.");
    }
  });

  timerToggle.addEventListener("change", async () => {
    const enabled = timerToggle.checked;
    const [hh, mm, ss] = timerTimeInput.value.split(":").map(Number);
    const totalSeconds = (hh || 0) * 3600 + (mm || 0) * 60 + (ss || 0);

    try {
      await sendStateCommand(
          timerToggle, 'TimerActive', enabled, 'timer.set',
          enabled ? {enabled, seconds: totalSeconds} : {enabled});
    } catch (e) {
      console.error(e);
    }
  });

  timerTimeInput.addEventListener("change", async () => {
    if (timerToggle.checked) {
      const [hh, mm, ss] = timerTimeInput.value.split(":").map(Number);
      const totalSeconds = (hh || 0) * 3600 + (mm || 0) * 60 + (ss || 0);

      try {
        await sendStateCommand(
            timerTimeInput, 'TimerConfiguredSeconds', totalSeconds, 'timer.set',
            {enabled: true, seconds: totalSeconds});
      } catch (e) {
        console.error(e);
      }
    }
  });

  applyBrightnessConfig.addEventListener("click", async () => {
    try {
      [brightnessNightStartInput, brightnessNightEndInput, brightnessDimStartInput,
       brightnessDimEndInput, brightnessNightValueInput, brightnessDimValueInput,
       brightnessDayValueInput]
          .forEach(input => {
            if (input)
              holdSkipSync(input.id, 30000);
          });

      const requests = [
        sendCommand(
            'brightness.schedule',
            {field: 'nightStart', value: Number(brightnessNightStartInput.value)}),
        sendCommand(
            'brightness.schedule',
            {field: 'nightEnd', value: Number(brightnessNightEndInput.value)}),
        sendCommand(
            'brightness.schedule',
            {field: 'dimStart', value: Number(brightnessDimStartInput.value)}),
        sendCommand(
            'brightness.schedule', {field: 'dimEnd', value: Number(brightnessDimEndInput.value)}),
        sendCommand(
            'brightness.schedule',
            {field: 'nightValue', value: Number(brightnessNightValueInput.value)}),
        sendCommand(
            'brightness.schedule',
            {field: 'dimValue', value: Number(brightnessDimValueInput.value)}),
        sendCommand(
            'brightness.schedule',
            {field: 'dayValue', value: Number(brightnessDayValueInput.value)})
      ];

      const results = await Promise.all(requests);
      const failed = results.find(r => !r.ok);

      if (failed) {
        alert("Mindestens ein Brightness-Wert konnte nicht gespeichert werden.");
        return;
      }

      await fetchInfo();
    } catch (e) {
      console.error(e);
      alert("Fehler beim Speichern der automatischen Helligkeit.");
    }
  });

  alarmToggle.addEventListener("change", async () => {
    const enabled = alarmToggle.checked;
    const [hh, mm] = alarmTimeInput.value.split(":").map(Number);
    const timeStr = `${hh.toString().padStart(2, '0')}:${mm.toString().padStart(2, '0')}`;

    try {
      await sendStateCommand(
          alarmToggle, 'AlarmActive', enabled, 'alarm.set',
          enabled ? {enabled, time: timeStr} : {enabled});
    } catch (e) {
      console.error(e);
    }
  });

  alarmTimeInput.addEventListener("change", async () => {
    if (alarmToggle.checked) {
      const [hh, mm] = alarmTimeInput.value.split(":").map(Number);
      const timeStr = `${hh.toString().padStart(2, '0')}:${mm.toString().padStart(2, '0')}`;

      try {
        await sendStateCommand(
            alarmTimeInput, 'AlarmTime', timeStr, 'alarm.set', {enabled: true, time: timeStr});
      } catch (e) {
        console.error(e);
      }
    }
  });

  [timeLimitFromInput, timeLimitToInput].forEach(input => {
    input.addEventListener("change", async () => {
      const from = timeLimitFromInput.value;
      const to = timeLimitToInput.value;
      const stateKey = input === timeLimitFromInput ? 'timeLimitFrom' : 'timeLimitTo';
      const expected = input.value;
      try {
        await sendStateCommand(
            input, stateKey, expected, 'time_limit.set',
            {enabled: timeLimitToggle.checked, from, to});
      } catch (e) {
        console.error(e);
      }
    });
  });

  tzNames.forEach((name, i) => {
    const opt = document.createElement("option");
    opt.value = String(i);
    opt.text = name;
    timezoneSelect.add(opt);
  });

  // Abgleich kurz pausieren
  timezoneSelect.addEventListener("focus", () => skipSync.add("timezoneSelect"));
  timezoneSelect.addEventListener("blur", () => skipSync.delete("timezoneSelect"));

  // Zeitfenster übernehmen
  applyTimezoneButton.addEventListener("click", async () => {
    const timezone = Number(timezoneSelect.value);
    try {
      await sendStateCommand(
          timezoneSelect, 'CurrentTimeZoneIndex', timezone, 'timezone.set', {timezone});
    } catch (e) {
      console.error("Error setting timezone:", e);
      alert("Fehler beim Setzen der Zeitzone.");
    }
  });

  // Helligkeit
  function syncBrightnessUI(manualEnabled, val) {
    brightnessToggle.checked = manualEnabled;
    brightnessContainer.style.display = manualEnabled ? "flex" : "none";

    brightnessSlider.value = val;
    brightnessValue.textContent = val;
  }

  // Beim Bedienen nicht überschreiben
  brightnessSlider.addEventListener('mousedown', () => holdSkipSync('brightnessSlider', 5000));
  brightnessSlider.addEventListener('touchstart', () => holdSkipSync('brightnessSlider', 5000));
  brightnessToggle.addEventListener('mousedown', () => holdSkipSync('brightnessToggle', 5000));
  brightnessToggle.addEventListener('touchstart', () => holdSkipSync('brightnessToggle', 5000));

  brightnessToggle.addEventListener("change", async () => {
    skipSync.delete("brightnessToggle");
    const enabled = brightnessToggle.checked;
    try {
      await sendStateCommand(
          brightnessToggle, 'manualBrightnessEnabled', enabled, 'brightness.manual',
          {enabled, brightness: Number(brightnessSlider.value)});
    } catch (error) {
      console.error(error);
    }
  });

  brightnessSlider.addEventListener("input", () => {
    brightnessValue.textContent = brightnessSlider.value;
  });

  brightnessSlider.addEventListener("change", async () => {
    skipSync.delete("brightnessSlider");
    const brightness = Number(brightnessSlider.value);
    try {
      await sendStateCommand(
          brightnessSlider, 'Brightness', brightness, 'brightness.manual',
          {enabled: brightnessToggle.checked, brightness});
    } catch (error) {
      console.error(error);
    }
  });

  zipInput.addEventListener('focus', () => skipSync.add('zipInput'));
  zipInput.addEventListener('blur', () => skipSync.delete('zipInput'));

  firmwareSelect.addEventListener('focus', () => skipSync.add('firmwareSelect'));
  firmwareSelect.addEventListener('blur', () => skipSync.delete('firmwareSelect'));

  timeLimitFromInput.addEventListener('focus', () => skipSync.add('timeLimitFrom'));
  timeLimitToInput.addEventListener('focus', () => skipSync.add('timeLimitTo'));

  alarmTimeInput.addEventListener('focus', () => skipSync.add('alarmTimeInput'));
  alarmTimeInput.addEventListener('blur', () => skipSync.delete('alarmTimeInput'));

  timerTimeInput.addEventListener('focus', () => skipSync.add('timerTimeInput'));
  timerTimeInput.addEventListener('blur', () => skipSync.delete('timerTimeInput'));

  // Firmware und Standort
  applyFirmware.addEventListener("click", async () => {
    const target = firmwareSelect.value;
    await sendStateCommand(firmwareSelect, 'Firmware_Target', target, 'firmware.set', {target});
  });

  applyZip.addEventListener("click", async () => {
    const zip = zipInput.value;
    await sendStateCommand(zipInput, 'zipCode', zip, 'zip.set', {zip});
  });

  // Logstufen
  // Bit positions are kept stable for persisted configurations.  The first
  // nine entries are the legacy mask and must not be renumbered.
  const logControls = [
    ["logHttpToggle", 0], ["logTimeToggle", 1], ["logHssToggle", 2],
    ["logDigitToggle", 3], ["logWebserverToggle", 4], ["logOtaToggle", 5],
    ["logButtonToggle", 6], ["logGeneralToggle", 7], ["logWifiToggle", 8],
    ["logStorageToggle", 9], ["logSensorToggle", 10], ["logPowerToggle", 11],
    ["logHistoryToggle", 12], ["logWeatherToggle", 13], ["logSystemToggle", 14],
    ["logAudioToggle", 15]
  ];
  const logIds = logControls.map(([id]) => id);

  logIds.forEach(id => {
    const cb = document.getElementById(id);
    cb.addEventListener("change", async () => {
      const value = logControls.reduce((mask, [controlId, bit]) =>
        mask | (document.getElementById(controlId).checked ? (1 << bit) : 0), 0);
      await sendStateCommand(
          logIds, 'logConfig', value, 'logging.set', {value});
    });
  });

  // OTA starten
  setOTA.addEventListener("click", async () => {
    try {
      otaUi.seenRunning = false;
      otaUi.completionHandled = false;
      otaResetBars();
      otaSetLiveView();
      showOtaModal();

      const res = await sendCommand('ota.start');
      const text = await res.text();

      if (!res.ok) {
        hideOtaModal();
        alert(`Fehler beim Starten des OTA-Updates: ${text}`);
        return;
      }

      startOtaPolling();
    } catch (e) {
      console.error(e);
      hideOtaModal();
      alert("Fehler beim Starten des OTA-Updates.");
    }
  });

  // WLAN
  const wifiListEl = document.getElementById('wifiList');
  const wifiRefreshBtn = document.getElementById('wifiRefreshBtn');
  const wifiAddBtn = document.getElementById('wifiAddBtn');
  const wifiSsidInput = document.getElementById('wifiSsidInput');
  const wifiPwdInput = document.getElementById('wifiPwdInput');

  let wifiData = [];  // [{ssid,priority,last_ok}, ...]
  let draggingEl = null;
  let reorderTimer = null;

  async function saveWifiNetwork(ssid, password, priority) {
    return apiJson('/wifi/networks', 'PUT', {ssid, password, priority});
  }
  const htmlEsc = s => (s || "").replace(
      /[&<>"']/g, c => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;"}[c]));

  async function loadWifiList() {
    try {
      const res = await fetch(`${API_BASE}/wifi/networks`, {cache: 'no-store'});
      if (!res.ok)
        throw new Error(res.status);
      wifiData = await res.json();
      renderWifiList();
    } catch (e) {
      console.error('loadWifiList failed', e);
    }
  }

  function renderWifiList() {
    wifiListEl.innerHTML = "";
    const arr = [...wifiData].sort((a, b) => (a.priority | 0) - (b.priority | 0));
    arr.forEach((item, idx) => wifiListEl.appendChild(createWifiItem(item, idx)));
  }

  function createWifiItem(item, idx) {
    const li = document.createElement('li');
    li.className = 'wifi-item';
    li.draggable = true;
    li.dataset.ssid = item.ssid;
    li.dataset.priority = item.priority;

    li.innerHTML = `
    <div class="drag" title="ziehen">≡</div>
    <div class="main">
      <div class="ssid">${htmlEsc(item.ssid || '')}</div>
      <div class="meta">Prio: <span class="prio">${item.priority}</span>${
        item.last_ok ? ` · last_ok: ${item.last_ok}` : ''}</div>
    </div>
    <div class="actions">
      <button class="del" title="Entfernen">✖</button>
    </div>
  `;

    // Netzwerk löschen
    li.querySelector('.del').addEventListener('click', async () => {
      const ssid = li.dataset.ssid;
      if (!confirm(`Netzwerk „${ssid}“ entfernen?`))
        return;
      try {
        const res = await apiJson('/wifi/networks', 'DELETE', {ssid});
        if (!res.ok)
          throw new Error(res.status);
        // Liste lokal aktualisieren
        wifiData = wifiData.filter(x => x.ssid !== ssid);
        renderWifiList();
        loadWifiList();
      } catch (e) {
        alert('Löschen fehlgeschlagen');
        console.error(e);
      }
    });

    // Sortieren
    li.addEventListener('dragstart', e => {
      draggingEl = li;
      li.classList.add('dragging');
      e.dataTransfer.effectAllowed = 'move';
    });
    li.addEventListener('dragend', () => {
      if (draggingEl)
        draggingEl.classList.remove('dragging');
      draggingEl = null;
      // Speichern leicht verzögern
      if (reorderTimer)
        clearTimeout(reorderTimer);
      reorderTimer = setTimeout(saveOrderToServer, 250);
    });
    li.addEventListener('dragover', e => {
      e.preventDefault();
      const after = getDragAfterElement(wifiListEl, e.clientY);
      if (after == null) {
        wifiListEl.appendChild(draggingEl);
      } else {
        wifiListEl.insertBefore(draggingEl, after);
      }
    });

    return li;
  }

  // Einfügeposition finden
  function getDragAfterElement(container, y) {
    const els = [...container.querySelectorAll('.wifi-item:not(.dragging)')];
    let closest = {offset: Number.NEGATIVE_INFINITY, element: null};
    els.forEach(el => {
      const box = el.getBoundingClientRect();
      const offset = y - box.top - box.height / 2;
      if (offset < 0 && offset > closest.offset)
        closest = {offset, element: el};
    });
    return closest.element;
  }

  // Reihenfolge speichern
  async function saveOrderToServer() {
    const lis = [...wifiListEl.querySelectorAll('.wifi-item')];
    // Lokale Prioritäten
    wifiData = lis.map((li, i) => ({
                         ssid: li.dataset.ssid,
                         priority: i,
                         last_ok: (wifiData.find(x => x.ssid === li.dataset.ssid)?.last_ok) || 0
                       }));
    // Anzeige aktualisieren
    lis.forEach((li, i) => {
      li.dataset.priority = String(i);
      const pr = li.querySelector('.prio');
      if (pr)
        pr.textContent = String(i);
    });

    // Nacheinander speichern
    for (let i = 0; i < wifiData.length; i++) {
      const n = wifiData[i];
      try {
        const res = await saveWifiNetwork(n.ssid, '', n.priority);
        if (!res.ok)
          console.warn('prio save failed for', n.ssid, res.status);
      } catch (e) {
        console.warn('prio save error for', n.ssid, e);
      }
    }
  }

  // Netzwerk hinzufügen
  wifiAddBtn.addEventListener('click', async () => {
    const ssid = (wifiSsidInput.value || "").trim();
    const pwd = wifiPwdInput.value || "";
    if (!ssid) {
      alert('Bitte SSID eingeben');
      return;
    }
    const prio = wifiListEl.children.length;  // ans Ende

    try {
      wifiAddBtn.disabled = true;
      const res = await saveWifiNetwork(ssid, pwd, prio);
      if (!res.ok)
        throw new Error(res.status);
      wifiSsidInput.value = "";
      wifiPwdInput.value = "";
      await loadWifiList();
    } catch (e) {
      alert('Hinzufügen fehlgeschlagen');
      console.error(e);
    } finally {
      wifiAddBtn.disabled = false;
    }
  });

  wifiRefreshBtn.addEventListener('click', loadWifiList);

  // Telemetrie und Oberfläche
  async function fetchInfo(pushedData = null, pushedRevision = null) {
    try {
      let data = pushedData;
      let revision = Number.isFinite(Number(pushedRevision)) && pushedRevision !== null ?
          Number(pushedRevision) :
          null;
      if (!data) {
        const res = await fetch(`${API_BASE}/status`, {cache: 'no-store'});
        if (!res.ok)
          throw new Error(res.status);
        const revisionHeader = res.headers.get('X-AppState-Revision');
        if (revisionHeader !== null) {
          const responseRevision = Number(revisionHeader);
          if (Number.isFinite(responseRevision))
            revision = responseRevision;
        }
        data = await res.json();
      }
      if (revision != null)
        lastStateRevision = Math.max(lastStateRevision, revision);
      data = applyPendingState(data, revision);
      lastData = data;

      // Werte anzeigen
      const map = {
        Chip: 'chipModel',
        SSID: 'ssid',
        Melody: 'melody',
        FreeHeap: 'freeHeap',
        CoreLoad0: 'coreLoad0',
        CoreLoad1: 'coreLoad1',
        TotalLoad: 'totalLoad',
        ChipId: 'chipId',
        FlashSize: 'flashSize',
        FlashSpeed: 'flashSpeed',
        SketchSize: 'sketchSize',
        SketchFreeSpace: 'sketchFreeSpace',
        CpuFrequencyMHz: 'cpuFreq',
        SdkVersion: 'sdkVersion',
        Autor: 'autor',
        Voltage_12V: 'voltage_12V',
        Voltage_5V: 'voltage_5V',
        Voltage_3V3: 'voltage_3V3',
        Voltage_18V: 'voltage_18V',
        Voltage_UHSS: 'voltage_UHSS',
        Current_mA: 'current_mA',
        Power_W: 'power_W',
        Energy_Wh: 'energy_Wh',
        HSS_Enabled: 'HSS_Enabled',
        HSS_190V: 'HSS_190V',
        HSS_Resistor: 'HSS_Resistor',
        CaseTemperature_C: 'temperature',
        Firmware_Target: 'firmwareTarget',
        Hardware_Version: 'hardwareVersion',
        Software_Version: 'softwareVersion',
        NixiePWM: 'NixiePWM',
        PWM_Frequenzy: 'PWM_Frequenzy',
        manualBrightnessEnabled: 'manualBrightnessEnabled',
        loadDetected: 'loadDetected',
        Brightness: 'brightness',
        brightnessNightStartHour: 'brightnessNightStartHour',
        brightnessNightEndHour: 'brightnessNightEndHour',
        brightnessDimStartHour: 'brightnessDimStartHour',
        brightnessDimEndHour: 'brightnessDimEndHour',
        brightnessNightValue: 'brightnessNightValue',
        brightnessDimValue: 'brightnessDimValue',
        brightnessDayValue: 'brightnessDayValue',
        timeLimitFrom: 'timeLimitFrom',
        timeLimitTo: 'timeLimitTo',
        noACPatNight: 'noACPatNight',
        DisplayEnabled: 'DisplayEnabled',
        tickerEnabled: 'tickerEnabled',
        timeLimitEnabled: 'timeLimitEnabled',
        silentModeEnabled: 'silentModeEnabled',
        WeatherUpdateEnabled: 'WeatherUpdateEnabled',
        zipCode: 'zipCode',
        CurrentTimeZone: 'CurrentTimeZone',
        TimerActive: 'TimerActive',
        TimerConfiguredSeconds: 'TimerConfiguredSeconds',
        AlarmActive: 'AlarmActive',
        AlarmTime: 'AlarmTime'
      };

      const boolFields = [
        'DisplayEnabled', 'tickerEnabled', 'timeLimitEnabled', 'silentModeEnabled',
        'WeatherUpdateEnabled', 'manualBrightnessEnabled', 'NixiePWM', 'TimerActive', 'AlarmActive',
        'loadDetected', 'noACPatNight'
      ];

      Object.entries(map).forEach(([k, id]) => {
        const el = document.getElementById(id);
        if (!el || skipSync.has(id))
          return;

        let val = data[k];

        if (boolFields.includes(k)) {
          val = val == 1 ? 'on' : 'off';
        }

        if (k === "brightnessNightStartHour" || k === "brightnessNightEndHour" ||
            k === "brightnessDimStartHour" || k === "brightnessDimEndHour") {
          val = formatHourMinute(val);
        }

        el.textContent = val;
      });

      // Helligkeit abgleichen
      if (!skipSync.has("brightnessSlider") && !skipSync.has("brightnessToggle")) {
        syncBrightnessUI(!!data.manualBrightnessEnabled, data.Brightness);
      }

      if (!skipSync.has("brightnessNightStartInput") && brightnessNightStartInput) {
        brightnessNightStartInput.value = data.brightnessNightStartHour ?? 22;
      }
      if (!skipSync.has("brightnessNightEndInput") && brightnessNightEndInput) {
        brightnessNightEndInput.value = data.brightnessNightEndHour ?? 6;
      }
      if (!skipSync.has("brightnessDimStartInput") && brightnessDimStartInput) {
        brightnessDimStartInput.value = data.brightnessDimStartHour ?? 20;
      }
      if (!skipSync.has("brightnessDimEndInput") && brightnessDimEndInput) {
        brightnessDimEndInput.value = data.brightnessDimEndHour ?? 8;
      }
      if (!skipSync.has("brightnessNightValueInput") && brightnessNightValueInput) {
        brightnessNightValueInput.value = data.brightnessNightValue ?? 15;
      }
      if (!skipSync.has("brightnessDimValueInput") && brightnessDimValueInput) {
        brightnessDimValueInput.value = data.brightnessDimValue ?? 75;
      }
      if (!skipSync.has("brightnessDayValueInput") && brightnessDayValueInput) {
        brightnessDayValueInput.value = data.brightnessDayValue ?? 100;
      }

      if (updateBannerBadge) {
        const rawUpdateValue =
            data.updateAvailable ?? data.UpdateAvailable ?? data.update_available ?? 0;

        const hasUpdate = rawUpdateValue === 1 || rawUpdateValue === "1" ||
            rawUpdateValue === true || rawUpdateValue === "true";

        updateBannerBadge.classList.toggle("hidden", !hasUpdate);
      }

      const tzDisplay = document.getElementById("CurrentTimeZone");
      if (tzDisplay && data.CurrentTimeZone != null) {
        const idx = parseInt(data.CurrentTimeZone, 10);
        tzDisplay.textContent = tzNames[idx] || "–";
      }

      // Livewerte anhängen
      appendLiveChartSample(data, false);
      if (!activeHistoryLoad)
        updateChartTimeWindow();

      // Schalter abgleichen
      if (!skipSync.has("displayToggle"))
        displayToggle.checked = !!data.DisplayEnabled;
      if (!skipSync.has("tickerToggle"))
        tickerToggle.checked = !!data.tickerEnabled;
      if (!skipSync.has("timeLimitToggle"))
        timeLimitToggle.checked = !!data.timeLimitEnabled;
      if (!skipSync.has("silentToggle"))
        silentToggle.checked = !!data.silentModeEnabled;
      if (!skipSync.has("noACPatNightToggle"))
        noACPatNightToggle.checked = !!data.noACPatNight;
      if (!skipSync.has("weatherToggle"))
        weatherToggle.checked = !!data.WeatherUpdateEnabled;
      if (!skipSync.has("PWMToggle"))
        PWMToggle.checked = !!data.NixiePWM;
      if (!skipSync.has("timerToggle"))
        timerToggle.checked = !!data.TimerActive;
      if (!skipSync.has("alarmToggle"))
        alarmToggle.checked = !!data.AlarmActive;
      if (!skipSync.has("cricketToggle"))
        cricketToggle.checked = !!data.cricketSoundEnabled;

      // Zeitzone abgleichen
      if (!skipSync.has("timezoneSelect") && data.CurrentTimeZoneIndex != null) {
        timezoneSelect.value = String(data.CurrentTimeZoneIndex);
      }

      // Logstufen abgleichen
      const configuredLogMask = Number.isInteger(Number(data.logConfig))
        ? Number(data.logConfig)
        : parseInt(data.logConfigBinary || '0', 2);
      logControls.forEach(([id, bit]) => {
        if (!skipSync.has(id))
          document.getElementById(id).checked = (configuredLogMask & (1 << bit)) !== 0;
      });

      // Firmware und Standort abgleichen
      if (!skipSync.has("firmwareSelect"))
        firmwareSelect.value = data.Firmware_Target;
      if (!skipSync.has("zipInput"))
        zipInput.value = data.zipCode;

      timeLimitSettings.style.display = data.timeLimitEnabled ? "flex" : "none";

      if (!skipSync.has("timerTimeInput")) {
        const totalSec = data.TimerConfiguredSeconds || 0;
        const h = String(Math.floor(totalSec / 3600)).padStart(2, "0");
        const m = String(Math.floor((totalSec % 3600) / 60)).padStart(2, "0");
        const s = String(totalSec % 60).padStart(2, "0");
        timerTimeInput.value = `${h}:${m}:${s}`;
      }

      if (!skipSync.has("alarmTimeInput")) {
        const alarmTime = data.AlarmTime || "06:00";
        alarmTimeInput.value = alarmTime.length === 5 ? alarmTime : "06:00";
      }

      if (data.timeLimitEnabled) {
        if (!skipSync.has("timeLimitFrom")) {
          timeLimitFromInput.value = data.timeLimitFrom || "00:00:00";
        }
        if (!skipSync.has("timeLimitTo")) {
          timeLimitToInput.value = data.timeLimitTo || "00:00:00";
        }
      }

    } catch (e) {
      console.warn("Info polling error:", e);
    }
  }

  async function pollInfo() {
    try {
      if (!otaPollingActive && !infoStreamConnected) {
        await fetchInfo();
      }
    } catch (e) {
      console.warn("pollInfo error:", e);
    } finally {
      setTimeout(pollInfo, 2000);
    }
  }

  function startInfoStream() {
    if (!("EventSource" in window))
      return;

    infoEventSource = new EventSource(`${API_BASE}/events`);
    infoEventSource.onopen = () => {
      infoStreamConnected = true;
    };
    infoEventSource.addEventListener('info', event => {
      infoStreamConnected = true;
      if (otaPollingActive)
        return;
      try {
        const revision = Number(event.lastEventId);
        void fetchInfo(JSON.parse(event.data), Number.isFinite(revision) ? revision : null);
      } catch (error) {
        console.warn('Invalid telemetry event:', error);
      }
    });
    infoEventSource.onerror = () => {
      infoStreamConnected = false;
    };

    window.addEventListener('beforeunload', () => infoEventSource?.close(), {once: true});
  }

  initCharts();
  void loadChartHistory(timeRangeSelect.value);
  startInfoStream();
  pollInfo();
  loadWifiList();
  fetchOtaStatus();

  (async () => {
    try {
      const data = await fetchOtaStatus();
      if (data.running || data.stage === "done" || data.stage === "error") {
        updateOtaUi(data);
        if (data.running) {
          startOtaPolling();
        }
      }
    } catch (e) {
      console.warn("Initial OTA status fetch failed", e);
    }
  })();
});
