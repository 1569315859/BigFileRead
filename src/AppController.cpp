#include "AppController.h"
#include "LicenseManager.h"
#include "TrialManager.h"
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QGuiApplication>
#include <QFileInfo>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings("BigFileViewer", "BigFileViewer")
{
    // 从设置中加载最近文件列表
    m_recentFiles = m_settings.value("recentFiles").toStringList();
    // 清理不存在的文件
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [](const QString &path) { return !QFileInfo::exists(path); }),
        m_recentFiles.end());
}

bool AppController::isRegistered() const {
    return LicenseManager::instance().isRegistered();
}

int AppController::trialDaysRemaining() const {
    return TrialManager::instance().daysRemaining();
}

bool AppController::isTrialExpired() const {
    return TrialManager::instance().isTrialExpired();
}

QString AppController::machineId() const {
    return LicenseManager::instance().getMachineId();
}

bool AppController::activateLicense(const QString &key) {
    bool success = LicenseManager::instance().verifyLicense(key);
    if (success) {
        emit licenseChanged();
    }
    return success;
}

void AppController::copyToClipboard(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
}

void AppController::openUrl(const QString &url) {
    QDesktopServices::openUrl(QUrl(url));
}

QStringList AppController::recentFiles() const {
    return m_recentFiles;
}

void AppController::addRecentFile(const QString &filePath) {
    // 移除已存在的相同路径（避免重复）
    m_recentFiles.removeAll(filePath);
    // 添加到列表开头
    m_recentFiles.prepend(filePath);
    // 限制最大数量
    while (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles.removeLast();
    }
    // 保存到设置
    m_settings.setValue("recentFiles", m_recentFiles);
    emit recentFilesChanged();
}

void AppController::clearRecentFiles() {
    m_recentFiles.clear();
    m_settings.setValue("recentFiles", m_recentFiles);
    emit recentFilesChanged();
}

QString AppController::getFileName(const QString &filePath) const {
    return QFileInfo(filePath).fileName();
}

QString AppController::urlToLocalPath(const QUrl &url) const {
    // 将 QUrl 转换为本地文件路径
    // 这个方法处理 macOS 上的 file:// URL 格式问题
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    // 如果不是本地文件 URL，尝试直接转换
    QString path = url.toString();
    // 移除 file:// 前缀
    if (path.startsWith("file://")) {
        path = path.mid(7);
        // macOS 可能会有 localhost
        if (path.startsWith("localhost")) {
            path = path.mid(9);
        }
    }
    return path;
}

void AppController::setCurrentViewMode(int mode) {
    if (m_currentViewMode != mode) {
        m_currentViewMode = mode;
        emit viewModeChanged();
    }
}

int AppController::detectFileType(const QString &filePath) const {
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    // 根据文件扩展名初步判断
    if (suffix == "csv" || suffix == "tsv") {
        return CSVFile;
    }
    if (suffix == "json") {
        return JSONFile;
    }
    if (suffix == "jsonl" || suffix == "ndjson") {
        return JSONLFile;
    }
    if (suffix == "log" || suffix == "txt") {
        // 需要进一步检测内容
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray sample = file.read(4096);
            file.close();
            
            QString content = QString::fromUtf8(sample);
            
            // 将内容按行分割
            QStringList lines = content.split('\n', Qt::SkipEmptyParts);
            
            // 检测是否为JSON
            QString trimmed = content.trimmed();
            if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
                // 可能是JSON，检查是否为JSONL（每行一个JSON对象）
                int jsonObjectLines = 0;
                for (int i = 0; i < qMin(5, lines.size()); ++i) {
                    QString line = lines[i].trimmed();
                    if (line.startsWith('{') && line.endsWith('}')) {
                        jsonObjectLines++;
                    }
                }
                if (jsonObjectLines >= 2) {
                    return JSONLFile;
                }
                return JSONFile;
            }
            
            // 检测是否为CSV（含有逗号、制表符等分隔符）
            int commaCount = content.count(',');
            int tabCount = content.count('\t');
            int semicolonCount = content.count(';');
            int pipeCount = content.count('|');
            
            int maxDelimiter = qMax(qMax(commaCount, tabCount), qMax(semicolonCount, pipeCount));
            int lineCount = lines.size();
            
            // 如果平均每行有多个分隔符，可能是结构化数据
            if (lineCount > 0 && maxDelimiter / lineCount >= 2) {
                return CSVFile;
            }
        }
        
        return LogFile;
    }
    
    // 默认作为日志文件处理
    return LogFile;
}

int AppController::suggestViewMode(int fileType) const {
    switch (static_cast<FileType>(fileType)) {
        case CSVFile:
            return CSVTableView;
        case JSONFile:
            return JSONTreeView;
        case JSONLFile:
            return JSONLTableView;
        case LogFile:
        default:
            return LogView;
    }
}

int AppController::openFileWithAutoMode(const QString &filePath) {
    int fileType = detectFileType(filePath);
    int suggestedMode = suggestViewMode(fileType);
    
    // 添加到最近文件列表
    addRecentFile(filePath);
    
    // 发射信号通知QML
    emit fileTypeDetected(fileType, suggestedMode);
    
    // 自动切换视图模式
    setCurrentViewMode(suggestedMode);
    
    return suggestedMode;
}

QString AppController::getFileTypeDescription(int fileType) const {
    switch (static_cast<FileType>(fileType)) {
        case CSVFile:
            return tr("CSV/TSV Structured Data");
        case JSONFile:
            return tr("JSON Document");
        case JSONLFile:
            return tr("JSON Lines (JSONL/NDJSON)");
        case LogFile:
            return tr("Log File");
        default:
            return tr("Unknown File Type");
    }
}

QString AppController::getViewModeDescription(int viewMode) const {
    switch (static_cast<ViewMode>(viewMode)) {
        case CSVTableView:
            return tr("Table View");
        case JSONTreeView:
            return tr("Tree View");
        case JSONLTableView:
            return tr("JSONL Table View");
        case LogView:
        default:
            return tr("Log View");
    }
}

QString AppController::readFileContent(const QUrl &fileUrl, int maxLines) const {
    QString filePath = fileUrl.toLocalFile();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream stream(&file);
    QStringList lines;
    int count = 0;
    while (!stream.atEnd() && count < maxLines) {
        lines.append(stream.readLine());
        count++;
    }
    file.close();
    return lines.join("\n");
}
