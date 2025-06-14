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
        Memory::saveGlobals();
        esp_timer_stop(self->timer_);
    }
}
