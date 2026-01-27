/**
 * @file TabManager.h
 * @brief 多文件标签页管理器 - 支持同时打开多个日志文件
 * 
 * 功能特性：
 * - 管理多个 BigFileModel 实例
 * - LRU (Least Recently Used) 内存卸载机制
 * - 标签页状态持久化
 * - 拖拽排序支持
 */

#ifndef TABMANAGER_H
#define TABMANAGER_H

#include <QObject>
#include <QList>
#include <QVariantList>
#include <QVariantMap>
#include <QDateTime>
#include <memory>

class BigFileModel;

/**
 * @brief 标签页信息结构
 */
struct TabInfo {
    Q_GADGET
    Q_PROPERTY(int id MEMBER id)
    Q_PROPERTY(QString filePath MEMBER filePath)
    Q_PROPERTY(QString fileName MEMBER fileName)
    Q_PROPERTY(bool isLoaded MEMBER isLoaded)
    Q_PROPERTY(bool isModified MEMBER isModified)
    Q_PROPERTY(qint64 fileSize MEMBER fileSize)
    Q_PROPERTY(int lineCount MEMBER lineCount)
    Q_PROPERTY(double scrollPosition MEMBER scrollPosition)
    Q_PROPERTY(QString filterKeyword MEMBER filterKeyword)
public:
    int id = -1;                      ///< 唯一标识符
    QString filePath;                 ///< 完整文件路径
    QString fileName;                 ///< 文件名（显示用）
    bool isLoaded = false;            ///< 是否已加载到内存
    bool isModified = false;          ///< 是否有未保存的更改（书签等）
    qint64 fileSize = 0;              ///< 文件大小
    int lineCount = 0;                ///< 行数
    double scrollPosition = 0.0;      ///< 滚动位置（恢复用）
    QString filterKeyword;            ///< 当前过滤关键词
    QDateTime lastAccessTime;         ///< 最后访问时间（LRU用）
    
    // 存储的状态（用于卸载后恢复）
    QVariantList bookmarks;           ///< 书签列表
    bool caseSensitive = false;       ///< 过滤区分大小写
    bool useRegex = false;            ///< 使用正则表达式
    QString logLevel;                 ///< 日志级别过滤
};

/**
 * @class TabManager
 * @brief 多文件标签页管理器
 * 
 * 管理多个日志文件的打开、切换、关闭，实现 LRU 内存管理策略。
 */
class TabManager : public QObject
{
    Q_OBJECT
    
    // QML 属性
    Q_PROPERTY(int tabCount READ tabCount NOTIFY tabCountChanged)
    Q_PROPERTY(int currentTabIndex READ currentTabIndex WRITE setCurrentTabIndex NOTIFY currentTabChanged)
    Q_PROPERTY(int currentTabId READ currentTabId NOTIFY currentTabChanged)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(int maxLoadedTabs READ maxLoadedTabs WRITE setMaxLoadedTabs NOTIFY maxLoadedTabsChanged)
    Q_PROPERTY(qint64 maxMemoryUsage READ maxMemoryUsage WRITE setMaxMemoryUsage NOTIFY maxMemoryUsageChanged)
    Q_PROPERTY(BigFileModel* currentModel READ currentModel NOTIFY currentModelChanged)

public:
    /**
     * @brief 单例访问
     */
    static TabManager& instance();
    
    // ===== 属性访问器 =====
    
    int tabCount() const { return m_tabs.size(); }
    int currentTabIndex() const { return m_currentTabIndex; }
    int currentTabId() const;
    QVariantList tabs() const;
    int maxLoadedTabs() const { return m_maxLoadedTabs; }
    qint64 maxMemoryUsage() const { return m_maxMemoryUsage; }
    BigFileModel* currentModel() const;
    
    Q_INVOKABLE void setCurrentTabIndex(int index);
    void setMaxLoadedTabs(int max);
    void setMaxMemoryUsage(qint64 bytes);
    
    // ===== Q_INVOKABLE 方法 =====
    
    /**
     * @brief 打开新文件（创建新标签页）- 别名
     * @param filePath 文件路径
     * @return 新标签页的索引，失败返回 -1
     */
    Q_INVOKABLE int openTab(const QString &filePath) { return openFile(filePath); }
    
    /**
     * @brief 打开新文件（创建新标签页）
     * @param filePath 文件路径
     * @return 新标签页的索引，失败返回 -1
     */
    Q_INVOKABLE int openFile(const QString &filePath);
    
    /**
     * @brief 在当前标签页打开文件（替换）
     * @param filePath 文件路径
     * @return 成功返回 true
     */
    Q_INVOKABLE bool openFileInCurrentTab(const QString &filePath);
    
    /**
     * @brief 关闭指定标签页
     * @param index 标签页索引
     * @return 成功返回 true
     */
    Q_INVOKABLE bool closeTab(int index);
    
    /**
     * @brief 关闭其他所有标签页
     * @param exceptIndex 保留的标签页索引
     */
    Q_INVOKABLE void closeOtherTabs(int exceptIndex);
    
    /**
     * @brief 关闭所有标签页
     */
    Q_INVOKABLE void closeAllTabs();
    
    /**
     * @brief 关闭右侧所有标签页
     * @param fromIndex 起始索引
     */
    Q_INVOKABLE void closeTabsToRight(int fromIndex);
    
    /**
     * @brief 移动标签页（拖拽排序）
     * @param fromIndex 源索引
     * @param toIndex 目标索引
     */
    Q_INVOKABLE void moveTab(int fromIndex, int toIndex);
    
    /**
     * @brief 复制标签页（打开同一文件的新标签）
     * @param index 源标签页索引
     * @return 新标签页索引
     */
    Q_INVOKABLE int duplicateTab(int index);
    
    /**
     * @brief 获取标签页信息
     * @param index 标签页索引
     * @return 标签页信息 QVariantMap
     */
    Q_INVOKABLE QVariantMap getTabInfo(int index) const;
    
    /**
     * @brief 获取指定标签的模型
     * @param index 标签页索引
     * @return BigFileModel 指针，无效返回 nullptr
     */
    Q_INVOKABLE BigFileModel* getModel(int index) const;
    
    /**
     * @brief 通过文件路径查找标签页索引
     * @param filePath 文件路径
     * @return 标签页索引，未找到返回 -1
     */
    Q_INVOKABLE int findTabByPath(const QString &filePath) const;
    
    /**
     * @brief 检查是否有未保存的更改
     * @return 如果任何标签页有未保存更改返回 true
     */
    Q_INVOKABLE bool hasUnsavedChanges() const;
    
    /**
     * @brief 保存所有标签页的会话状态
     */
    Q_INVOKABLE void saveSession();
    
    /**
     * @brief 恢复上次的会话状态
     * @return 恢复的标签页数量
     */
    Q_INVOKABLE int restoreSession();
    
    /**
     * @brief 获取当前内存使用估算
     * @return 已加载文件的总大小（字节）
     */
    Q_INVOKABLE qint64 estimatedMemoryUsage() const;
    
    /**
     * @brief 强制执行 LRU 卸载
     */
    Q_INVOKABLE void runLruEviction();

signals:
    void tabCountChanged();
    void currentTabChanged();
    void tabsChanged();
    void maxLoadedTabsChanged();
    void maxMemoryUsageChanged();
    void currentModelChanged();
    
    /**
     * @brief 标签页打开信号
     * @param index 新标签页索引
     * @param filePath 文件路径
     */
    void tabOpened(int index, const QString &filePath);
    
    /**
     * @brief 标签页关闭信号
     * @param index 关闭的标签页索引
     */
    void tabClosed(int index);
    
    /**
     * @brief 标签页激活信号
     * @param index 激活的标签页索引
     */
    void tabActivated(int index);
    
    /**
     * @brief 请求保存更改确认
     * @param index 标签页索引
     * @param fileName 文件名
     */
    void saveConfirmationRequested(int index, const QString &fileName);
    
    /**
     * @brief 内存不足警告
     * @param currentUsage 当前使用量
     * @param maxUsage 最大限制
     */
    void memoryWarning(qint64 currentUsage, qint64 maxUsage);

private:
    explicit TabManager(QObject *parent = nullptr);
    ~TabManager();
    
    // 禁用拷贝
    TabManager(const TabManager&) = delete;
    TabManager& operator=(const TabManager&) = delete;
    
    /**
     * @brief 创建新的 BigFileModel 实例
     */
    BigFileModel* createModel();
    
    /**
     * @brief 加载标签页内容到内存
     * @param index 标签页索引
     * @return 成功返回 true
     */
    bool loadTabContent(int index);
    
    /**
     * @brief 卸载标签页内容（保留元数据）
     * @param index 标签页索引
     */
    void unloadTabContent(int index);
    
    /**
     * @brief 保存标签页状态（用于卸载前）
     * @param index 标签页索引
     */
    void saveTabState(int index);
    
    /**
     * @brief 恢复标签页状态（用于加载后）
     * @param index 标签页索引
     */
    void restoreTabState(int index);
    
    /**
     * @brief 执行 LRU 卸载策略
     */
    void performLruEviction();
    
    /**
     * @brief 更新标签页访问时间
     * @param index 标签页索引
     */
    void updateAccessTime(int index);
    
    /**
     * @brief 生成唯一标签 ID
     */
    int generateTabId();
    
    /**
     * @brief 从文件路径提取文件名
     */
    QString extractFileName(const QString &filePath) const;

private:
    struct TabData {
        TabInfo info;
        std::shared_ptr<BigFileModel> model;
    };
    
    QList<TabData> m_tabs;              ///< 所有标签页数据
    int m_currentTabIndex = -1;         ///< 当前活动标签页索引
    int m_nextTabId = 1;                ///< 下一个标签 ID
    
    // LRU 配置
    int m_maxLoadedTabs = 5;            ///< 最大同时加载的标签数
    qint64 m_maxMemoryUsage = 2LL * 1024 * 1024 * 1024;  ///< 最大内存使用（默认 2GB）
};

#endif // TABMANAGER_H
