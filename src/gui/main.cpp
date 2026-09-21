#include <QApplication>
#include <QIcon>
#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Poppy");
    app.setApplicationDisplayName("Poppy - Native API Client");
    app.setOrganizationName("Poppy");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    // Apply modern dark stylesheet
    app.setStyleSheet(poppy::gui::Theme::darkStyleSheet());

    poppy::gui::MainWindow window;
    window.show();

    return app.exec();
}
