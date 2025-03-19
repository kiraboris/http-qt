#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include "http_server.h"

// Example task processor class
class TaskProcessor : public QObject {
    Q_OBJECT
public:
    explicit TaskProcessor(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void processRequest(const RequestData& request) {
        qDebug() << "Processing request:" << request.taskId
                 << "Content-Type:" << request.contentType
                 << "Payload size:" << request.payload.size();

        // Simulate processing
        QThread::msleep(2000);

        // Create response
        ResponseData response;
        response.taskId = request.taskId;
        response.success = true;
        response.contentType = "application/json";
        response.payload = "{\"processed\": true}";

        emit processingComplete(response);
    }

signals:
    void processingComplete(const ResponseData& response);
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // Create server and processor
    HttpServer server;
    TaskProcessor processor;
    
    // Connect signals/slots
    QObject::connect(&server, &HttpServer::serverStarted, [](bool success) {
        if (success) {
            qDebug() << "Server started successfully";
        } else {
            qDebug() << "Failed to start server";
            QCoreApplication::exit(1);
        }
    });
    
    QObject::connect(&server, &HttpServer::serverStopped, []() {
        qDebug() << "Server stopped";
        QCoreApplication::quit();
    });
    
    // Connect server to processor
    QObject::connect(&server, &HttpServer::newRequestReceived,
                    &processor, &TaskProcessor::processRequest);
    QObject::connect(&processor, &TaskProcessor::processingComplete,
                    &server, &HttpServer::handleProcessingComplete);
    
    if (!server.start("localhost", 8080)) {
        qDebug() << "Failed to start server";
        return 1;
    }
    
    return app.exec();
}

#include "main.moc" 