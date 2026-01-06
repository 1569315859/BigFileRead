/**
 * @file main.cpp
 * @brief 程序入口点 - BigFileViewer 高性能大文件查看器
 * @description 使用内存映射技术，支持打开 10GB+ 文本文件
 */

#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QIcon>
#include "MainWindow.h"

/**
 * @brief 程序入口点
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    // 设置应用程序信息（必须在创建 QApplication 之前设置，用于 QSettings）
    QCoreApplication::setOrganizationName("BigFileRead");
    QCoreApplication::setOrganizationDomain("bigfileread.local");
    QCoreApplication::setApplicationName("BigFileViewer");
    QCoreApplication::setApplicationVersion("1.5.0");

    // ★★★ 便携模式 (Portable Mode) 配置 ★★★
    // 1. 强制使用 INI 文件格式（不使用 Windows 注册表）
    QSettings::setDefaultFormat(QSettings::IniFormat);
    
    // 2. 将配置文件存储在 exe 所在目录（而非 %APPDATA%）
    //    这样可以将整个程序文件夹拷贝到 U 盘随身携带
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, 
                       QCoreApplication::applicationDirPath());

    // 创建 Qt 应用程序实例
    QApplication app(argc, argv);

    // ★★★ 设置应用程序图标（用于窗口标题栏和任务栏）★★★
    app.setWindowIcon(QIcon(":/app_icon.ico"));

    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();

    // 如果命令行传入了文件路径，直接打开
    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        mainWindow.openFile(filePath);
    }

    // 进入 Qt 事件循环
    return app.exec();
}
