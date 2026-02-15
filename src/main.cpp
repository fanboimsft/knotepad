#include <QApplication>
#include <KAboutData>
#include <KLocalizedString>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    KAboutData aboutData(
        QStringLiteral("knotepad"),
        i18n("KNotepad"),
        QStringLiteral("1.0"),
        i18n("A simple notepad with rich text support"),
        KAboutLicense::GPL_V3,
        i18n("(c) 2026")
    );
    KAboutData::setApplicationData(aboutData);

    MainWindow *window = new MainWindow();
    window->show();

    return app.exec();
}
