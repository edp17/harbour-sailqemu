#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickView>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <unistd.h>
#include "vmcontroller.h"
#include "diskmanager.h"
#include "runtimediagnostics.h"

// Short startup trace independent of terminal output. If this file is not
// updated after tapping the app icon, failure occurred before entering main()
// (e.g., invoker, Sailjail, dynamic-linker or preloaded Qt booster problems).
static void traceStartup(const QString &event)
{
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cache.isEmpty() || !QDir().mkpath(cache))
        return;
    QFile file(cache + QStringLiteral("/frontend-startup.log"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate)
           << " pid=" << ::getpid() << " " << event << "\n";
}

int main(int argc, char **argv)
{
    // Before SailfishApp::application() we cannot safely depend on
    // QStandardPaths or other application-initialized services.
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setOrganizationName(QStringLiteral("org.edp17"));
    app->setApplicationName(QStringLiteral("SailQEMU"));
    traceStartup(QStringLiteral("main(): QGuiApplication initialized"));

    VmController vm;
    DiskManager disks(&vm);
    RuntimeDiagnostics diagnostics(&vm);
    traceStartup(QStringLiteral("VmController initialized"));
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    if (!view) {
        traceStartup(QStringLiteral("ERROR: SailfishApp::createView returned null"));
        return 1;
    }
    QQuickView *quickView = view.data();
    QObject::connect(quickView, &QQuickView::statusChanged, quickView,
                     [quickView](QQuickView::Status status) {
        traceStartup(QStringLiteral("QQuickView status=%1").arg(static_cast<int>(status)));
        if (status == QQuickView::Error) {
            for (const QQmlError &error : quickView->errors())
                traceStartup(QStringLiteral("QML ERROR: ") + error.toString());
        }
    });
    view->rootContext()->setContextProperty(QStringLiteral("vm"), &vm);
    view->rootContext()->setContextProperty(QStringLiteral("disks"), &disks);
    view->rootContext()->setContextProperty(QStringLiteral("diagnostics"), &diagnostics);
    traceStartup(QStringLiteral("Loading qml/main.qml"));
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/main.qml")));
    if (view->status() == QQuickView::Error) {
        traceStartup(QStringLiteral("ERROR: failed to load application QML"));
        return 2;
    }
    view->show();
    traceStartup(QStringLiteral("QQuickView show() called"));
    const int result = app->exec();
    traceStartup(QStringLiteral("Event loop exited: %1").arg(result));
    return result;
}
