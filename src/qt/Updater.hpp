#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QTemporaryDir>
#include <QFile>
#include <QDebug>
#include <QProcess>

const QString VERSION = "1";  // Current version
const QString BASE_URL = "https://evolve.a1btraum.de/";  // Change this to your update URL

class Updater : public QObject {
Q_OBJECT

public:
    explicit Updater(QObject *parent = nullptr) : QObject(parent) {
        manager = new QNetworkAccessManager(this);
        checkForUpdate();
    }

signals:
    void newVersionDetected(const QString& version);
    void updateExtracted(const QString& path);

private:
    QNetworkAccessManager *manager;
    QString latestVersion;
    QString zipFilePath;
    QString extractDirPath;

    void checkForUpdate() {
        QUrl versionUrl(BASE_URL + "/version.txt");
        QNetworkRequest request(versionUrl);

        QNetworkReply *reply = manager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                QString serverVersion = QString::fromUtf8(reply->readAll()).trimmed();

                if (serverVersion > VERSION) {
                    latestVersion = serverVersion;
                    emit newVersionDetected(serverVersion);
                    // new version found
                    downloadUpdate();
                }
            }
            reply->deleteLater();
        });
    }

    void downloadUpdate() {
        QUrl zipUrl(BASE_URL + "/latest.zip");
        QNetworkRequest request(zipUrl);

        QNetworkReply *reply = manager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                QTemporaryDir tempDir;
                if (!tempDir.isValid()) {
                    return;
                }

                zipFilePath = tempDir.path() + "/latest.zip";
                extractDirPath = tempDir.path() + "/extracted";

                QFile file(zipFilePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(reply->readAll());
                    file.close();
                    extractZip();
                }
            }
            reply->deleteLater();
        });
    }

    void extractZip() {
        QDir().mkpath(extractDirPath);  // Ensure extraction directory exists

        QStringList args;
        auto process = new QProcess(this);
        connect(process, &QProcess::finished, this, [process, this](int exitCode) {
            if (exitCode == 0) {
                emit updateExtracted(extractDirPath);
            }
            process->deleteLater();
        });

        // Use PowerShell's Expand-Archive
        args << "-Command"
             << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(zipFilePath, extractDirPath);
        process->start("powershell", args);
    }
};

