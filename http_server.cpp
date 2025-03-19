#include "http_server.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QRunnable>
#include <QThread>

HttpServer::HttpServer(QObject *parent)
    : QObject(parent)
    , server_(std::make_unique<httplib::Server>())
    , isRunning_(false) {
    
    // Configure server's thread pool for parallel request handling
    // Use the number of CPU cores for optimal performance
    server_->new_task_queue = []{ return new httplib::ThreadPool(QThread::idealThreadCount()); };
    
    // Configure server settings
    server_->set_keep_alive_max_count(QThread::idealThreadCount() * 2);  // Max concurrent connections
    server_->set_payload_max_length(1024 * 1024 * 50);  // 50MB max payload
    server_->set_read_timeout(5, 0);  // 5 seconds read timeout
    server_->set_write_timeout(5, 0); // 5 seconds write timeout
    server_->set_mount_point("/", ".");
    setupRoutes();
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start(const QString& host, int port) {
    QMutexLocker locker(&mutex_);
    if (isRunning_) {
        qDebug() << "Server is already running";
        return false;
    }

    if (!server_->bind_to_port(host.toStdString(), port)) {
        qDebug() << "Failed to bind to port" << port;
        emit serverStarted(false);
        return false;
    }

    // Start server in a separate thread
    std::thread([this]() {
        if (server_->listen_after_bind()) {
            isRunning_ = true;
            emit serverStarted(true);
        } else {
            emit serverStarted(false);
        }
    }).detach();

    return true;
}

void HttpServer::stop() {
    QMutexLocker locker(&mutex_);
    if (!isRunning_) return;

    server_->stop();
    isRunning_ = false;
    emit serverStopped();
}

void HttpServer::setupRoutes() {
    // Handle new request
    server_->Post("/process", [this](const httplib::Request& req, httplib::Response& res) {
        handleNewRequest(req, res);
    });

    // Get result
    server_->Get("/result/:id", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetResult(req, res);
    });
}

void HttpServer::handleNewRequest(const httplib::Request& req, httplib::Response& res) {
    // Generate unique task ID
    QString taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    // Create request data
    RequestData requestData;
    requestData.taskId = taskId;
    requestData.payload = QByteArray::fromStdString(req.body);
    requestData.contentType = QString::fromStdString(req.get_header_value("Content-Type"));
    
    // Add any additional metadata from headers or query params
    if (req.has_header("X-Metadata")) {
        QJsonDocument metadataDoc = QJsonDocument::fromJson(
            QByteArray::fromStdString(req.get_header_value("X-Metadata")));
        if (!metadataDoc.isNull() && metadataDoc.isObject()) {
            requestData.metadata = metadataDoc.object().toVariantMap();
        }
    }

    // Store response object for async handling
    {
        QMutexLocker locker(&pendingResponsesMutex_);
        pendingResponses_[taskId] = &res;
    }

    // Emit signal for processing
    emit newRequestReceived(requestData);

    // Return task ID immediately
    QJsonObject response;
    response["taskId"] = taskId;
    res.set_content(QJsonDocument(response).toJson().toStdString(), "application/json");
}

void HttpServer::handleGetResult(const httplib::Request& req, httplib::Response& res) {
    QString taskId = QString::fromStdString(req.path_params.at("id"));
    
    // Check if result is ready
    {
        QMutexLocker locker(&completedResultsMutex_);
        auto it = completedResults_.find(taskId);
        if (it != completedResults_.end()) {
            const ResponseData& response = it.value();
            
            if (!response.success) {
                res.status = 500;
                res.set_content(response.errorMessage.toStdString(), "text/plain");
            } else {
                res.set_content(response.payload.toStdString(), 
                               response.contentType.toStdString());
            }
            
            // Don't remove the completed result here 
            // TODO: cleanup completed results after some time 
            return;
        }
    }
    
    // Result not ready
    res.status = 202; // Accepted but not ready
    QJsonObject response;
    response["taskId"] = taskId;
    response["status"] = "processing";
    res.set_content(QJsonDocument(response).toJson().toStdString(), "application/json");
}

void HttpServer::handleProcessingComplete(const ResponseData& response) {
    // Store the result
    {
        QMutexLocker locker(&completedResultsMutex_);
        completedResults_[response.taskId] = response;
    }
    
    // Check if there's a pending response waiting
    {
        QMutexLocker locker(&pendingResponsesMutex_);
        auto it = pendingResponses_.find(response.taskId);
        if (it != pendingResponses_.end()) {
            httplib::Response* res = it.value();
            
            if (!response.success) {
                res->status = 500;
                res->set_content(response.errorMessage.toStdString(), "text/plain");
            } else {
                res->set_content(response.payload.toStdString(), 
                                response.contentType.toStdString());
            }
            
            pendingResponses_.remove(response.taskId);
        }
    }
} 