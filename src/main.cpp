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
#include "TabManager.h"
#include "DatabaseConnector.h"
#include "CloudStorageManager.h"
#include "WindowsEventLogReader.h"
#include "SystemTraceReader.h"
#include "SqlScratchpad.h"
#include "CorrelationAnalyzer.h"
#include "TextTransformer.h"
#include "RuleEngine.h"
#include "NotificationManager.h"
#include "ReportScheduler.h"
#include "DistinctValueAnalyzer.h"
#include "UpdateChecker.h"
#include "PluginManager.h"
#include "SessionRecovery.h"
#include "CSVDataSource.h"
#include "JSONViewer.h"
#include "AutomationEngine.h"
#include "CrashReporter.h"
#include "LocalLLMEngine.h"
#include "LogParser.h"
#include "DataDiffer.h"
#include <QQuickStyle>
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
    
    // 2. 必须先创建 QApplication，才能调用 applicationDirPath()
    QGuiApplication app(argc, argv);
    
    // 3. 将配置文件存储在 exe 所在目录（而非 %APPDATA%）
    //    这样可以将整个程序文件夹拷贝到 U 盘随身携带
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, 
                       QCoreApplication::applicationDirPath());
    
    // ★★★ 初始化崩溃报告收集器（尽早初始化）★★★
    CrashReporter::initialize("1.0.0");
    //QQuickStyle::setStyle("Material");
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
    
    // ★★★ 注册 QML 类型 ★★★
    qmlRegisterType<CSVDataSource>("BigFileViewer", 1, 0, "CSVDataSource");
    qmlRegisterType<JSONTreeModel>("BigFileViewer", 1, 0, "JSONTreeModel");
    qmlRegisterType<JSONLTableModel>("BigFileViewer", 1, 0, "JSONLTableModel");
    qmlRegisterType<LocalLLMEngine>("BigFileViewer", 1, 0, "LocalLLMEngine");
    qmlRegisterType<DataDiffer>("BigFileViewer", 1, 0, "DataDiffer");
    qmlRegisterSingletonInstance("BigFileViewer", 1, 0, "AutomationEngine", &AutomationEngine::instance());
    qmlRegisterSingletonInstance("BigFileViewer", 1, 0, "CrashReporter", CrashReporter::instance());
    
    // 设置 QML 引擎引用，用于语言切换时刷新 UI
    LanguageManager::instance().setEngine(&engine);
    
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
    engine.rootContext()->setContextProperty("_tabManager", &TabManager::instance());
    engine.rootContext()->setContextProperty("_databaseConnector", &DatabaseConnector::instance());
    engine.rootContext()->setContextProperty("_cloudManager", &CloudStorageManager::instance());
    engine.rootContext()->setContextProperty("_eventLogReader", &WindowsEventLogReader::instance());
    engine.rootContext()->setContextProperty("_traceReader", &SystemTraceReader::instance());
    engine.rootContext()->setContextProperty("_sqlScratchpad", &SqlScratchpad::instance());
    engine.rootContext()->setContextProperty("_correlationAnalyzer", &CorrelationAnalyzer::instance());
    engine.rootContext()->setContextProperty("_textTransformer", &TextTransformer::instance());
    engine.rootContext()->setContextProperty("_ruleEngine", &RuleEngine::instance());
    engine.rootContext()->setContextProperty("_notificationManager", &NotificationManager::instance());
    engine.rootContext()->setContextProperty("_reportScheduler", &ReportScheduler::instance());
    engine.rootContext()->setContextProperty("_distinctAnalyzer", &DistinctValueAnalyzer::instance());
    engine.rootContext()->setContextProperty("_updateChecker", &UpdateChecker::instance());
    engine.rootContext()->setContextProperty("_pluginManager", &PluginManager::instance());
    engine.rootContext()->setContextProperty("_sessionRecovery", &SessionRecovery::instance());
    
    // ★★★ JSON 树形视图模型 ★★★
    static JSONTreeModel jsonTreeModel;
    engine.rootContext()->setContextProperty("_jsonTreeModel", &jsonTreeModel);
    
    // ★★★ CSV 数据源 ★★★
    static CSVDataSource csvDataSource;
    engine.rootContext()->setContextProperty("_csvDataSource", &csvDataSource);
    
    // ★★★ 数据对比器 (单例) ★★★
    engine.rootContext()->setContextProperty("_dataDiffer", &DataDiffer::instance());
    
    // ★★★ 日志解析器 (单例) ★★★
    engine.rootContext()->setContextProperty("_logParser", &LogParser::instance());
    
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

    int result = app.exec();
    
    // ★★★ 清理崩溃报告器 ★★★
    CrashReporter::shutdown();
    
    return result;
}
