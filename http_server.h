#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QMutex>
#include <QThreadPool>
#include <QVariant>
#include "httplib.h"

// Struct to hold request data
struct RequestData {
    QString taskId;
    QByteArray payload;
    QString contentType;
    QVariantMap metadata;
};

// Struct to hold response data
struct ResponseData {
    QString taskId;
    QByteArray payload;
    QString contentType;
    bool success;
    QString errorMessage;
};

class HttpServer : public QObject {
    Q_OBJECT

public:
    explicit HttpServer(QObject *parent = nullptr);
    ~HttpServer();

    bool start(const QString& host = "localhost", int port = 8080);
    void stop();

signals:
    // Emitted when a new request comes in that needs processing
    void newRequestReceived(const RequestData& request);
    void serverStarted(bool success);
    void serverStopped();

public slots:
    // Called when task processing is complete
    void handleProcessingComplete(const ResponseData& response);

private:
    void setupRoutes();
    void handleNewRequest(const httplib::Request& req, httplib::Response& res);
    void handleGetResult(const httplib::Request& req, httplib::Response& res);

    std::unique_ptr<httplib::Server> server_;
    QMutex mutex_;
    bool isRunning_;
    QThreadPool threadPool_;
    
    // Map to store pending responses
    QMap<QString, httplib::Response*> pendingResponses_;
    QMutex pendingResponsesMutex_;
    
    // Map to store completed results
    QMap<QString, ResponseData> completedResults_;
    QMutex completedResultsMutex_;
}; 