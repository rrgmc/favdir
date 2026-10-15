#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("favdir"));
    QApplication::setOrganizationName(QStringLiteral("favdir"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("favdir"));
    window.resize(640, 720);
    window.show();

    return app.exec();
}
