"""Host-side security contract tests for the native HTTP API.

The checks intentionally do not require an ESP32. Runtime negative tests live in
``live_http_security.py`` and only run when a device URL is explicitly supplied.
"""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SECURE = (ROOT / "src/HTTP/SecureApi.cpp").read_text(encoding="utf-8")
NATIVE = (ROOT / "src/HTTP/NativeHttpServer.cpp").read_text(encoding="utf-8")
HTTP = (ROOT / "src/HTTP/HTTP.cpp").read_text(encoding="utf-8")
PORTAL = (ROOT / "lib/CustomWiFiManager/ewm/Portal/CaptivePortal.cpp").read_text(encoding="utf-8")
WEATHER = (ROOT / "src/TimeWeather/WeatherClient/WeatherClient.cpp").read_text(encoding="utf-8")
FRONTEND = (ROOT / "data/script.js").read_text(encoding="utf-8")
PLATFORMIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")
GITIGNORE = (ROOT / ".gitignore").read_text(encoding="utf-8")
SECRETS_EXAMPLE = (ROOT / "src/Config/LocalSecrets.example.hpp").read_text(encoding="utf-8")
PACKAGER = (ROOT / "scripts/ota_packager.py").read_text(encoding="utf-8")
EXTRA_COMMANDS = (ROOT / "scripts/pio_extra_commands.py").read_text(encoding="utf-8")


SECURE_ROUTES = {
    ("/api/v1/commands", "HTTP_POST"),
    ("/api/v1/wifi/networks", "HTTP_PUT"),
    ("/api/v1/wifi/networks", "HTTP_DELETE"),
    ("/api/v1/wifi/credentials", "HTTP_DELETE"),
    ("/api/v1/status", "HTTP_GET"),
    ("/api/v1/events", "HTTP_GET"),
    ("/api/v1/history", "HTTP_GET"),
    ("/api/v1/wifi/networks", "HTTP_GET"),
    ("/api/v1/ota/status", "HTTP_GET"),
    ("/api/v1/task-stats", "HTTP_GET"),
    ("/api/v1/brownout-log", "HTTP_GET"),
    ("/api/v1/reset-log", "HTTP_GET"),
}

COMMANDS = {
    "system.restart", "config.reload", "config.reset", "load.toggle",
    "display.acp", "display.weather", "display.date", "sound.cricket",
    "ota.start", "ota.status.reset", "update.check", "brownout.clear",
    "display.set", "ticker.set", "pwm.set", "silent.set", "acp_night.set",
    "weather.set", "cricket.set", "display.single", "pwm.period",
    "time_limit.set", "brightness.schedule", "brightness.manual",
    "logging.set", "firmware.set", "zip.set", "timezone.set", "timer.set",
    "alarm.set",
}


def check_secure_routes_and_frontend() -> None:
    routes = set(re.findall(r'server_\.on\("([^"]+)",\s*(HTTP_[A-Z]+)', SECURE))
    assert routes == SECURE_ROUTES, f"secure route mismatch: {sorted(routes ^ SECURE_ROUTES)}"
    for command in COMMANDS:
        assert f'"{command}"' in SECURE, f"missing secure command {command}"

    assert "/set/" not in FRONTEND, "frontend still calls a legacy mutation route"
    assert "/get/checkUpdate" not in FRONTEND, "frontend still mutates state via GET"
    assert "'X-Nixie-CSRF': '1'" in FRONTEND, "frontend omits the CSRF request header"
    assert "apiJson('/wifi/networks', 'PUT'" in FRONTEND, "Wi-Fi upsert does not use PUT JSON"
    assert "apiJson('/wifi/networks', 'DELETE'" in FRONTEND, "Wi-Fi delete does not use DELETE JSON"
    assert not re.search(r"/api/v1/wifi/[^\s'\"`]*\?", FRONTEND, re.IGNORECASE), (
        "Wi-Fi API uses a query string"
    )
    assert "encodeURIComponent(password)" not in FRONTEND and "URLSearchParams" not in FRONTEND, (
        "Wi-Fi credentials may enter a query string"
    )


def check_negative_input_guards() -> None:
    assert "MAX_JSON_BODY_SIZE = 1024" in (ROOT / "src/HTTP/SecureApi.hpp").read_text(encoding="utf-8")
    assert "body_too_large" in SECURE and "sendError(413" in SECURE
    assert "unsupported_media_type" in SECURE and "sendError(415" in SECURE
    assert "separator == std::string::npos" in SECURE and "contentType.size()" in SECURE, (
        "application/json without parameters must be accepted"
    )
    assert "invalid_json" in SECURE and "cJSON_ParseWithLengthOpts" in SECURE
    assert "Trailing data" in SECURE, "trailing JSON data is accepted"
    assert "previous->string" in SECURE, "duplicate JSON fields are not rejected"
    assert "!allowed" in SECURE, "unknown JSON fields are not rejected"
    assert "std::floor(item->valuedouble)" in SECURE, "fractional JSON numbers can reach integer fields"
    assert "unsupported_command" in SECURE and "sendError(422" in SECURE
    assert 'reason != "user" && reason != "ota"' in SECURE, "restart reason is not validated"

    # Representative negative boundaries must be enforced in the implementation.
    boundaries = (
        ('"digit", 0, 59', "single digit"),
        ('"microseconds", 100, 1000000', "PWM period"),
        ('"value", 0, 511', "logging mask"),
        ('"seconds", 1, 604800', "timer"),
        ('"brightness", 0, 100', "manual brightness"),
        ('"priority", 0, 254', "Wi-Fi priority"),
    )
    for marker, description in boundaries:
        assert marker in SECURE, f"missing negative range guard for {description}"
    assert "value.size() == 64" in SECURE and "value.size() < 8" in SECURE, (
        "WPA password length/PSK validation missing"
    )
    assert "value.size() > 32" in SECURE and "isSsid" in SECURE, "SSID size validation missing"


def check_csrf_auth_and_legacy_gate() -> None:
    for marker in ("X-Nixie-CSRF", "Sec-Fetch-Site", "Origin", "Host"):
        assert marker in SECURE, f"missing CSRF input {marker}"
    assert "cross-site" in SECURE and "csrf_rejected" in SECURE
    assert "Access-Control-Allow-Origin" not in PORTAL, "portal exposes its CSRF token through CORS"
    assert "configureBasicAuth" in NATIVE and "Authorization" in NATIVE
    assert "WWW-Authenticate" in NATIVE and "send(401" in NATIVE
    assert "constantTimeEqual" in NATIVE, "authentication header comparison is not constant-time"
    assert "Authorization" not in HTTP, "HTTP layer should never log or process the auth secret"
    assert 'requestPath_.rfind("/set/", 0) == 0' in NATIVE
    assert 'requestPath_ == "/get/checkUpdate"' in NATIVE
    assert "send(410" in NATIVE, "legacy mutations are not disabled externally"
    for status in (401, 410, 413, 415, 422):
        assert f"case {status}:" in NATIVE, f"HTTP status {status} is not mapped"


def check_secret_and_weather_transport() -> None:
    assert "OPENWEATHER_API_KEY" not in PLATFORMIO, "weather key remains in build options"
    assert "src/Config/LocalSecrets.hpp" in GITIGNORE, "local secrets file is not ignored"
    assert "/private_bin/" in GITIGNORE, "private firmware artifacts are not ignored"
    defines = re.findall(r'#define\s+(OPENWEATHER_API_KEY|NIXIE_HTTP_USERNAME|NIXIE_HTTP_PASSWORD)\s+"([^"]*)"', SECRETS_EXAMPLE)
    assert len(defines) == 3 and all(not value for _, value in defines), "example contains a secret"
    # Public firmware deliberately includes the local weather key. The source
    # credentials remain ignored; all packaging targets share the release path.
    assert "OUTPUT_DIR = PUBLIC_OUTPUT_DIR" in PACKAGER
    assert "shutil.copy" not in EXTRA_COMMANDS, "custom targets bypass the OTA packager"

    assert 'config.host = "api.openweathermap.org"' in WEATHER
    assert "HTTP_TRANSPORT_OVER_SSL" in WEATHER
    assert "esp_crt_bundle_attach" in WEATHER, "weather TLS does not use the trusted CA bundle"
    assert "skip_cert_common_name_check = false" in WEATHER, "TLS hostname verification is disabled"
    assert "disable_auto_redirect = true" in WEATHER, "redirect could disclose the API key"
    assert "http://api.openweathermap.org/" not in WEATHER
    assert 'esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN)' in WEATHER
    assert 'esp_log_level_set("HTTP_CLIENT", ESP_LOG_INFO)' not in WEATHER
    for forbidden in ("API key length", "content = %s", "Requesting URL", "Raw JSON response"):
        assert forbidden not in WEATHER, f"secret-bearing weather log remains: {forbidden}"


def main() -> int:
    checks = (
        check_secure_routes_and_frontend,
        check_negative_input_guards,
        check_csrf_auth_and_legacy_gate,
        check_secret_and_weather_transport,
    )
    for check in checks:
        check()
        print(f"PASS {check.__name__}")
    print("PASS HTTP/API security contract")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
