#include "core/manifest.h"
#include "core/tool.h"
#include "core/http_server.h"
#include "core/process_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QEventLoop>

static void usage(QTextStream &out)
{
    out << "toolboxd - Toolbox service (" << QCoreApplication::applicationVersion() << ")\n"
        << "usage:\n"
        << "  toolboxd [--root DIR] list                 list tools loaded from DIR/manifests\n"
        << "  toolboxd [--root DIR] start <id>           start a tool (smoke test)\n"
        << "  toolboxd [--root DIR] stop <id>            stop a tool (smoke test)\n"
        << "  toolboxd [--root DIR] --daemon             run headless HTTP service (default port 29811)\n"
        << "  toolboxd [--root DIR] --gui                start the desktop GUI and embed the HTTP service\n";
}

static QString stateLabel(ToolState s)
{
    switch (s) {
    case ToolState::Stopped:  return QStringLiteral("stopped");
    case ToolState::Starting: return QStringLiteral("starting");
    case ToolState::Running:  return QStringLiteral("running");
    case ToolState::Detached: return QStringLiteral("detached");
    case ToolState::Failed:   return QStringLiteral("failed");
    }
    return QStringLiteral("?");
}

// Print a tool's process state after a short settle, then exit.
static int onelineOp(const QString &root, const QString &op, const QString &id)
{
    ManifestLoader loader;
    if (!loader.load(root, QStringLiteral("manifests"))) {
        QTextStream err(stderr);
        err << loader.error() << Qt::endl;
        return 2;
    }
    const QList<Tool> tools = loader.tools();
    auto toolIt = std::find_if(tools.begin(), tools.end(),
                               [&](const Tool &t) { return t.id == id; });
    if (toolIt == tools.end()) {
        QTextStream err(stderr);
        err << "no such tool: " << id << Qt::endl;
        return 2;
    }

    ProcessManager pm;
    if (op == QLatin1String("start"))
        pm.start(*toolIt);
    else if (op == QLatin1String("stop"))
        pm.stop(*toolIt);
    else if (op != QLatin1String("status"))
        return 2;

    QEventLoop loop;
    QTimer::singleShot(700, &loop, &QEventLoop::quit);
    loop.exec();

    QTextStream out(stdout);
    out << id << " " << stateLabel(pm.state(id));
    if (pm.pid(id))
        out << " pid=" << pm.pid(id);
    const QString errText = pm.lastError(id);
    if (!errText.isEmpty())
        out << " error=" << errText;
    out << Qt::endl;
    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("toolboxd"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QString root = QDir::currentPath();
    QStringList args = app.arguments();
    QStringList cmds = args.mid(1);

    bool daemon = false;
    bool gui = false;
    QString op;
    QString opArg;
    for (int i = 0; i < cmds.size(); ++i) {
        if (cmds[i] == QLatin1String("--root") && i + 1 < cmds.size()) {
            root = QDir(cmds[++i]).absolutePath();
        } else if (cmds[i] == QLatin1String("--daemon")) {
            daemon = true;
        } else if (cmds[i] == QLatin1String("--gui")) {
            gui = true;
        } else if (cmds[i] == QLatin1String("list")) {
            op = QStringLiteral("list");
        } else if (cmds[i] == QLatin1String("start") || cmds[i] == QLatin1String("stop")
                   || cmds[i] == QLatin1String("status")) {
            op = cmds[i];
        } else if (op == QLatin1String("start") || op == QLatin1String("stop")
                   || op == QLatin1String("status")) {
            opArg = cmds[i];
        } else if (cmds[i] == QLatin1String("help") || cmds[i] == QLatin1String("-h")) {
            QTextStream out(stdout);
            usage(out);
            return 0;
        } else {
            QTextStream out(stderr);
            usage(out);
            return 2;
        }
    }

    if (op == QLatin1String("start") || op == QLatin1String("stop")
        || op == QLatin1String("status")) {
        return onelineOp(root, op, opArg);
    }

    if (op == QLatin1String("list")) {
        ManifestLoader loader;
        if (!loader.load(root, QStringLiteral("manifests"))) {
            QTextStream err(stderr);
            err << loader.error() << Qt::endl;
            return 2;
        }
        QTextStream out(stdout);
        for (const Tool &t : loader.tools())
            out << t.id << "\t" << t.name << "\t" << t.typeLabel()
                << "\t" << t.summary << Qt::endl;
        return 0;
    }

    if (daemon) {
        HttpServer http;
        if (!http.listen(29811, QHostAddress::Any)) {
            QTextStream err(stderr);
            err << "failed to listen on 29811: " << http.errorString() << Qt::endl;
            return 1;
        }
        http.setHandler([](const HttpRequest &req, HttpResponse *resp) {
            if (req.path == QLatin1String("/api/ping")) {
                QJsonObject o;
                o.insert("ok", true);
                o.insert("version", QCoreApplication::applicationVersion());
                resp->contentType = QByteArrayLiteral("application/json");
                resp->body = QJsonDocument(o).toJson(QJsonDocument::Compact);
                return true;
            }
            if (req.path == QLatin1String("/")) {
                resp->contentType = QByteArrayLiteral("text/plain; charset=utf-8");
                resp->body = "toolboxd at your service\n";
                return true;
            }
            return false;
        });

        QTextStream out(stdout);
        out << "toolboxd listening on :29811 (web UI + REST)\n";
        return app.exec();
    }

    QTextStream err(stderr);
    err << "gui mode is not implemented yet" << Qt::endl;
    return 0;
}