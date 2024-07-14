#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <glibmm/dispatcher.h>
#include <gdkmm/rgba.h>
#include "Frame.h"

enum class ConnStatus
{
    Closed,         // Default
    Connecting,     // Opening port and initializing
    Connected,      // The reading loop has started
    FailedToOpen,   // Failed to open port
    IOError,        // I/O error while reading
    Timeout,        // Reading timed out -- No data to read (multimeter is OFF)
    Eof,            // EOF -- Cable got unplugged
};

std::string connStatusToStr(ConnStatus cs);
std::string connStatusGetColor(ConnStatus cs);

void startReadingData(
        const std::atomic<bool>& keepThreadAlive, std::atomic<bool>& stayConnected, ConnStatus& connStatus,
        std::vector<std::unique_ptr<Frame>>& frames, std::mutex& framesMutex,
        Glib::Dispatcher& dispatcher);
