/**
 * @file TabManager.cpp
 * @brief 多文件标签页管理器实现
 */

#include "TabManager.h"
#include "BigFileModel.h"
#include <QSettings>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

TabManager& TabManager::instance()
{
    static TabManager instance;
    return instance;
}

TabManager::TabManager(QObject *parent)
    : QObject(parent)
{
    // 从设置加载配置
    QSettings settings;
    m_maxLoadedTabs = settings.value("TabManager/maxLoadedTabs", 5).toInt();
    m_maxMemoryUsage = settings.value("TabManager/maxMemoryUsage", 2LL * 1024 * 1024 * 1024).toLongLong();
}

TabManager::~TabManager()
{
    // 保存会话
    saveSession();
}

int TabManager::currentTabId() const
{
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_tabs.size()) {
        return m_tabs[m_currentTabIndex].info.id;
    }
    return -1;
}

QVariantList TabManager::tabs() const
{
    QVariantList result;
    for (const auto &tab : m_tabs) {
        QVariantMap map;
        map["id"] = tab.info.id;
        map["filePath"] = tab.info.filePath;
        map["fileName"] = tab.info.fileName;
        map["isLoaded"] = tab.info.isLoaded;
        map["isModified"] = tab.info.isModified;
        map["fileSize"] = tab.info.fileSize;
        map["lineCount"] = tab.info.lineCount;
        result.append(map);
    }
    return result;
}

BigFileModel* TabManager::currentModel() const
{
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_tabs.size()) {
        return m_tabs[m_currentTabIndex].model.get();
    }
    return nullptr;
}

void TabManager::setCurrentTabIndex(int index)
{
    if (index < -1 || index >= m_tabs.size()) {
        return;
    }
    
    if (m_currentTabIndex != index) {
        // 保存旧标签页状态
        if (m_currentTabIndex >= 0 && m_currentTabIndex < m_tabs.size()) {
            saveTabState(m_currentTabIndex);
        }
        
        m_currentTabIndex = index;
        
        if (index >= 0) {
            // 更新访问时间
            updateAccessTime(index);
            
            // 确保内容已加载
            if (!m_tabs[index].info.isLoaded) {
                loadTabContent(index);
            }
            
            // 恢复状态
            restoreTabState(index);
        }
        
        emit currentTabChanged();
        emit currentModelChanged();
        emit tabActivated(index);
    }
}

void TabManager::setMaxLoadedTabs(int max)
{
    if (max < 1) max = 1;
    if (m_maxLoadedTabs != max) {
        m_maxLoadedTabs = max;
        QSettings settings;
        settings.setValue("TabManager/maxLoadedTabs", max);
        emit maxLoadedTabsChanged();
        
        // 可能需要执行卸载
        performLruEviction();
    }
}

void TabManager::setMaxMemoryUsage(qint64 bytes)
{
    if (bytes < 100 * 1024 * 1024) bytes = 100 * 1024 * 1024;  // 最小 100MB
    if (m_maxMemoryUsage != bytes) {
        m_maxMemoryUsage = bytes;
        QSettings settings;
        settings.setValue("TabManager/maxMemoryUsage", bytes);
        emit maxMemoryUsageChanged();
        
        // 可能需要执行卸载
        performLruEviction();
    }
}

int TabManager::openFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return -1;
    }
    
    // 检查文件是否已打开
    int existingIndex = findTabByPath(filePath);
    if (existingIndex >= 0) {
        setCurrentTabIndex(existingIndex);
        return existingIndex;
    }
    
    // 创建新标签页
    TabData newTab;
    newTab.info.id = generateTabId();
    newTab.info.filePath = filePath;
    newTab.info.fileName = extractFileName(filePath);
    newTab.info.lastAccessTime = QDateTime::currentDateTime();
    newTab.model = std::shared_ptr<BigFileModel>(createModel());
    
    // 加载文件
    if (newTab.model->loadFile(filePath)) {
        newTab.info.isLoaded = true;
        newTab.info.fileSize = newTab.model->fileSize();
        newTab.info.lineCount = newTab.model->lineCount();
        
        // 连接信号
        connect(newTab.model.get(), &BigFileModel::lineCountChanged, this, [this]() {
            if (m_currentTabIndex >= 0 && m_currentTabIndex < m_tabs.size()) {
                m_tabs[m_currentTabIndex].info.lineCount = m_tabs[m_currentTabIndex].model->lineCount();
                emit tabsChanged();
            }
        });
        connect(newTab.model.get(), &BigFileModel::bookmarksChanged, this, [this]() {
            if (m_currentTabIndex >= 0 && m_currentTabIndex < m_tabs.size()) {
                m_tabs[m_currentTabIndex].info.isModified = true;
                emit tabsChanged();
            }
        });
    } else {
        return -1;
    }
    
    // 添加到列表
    m_tabs.append(std::move(newTab));
    int newIndex = m_tabs.size() - 1;
    
    emit tabCountChanged();
    emit tabsChanged();
    emit tabOpened(newIndex, filePath);
    
    // 切换到新标签页
    setCurrentTabIndex(newIndex);
    
    // 检查是否需要 LRU 卸载
    performLruEviction();
    
    return newIndex;
}

bool TabManager::openFileInCurrentTab(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }
    
    if (m_currentTabIndex < 0 || m_currentTabIndex >= m_tabs.size()) {
        // 没有当前标签页，创建新的
        return openFile(filePath) >= 0;
    }
    
    auto &tab = m_tabs[m_currentTabIndex];
    
    // 检查是否有未保存的更改
    if (tab.info.isModified) {
        emit saveConfirmationRequested(m_currentTabIndex, tab.info.fileName);
        // 这里应该等待用户确认，但简化处理直接继续
    }
    
    // 加载新文件
    if (tab.model->loadFile(filePath)) {
        tab.info.filePath = filePath;
        tab.info.fileName = extractFileName(filePath);
        tab.info.isLoaded = true;
        tab.info.isModified = false;
        tab.info.fileSize = tab.model->fileSize();
        tab.info.lineCount = tab.model->lineCount();
        tab.info.scrollPosition = 0.0;
        tab.info.filterKeyword.clear();
        tab.info.bookmarks.clear();
        tab.info.lastAccessTime = QDateTime::currentDateTime();
        
        emit tabsChanged();
        emit currentModelChanged();
        return true;
    }
    
    return false;
}

bool TabManager::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) {
        return false;
    }
    
    auto &tab = m_tabs[index];
    
    // 检查是否有未保存的更改
    if (tab.info.isModified) {
        emit saveConfirmationRequested(index, tab.info.fileName);
        // 简化处理，实际应等待用户确认
    }
    
    // 保存书签
    if (tab.model && tab.info.isLoaded) {
        tab.model->autoSaveBookmarks();
    }
    
    // 移除标签页
    m_tabs.removeAt(index);
    
    emit tabClosed(index);
    emit tabCountChanged();
    emit tabsChanged();
    
    // 调整当前索引
    if (m_tabs.isEmpty()) {
        m_currentTabIndex = -1;
        emit currentTabChanged();
        emit currentModelChanged();
    } else if (index <= m_currentTabIndex) {
        int newIndex = qMax(0, m_currentTabIndex - 1);
        if (newIndex >= m_tabs.size()) {
            newIndex = m_tabs.size() - 1;
        }
        m_currentTabIndex = -1;  // 强制触发切换
        setCurrentTabIndex(newIndex);
    }
    
    return true;
}

void TabManager::closeOtherTabs(int exceptIndex)
{
    if (exceptIndex < 0 || exceptIndex >= m_tabs.size()) {
        return;
    }
    
    // 从后向前关闭，避免索引混乱
    for (int i = m_tabs.size() - 1; i >= 0; --i) {
        if (i != exceptIndex) {
            closeTab(i);
            // 调整 exceptIndex
            if (i < exceptIndex) {
                exceptIndex--;
            }
        }
    }
}

void TabManager::closeAllTabs()
{
    while (!m_tabs.isEmpty()) {
        closeTab(0);
    }
}

void TabManager::closeTabsToRight(int fromIndex)
{
    if (fromIndex < 0 || fromIndex >= m_tabs.size() - 1) {
        return;
    }
    
    // 从后向前关闭
    for (int i = m_tabs.size() - 1; i > fromIndex; --i) {
        closeTab(i);
    }
}

void TabManager::moveTab(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_tabs.size() ||
        toIndex < 0 || toIndex >= m_tabs.size() ||
        fromIndex == toIndex) {
        return;
    }
    
    // 移动标签
    m_tabs.move(fromIndex, toIndex);
    
    // 调整当前索引
    if (m_currentTabIndex == fromIndex) {
        m_currentTabIndex = toIndex;
    } else if (fromIndex < m_currentTabIndex && toIndex >= m_currentTabIndex) {
        m_currentTabIndex--;
    } else if (fromIndex > m_currentTabIndex && toIndex <= m_currentTabIndex) {
        m_currentTabIndex++;
    }
    
    emit tabsChanged();
}

int TabManager::duplicateTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) {
        return -1;
    }
    
    return openFile(m_tabs[index].info.filePath);
}

QVariantMap TabManager::getTabInfo(int index) const
{
    QVariantMap result;
    if (index >= 0 && index < m_tabs.size()) {
        const auto &info = m_tabs[index].info;
        result["id"] = info.id;
        result["filePath"] = info.filePath;
        result["fileName"] = info.fileName;
        result["isLoaded"] = info.isLoaded;
        result["isModified"] = info.isModified;
        result["fileSize"] = info.fileSize;
        result["lineCount"] = info.lineCount;
        result["scrollPosition"] = info.scrollPosition;
        result["filterKeyword"] = info.filterKeyword;
    }
    return result;
}

BigFileModel* TabManager::getModel(int index) const
{
    if (index >= 0 && index < m_tabs.size()) {
        return m_tabs[index].model.get();
    }
    return nullptr;
}

int TabManager::findTabByPath(const QString &filePath) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].info.filePath == filePath) {
            return i;
        }
    }
    return -1;
}

bool TabManager::hasUnsavedChanges() const
{
    for (const auto &tab : m_tabs) {
        if (tab.info.isModified) {
            return true;
        }
    }
    return false;
}

void TabManager::saveSession()
{
    QSettings settings;
    QJsonArray tabsArray;
    
    for (int i = 0; i < m_tabs.size(); ++i) {
        saveTabState(i);
        const auto &info = m_tabs[i].info;
        
        QJsonObject tabObj;
        tabObj["filePath"] = info.filePath;
        tabObj["scrollPosition"] = info.scrollPosition;
        tabObj["filterKeyword"] = info.filterKeyword;
        tabObj["caseSensitive"] = info.caseSensitive;
        tabObj["useRegex"] = info.useRegex;
        tabObj["logLevel"] = info.logLevel;
        
        // 书签单独存储
        QJsonArray bookmarksArray;
        for (const auto &bm : info.bookmarks) {
            bookmarksArray.append(QJsonValue::fromVariant(bm));
        }
        tabObj["bookmarks"] = bookmarksArray;
        
        tabsArray.append(tabObj);
    }
    
    QJsonDocument doc(tabsArray);
    settings.setValue("TabManager/session", doc.toJson(QJsonDocument::Compact));
    settings.setValue("TabManager/currentTabIndex", m_currentTabIndex);
}

int TabManager::restoreSession()
{
    QSettings settings;
    QByteArray sessionData = settings.value("TabManager/session").toByteArray();
    
    if (sessionData.isEmpty()) {
        return 0;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(sessionData);
    if (!doc.isArray()) {
        return 0;
    }
    
    QJsonArray tabsArray = doc.array();
    int restored = 0;
    
    for (const auto &tabValue : tabsArray) {
        QJsonObject tabObj = tabValue.toObject();
        QString filePath = tabObj["filePath"].toString();
        
        if (!filePath.isEmpty() && QFileInfo::exists(filePath)) {
            int index = openFile(filePath);
            if (index >= 0) {
                auto &info = m_tabs[index].info;
                info.scrollPosition = tabObj["scrollPosition"].toDouble();
                info.filterKeyword = tabObj["filterKeyword"].toString();
                info.caseSensitive = tabObj["caseSensitive"].toBool();
                info.useRegex = tabObj["useRegex"].toBool();
                info.logLevel = tabObj["logLevel"].toString();
                
                // 恢复书签
                QJsonArray bookmarksArray = tabObj["bookmarks"].toArray();
                info.bookmarks.clear();
                for (const auto &bm : bookmarksArray) {
                    info.bookmarks.append(bm.toVariant());
                }
                
                restoreTabState(index);
                restored++;
            }
        }
    }
    
    // 恢复当前标签页
    int savedIndex = settings.value("TabManager/currentTabIndex", 0).toInt();
    if (savedIndex >= 0 && savedIndex < m_tabs.size()) {
        setCurrentTabIndex(savedIndex);
    }
    
    return restored;
}

qint64 TabManager::estimatedMemoryUsage() const
{
    qint64 total = 0;
    for (const auto &tab : m_tabs) {
        if (tab.info.isLoaded) {
            total += tab.info.fileSize;
        }
    }
    return total;
}

void TabManager::runLruEviction()
{
    performLruEviction();
}

BigFileModel* TabManager::createModel()
{
    return new BigFileModel(this);
}

bool TabManager::loadTabContent(int index)
{
    if (index < 0 || index >= m_tabs.size()) {
        return false;
    }
    
    auto &tab = m_tabs[index];
    if (tab.info.isLoaded) {
        return true;
    }
    
    // 检查是否需要先卸载其他标签
    performLruEviction();
    
    // 加载文件
    if (!tab.model) {
        tab.model = std::shared_ptr<BigFileModel>(createModel());
    }
    
    if (tab.model->loadFile(tab.info.filePath)) {
        tab.info.isLoaded = true;
        tab.info.fileSize = tab.model->fileSize();
        tab.info.lineCount = tab.model->lineCount();
        tab.info.lastAccessTime = QDateTime::currentDateTime();
        
        // 恢复状态
        restoreTabState(index);
        
        emit tabsChanged();
        return true;
    }
    
    return false;
}

void TabManager::unloadTabContent(int index)
{
    if (index < 0 || index >= m_tabs.size()) {
        return;
    }
    
    auto &tab = m_tabs[index];
    if (!tab.info.isLoaded || index == m_currentTabIndex) {
        // 不卸载当前标签页
        return;
    }
    
    // 先保存状态
    saveTabState(index);
    
    // 关闭文件并释放内存
    if (tab.model) {
        tab.model->closeFile();
    }
    
    tab.info.isLoaded = false;
    emit tabsChanged();
}

void TabManager::saveTabState(int index)
{
    if (index < 0 || index >= m_tabs.size()) {
        return;
    }
    
    auto &tab = m_tabs[index];
    if (!tab.model || !tab.info.isLoaded) {
        return;
    }
    
    // 保存书签
    tab.info.bookmarks = tab.model->getAllBookmarks();
    
    // 保存过滤状态
    tab.info.filterKeyword = tab.model->filterKeyword();
    
    // 注意：scrollPosition 需要从 QML 端获取，这里暂时不处理
}

void TabManager::restoreTabState(int index)
{
    if (index < 0 || index >= m_tabs.size()) {
        return;
    }
    
    auto &tab = m_tabs[index];
    if (!tab.model || !tab.info.isLoaded) {
        return;
    }
    
    // 恢复过滤
    if (!tab.info.filterKeyword.isEmpty()) {
        tab.model->applyFilter(tab.info.filterKeyword, tab.info.useRegex);
    }
    
    // 恢复书签
    tab.model->autoLoadBookmarks();
    
    // 注意：滚动位置需要在 QML 端恢复
}

void TabManager::performLruEviction()
{
    // 计算当前加载的标签数和内存使用
    int loadedCount = 0;
    qint64 memoryUsage = estimatedMemoryUsage();
    
    for (const auto &tab : m_tabs) {
        if (tab.info.isLoaded) {
            loadedCount++;
        }
    }
    
    // 检查是否需要卸载
    bool needEviction = (loadedCount > m_maxLoadedTabs) || 
                        (memoryUsage > m_maxMemoryUsage);
    
    if (!needEviction) {
        return;
    }
    
    // 发出内存警告
    if (memoryUsage > m_maxMemoryUsage * 0.9) {
        emit memoryWarning(memoryUsage, m_maxMemoryUsage);
    }
    
    // 按最后访问时间排序，找出最久未访问的标签
    QList<QPair<int, QDateTime>> loadedTabs;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].info.isLoaded && i != m_currentTabIndex) {
            loadedTabs.append({i, m_tabs[i].info.lastAccessTime});
        }
    }
    
    // 按时间排序（最旧的在前）
    std::sort(loadedTabs.begin(), loadedTabs.end(),
              [](const auto &a, const auto &b) {
                  return a.second < b.second;
              });
    
    // 卸载直到满足条件
    for (const auto &[idx, time] : loadedTabs) {
        if (loadedCount <= m_maxLoadedTabs && memoryUsage <= m_maxMemoryUsage) {
            break;
        }
        
        qint64 tabSize = m_tabs[idx].info.fileSize;
        unloadTabContent(idx);
        loadedCount--;
        memoryUsage -= tabSize;
    }
}

void TabManager::updateAccessTime(int index)
{
    if (index >= 0 && index < m_tabs.size()) {
        m_tabs[index].info.lastAccessTime = QDateTime::currentDateTime();
    }
}

int TabManager::generateTabId()
{
    return m_nextTabId++;
}

QString TabManager::extractFileName(const QString &filePath) const
{
    return QFileInfo(filePath).fileName();
}
