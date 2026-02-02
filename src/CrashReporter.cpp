/**
 * @file CrashReporter.cpp
 * @brief 崩溃报告收集器实现
 */

#include "CrashReporter.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#endif

#ifdef Q_OS_UNIX
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <sys/resource.h>
#endif

// ============================================================================
// CrashReport 实现
// ============================================================================

QJsonObject CrashReport::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["appVersion"] = appVersion;
    obj["osName"] = osName;
    obj["osVersion"] = osVersion;
    obj["cpuArch"] = cpuArch;
    obj["exceptionType"] = exceptionType;
    obj["exceptionMessage"] = exceptionMessage;
    obj["stackTrace"] = stackTrace;
    obj["context"] = QJsonObject::fromVariantMap(context);
    obj["lastAction"] = lastAction;
    obj["openedFile"] = openedFile;
    obj["memoryUsage"] = memoryUsage;
    return obj;
}

CrashReport CrashReport::fromJson(const QJsonObject &json)
{
    CrashReport report;
    report.id = json["id"].toString();
    report.timestamp = QDateTime::fromString(json["timestamp"].toString(), Qt::ISODate);
    report.appVersion = json["appVersion"].toString();
    report.osName = json["osName"].toString();
    report.osVersion = json["osVersion"].toString();
    report.cpuArch = json["cpuArch"].toString();
    report.exceptionType = json["exceptionType"].toString();
    report.exceptionMessage = json["exceptionMessage"].toString();
    report.stackTrace = json["stackTrace"].toString();
    report.context = json["context"].toObject().toVariantMap();
    report.lastAction = json["lastAction"].toString();
    report.openedFile = json["openedFile"].toString();
    report.memoryUsage = json["memoryUsage"].toVariant().toLongLong();
    return report;
}

// ============================================================================
// CrashReporter 静态成员
// ============================================================================

CrashReporter* CrashReporter::s_instance = nullptr;

CrashReporter* CrashReporter::instance()
{
    if (!s_instance) {
        s_instance = new CrashReporter();
    }
    return s_instance;
}

void CrashReporter::initialize(const QString &appVersion)
{
    CrashReporter* reporter = instance();
    reporter->m_appVersion = appVersion;
    reporter->installHandlers();
    
    qDebug() << "[CrashReporter] Initialized for version:" << appVersion;
}

void CrashReporter::shutdown()
{
    if (s_instance) {
        s_instance->uninstallHandlers();
        delete s_instance;
        s_instance = nullptr;
    }
}

// ============================================================================
// CrashReporter 构造/析构
// ============================================================================

CrashReporter::CrashReporter(QObject *parent)
    : QObject(parent)
{
    // 确保报告目录存在
    QDir().mkpath(getReportsDirectory());
}

CrashReporter::~CrashReporter()
{
    uninstallHandlers();
}

// ============================================================================
// 属性设置
// ============================================================================

void CrashReporter::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        
        if (enabled) {
            installHandlers();
        } else {
            uninstallHandlers();
        }
        
        emit enabledChanged();
    }
}

void CrashReporter::setAutoUpload(bool autoUpload)
{
    if (m_autoUpload != autoUpload) {
        m_autoUpload = autoUpload;
        emit autoUploadChanged();
    }
}

void CrashReporter::setUploadUrl(const QString &url)
{
    if (m_uploadUrl != url) {
        m_uploadUrl = url;
        emit uploadUrlChanged();
    }
}

int CrashReporter::pendingReportsCount() const
{
    QDir reportsDir(getReportsDirectory());
    return reportsDir.entryList(QStringList() << "*.json", QDir::Files).size();
}

// ============================================================================
// 上下文记录
// ============================================================================

void CrashReporter::setContext(const QString &key, const QVariant &value)
{
    m_context[key] = value;
}

void CrashReporter::clearContext()
{
    m_context.clear();
}

void CrashReporter::setLastAction(const QString &action)
{
    m_lastAction = action;
}

void CrashReporter::setOpenedFile(const QString &filePath)
{
    m_openedFile = filePath;
}

// ============================================================================
// 手动报告
// ============================================================================

void CrashReporter::reportException(const QString &type, const QString &message,
                                     const QString &stackTrace)
{
    if (!m_enabled) return;
    
    CrashReport report;
    report.id = generateReportId();
    report.timestamp = QDateTime::currentDateTime();
    report.appVersion = m_appVersion;
    report.osName = QSysInfo::productType();
    report.osVersion = QSysInfo::productVersion();
    report.cpuArch = QSysInfo::currentCpuArchitecture();
    report.exceptionType = type;
    report.exceptionMessage = message;
    report.stackTrace = stackTrace.isEmpty() ? captureStackTrace() : stackTrace;
    report.context = m_context;
    report.lastAction = m_lastAction;
    report.openedFile = m_openedFile;
    report.memoryUsage = getCurrentMemoryUsage();
    
    saveCrashReport(report);
    
    emit crashDetected(report.id);
    emit pendingReportsCountChanged();
    
    if (m_autoUpload && !m_uploadUrl.isEmpty()) {
        uploadReport(report.id);
    }
}

void CrashReporter::reportError(const QString &message)
{
    reportException("Error", message);
}

// ============================================================================
// 报告管理
// ============================================================================

QVariantList CrashReporter::getPendingReports()
{
    QVariantList reports;
    QDir reportsDir(getReportsDirectory());
    
    for (const QString &fileName : reportsDir.entryList(QStringList() << "*.json", QDir::Files)) {
        QFile file(reportsDir.filePath(fileName));
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            CrashReport report = CrashReport::fromJson(doc.object());
            
            QVariantMap summary;
            summary["id"] = report.id;
            summary["timestamp"] = report.timestamp.toString("yyyy-MM-dd HH:mm:ss");
            summary["exceptionType"] = report.exceptionType;
            summary["exceptionMessage"] = report.exceptionMessage.left(100);
            reports.append(summary);
        }
    }
    
    return reports;
}

bool CrashReporter::uploadReport(const QString &reportId)
{
    if (m_uploadUrl.isEmpty()) {
        qWarning() << "[CrashReporter] Upload URL not configured";
        return false;
    }
    
    QString filePath = getReportsDirectory() + "/" + reportId + ".json";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[CrashReporter] Report not found:" << reportId;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QUrl uploadUrl(m_uploadUrl);
    QNetworkRequest request{uploadUrl};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply *reply = manager->post(request, data);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, reportId, manager]() {
        bool success = (reply->error() == QNetworkReply::NoError);
        QString message = success ? "Uploaded successfully" : reply->errorString();
        
        if (success) {
            // 上传成功后删除本地报告
            deleteReport(reportId);
        }
        
        emit reportUploaded(reportId, success, message);
        
        reply->deleteLater();
        manager->deleteLater();
    });
    
    return true;
}

void CrashReporter::uploadAllPending()
{
    QDir reportsDir(getReportsDirectory());
    
    for (const QString &fileName : reportsDir.entryList(QStringList() << "*.json", QDir::Files)) {
        QString reportId = QFileInfo(fileName).baseName();
        uploadReport(reportId);
    }
}

bool CrashReporter::deleteReport(const QString &reportId)
{
    QString filePath = getReportsDirectory() + "/" + reportId + ".json";
    bool success = QFile::remove(filePath);
    
    if (success) {
        emit pendingReportsCountChanged();
    }
    
    return success;
}

void CrashReporter::clearAllReports()
{
    QDir reportsDir(getReportsDirectory());
    
    for (const QString &fileName : reportsDir.entryList(QStringList() << "*.json", QDir::Files)) {
        QFile::remove(reportsDir.filePath(fileName));
    }
    
    emit pendingReportsCountChanged();
}

QString CrashReporter::exportReport(const QString &reportId, const QString &filePath)
{
    QString srcPath = getReportsDirectory() + "/" + reportId + ".json";
    
    if (QFile::copy(srcPath, filePath)) {
        return filePath;
    }
    
    return QString();
}

QString CrashReporter::getReportDetails(const QString &reportId)
{
    QString filePath = getReportsDirectory() + "/" + reportId + ".json";
    QFile file(filePath);
    
    if (file.open(QIODevice::ReadOnly)) {
        return QString::fromUtf8(file.readAll());
    }
    
    return QString();
}

void CrashReporter::submitFeedback(const QString &reportId, const QString &userEmail,
                                    const QString &userComment)
{
    QString filePath = getReportsDirectory() + "/" + reportId + ".json";
    QFile file(filePath);
    
    if (file.open(QIODevice::ReadWrite)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        
        obj["userEmail"] = userEmail;
        obj["userComment"] = userComment;
        obj["feedbackTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        file.seek(0);
        file.write(QJsonDocument(obj).toJson());
        file.close();
        
        // 如果配置了上传，则上传包含反馈的报告
        if (!m_uploadUrl.isEmpty()) {
            uploadReport(reportId);
        }
    }
}

// ============================================================================
// 异常处理器安装
// ============================================================================

#ifdef Q_OS_WIN

static LPTOP_LEVEL_EXCEPTION_FILTER s_previousHandler = nullptr;

long __stdcall CrashReporter::windowsExceptionHandler(_EXCEPTION_POINTERS *exceptionInfo)
{
    if (!s_instance || !s_instance->m_enabled) {
        if (s_previousHandler) {
            return s_previousHandler(exceptionInfo);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    
    QString exceptionType;
    switch (exceptionInfo->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            exceptionType = "Access Violation";
            break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            exceptionType = "Array Bounds Exceeded";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            exceptionType = "Stack Overflow";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            exceptionType = "Illegal Instruction";
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            exceptionType = "Divide By Zero";
            break;
        default:
            exceptionType = QString("Exception 0x%1").arg(
                exceptionInfo->ExceptionRecord->ExceptionCode, 8, 16, QChar('0'));
    }
    
    QString address = QString("0x%1").arg(
        reinterpret_cast<quintptr>(exceptionInfo->ExceptionRecord->ExceptionAddress), 
        16, 16, QChar('0'));
    
    s_instance->reportException(exceptionType, 
        QString("At address: %1").arg(address),
        s_instance->captureStackTrace());
    
    if (s_previousHandler) {
        return s_previousHandler(exceptionInfo);
    }
    
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif

#ifdef Q_OS_UNIX

static struct sigaction s_previousHandlers[32];

void CrashReporter::unixSignalHandler(int signal)
{
    if (!s_instance || !s_instance->m_enabled) {
        return;
    }
    
    QString signalName;
    switch (signal) {
        case SIGSEGV: signalName = "Segmentation Fault (SIGSEGV)"; break;
        case SIGBUS:  signalName = "Bus Error (SIGBUS)"; break;
        case SIGFPE:  signalName = "Floating Point Exception (SIGFPE)"; break;
        case SIGILL:  signalName = "Illegal Instruction (SIGILL)"; break;
        case SIGABRT: signalName = "Abort (SIGABRT)"; break;
        default:      signalName = QString("Signal %1").arg(signal); break;
    }
    
    s_instance->reportException(signalName, 
        QString("Process received signal %1").arg(signal),
        s_instance->captureStackTrace());
    
    // 恢复默认处理
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
    
    raise(signal);
}

#endif

void CrashReporter::installHandlers()
{
#ifdef Q_OS_WIN
    s_previousHandler = SetUnhandledExceptionFilter(windowsExceptionHandler);
#endif

#ifdef Q_OS_UNIX
    struct sigaction sa;
    sa.sa_handler = unixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    
    int signals[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
    for (int sig : signals) {
        sigaction(sig, &sa, &s_previousHandlers[sig]);
    }
#endif
    
    qDebug() << "[CrashReporter] Exception handlers installed";
}

void CrashReporter::uninstallHandlers()
{
#ifdef Q_OS_WIN
    if (s_previousHandler) {
        SetUnhandledExceptionFilter(s_previousHandler);
        s_previousHandler = nullptr;
    }
#endif

#ifdef Q_OS_UNIX
    int signals[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
    for (int sig : signals) {
        sigaction(sig, &s_previousHandlers[sig], nullptr);
    }
#endif
    
    qDebug() << "[CrashReporter] Exception handlers uninstalled";
}

// ============================================================================
// 辅助方法
// ============================================================================

QString CrashReporter::collectSystemInfo()
{
    QStringList info;
    info << QString("OS: %1 %2").arg(QSysInfo::productType(), QSysInfo::productVersion());
    info << QString("Kernel: %1").arg(QSysInfo::kernelVersion());
    info << QString("CPU: %1").arg(QSysInfo::currentCpuArchitecture());
    info << QString("App Version: %1").arg(m_appVersion);
    info << QString("Qt Version: %1").arg(qVersion());
    return info.join("\n");
}

QString CrashReporter::captureStackTrace()
{
    QStringList trace;
    
#ifdef Q_OS_WIN
    void* stack[64];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);
    
    WORD frames = CaptureStackBackTrace(0, 64, stack, nullptr);
    
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    
    for (WORD i = 0; i < frames; i++) {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        trace << QString("#%1 0x%2 %3")
            .arg(i)
            .arg((quintptr)stack[i], 16, 16, QChar('0'))
            .arg(QString::fromLatin1(symbol->Name));
    }
    
    free(symbol);
    SymCleanup(process);
#endif

#ifdef Q_OS_UNIX
    void* stack[64];
    int frames = backtrace(stack, 64);
    char** symbols = backtrace_symbols(stack, frames);
    
    if (symbols) {
        for (int i = 0; i < frames; i++) {
            trace << QString("#%1 %2").arg(i).arg(QString::fromLatin1(symbols[i]));
        }
        free(symbols);
    }
#endif

    return trace.join("\n");
}

qint64 CrashReporter::getCurrentMemoryUsage()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#endif

#ifdef Q_OS_UNIX
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024;
    }
#endif

    return 0;
}

void CrashReporter::saveCrashReport(const CrashReport &report)
{
    QString filePath = getReportsDirectory() + "/" + report.id + ".json";
    QFile file(filePath);
    
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(report.toJson());
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        
        qDebug() << "[CrashReporter] Report saved:" << filePath;
    } else {
        qWarning() << "[CrashReporter] Failed to save report:" << filePath;
    }
}

QString CrashReporter::getReportsDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/crash_reports";
}

QString CrashReporter::generateReportId() const
{
    return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_" +
           QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}
