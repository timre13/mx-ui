#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <glibmm/dispatcher.h>
#include "Frame.h"

int startReadingData(const bool& stayConnected, std::vector<std::unique_ptr<Frame>>& frames, std::mutex& framesMutex, Glib::Dispatcher& dispatcher);
