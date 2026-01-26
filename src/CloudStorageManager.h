/**
 * @file CloudStorageManager.h
 * @brief Cloud Storage Manager for BigFileViewer
 * 
 * Support for:
 * - Amazon S3
 * - Azure Blob Storage
 * - Google Cloud Storage (GCS)
 * 
 * SDK download on first use (runtime download)
 */

#ifndef CLOUDSTORAGEMANAGER_H
#define CLOUDSTORAGEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

/**
 * @class CloudStorageManager
 * @brief Manages cloud storage access via CLI tools (runtime downloaded)
 * 
 * Features:
 * - Runtime SDK/CLI download on first use
 * - S3 bucket browsing and file download
 * - Azure Blob container browsing and file download
 * - GCS bucket browsing and file download
 * - Credential management
 * - Download progress tracking
 */
class CloudStorageManager : public QObject
{
    Q_OBJECT
    
    // ===== QML Properties =====
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString currentProvider READ currentProvider NOTIFY connectionChanged)
    Q_PROPERTY(QString currentBucket READ currentBucket NOTIFY pathChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY pathChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    Q_PROPERTY(bool sdkAvailable READ isSdkAvailable NOTIFY sdkStatusChanged)
    Q_PROPERTY(bool sdkDownloading READ isSdkDownloading NOTIFY sdkStatusChanged)
    Q_PROPERTY(double sdkDownloadProgress READ sdkDownloadProgress NOTIFY sdkDownloadProgressChanged)
    
public:
    /**
     * @brief Cloud storage provider type
     */
    enum class Provider {
        None,
        S3,      // Amazon S3
        Azure,   // Azure Blob Storage
        GCS      // Google Cloud Storage
    };
    Q_ENUM(Provider)
    
    /**
     * @brief Singleton instance accessor
     */
    static CloudStorageManager& instance();
    
    // ===== State Properties =====
    
    bool isConnected() const { return m_isConnected; }
    QString currentProvider() const;
    QString currentBucket() const { return m_currentBucket; }
    QString currentPath() const { return m_currentPath; }
    bool isLoading() const { return m_loading; }
    QString lastError() const { return m_lastError; }
    bool isSdkAvailable() const { return m_sdkAvailable; }
    bool isSdkDownloading() const { return m_sdkDownloading; }
    double sdkDownloadProgress() const { return m_sdkDownloadProgress; }
    
    // ===== Q_INVOKABLE Methods for QML =====
    
    /**
     * @brief Check if SDK is available for the provider
     */
    Q_INVOKABLE bool checkSdkAvailable(const QString &provider);
    
    /**
     * @brief Download SDK for the specified provider
     */
    Q_INVOKABLE void downloadSdk(const QString &provider);
    
    /**
     * @brief Cancel SDK download
     */
    Q_INVOKABLE void cancelSdkDownload();
    
    /**
     * @brief Connect to cloud storage
     * @param provider "s3", "azure", or "gcs"
     * @param credentials Provider-specific credentials
     */
    Q_INVOKABLE void connectToCloud(const QString &provider, const QVariantMap &credentials);
    
    /**
     * @brief Disconnect from current cloud storage
     */
    Q_INVOKABLE void disconnect();
    
    /**
     * @brief Test connection with given credentials
     */
    Q_INVOKABLE void testConnection(const QString &provider, const QVariantMap &credentials);
    
    /**
     * @brief List buckets/containers
     */
    Q_INVOKABLE void listBuckets();
    
    /**
     * @brief List objects in a bucket/container
     * @param bucket Bucket/container name
     * @param prefix Path prefix (folder)
     */
    Q_INVOKABLE void listObjects(const QString &bucket, const QString &prefix = QString());
    
    /**
     * @brief Download a file from cloud storage
     * @param bucket Bucket/container name
     * @param key Object key (full path)
     */
    Q_INVOKABLE void downloadFile(const QString &bucket, const QString &key);
    
    /**
     * @brief Navigate to parent directory
     */
    Q_INVOKABLE void navigateUp();
    
    /**
     * @brief Navigate to a specific path
     */
    Q_INVOKABLE void navigateTo(const QString &bucket, const QString &path);
    
    // ===== Credential Management =====
    
    /**
     * @brief Save credentials for a provider
     */
    Q_INVOKABLE void saveCredentials(const QString &name, const QString &provider, const QVariantMap &credentials);
    
    /**
     * @brief Delete saved credentials
     */
    Q_INVOKABLE void deleteCredentials(const QString &name);
    
    /**
     * @brief Get all saved credentials
     */
    Q_INVOKABLE QVariantList getSavedCredentials() const;
    
    /**
     * @brief Get specific credentials
     */
    Q_INVOKABLE QVariantMap getCredentials(const QString &name) const;

signals:
    void connectionChanged();
    void pathChanged();
    void loadingChanged();
    void errorOccurred(const QString &error);
    void sdkStatusChanged();
    void sdkDownloadProgressChanged();
    
    /**
     * @brief Emitted when connection test completes
     */
    void connectionTested(bool success, const QString &message);
    
    /**
     * @brief Emitted when bucket list is ready
     */
    void bucketsListed(const QVariantList &buckets);
    
    /**
     * @brief Emitted when object list is ready
     */
    void objectsListed(const QVariantList &objects);
    
    /**
     * @brief Emitted when file download completes
     */
    void fileDownloaded(const QString &localPath, const QString &cloudPath);
    
    /**
     * @brief Emitted during file download
     */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    
    /**
     * @brief Emitted when SDK download completes
     */
    void sdkDownloadComplete(bool success, const QString &message);
    
    /**
     * @brief Emitted when credentials change
     */
    void credentialsChanged();

private:
    explicit CloudStorageManager(QObject *parent = nullptr);
    ~CloudStorageManager();
    
    // Disable copy
    CloudStorageManager(const CloudStorageManager&) = delete;
    CloudStorageManager& operator=(const CloudStorageManager&) = delete;
    
    // ===== Internal Methods =====
    
    void setLoading(bool loading);
    void setError(const QString &error);
    QString getSdkPath(Provider provider) const;
    QString getSdkDownloadUrl(Provider provider) const;
    QString getCliExecutable(Provider provider) const;
    
    // Provider-specific implementations
    void connectS3(const QVariantMap &credentials);
    void connectAzure(const QVariantMap &credentials);
    void connectGCS(const QVariantMap &credentials);
    
    void listS3Buckets();
    void listAzureContainers();
    void listGCSBuckets();
    
    void listS3Objects(const QString &bucket, const QString &prefix);
    void listAzureBlobs(const QString &container, const QString &prefix);
    void listGCSObjects(const QString &bucket, const QString &prefix);
    
    void downloadS3Object(const QString &bucket, const QString &key);
    void downloadAzureBlob(const QString &container, const QString &blobName);
    void downloadGCSObject(const QString &bucket, const QString &objectName);
    
    // SDK download helpers
    void downloadAwsCli();
    void downloadAzureCli();
    void downloadGcloudCli();
    void extractSdk(const QString &archivePath, Provider provider);
    
    // Credential helpers
    void loadCredentials();
    void saveCredentialsToSettings();
    QString encryptCredential(const QString &credential) const;
    QString decryptCredential(const QString &encrypted) const;
    
    // Process helpers
    void runCliCommand(const QStringList &args, std::function<void(int, const QString&, const QString&)> callback);
    
    // ===== Member Variables =====
    
    // Connection state
    bool m_isConnected;
    Provider m_currentProvider;
    QString m_currentBucket;
    QString m_currentPath;
    QVariantMap m_currentCredentials;
    
    // SDK state
    bool m_sdkAvailable;
    bool m_sdkDownloading;
    double m_sdkDownloadProgress;
    
    // State
    bool m_loading;
    QString m_lastError;
    
    // Network
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentDownload;
    
    // Process
    QProcess *m_cliProcess;
    
    // Saved credentials
    QMap<QString, QVariantMap> m_savedCredentials;
    
    // Paths
    QString m_sdkBasePath;
    QString m_tempDownloadPath;
};

#endif // CLOUDSTORAGEMANAGER_H
