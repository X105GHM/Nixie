document.addEventListener("DOMContentLoaded", () => {
  const skipSync = new Set();
  let lastData = {};

  // === Netzwerk-Logger ===========================================
  const netLogEl = document.getElementById("networkLog");
  const clearBtn = document.getElementById("clearNetworkLog");
  const logToUI = msg => {
    if (!netLogEl) return;
    const ts = new Date().toLocaleTimeString("de-DE");
    netLogEl.textContent += `[${ts}] ${msg}\n`;
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

  // wrap fetch to log
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

  // === Dark/Light Mode ==================================================
  const themeToggle = document.getElementById("themeToggle");
  if (localStorage.getItem("darkMode") === "true") document.body.classList.add("dark");
  themeToggle.checked = document.body.classList.contains("dark");
  themeToggle.addEventListener("change", () => {
    document.body.classList.toggle("dark", themeToggle.checked);
    localStorage.setItem("darkMode", themeToggle.checked);
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
    { key: "freeHeap", canvasId: "chart_freeHeap", label: "FreeHeap (Bytes)" }
  ];
  const chartData = {}, charts = {};
  function getColor(i) {
    return ["#2196F3", "#FF9800", "#4CAF50", "#E91E63", "#9C27B0", "#00BCD4", "#FFC107", "#795548", "#607D8B", "#F44336"][i % 10];
  }
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

  // === Device Controls ====================================================
  const displayToggle = document.getElementById("displayToggle"),
    ResetButton = document.getElementById("resetBtn"),
    ACPButton = document.getElementById("acpBtn"),
    DateButton = document.getElementById("dateBtn"),
    WeatherButton = document.getElementById("weatherBtn"),
    tickerToggle = document.getElementById("tickerToggle"),
    timeLimitToggle = document.getElementById("timeLimitToggle"),
    silentToggle = document.getElementById("silentToggle"),
    weatherToggle = document.getElementById("weatherToggle"),
    brightnessToggle = document.getElementById("brightnessToggle"),
    brightnessSlider = document.getElementById("brightnessSlider"),
    brightnessValue = document.getElementById("brightnessValue"),
    brightnessContainer = document.querySelector('.brightness-slider-setting'),
    firmwareSelect = document.getElementById("firmwareSelect"),
    applyFirmware = document.getElementById("applyFirmware"),
    zipInput = document.getElementById("zipInput"),
    applyZip = document.getElementById("applyZip");

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

  // standard toggles
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
  makeToggleHandler(weatherToggle, "weatherUpdate");

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
      await fetch("/set/acp");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Auslösen des ACP-Events.");
    }
  });

  // Date Button
  DateButton.addEventListener("click", async () => {
    try {
      await fetch("/set/date");
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

  // Brightness
  function syncBrightnessUI(enabled, val) {
    brightnessToggle.checked = enabled;
    brightnessContainer.style.display = enabled ? "flex" : "none";
    brightnessSlider.value = val;
    brightnessValue.textContent = val;
  }
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


    setTimeout(() => skipSync.delete("brightnessSlider"), 2000);
  });

  zipInput.addEventListener('focus', () => skipSync.add('zipInput'));
  zipInput.addEventListener('blur', () => skipSync.delete('zipInput'));

  firmwareSelect.addEventListener('focus', () => skipSync.add('firmwareSelect'));
  firmwareSelect.addEventListener('blur', () => skipSync.delete('firmwareSelect'));

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
        DisplayEnabled: 'displayStatus', Melody: 'melody', FreeHeap: 'freeHeap',
        ChipId: 'chipId', FlashSize: 'flashSize', FlashSpeed: 'flashSpeed',
        SketchSize: 'sketchSize', SketchFreeSpace: 'sketchFreeSpace',
        CpuFrequencyMHz: 'cpuFreq', SdkVersion: 'sdkVersion',
        Autor: 'autor', Voltage_12V: 'voltage_12V', Voltage_5V: 'voltage_5V',
        Voltage_3V3: 'voltage_3V3', Voltage_18V: 'voltage_18V', Voltage_UHSS: 'voltage_UHSS',
        Current_mA: 'current_mA', Power_W: 'power_W', Energy_Wh: 'energy_Wh',
        HSS_Enabled: 'HSS_Enabled', HSS_190V: 'HSS_190V', HSS_Resistor: 'HSS_Resistor',
        CaseTemperature_C: 'temperature', Firmware_Target: 'firmwareTarget',
        Hardware_Version: 'hardwareVersion', Software_Version: 'softwareVersion',
        zipCode: 'zipCode', tickerEnabled: 'tickerEnabled',
        timeLimitEnabled: 'timeLimitEnabled', silentModeEnabled: 'silentModeEnabled',
        manualBrightnessEnabled: 'manualBrightnessEnabled', WeatherUpdateEnabled: 'weatherEnabled',
        loadDetected: 'loadDetected', Brightness: 'brightness'
      };
      Object.entries(map).forEach(([k, id]) => {
        const el = document.getElementById(id);
        if (el && !skipSync.has(id)) el.textContent = data[k];
      });

      // sync brightness
      syncBrightnessUI(!!data.manualBrightnessEnabled, data.Brightness);

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

      // toggles
      if (!skipSync.has("displayToggle")) displayToggle.checked = !!data.DisplayEnabled;
      if (!skipSync.has("tickerToggle")) tickerToggle.checked = !!data.tickerEnabled;
      if (!skipSync.has("timeLimitToggle")) timeLimitToggle.checked = !!data.timeLimitEnabled;
      if (!skipSync.has("silentToggle")) silentToggle.checked = !!data.silentModeEnabled;
      if (!skipSync.has("weatherToggle")) weatherToggle.checked = !!data.WeatherUpdateEnabled;

      // logConfig
      const bin = data.logConfigBinary.padStart(9, '0');
      logIds.forEach((id, i) => {
        if (!skipSync.has(id)) document.getElementById(id).checked = (bin[i] === '1');
      });

      // firmware & zip
      if (!skipSync.has("firmwareSelect")) firmwareSelect.value = data.Firmware_Target;
      if (!skipSync.has("zipInput")) zipInput.value = data.zipCode;

    } catch (e) {
      console.warn("Info polling error:", e);
    }
  }

  initCharts();
  fetchInfo();
  setInterval(fetchInfo, 1000);
});
