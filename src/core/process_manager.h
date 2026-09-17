#pragma once

#include "tool.h"

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

enum class ToolState {
    Stopped,
    Starting,
    Running,  // tracked process, toolbox can stop it
    Detached, // launched detached, not tracked
    Failed,
};

// Owns the lifecycle of managed tool processes. Non-managed tools are simply
// launched detached. Health is probed asynchronously over TCP on the tool port.
class ProcessManager : public QObject
{
    Q_OBJECT
public:
    explicit ProcessManager(QObject *parent = nullptr);

    void start(const Tool &tool);
    void stop(const Tool &tool);
    void stopAll();

    ToolState state(const QString &id) const;
    quint64 pid(const QString &id) const;
    QString lastError(const QString &id) const;
    bool portOpen(const QString &id) const;
    QString logTail(const QString &id, int maxLines) const;

signals:
    void changed(const QString &id);

private:
    struct Entry {
        Tool tool;
        QProcess *proc = nullptr;
        ToolState state = ToolState::Stopped;
        QString lastError;
        bool portOpen = false;
        bool stopping = false;
        QStringList logLines;
    };

    Entry &entryFor(const QString &id);
    void probe(Entry &entry);
    void appendLog(Entry &entry, const QByteArray &data);

    QHash<QString, Entry> m_entries;
    QTimer *m_probeTimer = nullptr;
};