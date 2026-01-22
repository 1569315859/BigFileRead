/**
 * @file FilterTemplateManager.h
 * @brief 过滤模板管理器 - 保存和加载过滤条件模板
 */

#ifndef FILTERTEMPLATEMANAGER_H
#define FILTERTEMPLATEMANAGER_H

#include <QDateTime>
#include <QObject>
#include <QSettings>
#include <QVariantList>

/**
 * @brief 过滤模板结构体
 */
struct FilterTemplate {
  QString name;           ///< 模板名称
  QString keyword;        ///< 过滤关键词
  bool caseSensitive;     ///< 是否区分大小写
  bool useRegex;          ///< 是否使用正则表达式
  bool includeContext;    ///< 是否包含上下文行
  int contextLines;       ///< 上下文行数
  QString logLevel;       ///< 日志级别过滤（空表示全部）
  QDateTime createdAt;    ///< 创建时间
  QDateTime lastUsed;     ///< 最后使用时间
  int useCount;           ///< 使用次数
  
  FilterTemplate()
      : caseSensitive(false), useRegex(false), includeContext(false),
        contextLines(0), useCount(0) {}
};

/**
 * @brief 过滤模板管理器
 * 
 * 功能：
 * - 保存/加载过滤模板
 * - 管理模板列表（增删改查）
 * - 模板导入/导出
 * - 最近使用的模板快速访问
 */
class FilterTemplateManager : public QObject {
  Q_OBJECT
  
  Q_PROPERTY(QVariantList templates READ templates NOTIFY templatesChanged)
  Q_PROPERTY(QVariantList recentTemplates READ recentTemplates NOTIFY templatesChanged)
  Q_PROPERTY(int templateCount READ templateCount NOTIFY templatesChanged)

public:
  static FilterTemplateManager &instance() {
    static FilterTemplateManager inst;
    return inst;
  }
  
  explicit FilterTemplateManager(QObject *parent = nullptr);
  ~FilterTemplateManager() override;

  // Properties
  QVariantList templates() const;
  QVariantList recentTemplates() const;
  int templateCount() const { return m_templates.size(); }

public slots:
  /**
   * @brief 保存新模板
   * @param name 模板名称
   * @param keyword 过滤关键词
   * @param caseSensitive 是否区分大小写
   * @param useRegex 是否使用正则
   * @param includeContext 是否包含上下文
   * @param contextLines 上下文行数
   * @param logLevel 日志级别
   * @return 成功返回 true
   */
  Q_INVOKABLE bool saveTemplate(const QString &name, const QString &keyword,
                                 bool caseSensitive = false, bool useRegex = false,
                                 bool includeContext = false, int contextLines = 0,
                                 const QString &logLevel = QString());
  
  /**
   * @brief 更新现有模板
   */
  Q_INVOKABLE bool updateTemplate(const QString &name, const QString &keyword,
                                   bool caseSensitive, bool useRegex,
                                   bool includeContext, int contextLines,
                                   const QString &logLevel);
  
  /**
   * @brief 删除模板
   */
  Q_INVOKABLE bool deleteTemplate(const QString &name);
  
  /**
   * @brief 获取模板详情
   */
  Q_INVOKABLE QVariantMap getTemplate(const QString &name) const;
  
  /**
   * @brief 检查模板是否存在
   */
  Q_INVOKABLE bool hasTemplate(const QString &name) const;
  
  /**
   * @brief 应用模板（更新使用统计）
   */
  Q_INVOKABLE QVariantMap applyTemplate(const QString &name);
  
  /**
   * @brief 重命名模板
   */
  Q_INVOKABLE bool renameTemplate(const QString &oldName, const QString &newName);
  
  /**
   * @brief 导出模板到文件
   */
  Q_INVOKABLE bool exportTemplates(const QString &path);
  
  /**
   * @brief 从文件导入模板
   */
  Q_INVOKABLE bool importTemplates(const QString &path, bool overwrite = false);
  
  /**
   * @brief 清除所有模板
   */
  Q_INVOKABLE void clearAllTemplates();

signals:
  void templatesChanged();
  void templateApplied(const QString &name, const QString &keyword);

private:
  void loadTemplates();
  void saveTemplates();
  QVariantMap templateToVariant(const FilterTemplate &tmpl) const;
  FilterTemplate variantToTemplate(const QVariantMap &map) const;
  
  QList<FilterTemplate> m_templates;
  QSettings m_settings;
  static constexpr int MAX_RECENT = 5;
};

#endif // FILTERTEMPLATEMANAGER_H
