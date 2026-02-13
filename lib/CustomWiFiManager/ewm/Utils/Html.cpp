#include "ewm/Utils/Html.hpp"

namespace ewm::utils
{
    String html_escape(const String& s)
    {
        String r;
        r.reserve(s.length());
        for (char c : s)
        {
            switch (c)
            {
                case '&': r += "&amp;"; break;
                case '<': r += "&lt;"; break;
                case '>': r += "&gt;"; break;
                case '"': r += "&quot;"; break;
                case '\'': r += "&#39;"; break;
                default: r += c; break;
            }
        }
        return r;
    }

    String json_escape(const String& s)
    {
        String r;
        r.reserve(s.length() + 8);
        for (size_t i = 0; i < s.length(); ++i)
        {
            char c = s[i];
            switch (c)
            {
                case '\"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\b': r += "\\b"; break;
                case '\f': r += "\\f"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default:
                    if ((uint8_t)c < 0x20)
                    {
                        char buf[7];
                        sprintf(buf, "\\u%04x", (uint8_t)c);
                        r += buf;
                    }
                    else r += c;
            }
        }
        return r;
    }
}
