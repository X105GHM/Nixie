"""Opt-in negative tests against a flashed Nixie clock.

Set NIXIE_TEST_BASE_URL, and optionally NIXIE_TEST_USERNAME/PASSWORD when Basic
authentication is enabled. The cases are deliberately invalid and must not
change device state.
"""

import base64
import json
import os
import sys
import urllib.error
import urllib.request


BASE = os.environ.get("NIXIE_TEST_BASE_URL", "").rstrip("/")
USERNAME = os.environ.get("NIXIE_TEST_USERNAME", "")
PASSWORD = os.environ.get("NIXIE_TEST_PASSWORD", "")


def request(path: str, method: str, body=None, csrf=True, content_type="application/json") -> int:
    headers = {}
    data = None
    if body is not None:
        data = body if isinstance(body, bytes) else json.dumps(body).encode("utf-8")
        headers["Content-Type"] = content_type
    if csrf:
        headers["X-Nixie-CSRF"] = "1"
    if USERNAME or PASSWORD:
        token = base64.b64encode(f"{USERNAME}:{PASSWORD}".encode()).decode()
        headers["Authorization"] = f"Basic {token}"
    try:
        with urllib.request.urlopen(
            urllib.request.Request(f"{BASE}{path}", data=data, headers=headers, method=method),
            timeout=5,
        ) as response:
            return response.status
    except urllib.error.HTTPError as error:
        return error.code


def main() -> int:
    if not BASE:
        print("SKIP live HTTP security tests (NIXIE_TEST_BASE_URL is not set)")
        return 0

    cases = (
        ("legacy mutation", "/set/ON", "GET", None, True, "application/json", 410),
        ("missing CSRF", "/api/v1/commands", "POST", {"command": "display.set", "enabled": True}, False, "application/json", 403),
        ("wrong media type", "/api/v1/commands", "POST", b"{}", True, "text/plain", 415),
        ("malformed JSON", "/api/v1/commands", "POST", b"{", True, "application/json", 400),
        ("unknown field", "/api/v1/commands", "POST", {"command": "display.set", "enabled": True, "extra": 1}, True, "application/json", 400),
        ("wrong bool type", "/api/v1/commands", "POST", {"command": "display.set", "enabled": 1}, True, "application/json", 400),
        ("out-of-range digit", "/api/v1/commands", "POST", {"command": "display.single", "digit": 60}, True, "application/json", 400),
        ("unsupported command", "/api/v1/commands", "POST", {"command": "does.not.exist"}, True, "application/json", 422),
        ("oversized body", "/api/v1/commands", "POST", b"{" + b" " * 1100 + b"}", True, "application/json", 413),
        ("invalid WPA password", "/api/v1/wifi/networks", "PUT", {"ssid": "negative-test", "password": "short", "priority": 1}, True, "application/json", 400),
    )
    for name, path, method, body, csrf, content_type, expected in cases:
        actual = request(path, method, body, csrf, content_type)
        assert actual == expected, f"{name}: expected {expected}, got {actual}"
        print(f"PASS {name}: {actual}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
