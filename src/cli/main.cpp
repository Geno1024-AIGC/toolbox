#include "core/manifest.h"
#include "core/tool.h"

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

static void usage(QTextStream &out)
{
    out << "toolboxd - Toolbox service (" << QCoreApplication::applicationVersion() << ")\n"
        << "usage:\n"
        << "  toolboxd [--root DIR] list         list tools loaded from DIR/manifests\n"
        << "  toolboxd [--root DIR] --daemon     run headless HTTP service (default port 29811)\n"
        << "  toolboxd [--root DIR] --gui        start the desktop GUI and embed the HTTP service\n";
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
    bool list = false;
    for (int i = 0; i < cmds.size(); ++i) {
        if (cmds[i] == QLatin1String("--root") && i + 1 < cmds.size()) {
            root = QDir(cmds[++i]).absolutePath();
        } else if (cmds[i] == QLatin1String("--daemon")) {
            daemon = true;
        } else if (cmds[i] == QLatin1String("--gui")) {
            gui = true;
        } else if (cmds[i] == QLatin1String("list")) {
            list = true;
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

    if (list || !(daemon || gui)) {
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
        return list ? 0 : 0;
    }

    QTextStream err(stderr);
    err << "daemon / gui modes are not implemented yet" << Qt::endl;
    return 0;
}