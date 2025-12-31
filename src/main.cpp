/**
 * @file main.cpp
 * @brief 程序入口点 - BigFileViewer 高性能大文件查看器
 * @description 使用内存映射技术，支持打开 10GB+ 文本文件
 */

#include <QApplication>
#include "MainWindow.h"

/**
 * @brief 程序入口点
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    // 创建 Qt 应用程序实例
    QApplication app(argc, argv);

    // 设置应用程序信息
    QApplication::setApplicationName("BigFileViewer");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("BigFileRead");

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
