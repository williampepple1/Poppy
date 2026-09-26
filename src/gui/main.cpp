#include <QApplication>
#include <QIcon>
#include <QSettings>
#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Poppy");
    app.setApplicationDisplayName("Poppy - Native API Client");
    app.setOrganizationName("Poppy");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    QSettings settings;
    QString savedTheme = settings.value("ui/theme", "obsidian").toString();
    poppy::gui::Theme::setTheme(savedTheme);

    // Apply configured theme stylesheet
    app.setStyleSheet(poppy::gui::Theme::currentStyleSheet());

    poppy::gui::MainWindow window;
    if (argc > 1) {
        window.openPath(QString::fromLocal8Bit(argv[1]));
    }
    window.show();

    return app.exec();
}
