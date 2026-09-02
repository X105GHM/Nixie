#include "Brownout.hpp"

Brownout::Brownout(SupplyWatch &sw, HSS &hss, Memory::PersistentStorage &storage, uint32_t interval_us) noexcept
    : sw_(sw),
      hss_(hss),
      storage_(storage),
      timer_(nullptr),
      restore_timer_(nullptr),
      monitor_task_(nullptr),
      stop_requested_(false),
      interval_us_(interval_us)
{
}

void Brownout::start() noexcept
{
    if (timer_ || monitor_task_.load(std::memory_order_acquire))
    {
        return;
    }

    stop_requested_.store(false, std::memory_order_release);
    TaskHandle_t monitorTaskHandle = nullptr;
    if (xTaskCreatePinnedToCore(&Brownout::monitorTask, "BrownoutMonitor", 8192, this,  4, &monitorTaskHandle, 0) != pdPASS)
    {
        Logger::log(LoggerType::GENERAL, "Brownout: monitor task creation failed");
        return;
    }
    monitor_task_.store(monitorTaskHandle, std::memory_order_release);

    esp_timer_create_args_t args{};
    args.callback = &Brownout::timerCallback;
    args.arg = this;
    args.name = "brownout_timer";
    esp_err_t err = esp_timer_create(&args, &timer_);
    if (err != ESP_OK)
    {
        timer_ = nullptr;
        stop_requested_.store(true, std::memory_order_release);
        xTaskNotifyGive(monitorTaskHandle);
        Logger::log(LoggerType::GENERAL, "Brownout: timer creation failed: %s", esp_err_to_name(err));
        return;
    }

    esp_timer_create_args_t restoreArgs{};
    restoreArgs.callback = &Brownout::restoreDisplayCallback;
    restoreArgs.arg = this;
    restoreArgs.name = "brownout_restore";
    err = esp_timer_create(&restoreArgs, &restore_timer_);
    if (err != ESP_OK)
    {
        esp_timer_delete(timer_);
        timer_ = nullptr;
        restore_timer_ = nullptr;
        stop_requested_.store(true, std::memory_order_release);
        xTaskNotifyGive(monitorTaskHandle);
        Logger::log(LoggerType::GENERAL, "Brownout: restore timer creation failed: %s", esp_err_to_name(err));
        return;
    }

    err = esp_timer_start_periodic(timer_, interval_us_);
    if (err != ESP_OK)
    {
        esp_timer_delete(restore_timer_);
        esp_timer_delete(timer_);
        restore_timer_ = nullptr;
        timer_ = nullptr;
        stop_requested_.store(true, std::memory_order_release);
        xTaskNotifyGive(monitorTaskHandle);
        Logger::log(LoggerType::GENERAL, "Brownout: timer start failed: %s", esp_err_to_name(err));
    }
}

void Brownout::stop() noexcept
{
    stop_requested_.store(true, std::memory_order_release);

    if (timer_)
    {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
        timer_ = nullptr;
    }

    if (restore_timer_)
    {
        esp_timer_stop(restore_timer_);
        esp_timer_delete(restore_timer_);
        restore_timer_ = nullptr;
    }

    if (TaskHandle_t task = monitor_task_.load(std::memory_order_acquire))
    {
        xTaskNotifyGive(task);
    }
}

void Brownout::timerCallback(void *arg)
{
    auto *self = static_cast<Brownout *>(arg);
    if (!self)
    {
        return;
    }

    if (TaskHandle_t task = self->monitor_task_.load(std::memory_order_acquire))
    {
        xTaskNotifyGive(task);
    }
}

void Brownout::monitorTask(void *arg)
{
    auto *self = static_cast<Brownout *>(arg);
    if (!self)
    {
        vTaskDelete(nullptr);
        return;
    }

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (self->stop_requested_.load(std::memory_order_acquire))
        {
            break;
        }
        if (self->processSample())
        {
            break;
        }
    }

    self->monitor_task_.store(nullptr, std::memory_order_release);
    vTaskDelete(nullptr);
}

bool Brownout::processSample() noexcept
{
    const float v12 = sw_.read12V();
    if (v12 < 10.0f)
    {
        hss_.disable190();
        hss_.disable160();
        hss_.disableResistorReduction();
        displayEnabled = false;
        Memory::saveGlobals();

        time_t now = 0;
        time(&now);
        struct tm tm_local{};
        gmtime_r(&now, &tm_local);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_local);

        std::string json =
            "{\n"
            "  \"timestamp\": \"" + std::string(buf) + "\",\n"
            "  \"event\": \"brownout\",\n"
            "  \"voltage\": " + std::to_string(v12) + "\n"
            "}";

        Logger::log(LoggerType::GENERAL, "%s", json.c_str());

        if (auto err = storage_.saveEventLog(json); err != ESP_OK)
        {
            Logger::log(LoggerType::GENERAL, "Brownout: saveEventLog failed: %s", esp_err_to_name(err));
        }

        if (timer_)
        {
            esp_timer_stop(timer_);
        }
        if (!restore_timer_ || esp_timer_start_once(restore_timer_, 1000000) != ESP_OK)
        {
            displayEnabled = true;
        }
        return true;
    }
    return false;
}

void Brownout::restoreDisplayCallback(void *arg)
{
    auto *self = static_cast<Brownout *>(arg);
    if (self)
    {
        displayEnabled = true;
    }
}
