#include <QApplication>
#include <QLoggingCategory>
#include "main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application info
    app.setApplicationName("HyniGUI");
    app.setOrganizationName("Hyni");
    app.setApplicationDisplayName("Hyni - LLM Chat Interface");

    // Enable logging
    QLoggingCategory::setFilterRules("hyni.*=true");

    // Set application style
    app.setStyle("Fusion");

    MainWindow window;
    window.show();

    return app.exec();
}
