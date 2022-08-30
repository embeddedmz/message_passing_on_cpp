#pragma once

#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <thread>
#include <vector>

#include "observable/obs.h"

struct ResourceStatus
{
    int SomeValue;
    std::time_t Timestamp;
};

class ResourceManager
{
public:
    std::string StringFormat(std::string strFormat, ...)
    {
        va_list args;
        va_start(args, strFormat);
        size_t len = std::vsnprintf(NULL, 0, strFormat.c_str(), args);
        va_end(args);
        std::vector<char> vec(len + 1);
        va_start(args, strFormat);
        std::vsnprintf(&vec[0], len + 1, strFormat.c_str(), args);
        va_end(args);
        return &vec[0];
    }

    obs::signal<void(ResourceStatus)> ResourceStatusUpdatedSignal;

    void UpdateStatus(int value)
    {
        ResourceStatus status;
        status.SomeValue = value;
        status.Timestamp = std::time(nullptr);
        ResourceStatusUpdatedSignal(status);
    }

    std::string SendData(size_t threadId)
    {
        std::stringstream ss;
        ss << std::this_thread::get_id();

        std::size_t thisThreadId;
        ss >> thisThreadId;

        std::time_t now = std::time(NULL);
        std::tm* ptm = std::localtime(&now);
        char buffer[32];
        std::strftime(buffer, 32, "%d/%m/%Y %H:%M:%S", ptm);

        return StringFormat("Reply to thread Id %zu received at %s from thread %zu.",
            threadId,
            buffer,
            thisThreadId);
    }
};
