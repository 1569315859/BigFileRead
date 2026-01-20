#ifndef KEYWORDCONFIGMANAGER_H
#define KEYWORDCONFIGMANAGER_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QSettings>
#include <QColor>

/**
 * @brief 关键字和颜色配置管理器
 * 
 * 管理日志关键字及其对应的显示颜色，支持持久化存储
 */
class KeywordConfigManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap keywords READ keywords NOTIFY keywordsChanged)
    Q_PROPERTY(QVariantList keywordList READ keywordList NOTIFY keywordsChanged)

public:
    explicit KeywordConfigManager(QObject *parent = nullptr);

    // 获取所有关键字配置 {keyword: color}
    QVariantMap keywords() const;
    
    // 获取关键字列表（用于 QML ListView）
    QVariantList keywordList() const;

    // 添加或更新关键字
    Q_INVOKABLE void setKeyword(const QString &keyword, const QString &color);
    
    // 删除关键字
    Q_INVOKABLE void removeKeyword(const QString &keyword);
    
    // 获取关键字颜色
    Q_INVOKABLE QString getColor(const QString &keyword) const;
    
    // 检查关键字是否存在
    Q_INVOKABLE bool hasKeyword(const QString &keyword) const;
    
    // 重置为默认配置
    Q_INVOKABLE void resetToDefaults();
    
    // 导出配置为 JSON 字符串
    Q_INVOKABLE QString exportConfig() const;
    
    // 从 JSON 字符串导入配置
    Q_INVOKABLE bool importConfig(const QString &json);

signals:
    void keywordsChanged();

private:
    void loadFromSettings();
    void saveToSettings();
    void initDefaults();

    QSettings m_settings;
    QVariantMap m_keywords;  // {keyword: colorString}
};

#endif // KEYWORDCONFIGMANAGER_H
