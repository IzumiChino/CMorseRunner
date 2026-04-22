#include <QApplication>
#include "ui/main_window.hpp"
#include "core/ini.hpp"
#include <cstdlib>
#include <ctime>

int main(int argc, char* argv[]) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    QApplication app(argc, argv);
    app.setApplicationName("CMorseRunner");
    app.setOrganizationName("CMorseRunner");

    Ini::instance().load("morserunner.ini");

    MainWindow w;
    w.show();

    int ret = app.exec();

    Ini::instance().save("morserunner.ini");
    return ret;
}
