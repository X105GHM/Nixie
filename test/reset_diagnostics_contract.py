"""Host-side contracts for reset diagnostics and flash-wear boundaries."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/Diagnostics/ResetDiagnostics.hpp").read_text(encoding="utf-8")
SOURCE = (ROOT / "src/Diagnostics/ResetDiagnostics.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
APP_STATE = (ROOT / "src/AppState/AppState.cpp").read_text(encoding="utf-8")
HTTP = (ROOT / "src/HTTP/HTTP.cpp").read_text(encoding="utf-8")
SECURE = (ROOT / "src/HTTP/SecureApi.cpp").read_text(encoding="utf-8")
WIFI = (ROOT / "src/WiFiConnector/WiFiConnector.cpp").read_text(encoding="utf-8")
FRONTEND = (ROOT / "data/script.js").read_text(encoding="utf-8")
HTML = (ROOT / "data/index.html").read_text(encoding="utf-8")


def function_body(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


def check_reset_reason_ring() -> None:
    assert "MAX_EVENTS = 12" in HEADER, "reset log capacity changed unexpectedly"
    assert "esp_reset_reason()" in SOURCE, "native reset reason is not captured"
    for reason in (
        "ESP_RST_BROWNOUT", "ESP_RST_PANIC", "ESP_RST_INT_WDT",
        "ESP_RST_TASK_WDT", "ESP_RST_SW", "ESP_RST_PWR_GLITCH",
        "ESP_RST_CPU_LOCKUP",
    ):
        assert reason in SOURCE, f"missing reset reason mapping: {reason}"
    assert "writeIndex" in SOURCE and "% MAX_EVENTS" in SOURCE, "persistent log is not a ring"
    assert "structureChecksum" in SOURCE and "LOG_VERSION" in SOURCE, "persistent format is not validated"
    assert "nvs_set_blob" in SOURCE and "nvs_commit" in SOURCE, "boot event is not persisted"
    assert "ResetDiagnostics::instance().initialize()" in MAIN, "boot diagnostics are not initialized"


def check_rtc_breadcrumb_and_wear() -> None:
    assert "RTC_NOINIT_ATTR" in SOURCE, "last-known telemetry is not kept in RTC memory"
    assert "ResetDiagnostics::instance().updateBreadcrumb" in APP_STATE, "1 Hz telemetry does not update breadcrumb"
    update = function_body(
        SOURCE,
        "void ResetDiagnostics::updateBreadcrumb",
        "void ResetDiagnostics::markPlannedRestart",
    )
    assert "nvs_" not in update, "periodic breadcrumb update writes flash"
    assert "heap_caps_get_free_size" in update and "voltage12V" in update
    initialize = function_body(
        SOURCE,
        "void ResetDiagnostics::initialize",
        "void ResetDiagnostics::updateBreadcrumb",
    )
    assert initialize.count("saveLog(log)") == 1, "more than one NVS commit path exists per boot"


def check_planned_restarts_and_api() -> None:
    assert 'markPlannedRestart("connectivity")' in WIFI
    assert "markPlannedRestart(plannedReason)" in HTTP
    assert 'requestedSource == "ota"' in HTTP
    assert 'server_.on("/api/v1/reset-log", HTTP_GET' in SECURE
    assert "ResetDiagnostics::instance().json()" in SECURE
    assert 'reason != "user" && reason != "ota"' in SECURE


def check_frontend() -> None:
    assert 'fetch(`${API_BASE}/reset-log`' in FRONTEND
    assert "resetReasonLabel" in FRONTEND
    assert "previous.historyState" in FRONTEND
    assert "reason: 'ota'" in FRONTEND and "reason: 'user'" in FRONTEND
    assert 'id="resetDiagnosticsOutput"' in HTML
    assert 'id="resetDiagnosticsRefresh"' in HTML


def main() -> int:
    checks = (
        check_reset_reason_ring,
        check_rtc_breadcrumb_and_wear,
        check_planned_restarts_and_api,
        check_frontend,
    )
    for check in checks:
        check()
        print(f"PASS {check.__name__}")
    print("PASS reset diagnostics contract")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, ValueError) as error:
        print(f"FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
