"""Static compatibility checks for the HTTP API migration.

These checks deliberately run without ESP32 hardware. They guard the route/method,
request parameter and response status contract used by the existing SPIFFS UI.
Runtime behaviour still needs the hardware test matrix in docs/esp-idf-phase7.md.
"""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
HTTP_SOURCE = (ROOT / "src/HTTP/HTTP.cpp").read_text(encoding="utf-8")
MAIN_SOURCE = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
NATIVE_SOURCE = (ROOT / "src/HTTP/NativeHttpServer.cpp").read_text(encoding="utf-8")
STATIC_SOURCE = (ROOT / "src/HTTP/StaticFileServer.cpp").read_text(encoding="utf-8")
COMMAND_SOURCE = (ROOT / "src/HTTP/HttpCommandQueue.cpp").read_text(encoding="utf-8")
APP_STATE_SOURCE = (ROOT / "src/AppState/AppState.cpp").read_text(encoding="utf-8")
FRONTEND_SOURCE = (ROOT / "data/script.js").read_text(encoding="utf-8")
FRONTEND_HTML = (ROOT / "data/index.html").read_text(encoding="utf-8")
HISTORY_PYRAMID = (ROOT / "src/History/TimeSeriesPyramid.cpp").read_text(encoding="utf-8")
HISTORY_SERVICE = (ROOT / "src/History/ChartHistory.cpp").read_text(encoding="utf-8")
SECURE_SOURCE = (ROOT / "src/HTTP/SecureApi.cpp").read_text(encoding="utf-8")

# path: (method, parameters referenced by the handler, response codes)
CONTRACTS = {
    "/set/OFF": ("HTTP_GET", (), (200,)),
    "/set/ON": ("HTTP_GET", (), (200,)),
    "/set/reset": ("HTTP_GET", (), (200,)),
    "/set/setOldValue": ("HTTP_GET", (), (200,)),
    "/set/loadDetectedOverwrite": ("HTTP_GET", (), (200,)),
    "/set/resetValue": ("HTTP_GET", (), (200,)),
    "/set/resetWiFi": ("HTTP_GET", (), (200,)),
    "/get/wifiSaved": ("HTTP_GET", (), (200,)),
    "/set/wifiAdd": ("HTTP_POST", ("plain",), (200, 400)),
    "/set/wifiRemove": ("HTTP_GET", ("ssid",), (200, 400, 404)),
    "/set/ACP": ("HTTP_GET", (), (200, 400)),
    "/set/tempDisplay": ("HTTP_GET", (), (200, 400)),
    "/set/DATE": ("HTTP_GET", (), (200, 400)),
    "/set/CRICKET": ("HTTP_GET", (), (200,)),
    "/set/ticker": ("HTTP_GET", ("value",), (200, 400)),
    "/set/singleDigitControl": ("HTTP_GET", ("value", "digit"), (200, 400)),
    "/set/NixiePWM": ("HTTP_GET", ("value",), (200, 400)),
    "/set/PWMPeriod": ("HTTP_GET", ("value",), (200, 400)),
    "/set/timeLimit": ("HTTP_GET", ("value", "from", "to"), (200, 400)),
    "/set/brightnessConfig": ("HTTP_GET", ("field", "value"), (200, 400)),
    "/set/silentMode": ("HTTP_GET", ("value",), (200, 400)),
    "/set/noACPatNight": ("HTTP_GET", ("value",), (200, 400)),
    "/set/manualBrightness": ("HTTP_GET", ("value", "brightness"), (200,)),
    "/set/weatherUpdate": ("HTTP_GET", ("value",), (200, 400)),
    "/set/randomCricket": ("HTTP_GET", ("value",), (200, 400)),
    "/set/logConfig": ("HTTP_GET", ("value",), (200, 400)),
    "/set/firmware": ("HTTP_GET", ("target",), (200, 400)),
    "/set/ota": ("HTTP_GET", (), (202, 400, 409, 500)),
    "/get/otaStatus": ("HTTP_GET", (), (200,)),
    "/set/otaResetStatus": ("HTTP_GET", (), (200, 409)),
    "/get/checkUpdate": ("HTTP_GET", (), (202,)),
    "/set/zip": ("HTTP_GET", ("zip",), (200, 400)),
    "/set/timezone": ("HTTP_GET", ("tz",), (200, 400)),
    "/set/timer": ("HTTP_GET", ("enabled", "seconds"), (200, 400)),
    "/set/alarm": ("HTTP_GET", ("enabled", "time"), (200, 400)),
    "/get/brownout": ("HTTP_GET", (), (200,)),
    "/set/brownout": ("HTTP_GET", (), (204,)),
    "/get/taskStats": ("HTTP_GET", (), (200,)),
    "/get/info": ("HTTP_GET", (), (200,)),
    "/events": ("HTTP_GET", (), (500, 503)),
}


def route_block(path: str, method: str) -> str:
    marker = f'server_.on("{path}", {method}'
    start = HTTP_SOURCE.find(marker)
    assert start >= 0, f"missing route registration: {method} {path}"
    end = HTTP_SOURCE.find("server_.on(", start + len(marker))
    if end < 0:
        end = HTTP_SOURCE.find("if (!commands_.start()", start)
    return HTTP_SOURCE[start:end]


def check_contracts() -> None:
    registrations = re.findall(r'server_\.on\("([^\"]+)",\s*(HTTP_[A-Z]+)', HTTP_SOURCE)
    assert len(registrations) == len(CONTRACTS), (
        f"route count changed: expected {len(CONTRACTS)}, got {len(registrations)}"
    )
    assert set(registrations) == {(path, spec[0]) for path, spec in CONTRACTS.items()}

    for path, (method, parameters, statuses) in CONTRACTS.items():
        block = route_block(path, method)
        for parameter in parameters:
            assert f'"{parameter}"' in block, f"{method} {path}: missing parameter {parameter}"
        for status in statuses:
            assert re.search(rf"\b{status}\b", block), f"{method} {path}: missing status {status}"


def check_frontend_routes() -> None:
    registered = set(CONTRACTS)
    referenced = set()
    for path in (ROOT / "data").rglob("*"):
        if path.suffix.lower() not in {".js", ".html"}:
            continue
        text = path.read_text(encoding="utf-8")
        referenced.update(re.findall(r"/(?:set|get)/[A-Za-z0-9_]+", text))
    missing = sorted(referenced - registered)
    assert not missing, f"frontend refers to unregistered routes: {missing}"


def check_native_server_only() -> None:
    project_sources = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for base in (ROOT / "src", ROOT / "lib")
        for path in base.rglob("*")
        if path.suffix.lower() in {".c", ".cpp", ".h", ".hpp"}
    )
    assert not re.search(r"#\s*include\s*[<\"]WebServer\.h[>\"]", project_sources), "WebServer.h remains"
    assert not re.search(r"\bWebServer\s+[A-Za-z_]", project_sources), "Arduino WebServer instance remains"
    assert "handleClient(" not in project_sources, "HTTP polling remains"
    assert not re.search(r"#\s*include\s*[<\"]SPIFFS\.h[>\"]", project_sources), "Arduino SPIFFS remains"
    assert "httpd_start(" in NATIVE_SOURCE, "native server is not started"
    assert "httpd_register_uri_handler(" in NATIVE_SOURCE, "native routes are not registered"
    assert "esp_vfs_spiffs_register(" in STATIC_SOURCE, "native SPIFFS VFS is not registered"
    assert "std::fopen(" in STATIC_SOURCE and "std::fread(" in STATIC_SOURCE, "POSIX file access is missing"
    assert 'isPdf ? "SAMEORIGIN" : "DENY"' in STATIC_SOURCE, (
        "same-origin PDF embedding is blocked by X-Frame-Options"
    )
    assert "frame-src 'self'" in STATIC_SOURCE, "same-origin manual iframe is blocked by CSP"
    assert 'server_.on("/",' not in HTTP_SOURCE, "explicit root route shadows the static-file fallback"
    assert "server_.onNotFound(" in HTTP_SOURCE and "staticFiles_.serve(" in HTTP_SOURCE, (
        "static root fallback is missing"
    )
    send_start = NATIVE_SOURCE.index("void NativeHttpServer::send(int statusCode, const char* contentType, const std::string& body)")
    send_end = NATIVE_SOURCE.index("const char* NativeHttpServer::statusText", send_start)
    send_body = NATIVE_SOURCE[send_start:send_end]
    assert send_body.rfind("responseHeaders_.clear()") > send_body.rfind("httpd_resp_send("), (
        "dynamic response headers are released before esp_http_server sends them"
    )
    start_task = re.search(r"static void httpStartTask\([^)]*\)\s*\{(.*?)\n\}", MAIN_SOURCE, re.DOTALL)
    assert start_task and "for (;;)" not in start_task.group(1), "HTTP launcher still polls"


def check_phase7_async() -> None:
    info = route_block("/get/info", "HTTP_GET")
    assert "AppState::instance().snapshot()" in info, "/get/info bypasses AppState"
    assert "read12V" not in info and "readTelemetry" not in info, "/get/info reads hardware"
    assert "readTelemetry()" in APP_STATE_SOURCE, "periodic sensor snapshot missing"
    assert '"AppTelemetry"' in APP_STATE_SOURCE and "pdMS_TO_TICKS(1000)" in (ROOT / "src/AppState/AppState.hpp").read_text(encoding="utf-8"), "1 Hz telemetry task missing"
    assert "httpd_req_async_handler_begin" in APP_STATE_SOURCE, "SSE is not detached from HTTP server task"
    assert "text/event-stream" in APP_STATE_SOURCE, "SSE content type missing"
    assert "commands_.enqueue" in HTTP_SOURCE, "mutating routes remain fully synchronous"
    assert "xQueueSend(queue_, &command, 0)" in COMMAND_SOURCE, "command enqueue can block HTTP"
    assert "new EventSource(`${API_BASE}/events`)" in FRONTEND_SOURCE, "frontend does not use SSE"
    assert "setTimeout(pollInfo, 500)" not in FRONTEND_SOURCE, "500 ms info polling remains"
    assert "const pendingState = new Map()" in FRONTEND_SOURCE, "pending UI state tracking is missing"
    assert "pending.requestAccepted && revisionConfirmed" in FRONTEND_SOURCE, (
        "UI state can be acknowledged by stale telemetry"
    )
    assert "pending.minimumRevision = lastStateRevision + 1" in FRONTEND_SOURCE, (
        "command acknowledgement is not tied to a newer AppState revision"
    )
    assert FRONTEND_SOURCE.count("[timeLimitFromInput, timeLimitToInput].forEach") == 1, (
        "time-limit inputs have duplicate change handlers"
    )
    assert 'brightnessContainer.style.display = manualEnabled ? "flex" : "none"' in FRONTEND_SOURCE, (
        "manual brightness slider visibility is no longer synchronized"
    )
    assert "autoBrightnessConfigGroup.style.display" not in FRONTEND_SOURCE, (
        "manual brightness must not collapse subsequent settings"
    )

    mutating_routes = [path for path in CONTRACTS if path.startswith("/set/")]
    mutating_routes.append("/get/checkUpdate")
    for path in mutating_routes:
        method = CONTRACTS[path][0]
        block = route_block(path, method)
        assert "enqueueCommand(" in block or "executeCommand(" in block, (
            f"{method} {path}: mutation bypasses command queue"
        )

    all_sources = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in (ROOT / "src").rglob("*")
        if path.suffix.lower() in {".cpp", ".hpp"}
    )
    assert "extern bool displayEnabled" not in all_sources, "display state is not atomic"
    assert "std::atomic_bool displayEnabled" in all_sources, "atomic display state missing"
    assert not re.search(
        r"Globals::(?:zipCode|HardwareVersion|SoftwareVersion|timeLimitFrom|timeLimitTo)",
        all_sources,
    ), "direct shared text configuration remains"
    assert "getTextConfig()" in all_sources, "text configuration snapshot missing"
    assert "energyMonitor.getSnapshot()" in APP_STATE_SOURCE, "energy snapshot is not synchronized"
    assert "timer.getSnapshot()" in APP_STATE_SOURCE, "timer snapshot is not synchronized"
    assert "alarmClock.getSnapshot()" in APP_STATE_SOURCE, "alarm snapshot is not synchronized"


def check_chart_history() -> None:
    for value in ("10m", "1h", "6h", "12h", "24h", "7d"):
        assert f'value="{value}"' in FRONTEND_HTML, f"missing chart range {value}"
    assert 'fetch(`${API_BASE}/history?range=${encodeURIComponent(rangeName)}`' in FRONTEND_SOURCE, (
        "frontend does not load history for the selected range"
    )
    assert "activeHistoryLoad.live.push" in FRONTEND_SOURCE, "live SSE samples are not buffered during history load"
    assert "points.length = 0" in FRONTEND_SOURCE, "range changes do not replace existing chart history"
    assert "bufferedLive.forEach" in FRONTEND_SOURCE, "live samples are not appended after history load"

    assert "MALLOC_CAP_SPIRAM" in HISTORY_SERVICE, "history storage is not explicitly allocated in PSRAM"
    assert "TOTAL_POINT_COUNT * sizeof(Point)" in HISTORY_SERVICE, "history is not allocated once at startup"
    assert "xSemaphoreTake" in HISTORY_SERVICE and "xSemaphoreGive" in HISTORY_SERVICE, (
        "history writer/snapshot synchronization is missing"
    )
    assert "ChartHistory::instance().record" in APP_STATE_SOURCE, "1 Hz AppState does not feed chart history"
    assert "supplyWatch.readTelemetry" not in HISTORY_SERVICE, "history service reads sensor hardware"
    assert all(marker not in HISTORY_SERVICE for marker in ("nvs_", "spiffs", "fopen", "esp_partition")), (
        "history unexpectedly persists data in flash"
    )
    assert "weightedSum" in HISTORY_PYRAMID and "source.sampleCount" in HISTORY_PYRAMID, (
        "hierarchical average is not sample-count weighted"
    )
    assert "minimum" in HISTORY_PYRAMID and "maximum" in HISTORY_PYRAMID, "peak preservation is missing"
    assert 'server_.on("/api/v1/history", HTTP_GET' in SECURE_SOURCE, "native history route is missing"
    assert "httpd_resp_send_chunk" in SECURE_SOURCE, "history response is buffered instead of streamed"
    for field in ("average", "minimum", "maximum"):
        assert f'appendMatrix("{field}"' in SECURE_SOURCE, f"history response omits {field}"
    for field in ("sampleCounts", "timestamps"):
        assert f'\\"{field}\\"' in SECURE_SOURCE, f"history response omits {field}"


def main() -> int:
    checks = (
        check_contracts,
        check_frontend_routes,
        check_native_server_only,
        check_phase7_async,
        check_chart_history,
    )
    for check in checks:
        check()
        print(f"PASS {check.__name__}")
    print(f"PASS {len(CONTRACTS)} HTTP route contracts")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
