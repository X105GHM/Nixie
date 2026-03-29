document.addEventListener("DOMContentLoaded", () => {
  const skipSync = new Set();
  let lastData = {};
  let otaPollingActive = false;

  // === Netzwerk-Logger ===========================================
  const netLogEl = document.getElementById("networkLog");
  const clearBtn = document.getElementById("clearNetworkLog");
  const trimCheckbox = document.getElementById("logTrimToggle");
  const logToUI = msg => {
    if (!netLogEl) return;

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

  // Enter → Send Command
  const sendBtn = document.getElementById("sendCommand"),
    inputEl = document.getElementById("manualCommand");
  inputEl.addEventListener("keydown", e => {
    if (e.key === "Enter") {
      e.preventDefault();
      sendBtn.click();
    }
  });

  // fetch umwickeln, um Protokollierung hinzuzufügen
  const _fetch = window.fetch;
  window.fetch = async (input, init) => {
    const url = typeof input === "string" ? input : input.url,
      method = init?.method || "GET",
      start = Date.now();
    logToUI(`→ ${method} ${url}`);
    try {
      const res = await _fetch(input, init),
        dur = Date.now() - start;
      logToUI(`← ${res.status} ${url} (${dur}ms)`);
      return res;
    } catch (err) {
      const dur = Date.now() - start;
      logToUI(`!! ${method} ${url} ERROR (${dur}ms): ${err}`);
      throw err;
    }
  };

  // === Kurz-Syntax Command-Parser ================================
  sendBtn.addEventListener("click", () => {
    const txt = inputEl.value.trim();
    if (!txt) return;

    const parts = txt.split('/');
    let url = null;

    if (parts[0] === 'set') {
      switch (parts[1]) {
        case 'ON':
        case 'OFF':
        case 'reset':
        case 'setOldValue':
        case 'resetValue':
        case 'ACP':
        case 'tempDisplay':
        case 'DATE':
          url = `/set/${parts[1]}`;
          break;

        case 'zip':
          if (parts[2]) url = `/set/zip?zip=${encodeURIComponent(parts[2])}`;
          break;

        case 'ticker':
        case 'silentMode':
        case 'weatherUpdate':
          if (parts[2] === '0' || parts[2] === '1') {
            url = `/set/${parts[1]}?value=${parts[2]}`;
          }
          break;

        case 'timeLimit':
          if (parts[2] === '0' || parts[2] === '1') {
            url = `/set/timeLimit?value=${parts[2]}`;
            if (parts[3] && parts[4]) {
              url += `&from=${encodeURIComponent(parts[3])}&to=${encodeURIComponent(parts[4])}`;
            }
          }
          break;

        case 'manualBrightness':
          if (parts[2]) {
            url = `/set/manualBrightness?value=${encodeURIComponent(parts[2])}`;
            if (parts[3]) {
              url += `&brightness=${encodeURIComponent(parts[3])}`;
            }
          }
          break;

        case 'logConfig':
          if (parts[2] && /^\d+$/.test(parts[2])) {
            url = `/set/logConfig?value=${parts[2]}`;
          }
          break;

        case 'firmware':
          if (parts[2]) {
            url = `/set/firmware?target=${encodeURIComponent(parts[2])}`;
          }
          break;

        case 'ota':
          url = `/set/ota`;
          break;

        case 'pwmPeriod':
          if (parts[2] && /^\d+$/.test(parts[2])) {
            url = `/set/PWMPeriod?value=${parts[2]}`;
          }
          break;

        case 'brownout':
          url = '/set/brownout';
          break;
      }
    }
    else if (parts[0] === 'get') {

      switch (parts[1]) {
        case 'info':
          url = `/get/info`;
          break;

        case 'brownout':
          url = `/get/brownout`;
          break;

        case 'taskStats':
          url = `/get/taskStats`;
          break;
      }
    }

    if (url) {
      fetch(url)
        .then(async res => {
          const body = await res.text();
          if (body) logToUI(`   ▶ body: ${body}`);
        })
        .catch(err => logToUI(`!! fetch ${url} ERROR: ${err}`));
      inputEl.value = '';
    } else {
      logToUI(`Ungültiger Befehl: ${txt}`);
    }
  });

  // === Navigation & Tabs ===========================================
  const tabs = document.querySelectorAll("nav .menu li"),
    mobileTabs = document.querySelectorAll("#mobileMenu li"),
    tabContents = document.querySelectorAll(".tab-content"),
    hamburger = document.getElementById("hamburger"),
    mobileMenu = document.getElementById("mobileMenu");

  function activateTab(id) {
    tabs.forEach(t => t.classList.toggle("active", t.dataset.tab === id));
    mobileTabs.forEach(t => t.classList.toggle("active", t.dataset.tab === id));
    tabContents.forEach(sec => sec.id === id
      ? sec.classList.add("active")
      : sec.classList.remove("active")
    );
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

  // === Live Clock =======================================================
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

  // === Stats Monitor ======================================================
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

  const statsMonitor = {
    enabled: true,
    timer: null,
    data: null,
    displayUnit: "ms"
  };

  function statsMonitorAvailable() {
    return !!(
      statsMonitorEls.barCanvas &&
      statsMonitorEls.pie0 &&
      statsMonitorEls.pie1
    );
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
    return { ctx, w: cssW, h: cssH };
  }

  function statsSum(arr) {
    return arr.reduce((a, b) => a + b, 0);
  }

  function statsUnitSymbol(u) {
    if (u === "µs") return "µs";
    if (u === "ns") return "ns";
    return "ms";
  }

  function statsConvertFromUs(us, u) {
    if (u === "ms") return us / 1000;
    if (u === "ns") return us * 1000;
    return us;
  }

  function statsFormatNumber(x) {
    const ax = Math.abs(x);
    if (ax >= 1000) return Math.round(x).toLocaleString("de-DE");
    if (ax >= 100) return (Math.round(x * 10) / 10).toString();
    return (Math.round(x * 100) / 100).toString();
  }

  function statsCompact(x) {
    const ax = Math.abs(x);
    const sign = x < 0 ? "-" : "";
    if (ax >= 1e9) return sign + (Math.round((ax / 1e9) * 100) / 100) + "G";
    if (ax >= 1e6) return sign + (Math.round((ax / 1e6) * 100) / 100) + "M";
    if (ax >= 1e3) return sign + (Math.round((ax / 1e3) * 100) / 100) + "k";
    if (ax >= 100) return sign + Math.round(ax);
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
    if (!el) return;
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
    if (!statsMonitorEls.legend) return;
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
    if (!statsMonitor.data || !statsMonitorEls.barCanvas) return;

    const palette = statsPalette();
    const tasks = Array.isArray(statsMonitor.data.tasks) ? statsMonitor.data.tasks : [];

    const minHeight = 320;
    const perTaskHeight = 34;
    const dynamicHeight = Math.max(minHeight, 70 + tasks.length * perTaskHeight);
    statsMonitorEls.barCanvas.style.height = `${dynamicHeight}px`;

    const { ctx, w, h } = fitStatsCanvas(statsMonitorEls.barCanvas);

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

    const baseFont = (w < 420)
      ? "11px ui-monospace, Menlo, Consolas, monospace"
      : "12px ui-monospace, Menlo, Consolas, monospace";

    ctx.font = baseFont;

    let ticks = 5;
    while (ticks > 2) {
      const spacing = plotW / ticks;
      const testLabel = statsCompact(maxDisp);
      const labelW = ctx.measureText(testLabel).width;
      if (spacing >= labelW + 14) break;
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
      const txt = `${statsFormatValue(valDisp)} ${statsUnitSymbol(statsMonitor.displayUnit)} • ${statsFormatPct(t.pct_total)}%`;

      ctx.fillStyle = palette.text;
      ctx.font = "11px ui-monospace, Menlo, Consolas, monospace";
      ctx.textAlign = "left";
      ctx.fillText(txt, padL + Math.min(plotW - 140, barW + 8), yCenter);
    });
  }

  function drawStatsPie(canvas, coreIndex) {
    if (!statsMonitor.data || !canvas) return;

    const palette = statsPalette();
    const tasks = (statsMonitor.data.tasks || []).filter(t => Number(t.core) === coreIndex);
    const { ctx, w, h } = fitStatsCanvas(canvas);

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
      slices.push({
        name: "Idle",
        pct: 100 - loadPct,
        color: palette.idle
      });
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

    const legendItems = slices.map(s => ({
      name: s.name,
      color: s.color,
      pct: statsFormatPct(s.pct)
    }));

    const usedDisp = statsConvertFromUs(usedUs, statsMonitor.displayUnit);
    const idleDisp = statsConvertFromUs(idleUs, statsMonitor.displayUnit);
    const uSym = statsUnitSymbol(statsMonitor.displayUnit);

    if (coreIndex === 0) {
      setStatsPieLegend(statsMonitorEls.pieLegend0, legendItems);
      if (statsMonitorEls.core0Total) {
        statsMonitorEls.core0Total.textContent =
          `Used: ${statsFormatNumber(usedDisp)} ${uSym} • Idle: ${statsFormatNumber(idleDisp)} ${uSym}`;
      }
    } else {
      setStatsPieLegend(statsMonitorEls.pieLegend1, legendItems);
      if (statsMonitorEls.core1Total) {
        statsMonitorEls.core1Total.textContent =
          `Used: ${statsFormatNumber(usedDisp)} ${uSym} • Idle: ${statsFormatNumber(idleDisp)} ${uSym}`;
      }
    }
  }

  function renderStatsMonitor() {
    if (!statsMonitorAvailable() || !statsMonitor.data) return;

    buildStatsLegend(statsMonitor.data.tasks || []);
    drawStatsBars();
    drawStatsPie(statsMonitorEls.pie0, 0);
    drawStatsPie(statsMonitorEls.pie1, 1);
  }

  async function fetchStatsMonitor() {
    if (!statsMonitorAvailable() || !statsMonitor.enabled || otaPollingActive) return;

    try {
      const res = await fetch("/get/taskStats", { cache: "no-store" });
      if (!res.ok) throw new Error(res.status);

      const data = await res.json();
      statsMonitor.data = data;

      if (statsMonitorEls.windowText) {
        statsMonitorEls.windowText.textContent = `${data.window_ms || 0} ms`;
      }

      if (statsMonitorEls.lastUpdateText) {
        statsMonitorEls.lastUpdateText.textContent = data.timestamp || new Date().toLocaleTimeString("de-DE");
      }

      renderStatsMonitor();
    } catch (e) {
      console.warn("Stats monitor error:", e);
    }
  }

  function startStatsMonitor() {
    if (!statsMonitorAvailable()) return;

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
    if (!statsMonitorEls.mainBody || !statsMonitorEls.coreCard) return;

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
    if (!Number.isFinite(n)) return "–";
    return `${String(n).padStart(2, "0")}:00`;
  }

  // === Dark/Light Mode ==================================================
  const themeToggle = document.getElementById("themeToggle");
  if (localStorage.getItem("darkMode") === "true") document.body.classList.add("dark");
  themeToggle.checked = document.body.classList.contains("dark");
  themeToggle.addEventListener("change", () => {
    document.body.classList.toggle("dark", themeToggle.checked);
    localStorage.setItem("darkMode", themeToggle.checked);
    renderRuntimeMonitor();
  });

  // === Nixie-Single-Digit-Flow unter ACP-Panel ======================
  const acpToggle = document.getElementById('acpToggle');
  const acpControls = document.getElementById('acpControls');
  const nixieContainer = document.getElementById('acpNixieContainer');
  const nixieTubes = Array.from(nixieContainer.querySelectorAll('.nixie-tube'));
  const applyBtn = document.getElementById('acpCleanButton');

  let selectedTube = null;

  // Initialzustand
  acpControls.style.display = 'none';
  nixieContainer.classList.add('hidden');
  applyBtn.classList.add('hidden');

  // Toggle umschalten → nur Panel öffnen/schließen
  acpToggle.addEventListener('change', () => {
    const on = acpToggle.checked;

    // UI
    acpControls.style.display = on ? 'block' : 'none';
    nixieContainer.classList.toggle('hidden', !on);
    applyBtn.classList.toggle('hidden', !on);
    if (on) initNixie();

    // Haupt-ACP-, Datum-, Wetter-, Startwerte-Buttons deaktivieren
    [ACPButton, DateButton, WeatherButton, CricketButton, StartValuesButton].forEach(btn => {
      btn.disabled = on;
    });

    if (!on) {
      fetch('/set/singleDigitControl?value=0')
        .then(res => {
          if (!res.ok) console.error('singleDigitControl:value=0 failed', res.status);
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
      const num = span.textContent === ''
        ? 0
        : (parseInt(span.textContent, 10) + 1) % 10;
      span.textContent = num;
      tube.classList.add('selected');
      selectedTube = tube;
      applyBtn.disabled = false;
    });
  });

  applyBtn.addEventListener('click', () => {
    if (!selectedTube) return;

    const idx = Number(selectedTube.dataset.index) - 1; // 0…5
    const digit = parseInt(selectedTube.querySelector('.digit').textContent, 10);
    const singleDigit = idx * 10 + digit;                // 0…59

    fetch(`/set/singleDigitControl?value=1&digit=${singleDigit}`)
      .then(res => {
        if (!res.ok) {
          console.error('singleDigitControl:value=1 failed', res.status);
        } else {
          console.log('singleDigitControl OK');
        }
      })
      .catch(err => console.error('Network error:', err));
  });

  // === Charts Setup ======================================================
  const configs = [
    { key: "voltage_12V", canvasId: "chart_voltage_12V", label: "Spannung 12 V (V)" },
    { key: "voltage_5V", canvasId: "chart_voltage_5V", label: "Spannung 5 V (V)" },
    { key: "voltage_3V3", canvasId: "chart_voltage_3V3", label: "Spannung 3.3 V (V)" },
    { key: "voltage_18V", canvasId: "chart_voltage_18V", label: "Spannung 18 V (V)" },
    { key: "voltage_UHSS", canvasId: "chart_voltage_UHSS", label: "Spannung UHSS (V)" },
    { key: "current_mA", canvasId: "chart_current_mA", label: "Strom (mA)" },
    { key: "power_W", canvasId: "chart_power_W", label: "Leistung (W)" },
    { key: "energy_Wh", canvasId: "chart_energy_Wh", label: "Energie (Wh)" },
    { key: "temperature", canvasId: "chart_temperature", label: "Case-Temperatur (°C)" },
    { key: "freeHeap", canvasId: "chart_freeHeap", label: "FreeHeap (Bytes)" },
    { key: "totalLoad", canvasId: "chart_totalLoad", label: "CPU Auslastung (0-100%)" },
    { key: "coreLoad0", canvasId: "chart_coreLoad0", label: "Kern 0 (0-100%)" },
    { key: "coreLoad1", canvasId: "chart_coreLoad1", label: "Kern 1 (0-100%)" }
  ];
  const chartData = {}, charts = {};
  function getColor(i) {
    return ["#2196F3", "#FF9800", "#4CAF50", "#E91E63", "#9C27B0", "#00BCD4", "#FFC107", "#795548", "#607D8B", "#F44336", "#3F51B5", "#009688", "#FF5722"][i % 13];
  }

  const timeRangeSelect = document.getElementById('timeRange');

  function updateChartTimeWindow() {
    const now = Date.now();
    let spanMs;

    switch (timeRangeSelect.value) {
      case '1h': spanMs = 1 * 60 * 60 * 1000; break;
      case '6h': spanMs = 6 * 60 * 60 * 1000; break;
      case '24h': spanMs = 24 * 60 * 60 * 1000; break;
      case '7d': spanMs = 7 * 24 * 60 * 60 * 1000; break;
      default: spanMs = null;
    }

    configs.forEach(cfg => {
      const chart = charts[cfg.key];
      const dataArr = chartData[cfg.key];
      if (spanMs && dataArr.length > 0 && now - dataArr[0].x > spanMs) {
        chart.options.scales.x.min = now - spanMs;
        chart.options.scales.x.max = now;
      } else {
        delete chart.options.scales.x.min;
        delete chart.options.scales.x.max;
      }
      chart.update('none');
    });
  }

  timeRangeSelect.addEventListener('change', updateChartTimeWindow);

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
              time: { unit: 'minute', displayFormats: { minute: 'HH:mm' } },
              title: { display: true, text: 'Zeit' }
            },
            y: {
              title: { display: true, text: cfg.label }
            }
          },
          plugins: { legend: { display: false } }
        }
      });
    });
  }

  // === OTA Popup / Progress ============================================
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

  const otaUi = {
    pollTimer: null,
    visible: false,
    seenRunning: false,
    completionHandled: false
  };

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

    // Balken nur während aktuellem Update sichtbar
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

    // Nur wenn wirklich ein aktuelles Update lief oder wir schon sichtbar sind
    const shouldShowResult = otaUi.seenRunning || otaUi.visible;

    if (shouldShowResult && !otaUi.completionHandled) {
      otaUi.completionHandled = true;
      otaSetResultView();
      showOtaModal();

      if (data.stage === "done") {
        otaResultTitle.textContent = "OTA abgeschlossen";
        otaResultText.textContent =
          data.rebootRequired
            ? "Das Update wurde erfolgreich abgeschlossen."
            : "Das Update wurde erfolgreich abgeschlossen. Kein Neustart erforderlich.";
      } else if (data.stage === "error") {
        otaResultTitle.textContent = "OTA fehlgeschlagen";
        otaResultText.textContent =
          `${data.message || "Unbekannter Fehler"} (${data.lastResult || "ESP_FAIL"})`;
      }
    }
  }

  async function fetchOtaStatus() {
    const res = await fetch("/get/otaStatus", { cache: "no-store" });
    if (!res.ok) throw new Error(`OTA status ${res.status}`);
    return await res.json();
  }

  async function otaResetStatusOnController() {
    try {
      await fetch("/set/otaResetStatus", { cache: "no-store" });
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
      await fetch("/set/reset");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Neustart.");
    }
  });

  // === Device Controls ====================================================
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

  const tzNames = ["CET", "EET", "WET", "UTC", "EST", "CST", "MST", "PST", "HST", "JST", "IST", "AEST", "AWST"];


  [
    brightnessNightStartInput,
    brightnessNightEndInput,
    brightnessDimStartInput,
    brightnessDimEndInput,
    brightnessNightValueInput,
    brightnessDimValueInput,
    brightnessDayValueInput
  ].forEach(input => {
    if (!input) return;

    input.addEventListener("focus", () => holdSkipSync(input.id, 30000));
    input.addEventListener("input", () => holdSkipSync(input.id, 30000));
    input.addEventListener("change", () => holdSkipSync(input.id, 30000));
  });

  // Display ON/OFF special
  displayToggle.addEventListener("change", async () => {
    if (displayToggle.checked && !lastData.loadDetected) {
      alert("Keine Last erkannt – Display bleibt aus.");
      displayToggle.checked = false;
      return;
    }
    skipSync.add("displayToggle");
    try {
      if (displayToggle.checked) await fetch("/set/ON");
      else await fetch("/set/OFF");
    } catch (e) {
      console.error(e);
    } finally {
      setTimeout(() => skipSync.delete("displayToggle"), 2000);
    }
  });

  // Standard toggles
  function makeToggleHandler(el, path) {
    el.addEventListener("change", async () => {
      skipSync.add(el.id);
      try {
        await fetch(`/set/${path}?value=${el.checked ? 1 : 0}`);
      } catch (e) {
        console.error(e);
      } finally {
        setTimeout(() => skipSync.delete(el.id), 2000);
      }
    });
  }

  makeToggleHandler(tickerToggle, "ticker");
  makeToggleHandler(timeLimitToggle, "timeLimit");
  makeToggleHandler(silentToggle, "silentMode");
  makeToggleHandler(noACPatNightToggle, "noACPatNight");
  makeToggleHandler(weatherToggle, "weatherUpdate");
  makeToggleHandler(PWMToggle, "NixiePWM");
  makeToggleHandler(cricketToggle, "randomCricket");

  // Reset Button
  ResetButton.addEventListener("click", async () => {
    try {
      await fetch("/set/reset");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Zurücksetzen des Geräts.");
    }

  });

  // ACP Button
  ACPButton.addEventListener("click", async () => {
    try {
      await fetch("/set/ACP");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Auslösen des ACP-Events.");
    }
  });

  // Date Button
  DateButton.addEventListener("click", async () => {
    try {
      await fetch("/set/DATE");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Setzen des Datums.");
    }
  });

  // Weather Button
  WeatherButton.addEventListener("click", async () => {
    try {
      await fetch("/set/tempDisplay");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Abrufen der Wetterdaten.");
    }
  });

  // Cricket Button
  CricketButton.addEventListener("click", async () => {
    try {
      await fetch("/set/CRICKET");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Setzen des Cricket-Sounds.");
    }
  });

  // Check Update
  CheckUpdate.addEventListener("click", async () => {
    try {
      await fetch("/get/checkUpdate");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Überprüfen auf Updates.");
    }
  });

  StartValuesButton.addEventListener("click", async () => {
    try {
      await fetch("/set/setOldValue");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Setzen der Startwerte.");
    }
  });

  ResetStartValuesButton.addEventListener("click", async () => {
    try {
      await fetch("/set/resetValue");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Zurücksetzen der Startwerte.");
    }
  });

  timerToggle.addEventListener("change", async () => {
    skipSync.add("timerToggle");

    const enabled = timerToggle.checked ? 1 : 0;
    const [hh, mm, ss] = timerTimeInput.value.split(":").map(Number);
    const totalSeconds = (hh || 0) * 3600 + (mm || 0) * 60 + (ss || 0);

    try {
      await fetch(`/set/timer?enabled=${enabled}&seconds=${totalSeconds}`);
    } catch (e) {
      console.error(e);
    } finally {
      setTimeout(() => skipSync.delete("timerToggle"), 5000);
    }
  });

  timerTimeInput.addEventListener("change", async () => {
    skipSync.add("timerTimeInput");

    if (timerToggle.checked) {
      const [hh, mm, ss] = timerTimeInput.value.split(":").map(Number);
      const totalSeconds = (hh || 0) * 3600 + (mm || 0) * 60 + (ss || 0);

      try {
        await fetch(`/set/timer?enabled=1&seconds=${totalSeconds}`);
      } catch (e) {
        console.error(e);
      }
    }
    setTimeout(() => skipSync.delete("timerTimeInput"), 5000);
  });

  applyBrightnessConfig.addEventListener("click", async () => {
    try {
      [
        brightnessNightStartInput,
        brightnessNightEndInput,
        brightnessDimStartInput,
        brightnessDimEndInput,
        brightnessNightValueInput,
        brightnessDimValueInput,
        brightnessDayValueInput
      ].forEach(input => {
        if (input) holdSkipSync(input.id, 30000);
      });

      const requests = [
        fetch(`/set/brightnessConfig?field=nightStart&value=${encodeURIComponent(brightnessNightStartInput.value)}`),
        fetch(`/set/brightnessConfig?field=nightEnd&value=${encodeURIComponent(brightnessNightEndInput.value)}`),
        fetch(`/set/brightnessConfig?field=dimStart&value=${encodeURIComponent(brightnessDimStartInput.value)}`),
        fetch(`/set/brightnessConfig?field=dimEnd&value=${encodeURIComponent(brightnessDimEndInput.value)}`),
        fetch(`/set/brightnessConfig?field=nightValue&value=${encodeURIComponent(brightnessNightValueInput.value)}`),
        fetch(`/set/brightnessConfig?field=dimValue&value=${encodeURIComponent(brightnessDimValueInput.value)}`),
        fetch(`/set/brightnessConfig?field=dayValue&value=${encodeURIComponent(brightnessDayValueInput.value)}`)
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
    skipSync.add("alarmToggle");

    const enabled = alarmToggle.checked ? 1 : 0;
    const [hh, mm] = alarmTimeInput.value.split(":").map(Number);
    const timeStr = `${hh.toString().padStart(2, '0')}:${mm.toString().padStart(2, '0')}`;

    try {
      await fetch(`/set/alarm?enabled=${enabled}&time=${encodeURIComponent(timeStr)}`);
    } catch (e) {
      console.error(e);
    } finally {
      setTimeout(() => skipSync.delete("alarmToggle"), 5000);
    }
  });


  alarmTimeInput.addEventListener("change", async () => {
    skipSync.add("alarmTimeInput");

    if (alarmToggle.checked) {
      const [hh, mm] = alarmTimeInput.value.split(":").map(Number);
      const timeStr = `${hh.toString().padStart(2, '0')}:${mm.toString().padStart(2, '0')}`;

      try {
        await fetch(`/set/alarm?enabled=1&time=${encodeURIComponent(timeStr)}`);
      } catch (e) {
        console.error(e);
      }
    }
    setTimeout(() => skipSync.delete("alarmTimeInput"), 5000);
  });


  [timeLimitFromInput, timeLimitToInput].forEach(input => {
    input.addEventListener("change", async () => {
      skipSync.add(input.id);
      const from = timeLimitFromInput.value;
      const to = timeLimitToInput.value;
      try {
        await fetch(
          `/set/timeLimit?value=${timeLimitToggle.checked ? 1 : 0}` +
          `&from=${encodeURIComponent(from)}` +
          `&to=${encodeURIComponent(to)}`
        );
      } catch (e) {
        console.error(e);
      } finally {
        setTimeout(() => skipSync.delete(input.id), 5000);
      }
    });
  });

  tzNames.forEach((name, i) => {
    const opt = document.createElement("option");
    opt.value = String(i);
    opt.text = name;
    timezoneSelect.add(opt);
  });

  // Skip-Sync beim manuellen Ändern
  timezoneSelect.addEventListener("focus", () => skipSync.add("timezoneSelect"));
  timezoneSelect.addEventListener("blur", () => skipSync.delete("timezoneSelect"));

  // Anwenden-Button
  applyTimezoneButton.addEventListener("click", async () => {
    skipSync.add("timezoneSelect");
    try {
      await fetch(`/set/timezone?tz=${encodeURIComponent(timezoneSelect.value)}`);
    } catch (e) {
      console.error("Error setting timezone:", e);
      alert("Fehler beim Setzen der Zeitzone.");
    } finally {
      setTimeout(() => skipSync.delete("timezoneSelect"), 2000);
    }
  });

  // Brightness
  function syncBrightnessUI(manualEnabled, val) {
    brightnessToggle.checked = manualEnabled;
    brightnessContainer.style.display = manualEnabled ? "flex" : "none";

    const autoBrightnessConfigGroup = document.getElementById("autoBrightnessConfigGroup");
    if (autoBrightnessConfigGroup) {
      const nixiePwmEnabled = !!lastData.NixiePWM;
      autoBrightnessConfigGroup.style.display =
        (manualEnabled || nixiePwmEnabled) ? "none" : "block";
    }

    brightnessSlider.value = val;
    brightnessValue.textContent = val;
  }

  // → Beginn der manuellen Interaktion: Skip setzen
  brightnessSlider.addEventListener('mousedown', () => skipSync.add('brightnessSlider'));
  brightnessSlider.addEventListener('touchstart', () => skipSync.add('brightnessSlider'));
  brightnessToggle.addEventListener('mousedown', () => skipSync.add('brightnessToggle'));
  brightnessToggle.addEventListener('touchstart', () => skipSync.add('brightnessToggle'));

  brightnessToggle.addEventListener("change", async () => {
    skipSync.add("brightnessToggle");
    await fetch(
      `/set/manualBrightness?value=${brightnessToggle.checked}&` +
      `brightness=${brightnessSlider.value}`
    );
    setTimeout(() => skipSync.delete("brightnessToggle"), 2000);
  });

  brightnessSlider.addEventListener("input", () => {
    brightnessValue.textContent = brightnessSlider.value;
  });

  brightnessSlider.addEventListener("change", async () => {
    skipSync.add("brightnessSlider");
    await fetch(
      `/set/manualBrightness?value=${brightnessToggle.checked}&` +
      `brightness=${brightnessSlider.value}`
    )
    setTimeout(() => skipSync.delete("brightnessSlider"), 5000);
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

  // Firmware & ZIP
  applyFirmware.addEventListener("click", async () => {
    skipSync.add("firmwareSelect");
    await fetch(`/set/firmware?target=${encodeURIComponent(firmwareSelect.value)}`);
    setTimeout(() => skipSync.delete("firmwareSelect"), 1000);
  });

  applyZip.addEventListener("click", async () => {
    skipSync.add("zipInput");
    await fetch(`/set/zip?zip=${encodeURIComponent(zipInput.value)}`);
    setTimeout(() => skipSync.delete("zipInput"), 1000);
  });

  // logConfig Toggles
  const logIds = [
    "logHttpToggle", "logTimeToggle", "logHssToggle",
    "logDigitToggle", "logWebserverToggle", "logOtaToggle",
    "logButtonToggle", "logGeneralToggle", "logWifiToggle"
  ];

  logIds.forEach(id => {
    const cb = document.getElementById(id);
    cb.addEventListener("change", async () => {
      skipSync.add(id);
      const bits = logIds.map(i => document.getElementById(i).checked ? '1' : '0').join("");
      await fetch(`/set/logConfig?value=${parseInt(bits, 2)}`);
      setTimeout(() => skipSync.delete(id), 2000);
    });
  });

  // OTA Update Button
  setOTA.addEventListener("click", async () => {
    try {
      otaUi.seenRunning = false;
      otaUi.completionHandled = false;
      otaResetBars();
      otaSetLiveView();
      showOtaModal();

      const res = await fetch("/set/ota", { cache: "no-store" });
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

  [timeLimitFromInput, timeLimitToInput].forEach(input => {
    input.addEventListener("change", async () => {
      skipSync.add('timeLimitFrom');
      skipSync.add('timeLimitTo');
      try {
        await fetch(
          `/set/timeLimit?value=${timeLimitToggle.checked ? 1 : 0}` +
          `&from=${encodeURIComponent(timeLimitFromInput.value)}` +
          `&to=${encodeURIComponent(timeLimitToInput.value)}`
        );
      } catch (e) {
        console.error(e);
      } finally {
        // erst nach abgeschlossener Anfrage wieder freigeben
        skipSync.delete('timeLimitFrom');
        skipSync.delete('timeLimitTo');
      }
    });
  });

  // === WiFi Manager ==========================================================
  const wifiListEl = document.getElementById('wifiList');
  const wifiRefreshBtn = document.getElementById('wifiRefreshBtn');
  const wifiAddBtn = document.getElementById('wifiAddBtn');
  const wifiSsidInput = document.getElementById('wifiSsidInput');
  const wifiPwdInput = document.getElementById('wifiPwdInput');

  let wifiData = [];     // [{ssid,priority,last_ok}, ...]
  let draggingEl = null;
  let reorderTimer = null;

  const qEsc = s => encodeURIComponent(s || "");
  const htmlEsc = s => (s || "").replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;" }[c]));

  async function loadWifiList() {
    try {
      const res = await fetch('/get/wifiSaved');
      if (!res.ok) throw new Error(res.status);
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
      <div class="meta">Prio: <span class="prio">${item.priority}</span>${item.last_ok ? ` · last_ok: ${item.last_ok}` : ''}</div>
    </div>
    <div class="actions">
      <button class="del" title="Entfernen">✖</button>
    </div>
  `;

    // Löschen
    li.querySelector('.del').addEventListener('click', async () => {
      const ssid = li.dataset.ssid;
      if (!confirm(`Netzwerk „${ssid}“ entfernen?`)) return;
      try {
        const res = await fetch(`/set/wifiRemove?ssid=${qEsc(ssid)}`);
        if (!res.ok) throw new Error(res.status);
        // Aus lokaler Liste entfernen und neu rendern
        wifiData = wifiData.filter(x => x.ssid !== ssid);
        renderWifiList();
        loadWifiList();
      } catch (e) {
        alert('Löschen fehlgeschlagen');
        console.error(e);
      }
    });

    // Drag&Drop
    li.addEventListener('dragstart', e => {
      draggingEl = li;
      li.classList.add('dragging');
      e.dataTransfer.effectAllowed = 'move';
    });
    li.addEventListener('dragend', () => {
      if (draggingEl) draggingEl.classList.remove('dragging');
      draggingEl = null;
      // Reihenfolge auf dem Server speichern (debounced)
      if (reorderTimer) clearTimeout(reorderTimer);
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

  // Bestimme Node hinter Drop-Position
  function getDragAfterElement(container, y) {
    const els = [...container.querySelectorAll('.wifi-item:not(.dragging)')];
    let closest = { offset: Number.NEGATIVE_INFINITY, element: null };
    els.forEach(el => {
      const box = el.getBoundingClientRect();
      const offset = y - box.top - box.height / 2;
      if (offset < 0 && offset > closest.offset) closest = { offset, element: el };
    });
    return closest.element;
  }

  // Reihenfolge speichern: iteriere Liste von oben nach unten und setze priority = index
  async function saveOrderToServer() {
    const lis = [...wifiListEl.querySelectorAll('.wifi-item')];
    // lokale Struktur updaten
    wifiData = lis.map((li, i) => ({
      ssid: li.dataset.ssid,
      priority: i,
      last_ok: (wifiData.find(x => x.ssid === li.dataset.ssid)?.last_ok) || 0
    }));
    // Prio im UI aktualisieren
    lis.forEach((li, i) => {
      li.dataset.priority = String(i);
      const pr = li.querySelector('.prio');
      if (pr) pr.textContent = String(i);
    });

    // nacheinander an Server schicken (benutzt addOrUpdate mit neuer Prio)
    for (let i = 0; i < wifiData.length; i++) {
      const n = wifiData[i];
      try {
        const res = await fetch(`/set/wifiAdd?ssid=${qEsc(n.ssid)}&pwd=&prio=${n.priority}`);
        if (!res.ok) console.warn('prio save failed for', n.ssid, res.status);
      } catch (e) {
        console.warn('prio save error for', n.ssid, e);
      }
    }
  }

  // Hinzufügen (➕)
  wifiAddBtn.addEventListener('click', async () => {
    const ssid = (wifiSsidInput.value || "").trim();
    const pwd = wifiPwdInput.value || "";
    if (!ssid) { alert('Bitte SSID eingeben'); return; }
    const prio = wifiListEl.children.length; // ans Ende

    try {
      wifiAddBtn.disabled = true;
      const res = await fetch(`/set/wifiAdd?ssid=${qEsc(ssid)}&pwd=${qEsc(pwd)}&prio=${prio}`);
      if (!res.ok) throw new Error(res.status);
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

  // === Poll + DOM update ================================================
  async function fetchInfo() {
    try {
      const res = await fetch('/get/info');
      if (!res.ok) throw new Error(res.status);
      const data = await res.json();
      lastData = data;

      // Dashboard mapping
      const map = {
        Chip: 'chipModel', SSID: 'ssid', Password: 'password',
        Melody: 'melody', FreeHeap: 'freeHeap', CoreLoad0: 'coreLoad0', CoreLoad1: 'coreLoad1', TotalLoad: 'totalLoad',
        ChipId: 'chipId', FlashSize: 'flashSize', FlashSpeed: 'flashSpeed',
        SketchSize: 'sketchSize', SketchFreeSpace: 'sketchFreeSpace',
        CpuFrequencyMHz: 'cpuFreq', SdkVersion: 'sdkVersion',
        Autor: 'autor', Voltage_12V: 'voltage_12V', Voltage_5V: 'voltage_5V',
        Voltage_3V3: 'voltage_3V3', Voltage_18V: 'voltage_18V', Voltage_UHSS: 'voltage_UHSS',
        Current_mA: 'current_mA', Power_W: 'power_W', Energy_Wh: 'energy_Wh',
        HSS_Enabled: 'HSS_Enabled', HSS_190V: 'HSS_190V', HSS_Resistor: 'HSS_Resistor',
        CaseTemperature_C: 'temperature', Firmware_Target: 'firmwareTarget',
        Hardware_Version: 'hardwareVersion', Software_Version: 'softwareVersion',
        NixiePWM: 'NixiePWM', PWM_Frequenzy: 'PWM_Frequenzy',
        manualBrightnessEnabled: 'manualBrightnessEnabled',
        loadDetected: 'loadDetected', Brightness: 'brightness', brightnessNightStartHour: 'brightnessNightStartHour',
        brightnessNightEndHour: 'brightnessNightEndHour', brightnessDimStartHour: 'brightnessDimStartHour', brightnessDimEndHour: 'brightnessDimEndHour',
        brightnessNightValue: 'brightnessNightValue', brightnessDimValue: 'brightnessDimValue', brightnessDayValue: 'brightnessDayValue',
        timeLimitFrom: 'timeLimitFrom', timeLimitTo: 'timeLimitTo', noACPatNight: 'noACPatNight',
        DisplayEnabled: 'DisplayEnabled', tickerEnabled: 'tickerEnabled', timeLimitEnabled: 'timeLimitEnabled',
        silentModeEnabled: 'silentModeEnabled', WeatherUpdateEnabled: 'WeatherUpdateEnabled', zipCode: 'zipCode',
        CurrentTimeZone: 'CurrentTimeZone', TimerActive: 'TimerActive', TimerConfiguredSeconds: 'TimerConfiguredSeconds',
        AlarmActive: 'AlarmActive', AlarmTime: 'AlarmTime'
      };

      const boolFields = ['DisplayEnabled', 'tickerEnabled', 'timeLimitEnabled', 'silentModeEnabled',
        'WeatherUpdateEnabled', 'manualBrightnessEnabled', 'NixiePWM', 'TimerActive', 'AlarmActive', 'loadDetected', 'noACPatNight'];

      Object.entries(map).forEach(([k, id]) => {
        const el = document.getElementById(id);
        if (!el || skipSync.has(id)) return;

        let val = data[k];

        if (boolFields.includes(k)) {
          val = val == 1 ? 'on' : 'off';
        }

        if (
          k === "brightnessNightStartHour" ||
          k === "brightnessNightEndHour" ||
          k === "brightnessDimStartHour" ||
          k === "brightnessDimEndHour"
        ) {
          val = formatHourMinute(val);
        }

        el.textContent = val;
      });

      // sync brightness
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
          data.updateAvailable ??
          data.UpdateAvailable ??
          data.update_available ??
          0;

        const hasUpdate =
          rawUpdateValue === 1 ||
          rawUpdateValue === "1" ||
          rawUpdateValue === true ||
          rawUpdateValue === "true";

        updateBannerBadge.classList.toggle("hidden", !hasUpdate);
      }

      const tzDisplay = document.getElementById("CurrentTimeZone");
      if (tzDisplay && data.CurrentTimeZone != null) {
        const idx = parseInt(data.CurrentTimeZone, 10);
        tzDisplay.textContent = tzNames[idx] || "–";
      }

      // charts
      configs.forEach(cfg => {
        const jk = cfg.key === 'temperature'
          ? 'CaseTemperature_C'
          : cfg.key.charAt(0).toUpperCase() + cfg.key.slice(1);
        chartData[cfg.key].push({
          x: Date.now(),
          y: parseFloat(data[jk])
        });
        charts[cfg.key].update('none');
      });

      updateChartTimeWindow();

      // toggles
      if (!skipSync.has("displayToggle")) displayToggle.checked = !!data.DisplayEnabled;
      if (!skipSync.has("tickerToggle")) tickerToggle.checked = !!data.tickerEnabled;
      if (!skipSync.has("timeLimitToggle")) timeLimitToggle.checked = !!data.timeLimitEnabled;
      if (!skipSync.has("silentToggle")) silentToggle.checked = !!data.silentModeEnabled;
      if (!skipSync.has("noACPatNightToggle")) noACPatNightToggle.checked = !!data.noACPatNight;
      if (!skipSync.has("weatherToggle")) weatherToggle.checked = !!data.WeatherUpdateEnabled;
      if (!skipSync.has("PWMToggle")) PWMToggle.checked = !!data.NixiePWM;
      if (!skipSync.has("timerToggle")) timerToggle.checked = !!data.TimerActive;
      if (!skipSync.has("alarmToggle")) alarmToggle.checked = !!data.AlarmActive;
      if (!skipSync.has("cricketToggle")) cricketToggle.checked = !!data.cricketSoundEnabled;

      // timezone
      if (!skipSync.has("timezoneSelect") && data.CurrentTimeZoneIndex != null) {
        timezoneSelect.value = String(data.CurrentTimeZoneIndex);
      }

      // logConfig
      const bin = data.logConfigBinary.padStart(9, '0');
      logIds.forEach((id, i) => {
        if (!skipSync.has(id)) document.getElementById(id).checked = (bin[i] === '1');
      });

      // firmware & zip
      if (!skipSync.has("firmwareSelect")) firmwareSelect.value = data.Firmware_Target;
      if (!skipSync.has("zipInput")) zipInput.value = data.zipCode;

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
      if (!otaPollingActive) {
        await fetchInfo();
      }
    } catch (e) {
      console.warn("pollInfo error:", e);
    } finally {
      setTimeout(pollInfo, 500);
    }
  }

  initCharts();
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