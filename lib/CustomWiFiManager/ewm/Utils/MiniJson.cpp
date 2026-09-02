#include "ewm/Utils/MiniJson.hpp"

#include <cstdint>

namespace
{
    void jsonSkipWhitespace(const std::string &body, size_t &position)
    {
        while (position < body.size())
        {
            const char c = body[position];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            {
                break;
            }
            ++position;
        }
    }

    bool jsonReadString(const std::string &body, size_t &position, std::string &out)
    {
        if (position >= body.size() || body[position] != '"')
        {
            return false;
        }

        ++position;
        out.clear();
        while (position < body.size())
        {
            const char c = body[position++];
            if (c == '"')
            {
                return true;
            }
            if (c != '\\')
            {
                out += c;
                continue;
            }

            if (position >= body.size())
            {
                return false;
            }

            const char escaped = body[position++];
            switch (escaped)
            {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u':
                {
                    if (position + 4 > body.size())
                    {
                        return false;
                    }

                    uint16_t value = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        const char hex = body[position++];
                        value <<= 4;
                        if (hex >= '0' && hex <= '9') value |= static_cast<uint16_t>(hex - '0');
                        else if (hex >= 'a' && hex <= 'f') value |= static_cast<uint16_t>(hex - 'a' + 10);
                        else if (hex >= 'A' && hex <= 'F') value |= static_cast<uint16_t>(hex - 'A' + 10);
                        else return false;
                    }
                    out += value <= 0x7f ? static_cast<char>(value) : '?';
                    break;
                }
                default:
                    return false;
            }
        }
        return false;
    }

    bool findValue(const std::string &body, const char *key, size_t &position)
    {
        if (!key)
        {
            return false;
        }

        const std::string quotedKey = "\"" + std::string(key) + "\"";
        position = body.find(quotedKey);
        if (position == std::string::npos)
        {
            return false;
        }

        position = body.find(':', position + quotedKey.size());
        if (position == std::string::npos)
        {
            return false;
        }

        ++position;
        jsonSkipWhitespace(body, position);
        return true;
    }
}

namespace ewm::utils
{
    bool json_get_string(const std::string &body, const char *key, std::string &out)
    {
        size_t position = 0;
        return findValue(body, key, position) && jsonReadString(body, position, out);
    }

    bool json_get_int(const std::string &body, const char *key, int &out)
    {
        size_t position = 0;
        if (!findValue(body, key, position))
        {
            return false;
        }

        bool negative = false;
        if (position < body.size() && body[position] == '-')
        {
            negative = true;
            ++position;
        }

        long value = 0;
        bool any = false;
        while (position < body.size() && body[position] >= '0' && body[position] <= '9')
        {
            any = true;
            value = value * 10 + (body[position++] - '0');
        }
        if (!any)
        {
            return false;
        }

        out = static_cast<int>(negative ? -value : value);
        return true;
    }

    bool json_get_order_array(const std::string &body, std::vector<std::string> &order)
    {
        order.clear();
        size_t position = body.find("\"order\"");
        if (position == std::string::npos)
        {
            return false;
        }

        position = body.find('[', position);
        if (position == std::string::npos)
        {
            return false;
        }
        ++position;

        while (position < body.size())
        {
            jsonSkipWhitespace(body, position);
            if (position < body.size() && body[position] == ']')
            {
                return true;
            }

            std::string entry;
            if (!jsonReadString(body, position, entry))
            {
                return false;
            }
            order.push_back(std::move(entry));

            jsonSkipWhitespace(body, position);
            if (position < body.size() && body[position] == ',')
            {
                ++position;
                continue;
            }
            if (position < body.size() && body[position] == ']')
            {
                return true;
            }
            return false;
        }
        return false;
    }

    std::string json_escape(const std::string &input)
    {
        std::string output;
        output.reserve(input.size() + 8);

        auto hex = [](uint8_t value) -> char
        {
            value &= 0x0f;
            return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('A' + value - 10);
        };

        for (const char c : input)
        {
            switch (c)
            {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                {
                    const auto value = static_cast<uint8_t>(c);
                    if (value < 0x20)
                    {
                        output += "\\u00";
                        output += hex(value >> 4);
                        output += hex(value);
                    }
                    else
                    {
                        output += c;
                    }
                }
            }
        }
        return output;
    }
}
