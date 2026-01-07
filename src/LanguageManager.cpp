/**
 * @file LanguageManager.cpp
 * @brief 语言管理器实现
 */

#include "LanguageManager.h"

#include <QApplication>
#include <QTranslator>
#include <QSettings>
#include <QLocale>
#include <QLibraryInfo>
#include <QDebug>

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
    // 如果是相同语言，跳过
    if (languageCode == m_currentLanguage && m_translator) {
        return true;
    }

    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        qWarning() << "LanguageManager: No QApplication instance!";
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

    // 如果是英语，不需要加载翻译文件（英语是源语言）
    if (languageCode == "en_US" || languageCode.isEmpty()) {
        m_currentLanguage = "en_US";
        saveLanguagePreference();
        emit languageChanged(m_currentLanguage);
        return true;
    }

    // 加载应用程序翻译文件
    m_translator = new QTranslator(this);
    
    // 翻译文件路径：优先从文件系统加载（bin/translations/目录）
    QString fsPath = QCoreApplication::applicationDirPath() + 
                     QString("/translations/BigFileViewer_%1.qm").arg(languageCode);
    
    if (m_translator->load(fsPath)) {
        app->installTranslator(m_translator);
        qDebug() << "LanguageManager: Loaded translation from filesystem:" << fsPath;
    } else {
        // 尝试从资源系统加载（如果嵌入到exe中）
        QString qmFile = QString(":/i18n/BigFileViewer_%1.qm").arg(languageCode);
        if (m_translator->load(qmFile)) {
            app->installTranslator(m_translator);
            qDebug() << "LanguageManager: Loaded translation from resources:" << qmFile;
        } else {
            qWarning() << "LanguageManager: Translation file not found:" << fsPath;
            qWarning() << "LanguageManager: Also tried:" << qmFile;
            delete m_translator;
            m_translator = nullptr;
            return false;
        }
    }

    // 加载 Qt 内置翻译（对话框按钮等）
    m_qtTranslator = new QTranslator(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    QString qtTranslationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    
    if (m_qtTranslator->load("qt_" + languageCode, qtTranslationsPath)) {
        app->installTranslator(m_qtTranslator);
        qDebug() << "LanguageManager: Loaded Qt translation for" << languageCode;
    } else {
        // Qt 翻译加载失败不是致命错误
        qDebug() << "LanguageManager: Qt translation not available for" << languageCode;
    }

    m_currentLanguage = languageCode;
    saveLanguagePreference();
    
    // 发出语言变更信号，触发 UI 更新
    emit languageChanged(m_currentLanguage);
    
    return true;
}

void LanguageManager::saveLanguagePreference() {
    QSettings settings;
    settings.setValue("Language/current", m_currentLanguage);
    settings.sync();
}

QList<QPair<QString, QString>> LanguageManager::availableLanguages() const {
    QList<QPair<QString, QString>> languages;
    
    // 英语（源语言，始终可用）
    languages.append(qMakePair(QString("en_US"), QString("English")));
    
    // 简体中文
    languages.append(qMakePair(QString("zh_CN"), QString::fromUtf8("简体中文")));
    
    // 可以在这里添加更多语言支持...
    // languages.append(qMakePair(QString("ja_JP"), QString::fromUtf8("日本語")));
    // languages.append(qMakePair(QString("ko_KR"), QString::fromUtf8("한국어")));
    
    return languages;
}
