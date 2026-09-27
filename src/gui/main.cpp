#include "core/manifest.h"
#include <QDir>
#include "core/process_manager.h"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QTextStream>

static bool isLive(ToolState s)
{
    return s == ToolState::Running || s == ToolState::Detached;
}

static QString stateText(ToolState s)
{
    switch (s) {
    case ToolState::Stopped:  return QStringLiteral("已停止");
    case ToolState::Starting: return QStringLiteral("启动中…");
    case ToolState::Running:  return QStringLiteral("运行中");
    case ToolState::Detached: return QStringLiteral("已启动(后台)");
    case ToolState::Failed:   return QStringLiteral("启动失败");
    }
    return QStringLiteral("未知");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Toolbox"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QString root = QDir::currentPath();
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--root") && i + 1 < args.size())
            root = QDir(args[++i]).absolutePath();
    }

    ManifestLoader loader;
    if (!loader.load(root, QStringLiteral("manifests"))) {
        QTextStream err(stderr);
        err << loader.error() << Qt::endl;
        return 1;
    }
    const QList<Tool> tools = loader.tools();
    if (tools.isEmpty()) {
        QTextStream err(stderr);
        err << QStringLiteral("manifests/ 下没有任何工具定义") << Qt::endl;
        return 1;
    }

    ProcessManager pm;

    auto *tray = new QSystemTrayIcon(QIcon::fromTheme(QStringLiteral("applications-utilities")), &app);
    auto *trayMenu = new QMenu;
    tray->setContextMenu(trayMenu);
    tray->setToolTip(QStringLiteral("Toolbox — 全部工具入口"));
    tray->show();

    auto notify = [tray](const QString &title, const QString &body) {
        if (tray->isVisible())
            tray->showMessage(title, body, QSystemTrayIcon::Information, 2500);
    };

    auto *win = new QWidget;
    win->setWindowTitle(QStringLiteral("Toolbox — 全部工具入口"));
    auto *rootLay = new QVBoxLayout(win);
    auto *statusL = new QLabel;
    auto *view = new QListWidget;
    rootLay->addWidget(statusL);
    rootLay->addWidget(view, 1);

    auto refreshStatus = [&]() {
        int running = 0;
        for (const Tool &t : tools)
            if (isLive(pm.state(t.id)))
                ++running;
        statusL->setText(QStringLiteral("共 %1 个工具 · 运行中 %2")
                         .arg(tools.size()).arg(running));
    };

    auto rebuildTray = [&]() {
        trayMenu->clear();
        for (const Tool &t : tools) {
            if (!t.managed)
                continue;
            QAction *a = trayMenu->addAction(t.name);
            a->setCheckable(true);
            a->setChecked(isLive(pm.state(t.id)));
            QObject::connect(a, &QAction::triggered, &app, [&, t]() {
                if (isLive(pm.state(t.id)))
                    pm.stop(t);
                else
                    pm.start(t);
            });
        }
        if (trayMenu->isEmpty())
            trayMenu->addAction(QStringLiteral("无可用工具"));
        trayMenu->addSeparator();
        trayMenu->addAction(QStringLiteral("退出"), &app, &QCoreApplication::quit);
    };
    rebuildTray();

    QStringList categories;
    for (const Tool &t : tools)
        if (!categories.contains(t.category))
            categories << t.category;

    for (const QString &cat : categories) {
        auto *catItem = new QListWidgetItem(cat, view);
        catItem->setFlags(catItem->flags() & ~Qt::ItemIsEnabled);
        for (const Tool &t : tools) {
            if (t.category != cat)
                continue;
            auto *rowItem = new QListWidgetItem(view);
            auto *w = new QWidget;
            auto *lay = new QHBoxLayout(w);
            lay->setContentsMargins(8, 4, 8, 4);
            auto *name = new QLabel(QStringLiteral("<b>%1</b>").arg(t.name));
            name->setToolTip(t.summary);
            auto *typeL = new QLabel(t.typeLabel());
            typeL->setStyleSheet(QStringLiteral("color:#666;"));
            auto *stateL = new QLabel;
            auto *btn = new QPushButton;
            btn->setFixedWidth(64);
            lay->addWidget(name);
            lay->addWidget(typeL);
            lay->addStretch();
            lay->addWidget(stateL);
            lay->addWidget(btn);
            view->setItemWidget(rowItem, w);

            auto refreshRow = [&, t]() {
                stateL->setText(stateText(pm.state(t.id)));
                const bool running = isLive(pm.state(t.id));
                btn->setText(running ? QStringLiteral("停止") : QStringLiteral("启动"));
            };
            refreshRow();

            QObject::connect(btn, &QPushButton::clicked, &app, [&, t]() {
                if (isLive(pm.state(t.id)))
                    pm.stop(t);
                else
                    pm.start(t);
            });
            QObject::connect(&pm, &ProcessManager::changed, &app, [&, t, refreshRow]() {
                refreshRow();
                if (isLive(pm.state(t.id)))
                    notify(t.name, QStringLiteral("已启动 · pid %1").arg(pm.pid(t.id)));
                else if (pm.state(t.id) == ToolState::Failed)
                    notify(t.name, QStringLiteral("启动失败 · %1").arg(pm.lastError(t.id)));
            });
        }
    }

    QObject::connect(&pm, &ProcessManager::changed, &app, [&](const QString &) {
        refreshStatus();
        rebuildTray();
    });
    refreshStatus();

    win->resize(620, 480);
    win->show();

    return app.exec();
}
