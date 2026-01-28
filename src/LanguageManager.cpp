/**
 * @file LanguageManager.cpp
 * @brief 语言管理器实现
 */

#include "LanguageManager.h"

#include <QGuiApplication>
#include <QTranslator>
#include <QSettings>
#include <QLocale>
#include <QLibraryInfo>
#include <QDebug>
#include <QVariantList>
#include <QVariantMap>
#include <QDir>
#include <QQmlEngine>

LanguageManager& LanguageManager::instance() {
    static LanguageManager instance;
    return instance;
}

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
    , m_translator(nullptr)
    , m_qtTranslator(nullptr)
    , m_currentLanguage("en_US")
{
}

void LanguageManager::init() {
    // 从设置中读取保存的语言偏好
    QSettings settings;
    QString savedLanguage = settings.value("Language/current", "en_US").toString();
    
    // 如果没有保存的偏好，尝试使用系统语言
    if (savedLanguage.isEmpty() || savedLanguage == "en_US") {
        QLocale systemLocale = QLocale::system();
        QString systemLang = systemLocale.name(); // e.g., "zh_CN", "en_US"
        
        // 检查是否支持系统语言
        if (systemLang.startsWith("zh")) {
            savedLanguage = "zh_CN";
        } else {
            savedLanguage = "en_US";
        }
    }
    
    // 加载语言
    loadLanguage(savedLanguage);
}

bool LanguageManager::loadLanguage(const QString& languageCode) {
    qDebug() << "[LanguageManager] loadLanguage called with:" << languageCode 
             << "current:" << m_currentLanguage;
    
    // 如果是相同语言且已有翻译器，跳过
    if (languageCode == m_currentLanguage && m_translator) {
        qDebug() << "[LanguageManager] Same language, skipping";
        return true;
    }

    QGuiApplication* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!app) {
        qWarning() << "[LanguageManager] No QGuiApplication instance!";
        return false;
    }

    // 移除旧的翻译器
    if (m_translator) {
        app->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }
    
    if (m_qtTranslator) {
        app->removeTranslator(m_qtTranslator);
        delete m_qtTranslator;
        m_qtTranslator = nullptr;
    }

    QString oldLanguage = m_currentLanguage;

    // 如果是英语，不需要加载翻译文件（英语是源语言）
    if (languageCode == "en_US" || languageCode.isEmpty()) {
        m_currentLanguage = "en_US";
        saveLanguagePreference();
        qDebug() << "[LanguageManager] Switched to English (source language)";
        
        // 通知 QML 引擎重新翻译所有文本
        if (m_engine) {
            m_engine->retranslate();
            qDebug() << "[LanguageManager] QML engine retranslate() called";
        }
        
        emit languageChanged(m_currentLanguage);
        return true;
    }

    // 加载应用程序翻译文件
    m_translator = new QTranslator(this);
    
    // 获取应用程序目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString qmFileName = QString("BigFileViewer_%1.qm").arg(languageCode);
    
    // 构建多个可能的翻译文件搜索路径
    QStringList searchPaths;
    
#ifdef Q_OS_MACOS
    // macOS: 翻译文件可能在 .app/Contents/Resources 或与可执行文件同级
    searchPaths << appDir + "/../Resources/translations/" + qmFileName;
    searchPaths << appDir + "/../Resources/" + qmFileName;
    searchPaths << appDir + "/translations/" + qmFileName;
    searchPaths << appDir + "/" + qmFileName;
#else
    // Windows/Linux: 翻译文件在可执行文件旁边的 translations 目录
    searchPaths << appDir + "/translations/" + qmFileName;
    searchPaths << appDir + "/" + qmFileName;
#endif
    
    // 添加资源文件路径
    searchPaths << QString(":/i18n/BigFileViewer_%1.qm").arg(languageCode);
    searchPaths << QString(":/translations/BigFileViewer_%1.qm").arg(languageCode);
    
    bool loaded = false;
    QString loadedPath;
    
    for (const QString& path : searchPaths) {
        qDebug() << "[LanguageManager] Trying path:" << path;
        if (m_translator->load(path)) {
            loaded = true;
            loadedPath = path;
            break;
        }
    }
    
    if (loaded) {
        app->installTranslator(m_translator);
        qDebug() << "[LanguageManager] Loaded translation from:" << loadedPath;
    } else {
        qWarning() << "[LanguageManager] Translation file not found in any path!";
        qWarning() << "[LanguageManager] Searched paths:" << searchPaths;
        delete m_translator;
        m_translator = nullptr;
        // 注意：即使找不到翻译文件，也允许切换（可能用户只是想切换 locale）
    }

    // 加载 Qt 内置翻译（对话框按钮等）
    m_qtTranslator = new QTranslator(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    QString qtTranslationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    
    // Qt 翻译文件名格式：qt_zh_CN.qm
    QString qtLang = languageCode.left(2); // 只取语言部分，如 "zh"
    if (m_qtTranslator->load("qt_" + qtLang, qtTranslationsPath)) {
        app->installTranslator(m_qtTranslator);
        qDebug() << "[LanguageManager] Loaded Qt translation for" << qtLang;
    } else if (m_qtTranslator->load("qt_" + languageCode, qtTranslationsPath)) {
        app->installTranslator(m_qtTranslator);
        qDebug() << "[LanguageManager] Loaded Qt translation for" << languageCode;
    } else {
        qDebug() << "[LanguageManager] Qt translation not available for" << languageCode;
        delete m_qtTranslator;
        m_qtTranslator = nullptr;
    }

    m_currentLanguage = languageCode;
    saveLanguagePreference();
    
    qDebug() << "[LanguageManager] Language changed from" << oldLanguage << "to" << m_currentLanguage;
    
    // 通知 QML 引擎重新翻译所有文本（Qt 5.10+）
    // 这是解决 macOS/Linux 上语言切换不生效的关键！
    if (m_engine) {
        m_engine->retranslate();
        qDebug() << "[LanguageManager] QML engine retranslate() called";
    } else {
        qWarning() << "[LanguageManager] No QML engine set, UI may not update!";
    }
    
    // 发出语言变更信号，触发 UI 更新
    emit languageChanged(m_currentLanguage);
    
    return loaded || (languageCode == "en_US");
}

void LanguageManager::saveLanguagePreference() {
    QSettings settings;
    settings.setValue("Language/current", m_currentLanguage);
    settings.sync();
}

QVariantList LanguageManager::availableLanguages() const {
    QVariantList languages;
    
    // 英语（源语言，始终可用）
    QVariantMap en;
    en["code"] = "en_US";
    en["name"] = "English";
    languages.append(en);
    
    // 简体中文
    QVariantMap zh;
    zh["code"] = "zh_CN";
    zh["name"] = QString::fromUtf8("简体中文");
    languages.append(zh);
    
    // 繁体中文
    QVariantMap zhTW;
    zhTW["code"] = "zh_TW";
    zhTW["name"] = QString::fromUtf8("繁體中文");
    languages.append(zhTW);
    
    // 日语
    QVariantMap ja;
    ja["code"] = "ja_JP";
    ja["name"] = QString::fromUtf8("日本語");
    languages.append(ja);
    
    // 韩语
    QVariantMap ko;
    ko["code"] = "ko_KR";
    ko["name"] = QString::fromUtf8("한국어");
    languages.append(ko);
    
    // 俄语
    QVariantMap ru;
    ru["code"] = "ru_RU";
    ru["name"] = QString::fromUtf8("Русский");
    languages.append(ru);
    
    // 德语
    QVariantMap de;
    de["code"] = "de_DE";
    de["name"] = "Deutsch";
    languages.append(de);
    
    // 法语
    QVariantMap fr;
    fr["code"] = "fr_FR";
    fr["name"] = QString::fromUtf8("Français");
    languages.append(fr);
    
    // 西班牙语
    QVariantMap es;
    es["code"] = "es_ES";
    es["name"] = QString::fromUtf8("Español");
    languages.append(es);
    
    // 葡萄牙语
    QVariantMap pt;
    pt["code"] = "pt_BR";
    pt["name"] = QString::fromUtf8("Português");
    languages.append(pt);
    
    // 意大利语
    QVariantMap it;
    it["code"] = "it_IT";
    it["name"] = QString::fromUtf8("Italiano");
    languages.append(it);
    
    // 阿拉伯语
    QVariantMap ar;
    ar["code"] = "ar_SA";
    ar["name"] = QString::fromUtf8("العربية");
    languages.append(ar);
    
    // 印地语
    QVariantMap hi;
    hi["code"] = "hi_IN";
    hi["name"] = QString::fromUtf8("हिन्दी");
    languages.append(hi);
    
    return languages;
}
