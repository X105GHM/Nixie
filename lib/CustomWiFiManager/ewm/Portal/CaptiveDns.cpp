#include "ewm/Portal/CaptiveDns.hpp"

#include <cstring>

#include "ewm/Log.hpp"
#include "lwip/inet.h"
#include "lwip/sockets.h"

namespace ewm
{
    bool CaptiveDns::start(uint32_t accessPointAddress)
    {
        if (task_.load(std::memory_order_acquire))
        {
            return true;
        }

        socketFd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketFd_ < 0)
        {
            EWM_LOG("Captive DNS socket creation failed");
            return false;
        }

        int reuse = 1;
        setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        timeval timeout{};
        timeout.tv_usec = 200000;
        setsockopt(socketFd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sockaddr_in bindAddress{};
        bindAddress.sin_family = AF_INET;
        bindAddress.sin_port = htons(53);
        bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(socketFd_, reinterpret_cast<sockaddr*>(&bindAddress), sizeof(bindAddress)) != 0)
        {
            EWM_LOG("Captive DNS bind failed");
            close(socketFd_);
            socketFd_ = -1;
            return false;
        }

        accessPointAddress_ = accessPointAddress;
        stopRequested_.store(false, std::memory_order_release);
        TaskHandle_t task = nullptr;
        if (xTaskCreatePinnedToCore(taskEntry, "ewm_dns", 6144, this, 2, &task, 0) != pdPASS)
        {
            close(socketFd_);
            socketFd_ = -1;
            return false;
        }
        task_.store(task, std::memory_order_release);
        return true;
    }

    void CaptiveDns::stop()
    {
        stopRequested_.store(true, std::memory_order_release);
        for (int attempt = 0; task_.load(std::memory_order_acquire) && attempt < 20; ++attempt)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    void CaptiveDns::taskEntry(void* arg)
    {
        static_cast<CaptiveDns*>(arg)->run();
    }

    void CaptiveDns::run()
    {
        uint8_t request[512]{};
        uint8_t response[512]{};

        while (!stopRequested_.load(std::memory_order_acquire))
        {
            sockaddr_storage source{};
            socklen_t sourceLength = sizeof(source);
            const int received = recvfrom(
                socketFd_,
                request,
                sizeof(request),
                0,
                reinterpret_cast<sockaddr*>(&source),
                &sourceLength);
            if (received <= 0)
            {
                continue;
            }

            const size_t responseLength = createResponse(
                request,
                static_cast<size_t>(received),
                response,
                sizeof(response));
            if (responseLength > 0)
            {
                sendto(
                    socketFd_,
                    response,
                    responseLength,
                    0,
                    reinterpret_cast<sockaddr*>(&source),
                    sourceLength);
            }
        }

        close(socketFd_);
        socketFd_ = -1;
        task_.store(nullptr, std::memory_order_release);
        vTaskDelete(nullptr);
    }

    size_t CaptiveDns::createResponse(
        const uint8_t* request,
        size_t requestLength,
        uint8_t* response,
        size_t capacity) const
    {
        if (!request || !response || requestLength < 17 || capacity < requestLength + 16)
        {
            return 0;
        }

        size_t position = 12;
        while (position < requestLength && request[position] != 0)
        {
            const uint8_t labelLength = request[position];
            if ((labelLength & 0xc0) != 0 || labelLength > 63 || position + labelLength + 1 >= requestLength)
            {
                return 0;
            }
            position += static_cast<size_t>(labelLength) + 1;
        }
        if (position + 5 > requestLength)
        {
            return 0;
        }

        const size_t questionEnd = position + 5;
        const uint16_t queryType = static_cast<uint16_t>((request[position + 1] << 8) | request[position + 2]);
        const bool answerAddress = queryType == 1 || queryType == 255;
        const size_t responseLength = questionEnd + (answerAddress ? 16 : 0);
        if (responseLength > capacity)
        {
            return 0;
        }

        std::memcpy(response, request, questionEnd);
        response[2] = static_cast<uint8_t>(0x84 | (request[2] & 0x01));
        response[3] = 0x00;
        response[6] = 0;
        response[7] = answerAddress ? 1 : 0;
        response[8] = response[9] = response[10] = response[11] = 0;

        if (!answerAddress)
        {
            return questionEnd;
        }

        position = questionEnd;
        response[position++] = 0xc0;
        response[position++] = 0x0c;
        response[position++] = 0x00;
        response[position++] = 0x01;
        response[position++] = 0x00;
        response[position++] = 0x01;
        response[position++] = 0x00;
        response[position++] = 0x00;
        response[position++] = 0x00;
        response[position++] = 0x3c;
        response[position++] = 0x00;
        response[position++] = 0x04;
        std::memcpy(response + position, &accessPointAddress_, sizeof(accessPointAddress_));
        return responseLength;
    }
}
