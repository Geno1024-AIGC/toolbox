#pragma once

#include "http_server.h"

#include <QList>
#include <QString>

class ProcessManager;
class Tool;

// REST API dispatcher served by HttpServer. Hands /api/* requests, embedded
// web assets from the qrc, and docs/* markdown from the on-disk docs dir.
class ApiServer
{
public:
    ApiServer();

    void setData(const QList<Tool> &tools, ProcessManager *pm);
    void setRepoRoot(const QString &repoRoot);

    // HttpServer::Handler-compatible entry point.
    bool route(const HttpRequest &req, HttpResponse *resp);

private:
    bool routeApi(const HttpRequest &req, HttpResponse *resp);
    bool routeStatic(const HttpRequest &req, HttpResponse *resp);

    const QList<Tool> *m_tools = nullptr;
    ProcessManager *m_pm = nullptr;
    QString m_repoRoot;
};