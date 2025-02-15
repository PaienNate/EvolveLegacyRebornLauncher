
#ifndef EVOLVELEGACYREBORNLAUNCHER_FILEDOWNLOADER_H
#define EVOLVELEGACYREBORNLAUNCHER_FILEDOWNLOADER_H


#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QObject>
#include <QQueue>
#include <QUrl>
#include <xxhash.hpp>
#include <iostream>

enum DownloadState {
    VERIFYING,
    DOWNLOADING
};

class FileDownloader : public QObject {
Q_OBJECT

public:
    explicit FileDownloader(int maxParallelDownloads = 3, QObject* parent = nullptr) : QObject(parent), maxConcurrentDownloads(maxParallelDownloads) {
        connect(&manager, &QNetworkAccessManager::finished, this, &FileDownloader::onDownloadFinished);
    }

    void downloadFiles(const QList<QPair<QUrl, QString>>& fileList) {
        totalFiles = fileList.size();
        completedFiles = 0;

        for (const auto& file : fileList) {
            downloadAttempts[file] = 0;  // Initialize retry count
            checksumQueue.enqueue(file);
        }
        startNextChecksums();
    }

signals:
    void downloadProgressUpdated(int fileIndex, int percent, std::string* filename, DownloadState state);  // Per-file progress
    void totalProgressUpdated(int percent);  // Total progress
    void allDownloadsFinished(bool allSucceeded);  // Signal emitted when all downloads complete

private:
    struct DownloadTask {
        QFile* file{};
        qint64 totalSize = 0;
        qint64 receivedSize = 0;
        std::string* filename;
        DownloadState state = VERIFYING;
    };

    QNetworkAccessManager checksumManager;
    QNetworkAccessManager manager;
    QQueue<QPair<QUrl, QString>> checksumQueue;
    QQueue<QPair<QUrl, QString>> downloadQueue;
    QMap<QUrl, DownloadTask*> activeDownloads;
    QHash<QPair<QUrl, QString>, int> downloadAttempts;
    int maxConcurrentDownloads;
    const int maxRetries = 3;

    int totalFiles = 0;
    int completedFiles = 0;

    bool allSucceeded = true;

    void startNextChecksums() {
        while (!checksumQueue.isEmpty() && activeDownloads.size() < maxConcurrentDownloads) {
            auto [fileUrl, savePath] = checksumQueue.dequeue();
            QUrl checksumUrl = fileUrl.toString() + ".xxh64";

            QNetworkRequest request(checksumUrl);
            QNetworkReply* reply = checksumManager.get(request);

            connect(reply, &QNetworkReply::finished, this, [=, this]() { onChecksumReceived(reply, fileUrl, savePath);});
            connect(reply, &QNetworkReply::downloadProgress, this, &FileDownloader::onDownloadProgress);
            connect(reply, &QNetworkReply::errorOccurred, this, &FileDownloader::onError);

            auto task = (DownloadTask*) malloc(sizeof(DownloadTask));
            task->file = nullptr;
            task->totalSize = 0;
            task->receivedSize = 0;
            task->state = VERIFYING;

            auto filename = new std::string();
            filename->append(checksumUrl.fileName().toStdString());
            filename->replace(filename->length() - 6, 6, "");

            task->filename = filename;

            activeDownloads.insert(checksumUrl, task);
        }

        if (activeDownloads.isEmpty() && checksumQueue.isEmpty() && downloadQueue.isEmpty()) {
            emit allDownloadsFinished(allSucceeded);
        }
    }

    void onChecksumReceived(QNetworkReply* reply, const QUrl& fileUrl, const QString& savePath) {
        std::cout << "Checksum received: " << fileUrl.toString().toStdString() << std::endl;
        auto keys = activeDownloads.keys();
        for (const auto& key : keys) {
            std::cout << "Active Downloads: " << key.toString().toStdString() << std::endl;
        }
        if (!reply || !activeDownloads.contains(reply->url())) return;

        QByteArray responseRaw = reply->readAll();
        QByteArray serverChecksumArr = responseRaw.trimmed().split(' ')[0];

        bool parseOk = false;
        uint64_t serverChecksum = serverChecksumArr.toULongLong(&parseOk, 16);

        reply->deleteLater();
        activeDownloads.remove(reply->url());

        std::cout << "Response:" << responseRaw.data() << std::endl;
        std::cout << "Server Checksum Arr: " << serverChecksumArr.data() << " Conversion ok:" << parseOk << std::endl;
        std::cout << "Server Checksum: " << serverChecksum << " Conversion ok:" << parseOk << std::endl;

        if (serverChecksumArr.isEmpty() || !parseOk) {
            retryDownload(fileUrl, savePath);
            return;
        }

        if (fileExistsAndMatchesChecksum(savePath, serverChecksum)) {
            completedFiles++;
            updateTotalProgress();
            std::cout << "Verified: " << fileUrl.toString().toStdString() << std::endl;
        } else {
            downloadQueue.enqueue({fileUrl, savePath});
            std::cout << "Queued: " << fileUrl.toString().toStdString() << std::endl;
        }

        startNextDownloads();
    }

    static bool fileExistsAndMatchesChecksum(const QString& filePath, const uint64_t serverChecksum) {
        QFile file(filePath);
        if (!file.exists()) return false;

        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        constexpr size_t chunkSize = 1024 * 1024 * 2;  // Read 2MB chunks
        xxh::hash_state64_t stream;

        QByteArray buffer(chunkSize, Qt::Uninitialized);
        while (!file.atEnd()) {
            qint64 bytesRead = file.read(buffer.data(), chunkSize);
            stream.update(buffer.data(), static_cast<size_t>(bytesRead));
        }

        xxh::hash64_t hashValue = stream.digest();
        file.close();

        std::cout << "Checksum: " << hashValue << " - " << serverChecksum << std::endl;

        return hashValue == serverChecksum;
    }

    void startNextDownloads() {
        while (!downloadQueue.isEmpty() && activeDownloads.size() < maxConcurrentDownloads) {
            auto [url, savePath] = downloadQueue.dequeue();
            if (downloadAttempts[{url, savePath}] >= maxRetries) {
                allSucceeded = false;
                continue;
            }

            QNetworkRequest request(url);
            QNetworkReply* reply = manager.get(request);
            reply->setReadBufferSize(0);

            // make sure directory exists as QFile can't create them for us...
            QFileInfo fi(savePath);
            if (!fi.absoluteDir().mkpath(fi.absolutePath())) {
                continue;
            }

            auto file = new QFile(savePath);
            if (!file->open(QIODevice::ReadWrite)) {
                continue;
            }

            auto task = (DownloadTask*) malloc(sizeof(DownloadTask));
            task->file = file;
            task->totalSize = 0;
            task->receivedSize = 0;
            task->state = DOWNLOADING;

            auto filename = new std::string();
            filename->append(url.fileName().toStdString());

            task->filename = filename;

            activeDownloads.insert(url, task);

            connect(reply, &QNetworkReply::readyRead, this, &FileDownloader::onReadyRead);
            connect(reply, &QNetworkReply::downloadProgress, this, &FileDownloader::onDownloadProgress);
            connect(reply, &QNetworkReply::errorOccurred, this, &FileDownloader::onError);
        }

        if (!checksumQueue.isEmpty() && activeDownloads.size() < maxConcurrentDownloads) {
            startNextChecksums();
        }

        if (activeDownloads.isEmpty() && checksumQueue.isEmpty() && downloadQueue.isEmpty()) {
            emit allDownloadsFinished(allSucceeded);
        }
    }

private:
    void onReadyRead() {
        auto reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply || !activeDownloads.contains(reply->url())) return;

        auto task = activeDownloads[reply->url()];
        task->file->write(reply->readAll());
    }

    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
        auto reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply || !activeDownloads.contains(reply->url())) return;

        DownloadTask* task = activeDownloads[reply->url()];
        task->receivedSize = bytesReceived;
        task->totalSize = bytesTotal;

        // Update total progress
        QMapIterator<QUrl, DownloadTask*> i(activeDownloads);
        int x = 0;
        while (i.hasNext()) {
            i.next();
            emit downloadProgressUpdated(x++, (i.value()->totalSize > 0) ? (i.value()->receivedSize * 100 / i.value()->totalSize) : 0, i.value()->filename, i.value()->state);
        }

        updateTotalProgress();
    }

    void updateTotalProgress() {
        if (totalFiles > 0) {
            int overallProgress = completedFiles * 100 / totalFiles;
            emit totalProgressUpdated(overallProgress);
        }
    }

    void onDownloadFinished(QNetworkReply* reply) {
        if (!activeDownloads.contains(reply->url())) return;

        DownloadTask* task = activeDownloads[reply->url()];
        if (reply->error() == QNetworkReply::NoError) {
            completedFiles++;
            updateTotalProgress();
        } else {
            retryDownload(reply->url(), task->file->fileName());
        }

        if (task->file != nullptr) {
            task->file->close();
        }

        reply->deleteLater();
        activeDownloads.remove(reply->url());

        startNextDownloads();
    }

    void onError(QNetworkReply::NetworkError) {
        auto reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply || !activeDownloads.contains(reply->url())) return;

        DownloadTask* task = activeDownloads[reply->url()];

        if (task->file != nullptr) {
            retryDownload(reply->url(), task->file->fileName());
        } else {
            allSucceeded = false;
        }

        reply->deleteLater();
        activeDownloads.remove(reply->url());

        startNextDownloads();
    }

    void retryDownload(const QUrl& url, const QString& savePath) {
        QPair<QUrl, QString> fileKey = {url, savePath};
        int& attempts = downloadAttempts[fileKey];

        if (attempts < maxRetries) {
            attempts++;
            checksumQueue.enqueue(fileKey);
        } else {
            allSucceeded = false;
        }
    }
};

#endif //EVOLVELEGACYREBORNLAUNCHER_FILEDOWNLOADER_H
