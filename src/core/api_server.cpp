#include "api_server.h"

#include "process_manager.h"
#include "tool.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace {

QString stateString(ToolState s)
{
    switch (s) {
    case ToolState::Stopped:  return QStringLiteral("stopped");
    case ToolState::Starting: return QStringLiteral("starting");
    case ToolState::Running:  return QStringLiteral("running");
    case ToolState::Detached: return QStringLiteral("running");
    case ToolState::Failed:   return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

QByteArray mimeFor(const QString &path)
{
    if (path.endsWith(QStringLiteral(".html"))) return "text/html; charset=utf-8";
    if (path.endsWith(QStringLiteral(".css")))  return "text/css; charset=utf-8";
    if (path.endsWith(QStringLiteral(".js")))   return "text/javascript; charset=utf-8";
    if (path.endsWith(QStringLiteral(".json"))) return "application/json; charset=utf-8";
    if (path.endsWith(QStringLiteral(".svg")))  return "image/svg+xml";
    if (path.endsWith(QStringLiteral(".png")))  return "image/png";
    if (path.endsWith(QStringLiteral(".ico")))  return "image/x-icon";
    if (path.endsWith(QStringLiteral(".woff2"))) return "font/woff2";
    if (path.endsWith(QStringLiteral(".md")))   return "text/markdown; charset=utf-8";
    return "application/octet-stream";
}

QJsonObject toolJson(const Tool &t, const ProcessManager &pm)
{
    QJsonObject o;
    o.insert("id", t.id);
    o.insert("name", t.name);
    o.insert("category", t.category);
    o.insert("type", t.typeName());
    o.insert("typeLabel", t.typeLabel());
    o.insert("summary", t.summary);
    o.insert("port", t.port);
    o.insert("webPath", t.webPath);
    o.insert("managed", t.managed);
    o.insert("note", t.note);
    if (!t.docs.isEmpty())
        o.insert("docs", QLatin1String("/docs/") + QFileInfo(t.docs).fileName());
    o.insert("state", stateString(pm.state(t.id)));
    if (pm.pid(t.id))
        o.insert("pid", static_cast<double>(pm.pid(t.id)));
    const QString err = pm.lastError(t.id);
    if (!err.isEmpty())
        o.insert("error", err);
    if (t.type == ToolType::Service && t.port > 0)
        o.insert("portOpen", pm.portOpen(t.id));
    return o;
}

} // namespace

ApiServer::ApiServer() = default;

void ApiServer::setData(const QList<Tool> &tools, ProcessManager *pm)
{
    m_tools = &tools;
    m_pm = pm;
}

void ApiServer::setRepoRoot(const QString &repoRoot)
{
    m_repoRoot = repoRoot;
}

bool ApiServer::route(const HttpRequest &req, HttpResponse *resp)
{
    if (req.path == QLatin1String("/api")
        || req.path.startsWith(QLatin1String("/api/")))
        return routeApi(req, resp);
    return routeStatic(req, resp);
}

bool ApiServer::routeApi(const HttpRequest &req, HttpResponse *resp)
{
    resp->contentType = QByteArrayLiteral("application/json; charset=utf-8");
    QString path = req.path == QLatin1String("/api")
        ? QStringLiteral("/status") : req.path.mid(4); // strip "/api", keep leading slash

    const auto notFound = [&](const QString &msg) {
        QJsonObject o;
        o.insert("error", msg);
        resp->status = 404;
        resp->body = QJsonDocument(o).toJson(QJsonDocument::Compact);
        return true;
    };

    if (path == QLatin1String("/ping") || path == QLatin1String("/status")) {
        QJsonObject o;
        o.insert("ok", true);
        o.insert("version", QCoreApplication::applicationVersion());
        o.insert("root", m_repoRoot);
        o.insert("tools", m_tools ? m_tools->size() : 0);
        resp->body = QJsonDocument(o).toJson(QJsonDocument::Compact);
        return true;
    }

    if (path == QLatin1String("/tools") && req.method == QLatin1String("GET")) {
        QJsonArray arr;
        if (m_tools) {
            for (const Tool &t : *m_tools)
                arr.append(toolJson(t, *m_pm));
        }
        resp->body = QJsonDocument(arr).toJson(QJsonDocument::Compact);
        return true;
    }

    static const QRegularExpression toolRe(
        QStringLiteral("^/tools/([^/]+)(/(start|stop|logs))?$"));
    const QRegularExpressionMatch m = toolRe.match(path);
    if (!m.hasMatch())
        return notFound(QStringLiteral("no such route"));

    const QString id = m.captured(1);
    const QString action = m.captured(3);
    if (!m_tools) return notFound(id);

    const Tool *tool = nullptr;
    for (const Tool &t : *m_tools) {
        if (t.id == id) { tool = &t; break; }
    }
    if (!tool)
        return notFound(QStringLiteral("no such tool: %1").arg(id));

    if (action.isEmpty() && req.method == QLatin1String("GET")) {
        resp->body = QJsonDocument(toolJson(*tool, *m_pm)).toJson(QJsonDocument::Compact);
        return true;
    }
    if (action == QLatin1String("logs") && req.method == QLatin1String("GET")) {
        QJsonObject o;
        o.insert("log", m_pm->logTail(id, 300));
        resp->body = QJsonDocument(o).toJson(QJsonDocument::Compact);
        return true;
    }
    if (req.method != QLatin1String("POST")) {
        resp->status = 405;
        resp->body = QJsonDocument(QJsonObject{{"error", "method not allowed"}}).toJson(QJsonDocument::Compact);
        return true;
    }
    if (action == QLatin1String("start")) {
        m_pm->start(*tool);
    } else if (action == QLatin1String("stop")) {
        m_pm->stop(*tool);
    } else {
        return notFound(QStringLiteral("no such action"));
    }
    resp->body = QJsonDocument(toolJson(*tool, *m_pm)).toJson(QJsonDocument::Compact);
    return true;
}

bool ApiServer::routeStatic(const HttpRequest &req, HttpResponse *resp)
{
    if (req.method != QLatin1String("GET"))
        return false;

    QString path = req.path;
    if (path.endsWith(QLatin1String("/")))
        path += QLatin1String("index.html");

    // /docs/<file> served from the on-disk docs directory.
    if (path.startsWith(QLatin1String("/docs/"))) {
        const QString fileName = path.mid(6);
        QDir docsDir(m_repoRoot);
        docsDir.cd(QStringLiteral("docs"));
        const QString full = docsDir.filePath(fileName);
        QFile f(full);
        if (!f.open(QIODevice::ReadOnly))
            return false;
        resp->contentType = mimeFor(path);
        resp->body = f.readAll();
        return true;
    }

    // Everything else is an embedded qrc asset under :/web.
    if (path == QLatin1String("/")) {
        path = QLatin1String("/index.html");
    }
    QFile f(QStringLiteral(":/web%1").arg(path));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    resp->contentType = mimeFor(path);
    resp->body = f.readAll();
    return true;
}