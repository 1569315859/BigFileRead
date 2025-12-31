/**
 * @file MainWindow.h
 * @brief 主窗口类 - 大文件查看器 GUI
 * @version 1.5 - VS Code 风格暗色主题
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QListView;
class QLabel;
class QProgressBar;
class QTimer;
class BigFileModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /**
     * @brief 打开指定文件
     * @param filePath 文件路径
     */
    void openFile(const QString &filePath);

private slots:
    /**
     * @brief 打开文件对话框
     */
    void onOpenFile();

    /**
     * @brief 处理文件加载完成
     */
    void onFileLoaded(bool success, const QString &message);

    /**
     * @brief 更新索引进度
     */
    void onIndexingProgress(int percent);

    /**
     * @brief 跳转到指定行
     */
    void onGoToLine();

    /**
     * @brief 处理索引取消
     */
    void onIndexingCancelled();

private:
    /**
     * @brief 初始化 UI 组件
     */
    void setupUi();

    /**
     * @brief 创建菜单栏
     */
    void createMenus();

    /**
     * @brief 应用 VS Code 风格暗色主题
     */
    void applyDarkTheme();

    /**
     * @brief 更新窗口标题
     */
    void updateWindowTitle();

    /**
     * @brief 设置加载状态
     */
    void setLoadingState(bool loading);

private:
    QListView *m_listView = nullptr;        ///< 文件内容视图
    BigFileModel *m_model = nullptr;        ///< 大文件数据模型
    QLabel *m_statusLabel = nullptr;        ///< 状态栏标签
    QLabel *m_lineCountLabel = nullptr;     ///< 行数标签
    QProgressBar *m_progressBar = nullptr;  ///< 进度条
    QTimer *m_statusTimer = nullptr;        ///< 状态更新定时器
    bool m_isLoading = false;               ///< 是否正在加载
};

#endif // MAINWINDOW_H
