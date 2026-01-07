/**
 * @file LanguageManager.h
 * @brief 语言管理器 - 处理应用程序国际化 (i18n)
 * @description 单例类，管理翻译文件的加载和语言切换
 */

#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QObject>
#include <QString>

class QTranslator;
class QApplication;

/**
 * @class LanguageManager
 * @brief 应用程序语言管理器（单例模式）
 * 
 * 功能：
 * - 加载和卸载翻译文件 (.qm)
 * - 保存和恢复用户语言偏好
 * - 支持运行时语言切换
 */
class LanguageManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return LanguageManager 单例引用
     */
    static LanguageManager& instance();

    /**
     * @brief 初始化语言管理器
     * @details 从 QSettings 加载保存的语言偏好并应用
     */
    void init();

    /**
     * @brief 加载指定语言
     * @param languageCode 语言代码，如 "zh_CN", "en_US"
     * @return 是否成功加载
     */
    bool loadLanguage(const QString& languageCode);

    /**
     * @brief 获取当前语言代码
     * @return 当前语言代码
     */
    QString currentLanguage() const { return m_currentLanguage; }

    /**
     * @brief 获取可用语言列表
     * @return 语言代码和显示名称的键值对
     */
    QList<QPair<QString, QString>> availableLanguages() const;

signals:
    /**
     * @brief 语言变更信号
     * @param languageCode 新的语言代码
     */
    void languageChanged(const QString& languageCode);

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    explicit LanguageManager(QObject* parent = nullptr);
    
    /**
     * @brief 禁用拷贝构造
     */
    LanguageManager(const LanguageManager&) = delete;
    
    /**
     * @brief 禁用赋值操作
     */
    LanguageManager& operator=(const LanguageManager&) = delete;

    /**
     * @brief 保存语言偏好到设置
     */
    void saveLanguagePreference();

    QTranslator* m_translator = nullptr;     ///< 当前翻译器
    QTranslator* m_qtTranslator = nullptr;   ///< Qt 内置翻译器（按钮、对话框等）
    QString m_currentLanguage = "en_US";      ///< 当前语言代码
};

#endif // LANGUAGEMANAGER_H
