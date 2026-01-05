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
class QPlainTextEdit;
class QSplitter;
class QComboBox;
class QAction;
class BigFileModel;
class SearchBar;

class MainWindow : public QMainWindow {
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

  /**
   * @brief 处理搜索请求
   */
  void onSearchRequested(const QString &text, int direction);

  /**
   * @brief 处理搜索完成
   */
  void onSearchFinished(const std::vector<int> &results);

  /**
   * @brief 显示搜索栏
   */
  void onShowSearch();

  /**
   * @brief 复制选中行到剪贴板
   */
  void onCopySelected();

  /**
   * @brief 显示右键菜单
   */
  void onCustomContextMenu(const QPoint &pos);

  /**
   * @brief 处理日志追加（实时监控）
   */
  void onLogAppended();

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

  /**
   * @brief 窗口大小变化事件（用于定位浮动搜索栏）
   */
  void resizeEvent(QResizeEvent *event) override;

private:
  QListView *m_listView = nullptr;       ///< 文件内容视图（主列表）
  QPlainTextEdit *m_detailTextEdit = nullptr; ///< 详细文本视图（支持部分选择）
  QSplitter *m_splitter = nullptr;       ///< 主从视图分隔器
  BigFileModel *m_model = nullptr;       ///< 大文件数据模型
  QLabel *m_statusLabel = nullptr;       ///< 状态栏标签
  QLabel *m_lineCountLabel = nullptr;    ///< 行数标签
  QProgressBar *m_progressBar = nullptr; ///< 进度条
  QTimer *m_statusTimer = nullptr;       ///< 状态更新定时器
  bool m_isLoading = false;              ///< 是否正在加载

  // 搜索相关
  SearchBar *m_searchBar = nullptr; ///< 浮动搜索栏
  int m_currentSearchIndex = -1;    ///< 当前搜索结果索引

  // 编码选择
  QComboBox *m_encodingCombo = nullptr;  ///< 编码选择下拉框

  // 实时日志监控
  QAction *m_followTailAction = nullptr;  ///< 跟踪尾部开关
};

#endif // MAINWINDOW_H
