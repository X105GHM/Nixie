"""Run production OTA start/worker/cleanup methods with fake RTOS and transport.

No device is contacted. Only the actual download method is replaced, allowing
allocation failure, repeated starts and early cleanup paths to run on the host.
"""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

PREAMBLE = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

using BaseType_t = int;
using UBaseType_t = unsigned;
using StackType_t = uint8_t;
struct StaticTask_t {};
struct StaticSemaphore_t {};
using SemaphoreHandle_t = StaticSemaphore_t*;
using TaskHandle_t = StaticTask_t*;
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_STATE = 2;
constexpr int pdTRUE = 1, portMAX_DELAY = -1;
constexpr uint32_t MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2;
constexpr int pdMS_TO_TICKS(int ms) { return ms; }
enum class LoggerType { OTA };
struct Logger { template<class... T> static void log(T...) {} };
struct esp_vfs_spiffs_conf_t {
    const char* base_path;
    const char* partition_label;
    int max_files;
    bool format_if_mount_failed;
};

static bool failCreate = false, failLock = false;
static int createCalls = 0, notifications = 0, suspends = 0, resumes = 0;
static int unmountResult = ESP_OK, mountResult = ESP_OK, downloadResult = ESP_OK;
static int downloads = 0;
static void (*workerEntry)(void*) = nullptr;
static void* workerContext = nullptr;
static std::string downloadedUrl;
static std::atomic_bool displayEnabled{true};
static StaticTask_t displayTask;
static TaskHandle_t displayTaskHandle = &displayTask;
struct WorkerWaiting {};

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* p) { return p; }
int xSemaphoreTake(SemaphoreHandle_t, int) { return failLock ? 0 : pdTRUE; }
void xSemaphoreGive(SemaphoreHandle_t) {}
TaskHandle_t xTaskCreateStaticPinnedToCore(void (*entry)(void*), const char*,
    uint32_t bytes, void* context, int, StackType_t* stack, StaticTask_t* tcb, int) {
    ++createCalls;
    assert(bytes == 18432 && stack && tcb);
    if (failCreate) return nullptr;
    workerEntry = entry;
    workerContext = context;
    return tcb;
}
void xTaskNotifyGive(TaskHandle_t) { ++notifications; }
uint32_t ulTaskNotifyTake(int, int) {
    if (!notifications) throw WorkerWaiting{};
    notifications = 0;
    return 1;
}
void vTaskSuspend(TaskHandle_t) { ++suspends; }
void vTaskResume(TaskHandle_t) { ++resumes; }
void vTaskDelay(int) {}
uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 8192; }
size_t heap_caps_get_free_size(uint32_t) { return 60000; }
size_t heap_caps_get_largest_free_block(uint32_t) { return 1024; }
const char* esp_err_to_name(int e) { return e == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }
int esp_vfs_spiffs_unregister(const char*) { return unmountResult; }
int esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t*) { return mountResult; }
'''

TESTS = r'''
esp_err_t OTAManager::checkAndUpdate(const std::string& url) noexcept {
    assert(!displayEnabled);
    ++downloads;
    downloadedUrl = url;
    return downloadResult;
}

void runWorker() {
    try { workerEntry(workerContext); }
    catch (const WorkerWaiting&) {}
}

int main() {
    static OTAManager ota;
    failLock = true;
    assert(!ota.startAsync("lock-failure"));
    failLock = false;
    assert(!ota.isRunning() && displayEnabled);

    failCreate = true;
    assert(!ota.startAsync("create-failure"));
    assert(!ota.isRunning() && displayEnabled && notifications == 0);
    failCreate = false;

    // Internal heap has only 1 KB contiguous: the reserved 18 KB stack works.
    assert(ota.startAsync("first"));
    assert(ota.isRunning() && displayEnabled);
    assert(!ota.startAsync("must-not-replace-first"));
    ota.resetStatus();
    assert(ota.getStatusJson().find("OTA started") != std::string::npos);
    runWorker();
    assert(downloadedUrl == "first" && downloads == 1);
    assert(!ota.isRunning() && displayEnabled && suspends == resumes);
    assert(ota.getStatusJson().find("\"otaStackMinimumFreeBytes\":8192") != std::string::npos);
    const int initialized = createCalls;

    // Reuse the blocked worker and preserve an intentionally disabled display.
    displayEnabled = false;
    assert(ota.startAsync("second"));
    runWorker();
    assert(downloadedUrl == "second" && downloads == 2);
    assert(!ota.isRunning() && !displayEnabled && createCalls == initialized);

    displayEnabled = true;
    unmountResult = ESP_FAIL;
    assert(ota.startAsync("unmount-failure"));
    runWorker();
    assert(downloads == 2 && displayEnabled && !ota.isRunning());
    assert(suspends == resumes);

    unmountResult = ESP_OK;
    mountResult = ESP_FAIL;
    assert(ota.startAsync("remount-failure"));
    runWorker();
    assert(displayEnabled && !ota.isRunning() && suspends == resumes);
    assert(ota.getStatusJson().find("SPIFFS remount failed") != std::string::npos);

    mountResult = ESP_OK;
    downloadResult = ESP_FAIL;
    assert(ota.startAsync("download-failure"));
    runWorker();
    assert(displayEnabled && !ota.isRunning() && suspends == resumes);
    assert(ota.getStatusJson().find("OTA failed") != std::string::npos);
    assert(createCalls == initialized);
}
'''


def main() -> None:
    compiler = shutil.which("g++")
    if not compiler:
        raise SystemExit("g++ is required for the OTA lifecycle test")
    header = (ROOT / "src/OTA/OTA.hpp").read_text(encoding="utf-8")
    source = (ROOT / "src/OTA/OTA.cpp").read_text(encoding="utf-8")
    declaration = header[header.index("class OTAManager"):]
    display_guard = source[source.index("namespace\n{"):source.index("OTAManager& OTAManager::instance")]
    constructor = source[source.index("OTAManager::OTAManager"):source.index("static std::string normalizeVersion")]
    lifecycle = source[source.index("bool OTAManager::startAsync"):source.index("std::optional<std::string> OTAManager::fetchManifestVersion")]
    # The fake scheduler unwinds when the persistent task blocks, which requires
    # removing noexcept in this host translation unit only.
    production = (declaration + display_guard + constructor + lifecycle).replace("noexcept", "")
    with tempfile.TemporaryDirectory(prefix="nixie-ota-test-") as directory:
        test_source = Path(directory) / "ota_test.cpp"
        executable = Path(directory) / "ota_test.exe"
        test_source.write_text(PREAMBLE + production + TESTS.replace("noexcept", ""), encoding="utf-8")
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        str(test_source), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    print("PASS OTA lifecycle: allocation failures, repeated starts, busy rejection, display and SPIFFS cleanup")


if __name__ == "__main__":
    main()
