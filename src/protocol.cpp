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

int startReadingData(const bool& stayConnected, std::vector<std::unique_ptr<Frame>>& frames, std::mutex& framesMutex, Glib::Dispatcher& dispatcher)
{
    const int port = open("/dev/ttyUSB0", O_RDONLY | O_NOCTTY);

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

    //std::cout << std::hex << std::setfill('0');
    uint8_t buf[255]{};
    while (stayConnected)
    {
        int count = read(port, buf, 255);
        if (count == -1)
        {
            std::cerr << "I/O error\n";
            return 1;
        }
        if (count == 0)
        {
            std::cout << "EOF\n";
            break;
        }
#if 0
        for (int j{}; j < count; ++j)
        {
            std::cout << std::bitset<8>(buf[j]) << ' ';
        }
        std::cout << buf[0] << '\n';
#endif

        //if (buf[0] == 0)
        //    continue;
        //std::cout << std::bitset<8>(buf[0]);

        auto frame = std::make_unique<Frame>(buf, std::chrono::system_clock::now());
#if 0
        std::cout << '"';
        std::cout << (frame->isNegative ? "-" : " ");
        std::cout << Frame::digitToChar(frame->getDigitThousandVal());
        std::cout << Frame::digitToChar(frame->getDigitHundredVal());
        std::cout << Frame::digitToChar(frame->getDigitTenVal());
        std::cout << Frame::digitToChar(frame->getDigitSingleVal());
        std::cout << '"';
        std::cout << " = " << frame->getFloatVal() << ' ' << frame->getUnitStr();
        std::cout << '\n';
        std::cout << std::flush;
#endif

        {
            std::lock_guard<std::mutex> guard = std::lock_guard{framesMutex};
            frames.push_back(std::move(frame));
        }
        dispatcher.emit();
    }
    tcsetattr(port, TCSANOW, &oldTio);
    return 0;
}
