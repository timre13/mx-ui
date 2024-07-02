#include <cmath>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <bitset>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <gtkmm.h>
#include <gtkmm/application.h>

#define BAUDRATE 2400
#define READ_MIN 14

class Frame
{
public:
    enum Digit : int
    {
        D0,
        D1,
        D2,
        D3,
        D4,
        D5,
        D6,
        D7,
        D8,
        D9,
        DEmpty,
        DL,
    };

    bool AUTO{};
    bool DC{};
    bool AC{};
    bool isNegative{};
    uint8_t digitThousand{};
    uint8_t digitHundred{};
    uint8_t digitTen{};
    uint8_t digitSingle{};
    bool decimalPoint1{};
    bool decimalPoint2{};
    bool decimalPoint3{};

    Frame(uint8_t buf[14])
    {
        AUTO            = (buf[0] & (1 << 1)) >> 1;
        DC              = (buf[0] & (1 << 2)) >> 2;
        AC              = (buf[0] & (1 << 3)) >> 3;
        digitThousand   = ((buf[1] & 7) << 4) | (buf[2] & 15);
        digitHundred    = ((buf[3] & 7) << 4) | (buf[4] & 15);
        digitTen        = ((buf[5] & 7) << 4) | (buf[6] & 15);
        digitSingle     = ((buf[7] & 7) << 4) | (buf[8] & 15);

        isNegative      = (buf[1] & (1 << 3)) >> 3;
        decimalPoint1   = (buf[3] & (1 << 3)) >> 3;
        decimalPoint2   = (buf[5] & (1 << 3)) >> 3;
        decimalPoint3   = (buf[7] & (1 << 3)) >> 3;
    }

    Digit getDigitThousandVal() const { return digitToVal(digitThousand); }
    Digit getDigitHundredVal()  const { return digitToVal(digitHundred); }
    Digit getDigitTenVal()      const { return digitToVal(digitTen); }
    Digit getDigitSingleVal()   const { return digitToVal(digitSingle); }

    static char digitToChar(Digit digit)
    {
        if (digit == DEmpty)
            return ' ';
        if (digit == DL)
            return 'L';
        return digit + '0';
    }

    float getFloatVal() const
    {
        if (getDigitThousandVal() > D9
            || getDigitHundredVal() > D9
            || getDigitTenVal() > D9
            || getDigitSingleVal() > D9)
            return NAN;

        float result = getDigitSingleVal() + getDigitTenVal()*10
            + getDigitHundredVal()*100 + getDigitThousandVal()*1000;
        if (isNegative)
            result = -result;

        if (decimalPoint3)
            result /= 10;
        if (decimalPoint2)
            result /= 100;
        if (decimalPoint1)
            result /= 1000;

        return result;
    }

private:
    static Digit digitToVal(uint8_t digit)
    {
        switch (digit)
        {
        case 0b01111101: return D0;
        case 0b00000101: return D1;
        case 0b01011011: return D2;
        case 0b00011111: return D3;
        case 0b00100111: return D4;
        case 0b00111110: return D5;
        case 0b01111110: return D6;
        case 0b00010101: return D7;
        case 0b01111111: return D8;
        case 0b00111111: return D9;
        case 0b00000000: return DEmpty;
        case 0b01101000: return DL;
        default:
#ifndef NDEBUG
            std::cerr << "Invalid digit value: " << std::bitset<8>(digit) << '\n';
#endif
            assert(false);
            return DEmpty;
        }
    }
};

enum class ConnStatus
{
    Disconnected,
    Connected,
};

int main(int argc, char** argv)
{
    Gtk::ApplicationWindow* mainWindow{};
    Glib::RefPtr<Gtk::Builder> builder{};
    ConnStatus connStatus = ConnStatus::Disconnected;

    auto app = Gtk::Application::create("xyz.timre13.mx-ui");
    app->signal_activate().connect([&](){
        builder = Gtk::Builder::create_from_file("../src/main.ui");
        mainWindow = builder->get_widget<Gtk::ApplicationWindow>("main-window");
        mainWindow->set_application(app);

        auto cssProv = Gtk::CssProvider::create();
        cssProv->load_from_path("../src/style.css");
        Gtk::StyleContext::add_provider_for_display(mainWindow->get_display(), cssProv, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        builder->get_widget<Gtk::Button>("connect-button")->signal_clicked().connect([&](){
            std::cout << "Clicked\n";

            auto statDisp = builder->get_widget<Gtk::Label>("status-display");
            if (connStatus == ConnStatus::Disconnected)
            {
                connStatus = ConnStatus::Connected;
                statDisp->remove_css_class("status-display-disconnected");
                statDisp->add_css_class("status-display-connected");
                statDisp->set_label("Connected");
            }
            else if (connStatus == ConnStatus::Connected)
            {
                connStatus = ConnStatus::Disconnected;
                statDisp->remove_css_class("status-display-sconnected");
                statDisp->add_css_class("status-display-disconnected");
                statDisp->set_label("Disconnected");
            }
        });

        mainWindow->present();
    });

    /*
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
    while (true)
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

#if 1
        Frame frame{buf};
        std::cout << '"';
        std::cout << (frame.isNegative ? "-" : " ");
        std::cout << Frame::digitToChar(frame.getDigitThousandVal());
        std::cout << Frame::digitToChar(frame.getDigitHundredVal());
        std::cout << Frame::digitToChar(frame.getDigitTenVal());
        std::cout << Frame::digitToChar(frame.getDigitSingleVal());
        std::cout << '"';
        
        std::cout << " = " << frame.getFloatVal();
        //std::cout << std::bitset<8>(frame.digitSingle) << '\n';
#endif

        std::cout << '\n';
        std::cout << std::flush;
    }
    tcsetattr(port, TCSANOW, &oldTio);
    */

    return app->run(argc, argv);
}
