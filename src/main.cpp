/**
 * @file main.cpp
 * @brief 程序入口点
 * @description 根据平台类型选择性启用Qt GUI或控制台模式
 */

// 条件编译：检查是否启用Qt GUI支持
#if USE_QT_GUI

// ============================================================================
// Qt GUI 模式 - 桌面平台
// ============================================================================

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <QPushButton>
#include <QTextEdit>
#include <QMessageBox>

/**
 * @brief 主窗口类
 * @description 提供大文件读取的GUI界面
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit MainWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        // 设置窗口标题
        setWindowTitle(tr("BigFileRead - 大文件读取器"));
        
        // 设置窗口初始大小
        resize(800, 600);
        
        // 创建中心部件
        QWidget *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        // 创建垂直布局
        QVBoxLayout *layout = new QVBoxLayout(centralWidget);
        
        // 创建标题标签
        QLabel *titleLabel = new QLabel(tr("大文件读取器"), this);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; margin: 20px;");
        layout->addWidget(titleLabel);
        
        // 创建打开文件按钮
        QPushButton *openButton = new QPushButton(tr("打开文件"), this);
        openButton->setMinimumHeight(40);
        connect(openButton, &QPushButton::clicked, this, &MainWindow::openFile);
        layout->addWidget(openButton);
        
        // 创建文本显示区域
        m_textEdit = new QTextEdit(this);
        m_textEdit->setReadOnly(true);
        m_textEdit->setPlaceholderText(tr("文件内容将显示在这里..."));
        layout->addWidget(m_textEdit);
        
        // 创建状态标签
        m_statusLabel = new QLabel(tr("就绪"), this);
        layout->addWidget(m_statusLabel);
    }

private slots:
    /**
     * @brief 打开文件槽函数
     * @description 弹出文件选择对话框并读取文件内容
     */
    void openFile()
    {
        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("选择文件"),
            QString(),
            tr("所有文件 (*.*)")
        );
        
        if (!fileName.isEmpty())
        {
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                // 读取文件内容（大文件应考虑分块读取）
                QByteArray content = file.readAll();
                m_textEdit->setPlainText(QString::fromUtf8(content));
                m_statusLabel->setText(tr("已加载: %1 (大小: %2 字节)")
                    .arg(fileName)
                    .arg(file.size()));
                file.close();
            }
            else
            {
                QMessageBox::warning(this, tr("错误"), 
                    tr("无法打开文件: %1").arg(file.errorString()));
            }
        }
    }

private:
    QTextEdit *m_textEdit;      ///< 文本编辑器，用于显示文件内容
    QLabel *m_statusLabel;      ///< 状态标签，显示当前状态信息
};

// 包含MOC生成的文件（Qt元对象编译器）
#include "main.moc"

/**
 * @brief 程序入口点（Qt GUI模式）
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    // 创建Qt应用程序实例
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    QApplication::setApplicationName("BigFileRead");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("MyOrganization");
    
    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();
    
    // 进入Qt事件循环
    return app.exec();
}

#else

// ============================================================================
// 控制台模式 - 非桌面平台
// ============================================================================

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

/**
 * @brief 程序入口点（控制台模式）
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    std::cout << "========================================" << std::endl;
    std::cout << "  BigFileRead - 大文件读取器 (控制台版)" << std::endl;
    std::cout << "  版本: 1.0.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 检查命令行参数
    if (argc < 2)
    {
        std::cout << "用法: " << argv[0] << " <文件路径>" << std::endl;
        std::cout << std::endl;
        std::cout << "示例:" << std::endl;
        std::cout << "  " << argv[0] << " test.txt" << std::endl;
        return EXIT_FAILURE;
    }
    
    // 获取文件路径
    std::string filePath = argv[1];
    
    // 打开文件
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "错误: 无法打开文件 '" << filePath << "'" << std::endl;
        return EXIT_FAILURE;
    }
    
    // 获取文件大小
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::cout << "文件: " << filePath << std::endl;
    std::cout << "大小: " << fileSize << " 字节" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 读取并输出文件内容
    std::string line;
    int lineCount = 0;
    while (std::getline(file, line))
    {
        std::cout << line << std::endl;
        lineCount++;
    }
    
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "总行数: " << lineCount << std::endl;
    
    file.close();
    return EXIT_SUCCESS;
}

#endif // USE_QT_GUI
