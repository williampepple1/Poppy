#include <QApplication>
#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Poppy");
    app.setApplicationDisplayName("Poppy - Native API Client");
    app.setOrganizationName("Poppy");

    // Apply modern dark stylesheet
    app.setStyleSheet(poppy::gui::Theme::darkStyleSheet());

    poppy::gui::MainWindow window;
    window.show();

    return app.exec();
}
