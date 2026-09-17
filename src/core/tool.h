#pragma once

#include <QString>
#include <QStringList>

enum class ToolType {
    Service,   // long-running process with a web UI / port (managed lifecycle)
    WebApp,    // static web app opened in a browser, no server process
    DesktopApp,// native desktop app launched detached
    AndroidApp,// mobile app, built/installed via adb or built manually
    Document,  // not runnable software, just documentation
};

struct Tool {
    QString id;
    QString name;
    QString category;
    ToolType type = ToolType::Service;
    QString summary;
    QString repo;        // directory relative to the toolbox repo root
    QString workdir;     // working dir relative to the toolbox repo root (default: repo)
    QStringList command; // argv; argv[0] relative to workdir unless absolute
    int port = -1;       // web UI / health check port
    QString healthPath;  // path used for port health check (default "/")
    QString webPath;     // path prefix of the embedded web UI, e.g. "/ui/"
    QString docs;        // docs/<file>.md relative to the toolbox repo root
    QString note;        // extra hint shown in the UI
    bool managed = true; // toolbox owns the process and can stop/restart it

    QString typeName() const;
    QString typeLabel() const;
};