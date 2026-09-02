#include "ewm/Utils/Html.hpp"

namespace ewm::utils
{
    std::string html_escape(const std::string& s)
    {
        std::string r;
        r.reserve(s.size());
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
