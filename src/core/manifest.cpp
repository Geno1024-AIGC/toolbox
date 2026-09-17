#include "manifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QDebug>

bool ManifestLoader::load(const QString &repoRoot, const QString &manifestsDirName)
{
    m_tools.clear();
    m_error.clear();

    const QString manifestsDir = QDir(repoRoot).filePath(manifestsDirName);
    QDir dir(manifestsDir);
    if (!dir.exists()) {
        m_error = QStringLiteral("manifests dir not found: %1").arg(manifestsDir);
        return false;
    }

    const QStringList files = dir.entryList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        m_error = QStringLiteral("no manifests found in %1").arg(manifestsDir);
        return false;
    }

    QSet<QString> seen;
    for (const QString &file : files) {
        Tool tool;
        if (!parse(dir.filePath(file), repoRoot, &tool)) {
            m_error = QStringLiteral("invalid manifest %1: %2").arg(file, m_error);
            return false;
        }
        if (seen.contains(tool.id)) {
            m_error = QStringLiteral("duplicate tool id '%1'").arg(tool.id);
            return false;
        }
        seen.insert(tool.id);
        m_tools.append(tool);
    }
    return true;
}

const QList<Tool> &ManifestLoader::tools() const
{
    return m_tools;
}

QString ManifestLoader::error() const
{
    return m_error;
}

static QString pathOf(const QJsonObject &o, const char *key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isUndefined() ? QString() : v.toString();
}

bool ManifestLoader::parse(const QString &path, const QString &repoRoot, Tool *out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("cannot open file");
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        m_error = QStringLiteral("JSON parse error: %1").arg(err.errorString());
        return false;
    }
    const QJsonObject o = doc.object();

    out->id = pathOf(o, "id");
    out->name = pathOf(o, "name");
    out->category = pathOf(o, "category");
    out->summary = pathOf(o, "summary");
    out->repo = pathOf(o, "repo");
    out->workdir = pathOf(o, "workdir");
    out->healthPath = pathOf(o, "healthPath");
    out->webPath = pathOf(o, "webPath");
    out->docs = pathOf(o, "docs");
    out->note = pathOf(o, "note");
    out->port = o.value(QLatin1String("port")).toInt(-1);

    const QString typeStr = pathOf(o, "type");
    if (typeStr == QLatin1String("webapp"))       out->type = ToolType::WebApp;
    else if (typeStr == QLatin1String("desktop")) out->type = ToolType::DesktopApp;
    else if (typeStr == QLatin1String("android")) out->type = ToolType::AndroidApp;
    else if (typeStr == QLatin1String("document")) out->type = ToolType::Document;
    else if (typeStr.isEmpty() || typeStr == QLatin1String("service")) out->type = ToolType::Service;
    else {
        m_error = QStringLiteral("unknown type '%1'").arg(typeStr);
        return false;
    }

    const bool managed = o.value(QLatin1String("managed")).isUndefined()
        ? true : o.value(QLatin1String("managed")).toBool();
    out->managed = (out->type == ToolType::Service) && managed;

    const QJsonValue cmdVal = o.value(QLatin1String("command"));
    if (!cmdVal.isUndefined()) {
        if (cmdVal.isArray()) {
            for (const QJsonValue &v : cmdVal.toArray())
                out->command.append(v.toString());
        } else {
            out->command.append(cmdVal.toString());
        }
    }

    if (out->id.isEmpty()) {
        m_error = QStringLiteral("missing 'id'");
        return false;
    }
    if (out->name.isEmpty())
        out->name = out->id;
    if (out->repo.isEmpty())
        out->repo = QLatin1String(".");

    // Resolve all paths against the toolbox repo root.
    if (out->workdir.isEmpty())
        out->workdir = out->repo;
    out->repo = QFileInfo(QDir(repoRoot).filePath(out->repo)).absoluteFilePath();
    out->workdir = QFileInfo(QDir(repoRoot).filePath(out->workdir)).absoluteFilePath();
    if (!out->docs.isEmpty())
        out->docs = QFileInfo(QDir(repoRoot).filePath(out->docs)).absoluteFilePath();

    return true;
}