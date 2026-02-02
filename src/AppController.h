#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QSettings>
#include <QUrl>

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRegistered READ isRegistered NOTIFY licenseChanged)
    Q_PROPERTY(int trialDaysRemaining READ trialDaysRemaining CONSTANT)
    Q_PROPERTY(bool isTrialExpired READ isTrialExpired CONSTANT)
    Q_PROPERTY(QString machineId READ machineId CONSTANT)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(int currentViewMode READ currentViewMode WRITE setCurrentViewMode NOTIFY viewModeChanged)

public:
    /**
     * @brief 视图模式枚举
     */
    enum ViewMode {
        LogView = 0,      ///< 日志查看模式（默认）
        CSVTableView = 1, ///< CSV 表格视图模式
        JSONTreeView = 2, ///< JSON 树形视图模式
        JSONLTableView = 3 ///< JSONL 表格视图模式
    };
    Q_ENUM(ViewMode)
    
    /**
     * @brief 文件类型枚举
     */
    enum FileType {
        UnknownFile = 0,
        LogFile = 1,      ///< 日志文件
        CSVFile = 2,      ///< CSV/TSV 文件
        JSONFile = 3,     ///< JSON 文件
        JSONLFile = 4     ///< JSONL 文件（每行一个JSON）
    };
    Q_ENUM(FileType)
    
    explicit AppController(QObject *parent = nullptr);

    bool isRegistered() const;
    int trialDaysRemaining() const;
    bool isTrialExpired() const;
    QString machineId() const;

    Q_INVOKABLE bool activateLicense(const QString &key);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void openUrl(const QString &url);
    
    // File dialog helper for macOS compatibility
    Q_INVOKABLE QString urlToLocalPath(const QUrl &url) const;
    
    // Recent files management
    QStringList recentFiles() const;
    Q_INVOKABLE void addRecentFile(const QString &filePath);
    Q_INVOKABLE void clearRecentFiles();
    Q_INVOKABLE QString getFileName(const QString &filePath) const;
    
    // View mode management
    int currentViewMode() const { return m_currentViewMode; }
    void setCurrentViewMode(int mode);
    
    /**
     * @brief 检测文件类型
     * @param filePath 文件路径
     * @return 文件类型枚举值
     */
    Q_INVOKABLE int detectFileType(const QString &filePath) const;
    
    /**
     * @brief 根据文件类型获取建议的视图模式
     * @param fileType 文件类型
     * @return 视图模式枚举值
     */
    Q_INVOKABLE int suggestViewMode(int fileType) const;
    
    /**
     * @brief 打开文件并自动选择视图模式
     * @param filePath 文件路径
     * @return 建议的视图模式
     */
    Q_INVOKABLE int openFileWithAutoMode(const QString &filePath);
    
    /**
     * @brief 获取文件类型描述
     */
    Q_INVOKABLE QString getFileTypeDescription(int fileType) const;
    
    /**
     * @brief 获取视图模式描述
     */
    Q_INVOKABLE QString getViewModeDescription(int viewMode) const;

signals:
    void licenseChanged();
    void recentFilesChanged();
    void viewModeChanged();
    void fileTypeDetected(int fileType, int suggestedViewMode);
    
private:
    static constexpr int MAX_RECENT_FILES = 10;
    QSettings m_settings;
    QStringList m_recentFiles;
    int m_currentViewMode = LogView;
};

#endif // APPCONTROLLER_H
