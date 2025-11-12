#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLocale>
#include <QTranslator>
#include <QDebug>
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Set application/window icon from resources
    a.setWindowIcon(QIcon(":/icons/app-icon.svg"));

    QTranslator appTranslator;
    const QStringList args = a.arguments();

    auto normalizeLocale = [](const QString &value) -> QString {
        QString locale = value.trimmed();
        if (locale.isEmpty()) {
            return locale;
        }
        locale.replace('-', '_');
        if (locale.compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("en_US");
        }
        if (locale.compare(QStringLiteral("zh"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("zh_CN");
        }
        return locale;
    };

    QString requestedLocale;
    for (int i = 1; i < args.size(); ++i) {
        if ((args.at(i) == QStringLiteral("--lang") || args.at(i) == QStringLiteral("-l")) && (i + 1) < args.size()) {
            requestedLocale = normalizeLocale(args.at(i + 1));
            break;
        }
    }

    QString localeToLoad = requestedLocale;
    if (localeToLoad.isEmpty()) {
        const QString systemLocale = QLocale::system().bcp47Name();
        if (!systemLocale.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive)) {
            localeToLoad = QStringLiteral("en_US");
        }
    }

    auto installTranslatorForLocale = [&](const QString &localeName) -> bool {
        if (localeName.isEmpty() || localeName.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive)) {
            return false;
        }

        const QString appDir = QCoreApplication::applicationDirPath();
        QStringList searchDirs;
        searchDirs << appDir;
        searchDirs << appDir + QStringLiteral("/translations");
        searchDirs << QDir(appDir).absoluteFilePath(QStringLiteral("../translations"));
        searchDirs << QStringLiteral("translations");

        const QString baseName = QStringLiteral("SimpleLabel_%1").arg(localeName);
        for (const QString &dir : searchDirs) {
            if (appTranslator.load(baseName, dir)) {
                a.installTranslator(&appTranslator);
                return true;
            }
        }
        return false;
    };

    if (!localeToLoad.isEmpty() && !installTranslatorForLocale(localeToLoad)) {
        qWarning() << "Failed to load translation for locale" << localeToLoad;
    }

    MainWindow w;
    w.show();
    return a.exec();
}
