#include "ewm/Utils/MiniJson.hpp"

namespace
{
    static void json_skip_ws(const String &b, int &i)
    {
        while (i < (int)b.length())
        {
            char c = b[i];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
                ++i;
                continue;
            }
            break;
        }
    }

    static bool json_read_string(const String &b, int &i, String &out)
    {
        if (i >= (int)b.length() || b[i] != '"')
            return false;
        ++i;
        out = "";

        while (i < (int)b.length())
        {
            char c = b[i++];
            if (c == '"')
                return true;
            if (c == '\\')
            {
                if (i >= (int)b.length())
                    return false;
                char e = b[i++];
                switch (e)
                {
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                case '/':
                    out += '/';
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u':
                {
                    if (i + 4 > (int)b.length())
                        return false;
                    uint16_t v = 0;
                    for (int k = 0; k < 4; k++)
                    {
                        char h = b[i++];
                        v <<= 4;
                        if (h >= '0' && h <= '9')
                            v |= (h - '0');
                        else if (h >= 'a' && h <= 'f')
                            v |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F')
                            v |= (h - 'A' + 10);
                        else
                            return false;
                    }
                    out += (v <= 0x7F) ? (char)v : '?';
                    break;
                }
                default:
                    return false;
                }
            }
            else
                out += c;
        }
        return false;
    }
}

namespace ewm::utils
{
    bool json_get_string(const String &b, const char *key, String &out)
    {
        String k = String('"') + key + '"';
        int p = b.indexOf(k);
        if (p < 0)
            return false;
        p = b.indexOf(':', p + k.length());
        if (p < 0)
            return false;
        ++p;
        json_skip_ws(b, p);
        return json_read_string(b, p, out);
    }

    bool json_get_int(const String &b, const char *key, int &out)
    {
        String k = String('"') + key + '"';
        int p = b.indexOf(k);
        if (p < 0)
            return false;
        p = b.indexOf(':', p + k.length());
        if (p < 0)
            return false;
        ++p;
        json_skip_ws(b, p);

        bool neg = false;
        if (p < (int)b.length() && b[p] == '-')
        {
            neg = true;
            ++p;
        }

        long v = 0;
        bool any = false;
        while (p < (int)b.length())
        {
            char c = b[p];
            if (c < '0' || c > '9')
                break;
            any = true;
            v = v * 10 + (c - '0');
            ++p;
        }
        if (!any)
            return false;

        out = neg ? -(int)v : (int)v;
        return true;
    }

    bool json_get_order_array(const String &b, std::vector<String> &order)
    {
        order.clear();
        int p = b.indexOf("\"order\"");
        if (p < 0)
            return false;
        p = b.indexOf('[', p);
        if (p < 0)
            return false;
        ++p;

        while (p < (int)b.length())
        {
            json_skip_ws(b, p);
            if (p < (int)b.length() && b[p] == ']')
                return true;

            String s;
            if (!json_read_string(b, p, s))
                return false;
            order.push_back(s);

            json_skip_ws(b, p);
            if (p < (int)b.length() && b[p] == ',')
            {
                ++p;
                continue;
            }
            if (p < (int)b.length() && b[p] == ']')
                return true;
        }
        return false;
    }

    String json_escape(const String &in)
    {
        String out;
        out.reserve(in.length() + 8);

        auto hex = [](uint8_t v) -> char
        {
            v &= 0x0F;
            return (v < 10) ? char('0' + v) : char('A' + (v - 10));
        };

        for (int i = 0; i < (int)in.length(); ++i)
        {
            const char c = in[i];
            switch (c)
            {
            case '\"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
            {
                const uint8_t u = (uint8_t)c;
                if (u < 0x20)
                {
                    out += "\\u00";
                    out += hex(u >> 4);
                    out += hex(u);
                }
                else
                {
                    out += c;
                }
            }
            }
        }
        return out;
    }
}
