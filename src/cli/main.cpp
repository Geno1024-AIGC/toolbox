#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("toolboxd"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QTextStream out(stdout);
    out << "toolboxd v" << app.applicationVersion() << Qt::endl;
    return 0;
}