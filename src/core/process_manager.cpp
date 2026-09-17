#include "process_manager.h"

#include <QDir>
#include <QFileInfo>
#include <QTcpSocket>
#include <QTimer>

ProcessManager::ProcessManager(QObject *parent)
    : QObject(parent)
{
    m_probeTimer = new QTimer(this);
    m_probeTimer->setInterval(2000);
    connect(m_probeTimer, &QTimer::timeout, this, [this]() {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            Entry &e = it.value();
            if (e.state == ToolState::Running && e.tool.type == ToolType::Service)
                probe(e);
        }
    });
    m_probeTimer->start();
}

static QString resolveProgram(const Tool &tool)
{
    if (tool.command.isEmpty())
        return QString();
    const QString arg0 = tool.command.first();
    if (QFileInfo(arg0).isAbsolute())
        return arg0;
    return QDir(tool.workdir).filePath(arg0);
}

ProcessManager::Entry &ProcessManager::entryFor(const QString &id)
{
    if (!m_entries.contains(id)) {
        Entry e;
        e.tool.id = id;
        e.state = ToolState::Stopped;
        m_entries.insert(id, e);
    }
    return m_entries[id];
}

void ProcessManager::start(const Tool &tool)
{
    Entry &e = entryFor(tool.id);
    e.tool = tool;

    if (tool.type == ToolType::Document)
        return;

    // Desktop / web apps: launch detached, nothing to track.
    if (!tool.managed) {
        const QString program = resolveProgram(tool);
        if (program.isEmpty()) {
            e.state = ToolState::Failed;
            e.lastError = QStringLiteral("no command configured");
            emit changed(tool.id);
            return;
        }
        if (QProcess::startDetached(program, tool.command.mid(1), tool.workdir)) {
            e.state = ToolState::Detached;
            e.lastError.clear();
        } else {
            e.state = ToolState::Failed;
            e.lastError = QStringLiteral("failed to launch %1").arg(program);
        }
        emit changed(tool.id);
        return;
    }

    // Managed service process.
    if (e.proc && e.proc->state() != QProcess::NotRunning) {
        e.state = ToolState::Running;
        emit changed(tool.id);
        return;
    }

    auto *proc = e.proc ? e.proc : new QProcess(this);
    e.proc = proc;
    e.state = ToolState::Starting;
    e.lastError.clear();
    e.portOpen = false;
    e.stopping = false;

    const QString program = resolveProgram(tool);
    proc->setProgram(program);
    proc->setArguments(tool.command.mid(1));
    proc->setWorkingDirectory(tool.workdir);

    connect(proc, &QProcess::started, this, [this, id = tool.id]() {
        m_entries[id].state = ToolState::Running;
        m_entries[id].lastError.clear();
        // The port may not be bound yet; probe again shortly after start.
        QTimer::singleShot(300, this, [this, id] { probe(m_entries[id]); });
        QTimer::singleShot(1500, this, [this, id] { probe(m_entries[id]); });
        emit changed(id);
    });
    connect(proc, &QProcess::errorOccurred, this, [this, id = tool.id](QProcess::ProcessError err) {
        Entry &e = m_entries[id];
        if (err == QProcess::FailedToStart || err == QProcess::Crashed) {
            if (e.stopping && err == QProcess::Crashed)
                return; // deliberate termination; finished() will tidy up
            e.state = ToolState::Failed;
            e.lastError = e.proc && err == QProcess::FailedToStart
                ? QStringLiteral("failed to start: %1").arg(e.proc->errorString())
                : QStringLiteral("process crashed");
            e.stopping = false;
            emit changed(id);
        }
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, id = tool.id](int code, QProcess::ExitStatus) {
        Entry &e = m_entries[id];
        e.state = ToolState::Stopped;
        e.portOpen = false;
        if (!e.stopping && code != 0)
            e.lastError = QStringLiteral("exited with code %1").arg(code);
        e.stopping = false;
        emit changed(id);
    });
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, id = tool.id]() {
        if (auto it = m_entries.find(id); it != m_entries.end())
            appendLog(it.value(), it->proc->readAllStandardOutput());
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, id = tool.id]() {
        if (auto it = m_entries.find(id); it != m_entries.end())
            appendLog(it.value(), it->proc->readAllStandardError());
    });

    proc->start();
    emit changed(tool.id);
}

void ProcessManager::stop(const Tool &tool)
{
    Entry &e = entryFor(tool.id);
    if (!e.proc || e.proc->state() == QProcess::NotRunning) {
        e.state = ToolState::Stopped;
        emit changed(tool.id);
        return;
    }
    e.stopping = true;
    QProcess *proc = e.proc;
    proc->terminate();
    QTimer::singleShot(3000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning)
            proc->kill();
    });
}

void ProcessManager::stopAll()
{
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (it.value().proc && it.value().proc->state() != QProcess::NotRunning)
            it.value().proc->terminate();
    }
}

void ProcessManager::probe(Entry &entry)
{
    // Only probe tools that are actually running under our management.
    if (entry.state != ToolState::Running || entry.tool.type != ToolType::Service
        || entry.tool.port <= 0) {
        entry.portOpen = false;
        return;
    }
    auto *socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, [this, socket, id = entry.tool.id]() {
        Entry &e = m_entries[id];
        bool was = e.portOpen;
        e.portOpen = true;
        socket->close();
        socket->deleteLater();
        if (!was) emit changed(id);
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket, id = entry.tool.id]() {
        Entry &e = m_entries[id];
        bool was = e.portOpen;
        e.portOpen = false;
        socket->close();
        socket->deleteLater();
        if (was) emit changed(id);
    });
    socket->connectToHost(QHostAddress::LocalHost, entry.tool.port);
    QTimer::singleShot(1500, socket, [socket]() {
        if (socket->state() != QAbstractSocket::ConnectedState) {
            socket->close();
            socket->deleteLater();
        }
    });
}

void ProcessManager::appendLog(Entry &entry, const QByteArray &data)
{
    const QStringList lines = QString::fromUtf8(data).split('\n');
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty())
            continue;
        entry.logLines.append(line.trimmed());
        if (entry.logLines.size() > 500)
            entry.logLines.removeFirst();
    }
}

ToolState ProcessManager::state(const QString &id) const
{
    auto it = m_entries.constFind(id);
    return it != m_entries.constEnd() ? it->state : ToolState::Stopped;
}

quint64 ProcessManager::pid(const QString &id) const
{
    auto it = m_entries.constFind(id);
    return (it != m_entries.constEnd() && it->proc) ? it->proc->processId() : 0;
}

QString ProcessManager::lastError(const QString &id) const
{
    auto it = m_entries.constFind(id);
    return it != m_entries.constEnd() ? it->lastError : QString();
}

bool ProcessManager::portOpen(const QString &id) const
{
    auto it = m_entries.constFind(id);
    return it != m_entries.constEnd() && it->portOpen;
}

QString ProcessManager::logTail(const QString &id, int maxLines) const
{
    auto it = m_entries.constFind(id);
    if (it == m_entries.constEnd())
        return QString();
    const int from = qMax(0, it->logLines.size() - maxLines);
    return it->logLines.mid(from).join('\n');
}