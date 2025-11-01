#include "Brownout.hpp"

static const char *TAG = "Brownout";

Brownout::Brownout(SupplyWatch &sw, HSS &hss, Memory::PersistentStorage &storage, uint32_t interval_us) noexcept
    : sw_(sw), hss_(hss), storage_(storage), timer_(nullptr), interval_us_(interval_us)
{
}

void Brownout::start() noexcept
{
    esp_timer_create_args_t args{};
    args.callback = &Brownout::timerCallback;
    args.arg = this;
    args.name = "brownout_timer";
    esp_timer_create(&args, &timer_);
    esp_timer_start_periodic(timer_, interval_us_);
}

void Brownout::stop() noexcept
{
    if (timer_)
    {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void Brownout::timerCallback(void *arg)
{
    auto *self = static_cast<Brownout *>(arg);
    float v12 = self->sw_.read12V();
    if (v12 < 10.0f)
    {
        self->hss_.disable190();
        self->hss_.disable160();
        self->hss_.disableResistorReduction();
        displayEnabled = false;
        Memory::saveGlobals();

        time_t now = 0;
        time(&now);
        struct tm tm_local{};
        localtime_r(&now, &tm_local);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_local);

        std::string json =
            "{\n"
            "  \"timestamp\": \"" + std::string(buf) + "\",\n"
            "  \"event\": \"brownout\",\n"
            "  \"voltage\": " + std::to_string(v12) + "\n"
            "}";

        Logger::log(LoggerType::GENERAL, "%s", json.c_str());

        if (auto err = self->storage_.saveEventLog(json); err != ESP_OK)
        {
            Logger::log(LoggerType::GENERAL, "Brownout: saveEventLog failed: %s", esp_err_to_name(err));
        }

        esp_timer_stop(self->timer_);
    }
}
