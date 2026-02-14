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
}
