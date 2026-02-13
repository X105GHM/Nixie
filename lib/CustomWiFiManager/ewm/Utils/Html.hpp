#pragma once
#include <Arduino.h>

namespace ewm::utils
{
    String html_escape(const String& s);
    String json_escape(const String& s);
}
