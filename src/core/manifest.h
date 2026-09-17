#pragma once

#include "tool.h"

#include <QList>
#include <QString>

// Loads tool definitions from a directory of hand-written JSON manifests.
// One manifest file per tool, all paths resolved against the toolbox repo root.
class ManifestLoader
{
public:
    // Read every manifests/*.json under `repoRoot`. Hard failures set error().
    bool load(const QString &repoRoot, const QString &manifestsDirName);
    const QList<Tool> &tools() const;
    QString error() const;

private:
    bool parse(const QString &path, const QString &repoRoot, Tool *out);

    QList<Tool> m_tools;
    QString m_error;
};