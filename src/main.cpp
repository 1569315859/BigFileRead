/**
 * @file main.cpp
 * @brief 程序入口点 - BigFileViewer 高性能大文件查看器
 * @description 使用内存映射技术，支持打开 10GB+ 文本文件
 */

#include <QGuiApplication>
#include <QSettings>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "BigFileModel.h"
#include "ThemeManager.h"
#include "AppController.h"
#include "LanguageManager.h"
#include "KeywordConfigManager.h"
#include "FeatureGate.h"
#include "DirectoryWatcher.h"
#include "FilterTemplateManager.h"
#include "WorkspaceManager.h"
#include "SmtpAlertManager.h"
#include "JiraIntegration.h"
#include "GitHubIntegration.h"
#include "RemoteFileManager.h"
#include "DataSanitizer.h"
#include "AIAnalysisManager.h"

/**
 * @brief 程序入口点
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[]) {
    // ★★★ 便携模式 (Portable Mode) 配置 ★★★
    // 1. 强制使用 INI 文件格式（不使用 Windows 注册表）
    QSettings::setDefaultFormat(QSettings::IniFormat);
    
    // 2. 将配置文件存储在 exe 所在目录（而非 %APPDATA%）
    //    这样可以将整个程序文件夹拷贝到 U 盘随身携带
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, 
                       QCoreApplication::applicationDirPath());

    QGuiApplication app(argc, argv);

    // Set application info
    app.setOrganizationName("BigFileRead");
    app.setOrganizationDomain("bigfileread.local");
    app.setApplicationName("BigFileViewer");

    // ★★★ 设置应用程序图标（用于窗口标题栏和任务栏）★★★
    app.setWindowIcon(QIcon(":/app_icon.ico"));

    // ★★★ 初始化国际化 (i18n) ★★★
    // 从用户设置加载语言偏好，或使用系统语言
    LanguageManager::instance().init();

    // Create backend objects
    ThemeManager themeManager;
    BigFileModel logModel;
    AppController appController;
    KeywordConfigManager keywordConfig;

    QQmlApplicationEngine engine;
    
    // Register context properties
    engine.rootContext()->setContextProperty("_themeManager", &themeManager);
    engine.rootContext()->setContextProperty("_logModel", &logModel);
    engine.rootContext()->setContextProperty("_appController", &appController);
    engine.rootContext()->setContextProperty("_languageManager", &LanguageManager::instance());
    engine.rootContext()->setContextProperty("_keywordConfig", &keywordConfig);
    engine.rootContext()->setContextProperty("_featureGate", &FeatureGate::instance());
    engine.rootContext()->setContextProperty("_directoryWatcher", &DirectoryWatcher::instance());
    engine.rootContext()->setContextProperty("filterTemplateManager", &FilterTemplateManager::instance());
    engine.rootContext()->setContextProperty("workspaceManager", &WorkspaceManager::instance());
    engine.rootContext()->setContextProperty("_alertManager", &SmtpAlertManager::instance());
    engine.rootContext()->setContextProperty("_jiraIntegration", &JiraIntegration::instance());
    engine.rootContext()->setContextProperty("_githubIntegration", &GitHubIntegration::instance());
    engine.rootContext()->setContextProperty("_remoteManager", &RemoteFileManager::instance());
    engine.rootContext()->setContextProperty("_dataSanitizer", &DataSanitizer::instance());
    engine.rootContext()->setContextProperty("_aiManager", &AIAnalysisManager::instance());
    
    // ★★★ 连接语言切换信号，刷新 QML 界面翻译 ★★★
    QObject::connect(&LanguageManager::instance(), &LanguageManager::languageChanged,
                     &engine, &QQmlApplicationEngine::retranslate);

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    // 如果命令行传入了文件路径，直接打开
    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        if (!filePath.isEmpty()) {
            logModel.loadFile(filePath);
        }
    }

    return app.exec();
}
