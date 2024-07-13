#include <cmath>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <bitset>
#include <mutex>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <gtkmm.h>
#include <gtkmm/application.h>
#include <thread>
#include <format>
#include <fstream>
#include <chrono>

#define BAUDRATE 2400
#define READ_MIN 14

using timestamp_t = std::chrono::system_clock::time_point;

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

    struct Unit
    {
        enum class Prefix
        {
            Nano,
            Micro,
            Milli,
            None,
            Kilo,
            Mega,
        } prefix;

        enum class Base
        {
            Volt,
            Ohm,
            Farad,
            Hertz,
            Percent,
            Celsius,
            Ampere,
        } base;
    };

    timestamp_t timestamp{};
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
    bool diode{};
    bool kilo{};
    bool nano{};
    bool micro{};
    bool beep{};
    bool mega{};
    bool percent{};
    bool milli{};
    bool hold{};
    bool rel{};
    bool ohm{};
    bool farad{};
    bool battery{};
    bool hertz{};
    bool volt{};
    bool ampere{};
    bool celsius{};
    bool milliVolt{};

    Frame(uint8_t buf[14], const timestamp_t& ts)
    {
        timestamp       = ts;

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

        diode           = (buf[9] & (1 << 0)) >> 0;
        kilo            = (buf[9] & (1 << 1)) >> 1;
        nano            = (buf[9] & (1 << 2)) >> 2;
        micro           = (buf[9] & (1 << 3)) >> 3;

        beep            = (buf[10] & (1 << 0)) >> 0;
        mega            = (buf[10] & (1 << 1)) >> 1;
        percent         = (buf[10] & (1 << 2)) >> 2;
        milli           = (buf[10] & (1 << 3)) >> 3;

        hold            = (buf[11] & (1 << 0)) >> 0;
        rel             = (buf[11] & (1 << 1)) >> 1;
        ohm             = (buf[11] & (1 << 2)) >> 2;
        farad           = (buf[11] & (1 << 3)) >> 3;

        battery         = (buf[12] & (1 << 0)) >> 0;
        hertz           = (buf[12] & (1 << 1)) >> 1;
        volt            = (buf[12] & (1 << 2)) >> 2;
        ampere          = (buf[12] & (1 << 3)) >> 3;

        celsius         = (buf[13] & (1 << 1)) >> 1;
        milliVolt       = (buf[13] & (1 << 2)) >> 2;
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

    float getFloatValOrZero() const
    {
        const float result = getFloatVal();
        return std::isnan(result) ? 0 : result;
    }

    Unit getUnit() const
    {
        Unit result{};

        if (milliVolt)
        {
            result.prefix = Unit::Prefix::Milli;
            result.base = Unit::Base::Volt;
        }
        else
        {
            if (kilo)
                result.prefix = Unit::Prefix::Kilo;
            else if (nano)
                result.prefix = Unit::Prefix::Nano;
            else if (micro)
                result.prefix = Unit::Prefix::Micro;
            else if (mega)
                result.prefix = Unit::Prefix::Mega;
            else if (milli)
                result.prefix = Unit::Prefix::Milli;
            else
                result.prefix = Unit::Prefix::None;

            if (percent)
                result.base = Unit::Base::Percent;
            else if (ohm)
                result.base = Unit::Base::Ohm;
            else if (farad)
                result.base = Unit::Base::Farad;
            else if (hertz)
                result.base = Unit::Base::Hertz;
            else if (volt)
                result.base = Unit::Base::Volt;
            else if (ampere)
                result.base = Unit::Base::Ampere;
            else if (celsius)
                result.base = Unit::Base::Celsius;
            else
                assert(false);
        }

        return result;
    }

    std::string getUnitStr() const
    {
        Unit unit = getUnit();
        std::string result;
        switch (unit.prefix)
        {
            case Unit::Prefix::Nano:
                result = "n";
                break;
            case Unit::Prefix::Micro:
                result = "\u00B5";
                break;
            case Unit::Prefix::Milli:
                result = "m";
                break;
            case Unit::Prefix::None:
                break;
            case Unit::Prefix::Kilo:
                result = "k";
                break;
            case Unit::Prefix::Mega:
                result = "M";
                break;
        }

        switch (unit.base)
        {
            case Unit::Base::Volt:
                result += "V";
                break;
            case Unit::Base::Ohm:
                result += "\u03A9";
                break;
            case Unit::Base::Farad:
                result += "F";
                break;
            case Unit::Base::Hertz:
                result += "Hz";
                break;
            case Unit::Base::Percent:
                result += "%";
                break;
            case Unit::Base::Celsius:
                result += "\u00B0C";
                break;
            case Unit::Base::Ampere:
                result += "A";
                break;
        }

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

static std::string formatTime(const timestamp_t& point)
{
    const auto zt = std::chrono::zoned_time{std::chrono::current_zone(), point};
    return std::format("{0:%F}T{0:%T}", zt);
}

static void exportData(const std::string& path, const std::vector<std::unique_ptr<Frame>>& data)
{
    std::ofstream file{path};
    file << "Value;Unit;Timestamp\n";
    for (auto& frame : data)
    {
        file << std::format("{};{};{}", frame->getFloatVal(), frame->getUnitStr(), formatTime(frame->timestamp)) << '\n';
    }
    file.close();
}

std::vector<std::unique_ptr<Frame>> frames;
std::mutex framesMutex{};

int main(int argc, char** argv)
{
    Gtk::Window* mainWindow{};
    Glib::RefPtr<Gtk::Builder> builder{};
    ConnStatus connStatus = ConnStatus::Disconnected;
    Glib::Dispatcher dispatcher{};
    int plotGap = 20;
    std::optional<int> canvasMouseX{};
    std::optional<int> canvasMouseY{};

    bool stayConnected = true;

    std::thread connThread{[&](){
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
    }};

    auto app = Gtk::Application::create("xyz.timre13.mx-ui");
    app->signal_activate().connect([&](){
        builder = Gtk::Builder::create_from_resource("/data/main.ui");
        mainWindow = builder->get_widget<Gtk::Window>("main-window");
        mainWindow->set_application(app);

        auto cssProv = Gtk::CssProvider::create();
        cssProv->load_from_resource("/data/style.css");
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

        auto drawingArea = builder->get_widget<Gtk::DrawingArea>("plot-area");
        assert(drawingArea);
        drawingArea->set_draw_func([drawingArea, &plotGap, &canvasMouseX, &canvasMouseY](const Cairo::RefPtr<Cairo::Context>& cont, int width, int height){
            //std::cout << "Redrawing: w = " << width << ", h = " << height << '\n';
            //std::cout << "Gap: " << plotGap << '\n';
            //std::cout << "frames.size(): " << frames.size() << '\n';

            auto styleCont = drawingArea->get_style_context();
            styleCont->render_background(cont, 0, 0, width, height);

            const double middleY = height/2.;

            {
                cont->set_source_rgba(0.1, 0.1, 0.1, 0.8);
                cont->set_line_width(0.5);
                for (int i{}; i < width/10; i++)
                {
                    if (i % 2)
                    {
                        cont->move_to(i*10, middleY);
                        cont->line_to((i+1)*10, middleY);
                    }
                }
                cont->stroke();
            }

            if (!frames.empty())
            {
                std::lock_guard<std::mutex> guard = std::lock_guard{framesMutex};

                cont->set_line_width(1);
                cont->set_source_rgb(0.3, 1.0, 0.8);
                cont->move_to(0, middleY);
                const int framesToDraw = std::min((int)width/plotGap, (int)frames.size());
                int maxDiff{};
                {
                    float maxVal = frames[0]->getFloatValOrZero();
                    for (size_t i=frames.size()-framesToDraw; i < frames.size(); ++i)
                        if (frames[i]->getFloatValOrZero() > maxVal)
                            maxVal = frames[i]->getFloatValOrZero();
                    float minVal = frames[0]->getFloatValOrZero();
                    for (size_t i=frames.size()-framesToDraw; i < frames.size(); ++i)
                        if (frames[i]->getFloatValOrZero() < minVal)
                            minVal = frames[i]->getFloatValOrZero();
                    maxDiff = std::max(maxVal, std::abs(minVal));
                }
                //std::cout << "framesToDraw = "  << framesToDraw << std::endl;
                for (int i{}; i < framesToDraw; ++i)
                {
                    const double diff = frames[frames.size()-framesToDraw+i]->getFloatValOrZero()/maxDiff*(middleY-10);
                    const int x = width-framesToDraw*plotGap+(i+1)*plotGap;
                    const int y = middleY-(std::isnan(diff) || std::isinf(diff) ? 0 : diff);
                    //std::cout << i << '\t' << x << '\t' << y << '\n';
                    if (i == 0)
                        cont->move_to(x, y);
                    cont->line_to(x, y);
                }
                cont->stroke();

                if (canvasMouseX.has_value())
                {
                    const int endOffs = (width-*canvasMouseX+plotGap/2)/plotGap;
                    if (endOffs < (int)frames.size())
                    {
                        cont->set_source_rgb(0.2, 0.8, 0.8);
                        cont->move_to(width-endOffs*plotGap, 0);
                        cont->line_to(width-endOffs*plotGap, height);
                        cont->stroke();

                        const Frame* const hoveredFrame = frames[frames.size()-1-endOffs].get();
                        std::cout << "Value: " << hoveredFrame->getFloatVal() << std::endl;
                        cont->set_source_rgb(1.0, 1.0, 1.0);
                        cont->select_font_face("monoscape", Cairo::ToyFontFace::Slant::NORMAL, Cairo::ToyFontFace::Weight::NORMAL);
                        cont->set_font_size(18);
                        Cairo::TextExtents extends;
                        const std::string text = std::format("Value: {:^ 3.3f} {}", hoveredFrame->getFloatVal(), hoveredFrame->getUnitStr());
                        cont->get_text_extents(text, extends);
                        assert(canvasMouseY.has_value());
                        const int textX = std::min(*canvasMouseX+5, width-(int)extends.width-5);
                        const int textY = std::max(*canvasMouseY-5, 15);
                        cont->move_to(textX, textY);
                        cont->show_text(text);
                    }
                }
            }
        });


        auto drawingAreaScrollController = Gtk::EventControllerScroll::create();
        drawingArea->add_controller(drawingAreaScrollController);
        drawingAreaScrollController->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
        drawingAreaScrollController->signal_scroll().connect([&plotGap, builder](double, double scroll){
            //std::cout << "Scroll: " << scroll << '\n';
            plotGap -= scroll;
            plotGap = std::max(1, plotGap);
            auto drawingArea = builder->get_widget<Gtk::DrawingArea>("plot-area");
            assert(drawingArea);
            drawingArea->queue_draw();
            return true;
        }, false);

        auto drawingAreaMotionController = Gtk::EventControllerMotion::create();
        drawingArea->add_controller(drawingAreaMotionController);
        drawingAreaMotionController->signal_motion().connect([&canvasMouseX, &canvasMouseY, builder](double x, double y){
            //std::cout << "Move: x=" << x << ", y=" << y << '\n';
            canvasMouseX = (int)x;
            canvasMouseY = (int)y;
            auto drawingArea = builder->get_widget<Gtk::DrawingArea>("plot-area");
            assert(drawingArea);
            drawingArea->queue_draw();
        }, false);
        drawingAreaMotionController->signal_leave().connect([&canvasMouseX, &canvasMouseY, builder](){
            canvasMouseX.reset();
            canvasMouseY.reset();
            auto drawingArea = builder->get_widget<Gtk::DrawingArea>("plot-area");
            assert(drawingArea);
            drawingArea->queue_draw();
        }, false);

        builder->get_widget<Gtk::Button>("export-button")->signal_clicked().connect([mainWindow](){
            GtkFileDialog* dialog = gtk_file_dialog_new();
            gtk_file_dialog_save(dialog, mainWindow->gobj(), nullptr, [](GObject *source_object, GAsyncResult *res, gpointer){
                GError** err = nullptr;
                if (GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source_object), res, err))
                {
                    std::string path = g_file_get_path(file);
                    std::cout << "Exporting to " << path << std::endl;
                    exportData(path, frames);
                    g_object_unref(file);
                }
                if (err)
                {
                    std::cout << "File chooser error" << std::endl;
                    g_error_free(*err);
                }
            }, nullptr);
        });

        mainWindow->show();
    });

    app->signal_shutdown().connect([&](){
        std::cout << "Shutting down\n";
        stayConnected = false;
        connThread.join();
        std::cout << "Done\n";
    });

    dispatcher.connect([&](){
        std::cout << "Updating GUI\n" << std::flush;
        std::lock_guard<std::mutex> guard = std::lock_guard{framesMutex};
        if (frames.empty())
            return;
        const Frame* const frame = frames.back().get();
        const auto strVal = isnanf(frame->getFloatVal()) ? "-------" : std::format("{:^ 3.3f}", frame->getFloatVal());
        builder->get_widget<Gtk::Label>("lcd-display-1")->set_label(strVal);
        builder->get_widget<Gtk::Label>("lcd-display-2")->set_label(frame->getUnitStr());

        auto setActivateLabel{[&](const std::string& id, bool active){
            auto widget = builder->get_widget<Gtk::Label>("status-label-"+id);
            if (active) widget->add_css_class("status-label-active");
            else widget->remove_css_class("status-label-active");
        }};

        setActivateLabel("auto", frame->AUTO);
        setActivateLabel("dc", frame->DC);
        setActivateLabel("ac", frame->AC);
        setActivateLabel("diode", frame->diode);
        setActivateLabel("beep", frame->beep);
        setActivateLabel("hold", frame->hold);
        setActivateLabel("rel", frame->rel);
        setActivateLabel("batt", frame->battery);

        builder->get_widget<Gtk::DrawingArea>("plot-area")->queue_draw();
        return;
    });

    /*
    Glib::signal_timeout().connect([&](){
        auto cssProv = Gtk::CssProvider::create();
        cssProv->load_from_path("../src/style.css");
        Gtk::StyleContext::add_provider_for_display(mainWindow->get_display(), cssProv, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        return true;
    }, 300);
    */

    return app->run(argc, argv);
}
