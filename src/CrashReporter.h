/**
 * @file CrashReporter.h
 * @brief 崩溃报告收集器
 * 
 * 功能：
 * - 捕获未处理异常和崩溃
 * - 收集系统信息和崩溃上下文
 * - 生成崩溃报告文件
 * - 可选：上传到服务器
 */

#ifndef CRASHREPORTER_H
#define CRASHREPORTER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>

// 前向声明避免包含平台头文件
#ifdef Q_OS_WIN
struct _EXCEPTION_POINTERS;
#endif

/**
 * @brief 崩溃报告数据结构
 */
struct CrashReport {
    QString id;                     // 唯一报告ID
    QDateTime timestamp;            // 崩溃时间
    QString appVersion;             // 应用版本
    QString osName;                 // 操作系统名称
    QString osVersion;              // 操作系统版本
    QString cpuArch;                // CPU架构
    QString exceptionType;          // 异常类型
    QString exceptionMessage;       // 异常消息
    QString stackTrace;             // 堆栈跟踪
    QVariantMap context;            // 崩溃上下文
    QString lastAction;             // 最后执行的操作
    QString openedFile;             // 当前打开的文件
    qint64 memoryUsage;             // 内存使用量
    
    QJsonObject toJson() const;
    static CrashReport fromJson(const QJsonObject &json);
};

/**
 * @brief 崩溃报告收集器
 */
class CrashReporter : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool autoUpload READ autoUpload WRITE setAutoUpload NOTIFY autoUploadChanged)
    Q_PROPERTY(QString uploadUrl READ uploadUrl WRITE setUploadUrl NOTIFY uploadUrlChanged)
    Q_PROPERTY(int pendingReportsCount READ pendingReportsCount NOTIFY pendingReportsCountChanged)
    
public:
    static CrashReporter* instance();
    
    explicit CrashReporter(QObject *parent = nullptr);
    ~CrashReporter();
    
    // 初始化（在main()早期调用）
    static void initialize(const QString &appVersion);
    static void shutdown();
    
    // 属性
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    
    bool autoUpload() const { return m_autoUpload; }
    void setAutoUpload(bool autoUpload);
    
    QString uploadUrl() const { return m_uploadUrl; }
    void setUploadUrl(const QString &url);
    
    int pendingReportsCount() const;
    
    // 上下文记录（在关键操作前调用）
    Q_INVOKABLE void setContext(const QString &key, const QVariant &value);
    Q_INVOKABLE void clearContext();
    Q_INVOKABLE void setLastAction(const QString &action);
    Q_INVOKABLE void setOpenedFile(const QString &filePath);
    
    // 手动报告
    Q_INVOKABLE void reportException(const QString &type, const QString &message, 
                                      const QString &stackTrace = QString());
    Q_INVOKABLE void reportError(const QString &message);
    
    // 报告管理
    Q_INVOKABLE QVariantList getPendingReports();
    Q_INVOKABLE bool uploadReport(const QString &reportId);
    Q_INVOKABLE void uploadAllPending();
    Q_INVOKABLE bool deleteReport(const QString &reportId);
    Q_INVOKABLE void clearAllReports();
    
    // 导出报告
    Q_INVOKABLE QString exportReport(const QString &reportId, const QString &filePath);
    Q_INVOKABLE QString getReportDetails(const QString &reportId);
    
    // 用户反馈
    Q_INVOKABLE void submitFeedback(const QString &reportId, const QString &userEmail,
                                     const QString &userComment);
    
signals:
    void enabledChanged();
    void autoUploadChanged();
    void uploadUrlChanged();
    void pendingReportsCountChanged();
    
    void crashDetected(const QString &reportId);
    void reportUploaded(const QString &reportId, bool success, const QString &message);
    
private:
    static CrashReporter* s_instance;
    
    void installHandlers();
    void uninstallHandlers();
    
    QString collectSystemInfo();
    QString captureStackTrace();
    qint64 getCurrentMemoryUsage();
    
    void saveCrashReport(const CrashReport &report);
    QString getReportsDirectory() const;
    QString generateReportId() const;
    
    // Windows 特定
#ifdef Q_OS_WIN
    static long __stdcall windowsExceptionHandler(_EXCEPTION_POINTERS *exceptionInfo);
#endif
    
    // Unix 特定
#ifdef Q_OS_UNIX
    static void unixSignalHandler(int signal);
#endif
    
    bool m_enabled = true;
    bool m_autoUpload = false;
    QString m_uploadUrl;
    QString m_appVersion;
    
    QVariantMap m_context;
    QString m_lastAction;
    QString m_openedFile;
};

#endif // CRASHREPORTER_H
