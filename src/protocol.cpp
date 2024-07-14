#include <cassert>
#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#else
#error Only Linux is supported
#endif
#include <iostream>
#include <stdint.h>
#include <chrono>
#include "protocol.h"

#define BAUDRATE 2400
#define READ_MIN 14
#define READ_TIMEOUT_USEC 1000000

std::string connStatusToStr(ConnStatus cs)
{
    switch (cs)
    {
    case ConnStatus::Closed:        return "Closed";
    case ConnStatus::Connecting:    return "Connecting";
    case ConnStatus::Connected:     return "Connected";
    case ConnStatus::FailedToOpen:  return "Failed to open port";
    case ConnStatus::IOError:       return "Communication error";
    case ConnStatus::Timeout:       return "Timed out";
    case ConnStatus::Eof:           return "Interrupted";
    }
    assert(false);
}

std::string connStatusGetColor(ConnStatus cs)
{
    switch (cs)
    {
    case ConnStatus::Closed:        return "teal";
    case ConnStatus::Connecting:    return "yellow";
    case ConnStatus::Connected:     return "lime";
    case ConnStatus::FailedToOpen:  return "red";
    case ConnStatus::IOError:       return "red";
    case ConnStatus::Timeout:       return "red";
    case ConnStatus::Eof:           return "red";
    }
    assert(false);
}

void startReadingData(const bool& stayConnected, ConnStatus& connStatus, std::vector<std::unique_ptr<Frame>>& frames, std::mutex& framesMutex, Glib::Dispatcher& dispatcher)
{
    connStatus = ConnStatus::Connecting;
    dispatcher.emit();

    const int port = open("/dev/ttyUSB0", O_RDONLY | O_NOCTTY);

    if (port == -1)
    {
        std::cerr << "Failed to open port: " << strerror(errno) << '\n';
        connStatus = ConnStatus::FailedToOpen;
        dispatcher.emit();
        return;
    }

    std::cout << "Opened port, handle: " << port << '\n';

    termios oldTio = termios{};
    tcgetattr(port, &oldTio);

    tcflush(port, TCIFLUSH);

    termios newTio = termios{};
    newTio.c_cflag = CRTSCTS | CS8 | CLOCAL | CREAD;
    newTio.c_iflag = IGNPAR;
    newTio.c_oflag = 0;
    newTio.c_lflag = 0;
    newTio.c_cc[VTIME] = 0;
    newTio.c_cc[VMIN] = READ_MIN;
    cfsetispeed(&newTio, B2400);
    tcsetattr(port, TCSANOW, &newTio);

    uint8_t buf[255]{};

    std::cout << "Configured port\n";

    connStatus = ConnStatus::Connected;
    dispatcher.emit();
    while (stayConnected)
    {
        fd_set fdSet;
        FD_ZERO(&fdSet);
        FD_SET(port, &fdSet);
        timeval timeout{.tv_usec = READ_TIMEOUT_USEC};
        int retVal = select(port+1, &fdSet, nullptr, nullptr, &timeout);
        if (retVal == -1)
        {
            std::cerr << "select() error: " << strerror(errno) << '\n';
            connStatus = ConnStatus::IOError;
            dispatcher.emit();
            return;
        }
        if (retVal == 0)
        {
            std::cerr << "Timed out" << '\n';
            connStatus = ConnStatus::Timeout;
            dispatcher.emit();
            return;
        }

        int count = read(port, buf, 255);
        if (count == -1)
        {
            std::cerr << "I/O error: " << strerror(errno) << '\n';
            connStatus = ConnStatus::IOError;
            dispatcher.emit();
            return;
        }
        if (count == 0)
        {
            std::cout << "EOF\n";
            break;
        }

        auto frame = std::make_unique<Frame>(buf, std::chrono::system_clock::now());
        {
            std::lock_guard<std::mutex> guard = std::lock_guard{framesMutex};
            frames.push_back(std::move(frame));
        }
        dispatcher.emit();
    }
    tcsetattr(port, TCSANOW, &oldTio);
    connStatus = ConnStatus::Eof;
    dispatcher.emit();
}
