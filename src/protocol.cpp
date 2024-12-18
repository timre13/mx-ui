#include <cassert>
#include <iostream>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include "protocol.h"
#ifdef __linux__
#   include <unistd.h>
#   include <fcntl.h>
#   include <termios.h>
#else
#   include <Windows.h>
#endif

#define BAUDRATE 2400
#define READ_MIN 14
#define READ_TIMEOUT_USEC 1000000

std::string connStatusToStr(ConnStatus cs)
{
    switch (cs)
    {
    case ConnStatus::Closed:        return "Disconnected";
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
    case ConnStatus::Closed:        return "white";
    case ConnStatus::Connecting:    return "yellow";
    case ConnStatus::Connected:     return "lime";
    case ConnStatus::FailedToOpen:  return "red";
    case ConnStatus::IOError:       return "red";
    case ConnStatus::Timeout:       return "red";
    case ConnStatus::Eof:           return "red";
    }
    assert(false);
}

// Platform-specific state
struct PlatformState
{
#ifdef __linux__
    int port{-1};
    termios oldTio{};
#else
    HANDLE port{};
#endif
};

#ifdef __linux__

static int configurePort(PlatformState* state, const SerialDevice& dev)
{
    state->port = open(dev.path.c_str(), O_RDONLY | O_NOCTTY);

    if (state->port == -1)
    {
        std::cerr << "Failed to open port: " << strerror(errno) << '\n';
        return 1;
    }

    std::cout << "Opened port, handle: " << state->port << '\n';

    state->oldTio = termios{};
    tcgetattr(state->port, &state->oldTio);

    tcflush(state->port, TCIFLUSH);

    termios newTio = termios{};
    newTio.c_cflag = CRTSCTS | CS8 | CLOCAL | CREAD;
    newTio.c_iflag = IGNPAR;
    newTio.c_oflag = 0;
    newTio.c_lflag = 0;
    newTio.c_cc[VTIME] = 0;
    newTio.c_cc[VMIN] = READ_MIN;
    cfsetispeed(&newTio, B2400);
    tcsetattr(state->port, TCSANOW, &newTio);

    std::cout << "Configured port\n";

    return 0;
}

#else

static int configurePort(PlatformState* state)
{
    state->port = CreateFile("\\\\.\\COM3", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (state->port == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to open port (code " << GetLastError() << ")\n";
        return 1;
    }
    std::cout << "Opened port, handle: " << state->port << '\n';

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = READ_TIMEOUT_USEC/1000;
    if (SetCommTimeouts(state->port, &timeouts))
    {
        std::cerr << "Failed to set port timeout (code " << GetLastError() << ")\n";
        //return 1;
    }

    if (SetCommMask(state->port, EV_RXCHAR))
    {
        std::cerr << "Failed to set communications mask (code " << GetLastError() << ")\n";
        //return 1;
    }

    DCB dcbSerialParams{};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    BOOL status = GetCommState(state->port, &dcbSerialParams);
    if (status == FALSE)
    {
        std::cout << "Failed to get comm state (code " << GetLastError() << ")\n";
        //return 1;
    }
    dcbSerialParams.BaudRate = BAUDRATE;
    status = SetCommState(state->port, &dcbSerialParams);
    if (status == FALSE)
    {
        std::cout << "Failed to set comm state (code " << GetLastError() << ")\n";
        //return 1;
    }

    std::cout << "Configured port\n";

    return 0;
}

#endif

#ifdef __linux__

void closePort(PlatformState* state)
{
    if (tcsetattr(state->port, TCSANOW, &state->oldTio) == -1)
    {
        std::cerr << "Failed to reset port configuration\n";
    }
    if (close(state->port) == -1)
    {
        std::cerr << "Failed to close port\n";
    }
    else
    {
        std::cout << "Closed port\n";
    }
    state->oldTio = {};
    state->port = -1;
}

#else

void closePort(PlatformState* state)
{
    assert(state->port);
    if (CloseHandle(state->port) == 0)
    {
        std::cerr << "Failed to close port (code " << GetLastError() << ")\n";
    }
    else
    {
        std::cout << "Closed port\n";
    }
    state->port = nullptr;
}

#endif

#ifdef __linux__

static int readFrame(ConnStatus* connStatus, PlatformState* state, uint8_t* buf)
{
    fd_set fdSet;
    FD_ZERO(&fdSet);
    FD_SET(state->port, &fdSet);
    timeval timeout{.tv_usec = READ_TIMEOUT_USEC};
    int retVal = select(state->port+1, &fdSet, nullptr, nullptr, &timeout);
    if (retVal == -1)
    {
        std::cerr << "select() error: " << strerror(errno) << '\n';
        *connStatus = ConnStatus::IOError;
        return 1;
    }
    if (retVal == 0)
    {
        std::cerr << "Timed out" << '\n';
        *connStatus = ConnStatus::Timeout;
        return 1;
    }

    int count = read(state->port, buf, 255);
    if (count == -1)
    {
        std::cerr << "I/O error: " << strerror(errno) << '\n';
        *connStatus = ConnStatus::IOError;
        return 1;
    }
    if (count == 0)
    {
        std::cout << "EOF\n";
        *connStatus = ConnStatus::Eof;
        closePort(state);
        return 1;
    }
    return 0;
}

#else

static int readFrame(ConnStatus* connStatus, PlatformState* state, uint8_t* buf)
{
    DWORD dwEventMask;
	BOOL status = WaitCommEvent(state->port, &dwEventMask, nullptr);
	if (status == FALSE)
    {
        std::cout << "WaitCommEvent failed\n";
        closePort(state);
        *connStatus = ConnStatus::IOError;
		return 1;
	}

    DWORD bytesRead;
	status = ReadFile(state->port, buf, READ_MIN, &bytesRead, nullptr);
	if (status == FALSE)
    {
        std::cout << "ReadFile failed\n";
        closePort(state);
        *connStatus = ConnStatus::IOError;
		return 1;
	}
    if (bytesRead == 0)
    {
        std::cout << "EOF\n";
        closePort(state);
        *connStatus = ConnStatus::Eof;
		return 1;
    }
    return 0;
}

#endif

void startReadingData(
        const std::atomic<bool>& keepThreadAlive, std::atomic<bool>& stayConnected, ConnStatus& connStatus,
        std::vector<std::unique_ptr<Frame>>& frames, std::mutex& framesMutex,
        Glib::Dispatcher& dispatcher, const SerialDevice& device)
{
    std::cout << "Thread 0x" << std::hex << std::this_thread::get_id() << std::dec << " started\n";
    std::cout << "Active serial device: " << device.path << '\n';

    /*
     * while keepThreadAlive:
     *    while !stayConnected
     *    do setup
     *    while stayConnected
     */

    while (keepThreadAlive)
    {
        // Wait until a connection is requested
        while (!stayConnected && keepThreadAlive)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        connStatus = ConnStatus::Connecting;
        dispatcher.emit();

        PlatformState state;
        if (configurePort(&state, device))
        {
           connStatus = ConnStatus::FailedToOpen;
           dispatcher.emit();
           stayConnected = false;
           continue;
        }

        uint8_t buf[255]{};

        std::cout << "Configured port\n";

        connStatus = ConnStatus::Connected;
        dispatcher.emit();
        while (true)
        {
            if (!stayConnected || !keepThreadAlive)
            {
                std::cout << "Connection closed by user\n";
                closePort(&state);
                connStatus = ConnStatus::Closed;
                dispatcher.emit();
                break;
            }

            if (readFrame(&connStatus, &state, buf))
            {
                dispatcher.emit();
                stayConnected = false;
                break;
            }

            auto frame = std::make_unique<Frame>(buf, std::chrono::system_clock::now());
            {
                std::lock_guard<std::mutex> guard = std::lock_guard{framesMutex};
                frames.push_back(std::move(frame));
            }
            dispatcher.emit();
        }
    }
    std::cout << "Thread 0x" << std::hex << std::this_thread::get_id() << std::dec << " exited\n";
}

static std::string readLine(const std::filesystem::path& path)
{
    std::ifstream file{path};
    std::string output;
    std::getline(file, output);
    file.close();
    return output;
}

std::vector<SerialDevice> listSerialDevices()
{
    std::vector<SerialDevice> output;
    std::filesystem::directory_iterator dirIter{"/sys/class/tty"};
    for (const auto& file : dirIter)
    {
        if (!file.path().filename().string().starts_with("ttyUSB"))
            continue;

        const auto sysDevPath = std::filesystem::canonical(std::filesystem::canonical(file)/"../../../..");

        output.push_back({
                .manufacturer=readLine(sysDevPath/"manufacturer"),
                .product=readLine(sysDevPath/"product"),
                .path="/dev"/file.path().filename()
        });
    }
    std::sort(output.begin(), output.end(),
            [](const auto& x, const auto& y){ return x.path.compare(y.path); });
    return output;
}
