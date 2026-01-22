/**
 * @file WorkspaceManager.h
 * @brief 工作区管理器 - 保存/恢复完整会话状态
 * 
 * Pro 功能：保存和恢复工作区，包括：
 * - 打开的文件列表
 * - 过滤条件
 * - 书签
 * - 视图设置（滚动位置、列宽等）
 */

#ifndef WORKSPACEMANAGER_H
#define WORKSPACEMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QDateTime>

/**
 * @brief 工作区数据结构
 */
struct Workspace {
  QString name;              // 工作区名称
  QString description;       // 描述
  QDateTime createdAt;       // 创建时间
  QDateTime lastUsed;        // 最后使用时间
  
  // 文件状态
  QString filePath;          // 打开的文件路径
  qint64 scrollPosition;     // 滚动位置（行号）
  
  // 过滤状态
  QString filterKeyword;     // 过滤关键词
  bool caseSensitive;        // 区分大小写
  bool useRegex;             // 使用正则
  QString logLevel;          // 日志级别过滤
  
  // 书签
  QVariantList bookmarks;    // 书签列表
  
  // 视图设置
  bool followMode;           // 跟踪模式
  double lineNumberWidth;    // 行号列宽
  double contentWidth;       // 内容列宽
  
  Workspace() : scrollPosition(0), caseSensitive(false), useRegex(false),
                followMode(false), lineNumberWidth(80), contentWidth(800) {}
};

/**
 * @brief 工作区管理器单例类
 * 
 * 提供工作区的保存、加载、管理功能
 */
class WorkspaceManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList workspaces READ workspaces NOTIFY workspacesChanged)
  Q_PROPERTY(int workspaceCount READ workspaceCount NOTIFY workspacesChanged)
  Q_PROPERTY(QString currentWorkspace READ currentWorkspace NOTIFY currentWorkspaceChanged)

public:
  static WorkspaceManager &instance() {
    static WorkspaceManager inst;
    return inst;
  }
  
  QVariantList workspaces() const;
  int workspaceCount() const { return m_workspaces.size(); }
  QString currentWorkspace() const { return m_currentWorkspace; }
  
  /**
   * @brief 保存当前状态为工作区
   */
  Q_INVOKABLE bool saveWorkspace(const QString &name, const QString &description = QString());
  
  /**
   * @brief 更新工作区状态
   */
  Q_INVOKABLE bool updateWorkspace(const QString &name, const QVariantMap &state);
  
  /**
   * @brief 加载工作区
   */
  Q_INVOKABLE QVariantMap loadWorkspace(const QString &name);
  
  /**
   * @brief 删除工作区
   */
  Q_INVOKABLE bool deleteWorkspace(const QString &name);
  
  /**
   * @brief 重命名工作区
   */
  Q_INVOKABLE bool renameWorkspace(const QString &oldName, const QString &newName);
  
  /**
   * @brief 检查工作区是否存在
   */
  Q_INVOKABLE bool hasWorkspace(const QString &name) const;
  
  /**
   * @brief 获取工作区详情
   */
  Q_INVOKABLE QVariantMap getWorkspace(const QString &name) const;
  
  /**
   * @brief 导出工作区到文件
   */
  Q_INVOKABLE bool exportWorkspace(const QString &name, const QString &path);
  
  /**
   * @brief 从文件导入工作区
   */
  Q_INVOKABLE bool importWorkspace(const QString &path, bool overwrite = false);
  
  /**
   * @brief 设置当前工作区状态（用于更新）
   */
  Q_INVOKABLE void setCurrentState(const QVariantMap &state);
  
  /**
   * @brief 获取当前状态
   */
  Q_INVOKABLE QVariantMap getCurrentState() const { return m_currentState; }
  
  /**
   * @brief 快速保存当前工作区
   */
  Q_INVOKABLE bool quickSave();
  
  /**
   * @brief 获取最近使用的工作区
   */
  Q_INVOKABLE QVariantList recentWorkspaces(int count = 5) const;
  
signals:
  void workspacesChanged();
  void currentWorkspaceChanged();
  void workspaceLoaded(const QString &name);
  void workspaceSaved(const QString &name);
  
private:
  explicit WorkspaceManager(QObject *parent = nullptr);
  ~WorkspaceManager() override;
  
  void loadAllWorkspaces();
  void saveAllWorkspaces();
  QVariantMap workspaceToVariant(const Workspace &ws) const;
  Workspace variantToWorkspace(const QVariantMap &map) const;
  
  QSettings m_settings;
  QList<Workspace> m_workspaces;
  QString m_currentWorkspace;
  QVariantMap m_currentState;
  
  static constexpr int MAX_WORKSPACES = 50;
};

#endif // WORKSPACEMANAGER_H
