#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <functional>

struct HttpRequest
{
    QString method;
    QString path;
    QString query;
    QByteArray body;
    QHash<QString, QString> headers;
};

struct HttpResponse
{
    int status = 200;
    QByteArray contentType;
    QByteArray body;
};

// Minimal single-threaded HTTP/1.1 server built on QTcpServer. Each connection
// is served synchronously in the event loop and closed after one response.
class HttpServer : public QTcpServer
{
    Q_OBJECT
public:
    using Handler = std::function<bool(const HttpRequest &, HttpResponse *)>;

    explicit HttpServer(QObject *parent = nullptr);

    void setHandler(Handler handler);
    bool listen(quint16 port, const QHostAddress &address = QHostAddress::Any);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    void onReadyRead(QTcpSocket *socket, QByteArray &buffer);
    void send(QTcpSocket *socket, const HttpResponse &resp);

    Handler m_handler;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};