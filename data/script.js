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
          // manualBrightness/value/brightness
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
      }
    }
    else if (parts[0] === 'get' && parts[1] === 'info') {
      url = `/get/info`;
    }

    if (url) {
      fetch(url);
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
    setOTA = document.getElementById("otaUpdateButton");

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
  makeToggleHandler(PWMToggle, "NixiePWM");

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
      await fetch("/set//set/resetValue");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Zurücksetzen der Startwerte.");
    }
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
        setTimeout(() => skipSync.delete(input.id), 2000);
      }
    });
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


    setTimeout(() => skipSync.delete("brightnessSlider"), 5000);
  });

  zipInput.addEventListener('focus', () => skipSync.add('zipInput'));
  zipInput.addEventListener('blur', () => skipSync.delete('zipInput'));

  firmwareSelect.addEventListener('focus', () => skipSync.add('firmwareSelect'));
  firmwareSelect.addEventListener('blur', () => skipSync.delete('firmwareSelect'));

  timeLimitFromInput.addEventListener('focus', () => skipSync.add('timeLimitFrom'));
  timeLimitToInput.addEventListener('focus', () => skipSync.add('timeLimitTo'));


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
      await fetch("/set/ota");
    } catch (e) {
      console.error(e);
      alert("Fehler beim Starten des OTA-Updates.");
    }
  });

  [timeLimitFromInput, timeLimitToInput].forEach(input => {
    input.addEventListener("change", async () => {
      // Flag setzen, damit fetchInfo nicht drüber schreibt
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
        Melody: 'melody', FreeHeap: 'freeHeap',
        ChipId: 'chipId', FlashSize: 'flashSize', FlashSpeed: 'flashSpeed',
        SketchSize: 'sketchSize', SketchFreeSpace: 'sketchFreeSpace',
        CpuFrequencyMHz: 'cpuFreq', SdkVersion: 'sdkVersion',
        Autor: 'autor', Voltage_12V: 'voltage_12V', Voltage_5V: 'voltage_5V',
        Voltage_3V3: 'voltage_3V3', Voltage_18V: 'voltage_18V', Voltage_UHSS: 'voltage_UHSS',
        Current_mA: 'current_mA', Power_W: 'power_W', Energy_Wh: 'energy_Wh',
        HSS_Enabled: 'HSS_Enabled', HSS_190V: 'HSS_190V', HSS_Resistor: 'HSS_Resistor',
        CaseTemperature_C: 'temperature', Firmware_Target: 'firmwareTarget',
        Hardware_Version: 'hardwareVersion', Software_Version: 'softwareVersion',
        zipCode: 'zipCode',
        manualBrightnessEnabled: 'manualBrightnessEnabled',
        loadDetected: 'loadDetected', Brightness: 'brightness', timeLimitFrom: 'timeLimitFrom', timeLimitTo: 'timeLimitTo',
        DisplayEnabled: 'DisplayEnabled', tickerEnabled: 'tickerEnabled', timeLimitEnabled: 'timeLimitEnabled', 
        silentModeEnabled: 'silentModeEnabled', WeatherUpdateEnabled: 'WeatherUpdateEnabled', NixiePWM: 'nixiePWM'
      };

      const boolFields = ['DisplayEnabled', 'tickerEnabled', 'timeLimitEnabled', 'silentModeEnabled', 'WeatherUpdateEnabled', 'manualBrightnessEnabled','nixiePWM'];

      Object.entries(map).forEach(([k, id]) => {
        const el = document.getElementById(id);
        if (!el || skipSync.has(id)) return;

        let val = data[k];
        if (boolFields.includes(k)) {
          val = val == 1 ? 'on' : 'off';
        }
        el.textContent = val;
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
      if (!skipSync.has("PWMToggle")) PWMToggle.checked = !!data.NixiePWM;

      // logConfig
      const bin = data.logConfigBinary.padStart(9, '0');
      logIds.forEach((id, i) => {
        if (!skipSync.has(id)) document.getElementById(id).checked = (bin[i] === '1');
      });

      // firmware & zip
      if (!skipSync.has("firmwareSelect")) firmwareSelect.value = data.Firmware_Target;
      if (!skipSync.has("zipInput")) zipInput.value = data.zipCode;

      timeLimitSettings.style.display = data.timeLimitEnabled ? "flex" : "none";


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
    await fetchInfo();
    setTimeout(pollInfo, 500);
  }

  pollInfo();
  initCharts();
});
