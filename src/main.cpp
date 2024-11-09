#include <cmath>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <mutex>
#include <stdint.h>
#include <gtkmm.h>
#include <gtkmm/application.h>
#include <thread>
#include <format>
#include <fstream>
#include <chrono>
#include "Frame.h"
#include "protocol.h"

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
    ConnStatus connStatus = ConnStatus::Closed;
    Glib::Dispatcher dispatcher{};
    int plotGap = 20;
    std::optional<int> canvasMouseX{};
    std::optional<int> canvasMouseY{};

    std::atomic<bool> keepThreadAlive = true;
    std::atomic<bool> stayConnected = true;

    std::thread connThread{[](){}};
    static const auto startThread{[&](const SerialDevice& device){
        connThread.join();
        connThread = std::thread{&startReadingData,
            std::ref(keepThreadAlive), std::ref(stayConnected), std::ref(connStatus),
            std::ref(frames), std::ref(framesMutex), std::ref(dispatcher), device};
        keepThreadAlive = true;
        stayConnected = true;
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
            stayConnected = !stayConnected;
        });

        {
            const auto devs = listSerialDevices();
            if (devs.empty())
            {
                std::cout << "No serial devices found\n";
            }
            else
            {
                std::cout << "Enumerating serial devices:\n";
                for (const auto& file : devs)
                {
                    std::cout << '\t' << file.manufacturer << ' ' << file.product << " = " << file.path << std::endl;
                }

                std::vector<Glib::ustring> devStrings;
                std::transform(devs.begin(), devs.end(), std::back_inserter(devStrings), [](const SerialDevice& x){
                        return std::format("{} {} ({})", x.manufacturer, x.product, x.path);
                });
                auto list = Gtk::StringList::create(devStrings);

                auto dropdown = builder->get_widget<Gtk::DropDown>("port-dropdown");
                dropdown->set_model(list);

                dropdown->property_selected().signal_changed().connect([&keepThreadAlive, &stayConnected, &builder, devs](){
                    keepThreadAlive = false;
                    stayConnected = false;
                    const auto selected = builder->get_widget<Gtk::DropDown>("port-dropdown")->get_selected();
                    const auto device = devs[selected];
                    std::cout << "Selected device: " << device.path << '\n';
                    startThread(device);
                });

                // Connect to the first serial device that we could find
                startThread(devs[0]);
            }
        }


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
        keepThreadAlive = false;
        connThread.join();
        std::cout << "Done\n";
    });

    dispatcher.connect([&](){
        std::cout << "Updating GUI\n" << std::flush;

        auto statDisp = builder->get_widget<Gtk::Label>("status-display");
        statDisp->set_markup(std::format("<span foreground='{}'>{}</span>", connStatusGetColor(connStatus), connStatusToStr(connStatus)));

        builder->get_widget<Gtk::Button>("connect-button")->set_label(connStatus == ConnStatus::Connected ? "DISCONNECT" : "CONNECT");

        std::lock_guard<std::mutex> guard = std::lock_guard{framesMutex};
        if (frames.empty())
            return;
        const Frame* const frame = frames.back().get();
        const auto strVal = std::isnan(frame->getFloatVal()) ? "-------" : std::format("{:^ 3.3f}", frame->getFloatVal());
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

    return app->run(argc, argv);
}
