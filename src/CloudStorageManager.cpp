/**
 * @file CloudStorageManager.cpp
 * @brief Implementation of CloudStorageManager
 */

#include "CloudStorageManager.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QTemporaryFile>
#include <QDebug>

// ============ Singleton ============

CloudStorageManager& CloudStorageManager::instance()
{
    static CloudStorageManager instance;
    return instance;
}

// ============ Constructor/Destructor ============

CloudStorageManager::CloudStorageManager(QObject *parent)
    : QObject(parent)
    , m_isConnected(false)
    , m_currentProvider(Provider::None)
    , m_sdkAvailable(false)
    , m_sdkDownloading(false)
    , m_sdkDownloadProgress(0.0)
    , m_loading(false)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentDownload(nullptr)
    , m_cliProcess(new QProcess(this))
{
    // Set up SDK base path
    m_sdkBasePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/sdk";
    m_tempDownloadPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    
    // Ensure SDK directory exists
    QDir().mkpath(m_sdkBasePath);
    
    // Load saved credentials
    loadCredentials();
    
    qDebug() << "CloudStorageManager initialized, SDK path:" << m_sdkBasePath;
}

CloudStorageManager::~CloudStorageManager()
{
    if (m_currentDownload) {
        m_currentDownload->abort();
    }
    if (m_cliProcess->state() != QProcess::NotRunning) {
        m_cliProcess->terminate();
        m_cliProcess->waitForFinished(3000);
    }
}

// ============ Provider String ============

QString CloudStorageManager::currentProvider() const
{
    switch (m_currentProvider) {
        case Provider::S3: return "s3";
        case Provider::Azure: return "azure";
        case Provider::GCS: return "gcs";
        default: return "";
    }
}

// ============ SDK Management ============

bool CloudStorageManager::checkSdkAvailable(const QString &provider)
{
    Provider p = Provider::None;
    if (provider == "s3") p = Provider::S3;
    else if (provider == "azure") p = Provider::Azure;
    else if (provider == "gcs") p = Provider::GCS;
    
    QString cliPath = getCliExecutable(p);
    bool available = QFile::exists(cliPath);
    
    m_sdkAvailable = available;
    emit sdkStatusChanged();
    
    return available;
}

QString CloudStorageManager::getSdkPath(Provider provider) const
{
    switch (provider) {
        case Provider::S3:
            return m_sdkBasePath + "/aws";
        case Provider::Azure:
            return m_sdkBasePath + "/azure";
        case Provider::GCS:
            return m_sdkBasePath + "/gcloud";
        default:
            return QString();
    }
}

QString CloudStorageManager::getCliExecutable(Provider provider) const
{
    QString basePath = getSdkPath(provider);
    
#ifdef Q_OS_WIN
    switch (provider) {
        case Provider::S3:
            return basePath + "/aws.exe";
        case Provider::Azure:
            return basePath + "/az.cmd";
        case Provider::GCS:
            return basePath + "/bin/gcloud.cmd";
        default:
            return QString();
    }
#else
    switch (provider) {
        case Provider::S3:
            return basePath + "/aws";
        case Provider::Azure:
            return basePath + "/az";
        case Provider::GCS:
            return basePath + "/bin/gcloud";
        default:
            return QString();
    }
#endif
}

QString CloudStorageManager::getSdkDownloadUrl(Provider provider) const
{
#ifdef Q_OS_WIN
    switch (provider) {
        case Provider::S3:
            return "https://awscli.amazonaws.com/AWSCLIV2.msi";
        case Provider::Azure:
            return "https://aka.ms/installazurecliwindows";
        case Provider::GCS:
            return "https://dl.google.com/dl/cloudsdk/channels/rapid/GoogleCloudSDKInstaller.exe";
        default:
            return QString();
    }
#elif defined(Q_OS_MACOS)
    switch (provider) {
        case Provider::S3:
            return "https://awscli.amazonaws.com/AWSCLIV2.pkg";
        case Provider::Azure:
            return "https://aka.ms/InstallAzureCLIDeb"; // Use Homebrew instead
        case Provider::GCS:
            return "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-cli-darwin-x86_64.tar.gz";
        default:
            return QString();
    }
#else // Linux
    switch (provider) {
        case Provider::S3:
            return "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip";
        case Provider::Azure:
            return "https://aka.ms/InstallAzureCLIDeb";
        case Provider::GCS:
            return "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-cli-linux-x86_64.tar.gz";
        default:
            return QString();
    }
#endif
}

void CloudStorageManager::downloadSdk(const QString &provider)
{
    if (m_sdkDownloading) {
        setError(tr("SDK download already in progress"));
        return;
    }
    
    Provider p = Provider::None;
    if (provider == "s3") p = Provider::S3;
    else if (provider == "azure") p = Provider::Azure;
    else if (provider == "gcs") p = Provider::GCS;
    else {
        setError(tr("Unknown provider: %1").arg(provider));
        return;
    }
    
    QString url = getSdkDownloadUrl(p);
    if (url.isEmpty()) {
        setError(tr("SDK download URL not available for this platform"));
        return;
    }
    
    m_sdkDownloading = true;
    m_sdkDownloadProgress = 0.0;
    m_currentProvider = p;
    emit sdkStatusChanged();
    emit sdkDownloadProgressChanged();
    
    // Start download
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    m_currentDownload = m_networkManager->get(request);
    
    connect(m_currentDownload, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_sdkDownloadProgress = static_cast<double>(received) / total;
            emit sdkDownloadProgressChanged();
        }
    });
    
    connect(m_currentDownload, &QNetworkReply::finished, this, [this, p]() {
        if (m_currentDownload->error() != QNetworkReply::NoError) {
            m_sdkDownloading = false;
            emit sdkStatusChanged();
            setError(tr("Download failed: %1").arg(m_currentDownload->errorString()));
            emit sdkDownloadComplete(false, m_currentDownload->errorString());
            m_currentDownload->deleteLater();
            m_currentDownload = nullptr;
            return;
        }
        
        // Save downloaded file
        QString suffix;
        switch (p) {
            case Provider::S3:
#ifdef Q_OS_WIN
                suffix = ".msi";
#else
                suffix = ".zip";
#endif
                break;
            case Provider::Azure:
#ifdef Q_OS_WIN
                suffix = ".msi";
#else
                suffix = ".sh";
#endif
                break;
            case Provider::GCS:
#ifdef Q_OS_WIN
                suffix = ".exe";
#else
                suffix = ".tar.gz";
#endif
                break;
            default:
                suffix = ".bin";
        }
        
        QString downloadPath = m_tempDownloadPath + "/cloud_sdk_" + currentProvider() + suffix;
        QFile file(downloadPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_currentDownload->readAll());
            file.close();
            
            // Extract/install SDK
            extractSdk(downloadPath, p);
        } else {
            m_sdkDownloading = false;
            emit sdkStatusChanged();
            setError(tr("Failed to save downloaded file"));
            emit sdkDownloadComplete(false, tr("Failed to save downloaded file"));
        }
        
        m_currentDownload->deleteLater();
        m_currentDownload = nullptr;
    });
}

void CloudStorageManager::cancelSdkDownload()
{
    if (m_currentDownload) {
        m_currentDownload->abort();
        m_currentDownload->deleteLater();
        m_currentDownload = nullptr;
    }
    
    m_sdkDownloading = false;
    m_sdkDownloadProgress = 0.0;
    emit sdkStatusChanged();
    emit sdkDownloadProgressChanged();
}

void CloudStorageManager::extractSdk(const QString &archivePath, Provider provider)
{
    QString destPath = getSdkPath(provider);
    QDir().mkpath(destPath);
    
#ifdef Q_OS_WIN
    // On Windows, use msiexec or direct extraction
    // For now, provide instructions to user
    m_sdkDownloading = false;
    emit sdkStatusChanged();
    
    QString message = tr("SDK installer downloaded to: %1\n\nPlease run the installer manually, then restart the application.").arg(archivePath);
    emit sdkDownloadComplete(true, message);
    
#else
    // On Linux/macOS, extract archive
    QProcess *extractProcess = new QProcess(this);
    
    QStringList args;
    if (archivePath.endsWith(".zip")) {
        extractProcess->setProgram("unzip");
        args << "-o" << archivePath << "-d" << destPath;
    } else if (archivePath.endsWith(".tar.gz")) {
        extractProcess->setProgram("tar");
        args << "-xzf" << archivePath << "-C" << destPath;
    } else {
        // Shell script - make executable and run
        QFile::setPermissions(archivePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        extractProcess->setProgram("/bin/bash");
        args << archivePath << "--install-dir" << destPath;
    }
    
    extractProcess->setArguments(args);
    
    connect(extractProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, extractProcess, provider](int exitCode, QProcess::ExitStatus) {
        m_sdkDownloading = false;
        emit sdkStatusChanged();
        
        if (exitCode == 0) {
            m_sdkAvailable = true;
            emit sdkDownloadComplete(true, tr("SDK installed successfully"));
        } else {
            setError(tr("Failed to extract SDK: %1").arg(extractProcess->readAllStandardError()));
            emit sdkDownloadComplete(false, tr("Extraction failed"));
        }
        
        extractProcess->deleteLater();
    });
    
    extractProcess->start();
#endif
}

// ============ Connection Management ============

void CloudStorageManager::connectToCloud(const QString &provider, const QVariantMap &credentials)
{
    // Check SDK availability first
    if (!checkSdkAvailable(provider)) {
        setError(tr("SDK not available for %1. Please download it first.").arg(provider));
        return;
    }
    
    setLoading(true);
    m_currentCredentials = credentials;
    
    if (provider == "s3") {
        m_currentProvider = Provider::S3;
        connectS3(credentials);
    } else if (provider == "azure") {
        m_currentProvider = Provider::Azure;
        connectAzure(credentials);
    } else if (provider == "gcs") {
        m_currentProvider = Provider::GCS;
        connectGCS(credentials);
    } else {
        setLoading(false);
        setError(tr("Unknown provider: %1").arg(provider));
    }
}

void CloudStorageManager::disconnect()
{
    m_isConnected = false;
    m_currentProvider = Provider::None;
    m_currentBucket.clear();
    m_currentPath.clear();
    m_currentCredentials.clear();
    
    emit connectionChanged();
    emit pathChanged();
}

void CloudStorageManager::testConnection(const QString &provider, const QVariantMap &credentials)
{
    if (!checkSdkAvailable(provider)) {
        emit connectionTested(false, tr("SDK not available. Please download it first."));
        return;
    }
    
    setLoading(true);
    
    // Build test command based on provider
    QStringList args;
    QString cliPath;
    
    if (provider == "s3") {
        cliPath = getCliExecutable(Provider::S3);
        args << "sts" << "get-caller-identity";
        
        // Set AWS credentials environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("AWS_ACCESS_KEY_ID", credentials.value("accessKeyId").toString());
        env.insert("AWS_SECRET_ACCESS_KEY", credentials.value("secretAccessKey").toString());
        if (credentials.contains("region")) {
            env.insert("AWS_DEFAULT_REGION", credentials.value("region").toString());
        }
        m_cliProcess->setProcessEnvironment(env);
        
    } else if (provider == "azure") {
        cliPath = getCliExecutable(Provider::Azure);
        args << "account" << "show";
        
    } else if (provider == "gcs") {
        cliPath = getCliExecutable(Provider::GCS);
        args << "auth" << "list";
    }
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments(args);
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            emit connectionTested(true, tr("Connection successful"));
        } else {
            QString errorMsg = m_cliProcess->readAllStandardError();
            emit connectionTested(false, tr("Connection failed: %1").arg(errorMsg));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

// ============ S3 Implementation ============

void CloudStorageManager::connectS3(const QVariantMap &credentials)
{
    // Set up AWS credentials
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("AWS_ACCESS_KEY_ID", credentials.value("accessKeyId").toString());
    env.insert("AWS_SECRET_ACCESS_KEY", credentials.value("secretAccessKey").toString());
    if (credentials.contains("region")) {
        env.insert("AWS_DEFAULT_REGION", credentials.value("region").toString());
    }
    if (credentials.contains("sessionToken")) {
        env.insert("AWS_SESSION_TOKEN", credentials.value("sessionToken").toString());
    }
    m_cliProcess->setProcessEnvironment(env);
    
    // Test connection by listing buckets
    QString cliPath = getCliExecutable(Provider::S3);
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments({"s3", "ls", "--output", "json"});
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            m_isConnected = true;
            emit connectionChanged();
            
            // Parse and emit bucket list
            QString output = m_cliProcess->readAllStandardOutput();
            // AWS CLI s3 ls output is text, not JSON - parse it
            QVariantList buckets;
            for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
                // Format: "2023-01-15 10:30:00 bucket-name"
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QVariantMap bucket;
                    bucket["name"] = parts.last();
                    bucket["creationDate"] = parts[0] + " " + parts[1];
                    buckets.append(bucket);
                }
            }
            emit bucketsListed(buckets);
        } else {
            QString errorMsg = m_cliProcess->readAllStandardError();
            setError(tr("S3 connection failed: %1").arg(errorMsg));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::listS3Buckets()
{
    setLoading(true);
    
    QString cliPath = getCliExecutable(Provider::S3);
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments({"s3", "ls"});
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            QString output = m_cliProcess->readAllStandardOutput();
            QVariantList buckets;
            for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QVariantMap bucket;
                    bucket["name"] = parts.last();
                    bucket["creationDate"] = parts[0] + " " + parts[1];
                    bucket["type"] = "bucket";
                    buckets.append(bucket);
                }
            }
            emit bucketsListed(buckets);
        } else {
            setError(tr("Failed to list buckets: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::listS3Objects(const QString &bucket, const QString &prefix)
{
    setLoading(true);
    m_currentBucket = bucket;
    m_currentPath = prefix;
    emit pathChanged();
    
    QString cliPath = getCliExecutable(Provider::S3);
    QStringList args = {"s3", "ls"};
    
    QString s3Path = "s3://" + bucket;
    if (!prefix.isEmpty()) {
        s3Path += "/" + prefix;
    }
    args << s3Path;
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments(args);
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            QString output = m_cliProcess->readAllStandardOutput();
            QVariantList objects;
            
            for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
                QVariantMap obj;
                
                if (line.contains("PRE ")) {
                    // Directory (prefix)
                    QString dirName = line.mid(line.indexOf("PRE ") + 4).trimmed();
                    if (dirName.endsWith('/')) {
                        dirName.chop(1);
                    }
                    obj["name"] = dirName;
                    obj["isDir"] = true;
                    obj["size"] = 0;
                } else {
                    // File
                    // Format: "2023-01-15 10:30:00       1234 filename.txt"
                    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                    if (parts.size() >= 4) {
                        obj["modified"] = parts[0] + " " + parts[1];
                        obj["size"] = parts[2].toLongLong();
                        obj["name"] = parts.mid(3).join(' ');
                        obj["isDir"] = false;
                    }
                }
                
                if (!obj.isEmpty()) {
                    objects.append(obj);
                }
            }
            
            emit objectsListed(objects);
        } else {
            setError(tr("Failed to list objects: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::downloadS3Object(const QString &bucket, const QString &key)
{
    setLoading(true);
    
    // Create temp file for download
    QString localPath = m_tempDownloadPath + "/" + QFileInfo(key).fileName();
    
    QString cliPath = getCliExecutable(Provider::S3);
    QString s3Path = "s3://" + bucket + "/" + key;
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments({"s3", "cp", s3Path, localPath});
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, localPath, bucket, key](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            emit fileDownloaded(localPath, "s3://" + bucket + "/" + key);
        } else {
            setError(tr("Failed to download file: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

// ============ Azure Implementation ============

void CloudStorageManager::connectAzure(const QVariantMap &credentials)
{
    QString cliPath = getCliExecutable(Provider::Azure);
    QStringList args;
    
    // Azure login with service principal or interactive
    if (credentials.contains("tenantId") && credentials.contains("clientId")) {
        args << "login" << "--service-principal"
             << "-u" << credentials.value("clientId").toString()
             << "-p" << credentials.value("clientSecret").toString()
             << "--tenant" << credentials.value("tenantId").toString();
    } else if (credentials.contains("connectionString")) {
        // Use connection string directly for blob operations
        m_isConnected = true;
        emit connectionChanged();
        setLoading(false);
        listAzureContainers();
        return;
    } else {
        args << "login";
    }
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments(args);
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            m_isConnected = true;
            emit connectionChanged();
            listAzureContainers();
        } else {
            setError(tr("Azure login failed: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::listAzureContainers()
{
    setLoading(true);
    
    QString cliPath = getCliExecutable(Provider::Azure);
    QStringList args = {"storage", "container", "list", "--output", "json"};
    
    if (m_currentCredentials.contains("accountName")) {
        args << "--account-name" << m_currentCredentials.value("accountName").toString();
    }
    if (m_currentCredentials.contains("connectionString")) {
        args << "--connection-string" << m_currentCredentials.value("connectionString").toString();
    }
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments(args);
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            QString output = m_cliProcess->readAllStandardOutput();
            QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
            QVariantList containers;
            
            for (const QJsonValue &val : doc.array()) {
                QJsonObject obj = val.toObject();
                QVariantMap container;
                container["name"] = obj["name"].toString();
                container["type"] = "container";
                containers.append(container);
            }
            
            emit bucketsListed(containers);
        } else {
            setError(tr("Failed to list containers: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::listAzureBlobs(const QString &container, const QString &prefix)
{
    setLoading(true);
    m_currentBucket = container;
    m_currentPath = prefix;
    emit pathChanged();
    
    QString cliPath = getCliExecutable(Provider::Azure);
    QStringList args = {"storage", "blob", "list", "--container-name", container, "--output", "json"};
    
    if (!prefix.isEmpty()) {
        args << "--prefix" << prefix;
    }
    if (m_currentCredentials.contains("accountName")) {
        args << "--account-name" << m_currentCredentials.value("accountName").toString();
    }
    if (m_currentCredentials.contains("connectionString")) {
        args << "--connection-string" << m_currentCredentials.value("connectionString").toString();
    }
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments(args);
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            QString output = m_cliProcess->readAllStandardOutput();
            QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
            QVariantList blobs;
            
            for (const QJsonValue &val : doc.array()) {
                QJsonObject obj = val.toObject();
                QVariantMap blob;
                blob["name"] = obj["name"].toString();
                blob["size"] = obj["properties"].toObject()["contentLength"].toInt();
                blob["modified"] = obj["properties"].toObject()["lastModified"].toString();
                blob["isDir"] = obj["name"].toString().endsWith('/');
                blobs.append(blob);
            }
            
            emit objectsListed(blobs);
        } else {
            setError(tr("Failed to list blobs: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::downloadAzureBlob(const QString &container, const QString &blobName)
{
    setLoading(true);
    
    QString localPath = m_tempDownloadPath + "/" + QFileInfo(blobName).fileName();
    QString cliPath = getCliExecutable(Provider::Azure);
    QStringList args = {"storage", "blob", "download",
                        "--container-name", container,
                        "--name", blobName,
                        "--file", localPath};
    
    if (m_currentCredentials.contains("accountName")) {
        args << "--account-name" << m_currentCredentials.value("accountName").toString();
    }
    if (m_currentCredentials.contains("connectionString")) {
        args << "--connection-string" << m_currentCredentials.value("connectionString").toString();
    }
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments(args);
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, localPath, container, blobName](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            emit fileDownloaded(localPath, "azure://" + container + "/" + blobName);
        } else {
            setError(tr("Failed to download blob: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

// ============ GCS Implementation ============

void CloudStorageManager::connectGCS(const QVariantMap &credentials)
{
    QString cliPath = getCliExecutable(Provider::GCS);
    QStringList args;
    
    if (credentials.contains("keyFile")) {
        // Service account authentication
        args << "auth" << "activate-service-account" << "--key-file" << credentials.value("keyFile").toString();
    } else {
        // Interactive login
        args << "auth" << "login";
    }
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments(args);
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, credentials](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            // Set project if provided
            if (credentials.contains("project")) {
                QString cliPath = getCliExecutable(Provider::GCS);
                m_cliProcess->setProgram(cliPath);
                m_cliProcess->setArguments({"config", "set", "project", credentials.value("project").toString()});
                
                connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this](int exitCode, QProcess::ExitStatus) {
                    setLoading(false);
                    if (exitCode == 0) {
                        m_isConnected = true;
                        emit connectionChanged();
                        listGCSBuckets();
                    } else {
                        setError(tr("Failed to set GCS project"));
                    }
                }, Qt::SingleShotConnection);
                
                m_cliProcess->start();
            } else {
                setLoading(false);
                m_isConnected = true;
                emit connectionChanged();
                listGCSBuckets();
            }
        } else {
            setLoading(false);
            setError(tr("GCS authentication failed: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::listGCSBuckets()
{
    setLoading(true);
    
    QString cliPath = getCliExecutable(Provider::GCS);
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments({"storage", "buckets", "list", "--format=json"});
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            QString output = m_cliProcess->readAllStandardOutput();
            QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
            QVariantList buckets;
            
            for (const QJsonValue &val : doc.array()) {
                QJsonObject obj = val.toObject();
                QVariantMap bucket;
                // GCS bucket name format: gs://bucket-name
                QString name = obj["storage_url"].toString();
                if (name.startsWith("gs://")) {
                    name = name.mid(5);
                }
                if (name.endsWith('/')) {
                    name.chop(1);
                }
                bucket["name"] = name;
                bucket["type"] = "bucket";
                buckets.append(bucket);
            }
            
            emit bucketsListed(buckets);
        } else {
            setError(tr("Failed to list GCS buckets: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::listGCSObjects(const QString &bucket, const QString &prefix)
{
    setLoading(true);
    m_currentBucket = bucket;
    m_currentPath = prefix;
    emit pathChanged();
    
    QString cliPath = getCliExecutable(Provider::GCS);
    QString gcsPath = "gs://" + bucket;
    if (!prefix.isEmpty()) {
        gcsPath += "/" + prefix;
    }
    
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments({"storage", "ls", "-l", gcsPath});
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            QString output = m_cliProcess->readAllStandardOutput();
            QVariantList objects;
            
            for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
                // Skip summary line
                if (line.startsWith("TOTAL:")) continue;
                
                QVariantMap obj;
                QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                
                if (parts.size() >= 3) {
                    QString name = parts.last();
                    if (name.startsWith("gs://")) {
                        // Extract object name from full path
                        int slashIndex = name.indexOf('/', 5);
                        if (slashIndex > 0) {
                            name = name.mid(slashIndex + 1);
                        }
                    }
                    
                    obj["name"] = name;
                    obj["size"] = parts[0].toLongLong();
                    obj["modified"] = parts[1];
                    obj["isDir"] = name.endsWith('/');
                    objects.append(obj);
                }
            }
            
            emit objectsListed(objects);
        } else {
            setError(tr("Failed to list GCS objects: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

void CloudStorageManager::downloadGCSObject(const QString &bucket, const QString &objectName)
{
    setLoading(true);
    
    QString localPath = m_tempDownloadPath + "/" + QFileInfo(objectName).fileName();
    QString gcsPath = "gs://" + bucket + "/" + objectName;
    
    QString cliPath = getCliExecutable(Provider::GCS);
    m_cliProcess->setProgram(cliPath);
    m_cliProcess->setArguments({"storage", "cp", gcsPath, localPath});
    
    connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, localPath, bucket, objectName](int exitCode, QProcess::ExitStatus) {
        setLoading(false);
        
        if (exitCode == 0) {
            emit fileDownloaded(localPath, "gs://" + bucket + "/" + objectName);
        } else {
            setError(tr("Failed to download GCS object: %1").arg(m_cliProcess->readAllStandardError()));
        }
    }, Qt::SingleShotConnection);
    
    m_cliProcess->start();
}

// ============ Public Interface ============

void CloudStorageManager::listBuckets()
{
    switch (m_currentProvider) {
        case Provider::S3:
            listS3Buckets();
            break;
        case Provider::Azure:
            listAzureContainers();
            break;
        case Provider::GCS:
            listGCSBuckets();
            break;
        default:
            setError(tr("Not connected to any cloud provider"));
    }
}

void CloudStorageManager::listObjects(const QString &bucket, const QString &prefix)
{
    switch (m_currentProvider) {
        case Provider::S3:
            listS3Objects(bucket, prefix);
            break;
        case Provider::Azure:
            listAzureBlobs(bucket, prefix);
            break;
        case Provider::GCS:
            listGCSObjects(bucket, prefix);
            break;
        default:
            setError(tr("Not connected to any cloud provider"));
    }
}

void CloudStorageManager::downloadFile(const QString &bucket, const QString &key)
{
    switch (m_currentProvider) {
        case Provider::S3:
            downloadS3Object(bucket, key);
            break;
        case Provider::Azure:
            downloadAzureBlob(bucket, key);
            break;
        case Provider::GCS:
            downloadGCSObject(bucket, key);
            break;
        default:
            setError(tr("Not connected to any cloud provider"));
    }
}

void CloudStorageManager::navigateUp()
{
    if (m_currentPath.isEmpty()) {
        // At bucket root, go back to bucket list
        m_currentBucket.clear();
        emit pathChanged();
        listBuckets();
    } else {
        // Navigate to parent directory
        int lastSlash = m_currentPath.lastIndexOf('/');
        if (lastSlash > 0) {
            m_currentPath = m_currentPath.left(lastSlash);
        } else {
            m_currentPath.clear();
        }
        emit pathChanged();
        listObjects(m_currentBucket, m_currentPath);
    }
}

void CloudStorageManager::navigateTo(const QString &bucket, const QString &path)
{
    m_currentBucket = bucket;
    m_currentPath = path;
    emit pathChanged();
    listObjects(bucket, path);
}

// ============ Credential Management ============

void CloudStorageManager::saveCredentials(const QString &name, const QString &provider, const QVariantMap &credentials)
{
    QVariantMap cred = credentials;
    cred["provider"] = provider;
    cred["name"] = name;
    
    // Encrypt sensitive fields
    if (cred.contains("secretAccessKey")) {
        cred["secretAccessKey"] = encryptCredential(cred.value("secretAccessKey").toString());
    }
    if (cred.contains("clientSecret")) {
        cred["clientSecret"] = encryptCredential(cred.value("clientSecret").toString());
    }
    if (cred.contains("connectionString")) {
        cred["connectionString"] = encryptCredential(cred.value("connectionString").toString());
    }
    
    m_savedCredentials[name] = cred;
    saveCredentialsToSettings();
    emit credentialsChanged();
}

void CloudStorageManager::deleteCredentials(const QString &name)
{
    m_savedCredentials.remove(name);
    saveCredentialsToSettings();
    emit credentialsChanged();
}

QVariantList CloudStorageManager::getSavedCredentials() const
{
    QVariantList list;
    for (const auto &cred : m_savedCredentials) {
        QVariantMap item;
        item["name"] = cred.value("name");
        item["provider"] = cred.value("provider");
        list.append(item);
    }
    return list;
}

QVariantMap CloudStorageManager::getCredentials(const QString &name) const
{
    if (!m_savedCredentials.contains(name)) {
        return QVariantMap();
    }
    
    QVariantMap cred = m_savedCredentials.value(name);
    
    // Decrypt sensitive fields
    if (cred.contains("secretAccessKey")) {
        cred["secretAccessKey"] = decryptCredential(cred.value("secretAccessKey").toString());
    }
    if (cred.contains("clientSecret")) {
        cred["clientSecret"] = decryptCredential(cred.value("clientSecret").toString());
    }
    if (cred.contains("connectionString")) {
        cred["connectionString"] = decryptCredential(cred.value("connectionString").toString());
    }
    
    return cred;
}

void CloudStorageManager::loadCredentials()
{
    QSettings settings;
    settings.beginGroup("CloudStorage");
    
    int count = settings.beginReadArray("Credentials");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        QString name = settings.value("name").toString();
        QVariantMap cred;
        cred["name"] = name;
        cred["provider"] = settings.value("provider").toString();
        cred["accessKeyId"] = settings.value("accessKeyId").toString();
        cred["secretAccessKey"] = settings.value("secretAccessKey").toString();
        cred["region"] = settings.value("region").toString();
        cred["accountName"] = settings.value("accountName").toString();
        cred["connectionString"] = settings.value("connectionString").toString();
        cred["tenantId"] = settings.value("tenantId").toString();
        cred["clientId"] = settings.value("clientId").toString();
        cred["clientSecret"] = settings.value("clientSecret").toString();
        cred["project"] = settings.value("project").toString();
        cred["keyFile"] = settings.value("keyFile").toString();
        
        m_savedCredentials[name] = cred;
    }
    settings.endArray();
    settings.endGroup();
}

void CloudStorageManager::saveCredentialsToSettings()
{
    QSettings settings;
    settings.beginGroup("CloudStorage");
    
    settings.beginWriteArray("Credentials");
    int i = 0;
    for (const auto &cred : m_savedCredentials) {
        settings.setArrayIndex(i++);
        settings.setValue("name", cred.value("name"));
        settings.setValue("provider", cred.value("provider"));
        settings.setValue("accessKeyId", cred.value("accessKeyId"));
        settings.setValue("secretAccessKey", cred.value("secretAccessKey"));
        settings.setValue("region", cred.value("region"));
        settings.setValue("accountName", cred.value("accountName"));
        settings.setValue("connectionString", cred.value("connectionString"));
        settings.setValue("tenantId", cred.value("tenantId"));
        settings.setValue("clientId", cred.value("clientId"));
        settings.setValue("clientSecret", cred.value("clientSecret"));
        settings.setValue("project", cred.value("project"));
        settings.setValue("keyFile", cred.value("keyFile"));
    }
    settings.endArray();
    settings.endGroup();
}

QString CloudStorageManager::encryptCredential(const QString &credential) const
{
    // Simple XOR encryption with machine-specific key
    QByteArray key = QSysInfo::machineUniqueId();
    if (key.isEmpty()) {
        key = "BigFileViewer_CloudStorage_Key";
    }
    
    QByteArray data = credential.toUtf8();
    QByteArray result;
    
    for (int i = 0; i < data.size(); ++i) {
        result.append(data[i] ^ key[i % key.size()]);
    }
    
    return result.toBase64();
}

QString CloudStorageManager::decryptCredential(const QString &encrypted) const
{
    QByteArray key = QSysInfo::machineUniqueId();
    if (key.isEmpty()) {
        key = "BigFileViewer_CloudStorage_Key";
    }
    
    QByteArray data = QByteArray::fromBase64(encrypted.toLatin1());
    QByteArray result;
    
    for (int i = 0; i < data.size(); ++i) {
        result.append(data[i] ^ key[i % key.size()]);
    }
    
    return QString::fromUtf8(result);
}

// ============ Helper Methods ============

void CloudStorageManager::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}

void CloudStorageManager::setError(const QString &error)
{
    m_lastError = error;
    emit errorOccurred(error);
    qWarning() << "CloudStorageManager error:" << error;
}
