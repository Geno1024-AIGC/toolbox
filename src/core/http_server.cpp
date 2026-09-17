#include "http_server.h"

#include <QTcpSocket>
#include <QHash>

using HeaderMap = QHash<QString, QString>;

HttpServer::HttpServer(QObject *parent)
    : QTcpServer(parent)
{
}

void HttpServer::setHandler(Handler handler)
{
    m_handler = std::move(handler);
}

bool HttpServer::listen(quint16 port, const QHostAddress &address)
{
    return QTcpServer::listen(address, port);
}

// Parse "k: v" header lines into a case-insensitive map.
static HeaderMap parseHeaders(const QByteArray &block)
{
    HeaderMap headers;
    const QList<QByteArray> lines = block.split('\n');
    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        const int colon = trimmed.indexOf(':');
        if (colon <= 0)
            continue;
        const QString key = QString::fromUtf8(trimmed.left(colon)).trimmed().toLower();
        const QString val = QString::fromUtf8(trimmed.mid(colon + 1)).trimmed();
        headers.insert(key, val);
    }
    return headers;
}

static QString reasonPhrase(int status)
{
    switch (status) {
    case 200: return QStringLiteral("OK");
    case 400: return QStringLiteral("Bad Request");
    case 404: return QStringLiteral("Not Found");
    case 405: return QStringLiteral("Method Not Allowed");
    case 500: return QStringLiteral("Internal Server Error");
    default:  return QStringLiteral("OK");
    }
}

void HttpServer::onReadyRead(QTcpSocket *socket, QByteArray &buffer)
{
    buffer.append(socket->readAll());

    const int headEndCrlf = buffer.indexOf("\r\n\r\n");
    const int headEndLf = headEndCrlf < 0 ? buffer.indexOf("\n\n") : -1;
    if (headEndCrlf < 0 && headEndLf < 0)
        return; // headers incomplete, wait for more data

    const int split = headEndCrlf >= 0 ? headEndCrlf : headEndLf;
    const int headerLen = headEndCrlf >= 0 ? split + 4 : split + 2;
    const QByteArray head = buffer.left(headerLen);

    // Parse the request line.
    const int eol = head.indexOf('\n');
    const QList<QByteArray> parts = head.left(eol).trimmed().split(' ');
    if (parts.size() < 2) {
        HttpResponse resp; resp.status = 400;
        resp.body = QByteArrayLiteral("bad request");
        send(socket, resp);
        return;
    }

    HttpRequest req;
    req.method = QString::fromUtf8(parts[0]).toUpper();
    const QString target = QString::fromUtf8(parts[1]);
    const int qpos = target.indexOf('?');
    req.path = qpos >= 0 ? target.left(qpos) : target;
    req.query = qpos >= 0 ? target.mid(qpos + 1) : QString();
    req.headers = parseHeaders(head);

    if (req.method == QLatin1String("OPTIONS")) {
        HttpResponse preflight;
        preflight.status = 200;
        send(socket, preflight);
        return;
    }

    const int bodyLen = req.headers.value(QStringLiteral("content-length")).toInt();
    const QByteArray bodyBlock = buffer.mid(headerLen);
    if (bodyBlock.size() < bodyLen)
        return; // body incomplete, wait for more data
    req.body = bodyBlock.left(bodyLen);
    buffer = buffer.mid(headerLen + bodyLen); // keep a possible pipelined next request

    HttpResponse resp;
    if (m_handler && m_handler(req, &resp)) {
        send(socket, resp);
    } else {
        HttpResponse nf;
        nf.status = 404;
        nf.body = QByteArrayLiteral("not found");
        send(socket, nf);
    }
}

void HttpServer::send(QTcpSocket *socket, const HttpResponse &resp)
{
    QByteArray contentType = resp.contentType.isEmpty()
        ? QByteArrayLiteral("text/plain; charset=utf-8") : resp.contentType;

    QByteArray out;
    out += "HTTP/1.1 " + QByteArray::number(resp.status) + " " + reasonPhrase(resp.status).toUtf8() + "\r\n";
    out += "Content-Type: " + contentType + "\r\n";
    out += "Content-Length: " + QByteArray::number(resp.body.size()) + "\r\n";
    out += "Connection: close\r\n";
    out += "Access-Control-Allow-Origin: *\r\n";
    out += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out += "Access-Control-Allow-Headers: Content-Type\r\n";
    out += "\r\n";
    out += resp.body;

    socket->write(out);
    socket->disconnectFromHost(); // flushes pending writes, then emits disconnected
}

void HttpServer::incomingConnection(qintptr socketDescriptor)
{
    auto *socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        delete socket;
        return;
    }
    m_buffers.insert(socket, QByteArray());
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        if (auto it = m_buffers.find(socket); it != m_buffers.end())
            onReadyRead(socket, it.value());
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QObject::destroyed, this, [this, socket]() {
        m_buffers.remove(socket);
    });
}