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
class QLineEdit;
class QToolBar;
class QToolButton;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QMenu;
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
   * @param text 搜索文本
   * @param direction 搜索方向
   * @param useRegex 是否使用正则表达式
   */
  void onSearchRequested(const QString &text, int direction, bool useRegex = false);

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

  /**
   * @brief 处理过滤请求
   */
  void onFilterRequested();

  /**
   * @brief 处理过滤完成
   */
  void onFilterFinished(int matchCount);

  /**
   * @brief 清除过滤
   */
  void onClearFilter();

  /**
   * @brief 切换当前行书签
   */
  void onToggleBookmark();

  /**
   * @brief 跳转到下一个书签
   */
  void onNextBookmark();

  /**
   * @brief 导出当前可见行（尊重过滤状态）
   */
  void exportVisibleLines();

  /**
   * @brief 打开文件所在文件夹
   */
  void openContainingFolder();

  /**
   * @brief 清除最近文件列表
   */
  void clearRecentFiles();

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

  /**
   * @brief 窗口关闭事件（保存设置）
   */
  void closeEvent(QCloseEvent *event) override;

  /**
   * @brief 拖拽进入事件
   */
  void dragEnterEvent(QDragEnterEvent *event) override;

  /**
   * @brief 拖放事件
   */
  void dropEvent(QDropEvent *event) override;

  /**
   * @brief 加载用户设置
   */
  void loadSettings();

  /**
   * @brief 保存用户设置
   */
  void saveSettings();

  /**
   * @brief 添加文件到最近文件列表
   * @param filePath 文件路径
   */
  void addToRecentFiles(const QString &filePath);

  /**
   * @brief 更新最近文件菜单
   */
  void updateRecentFilesMenu();

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

  // 日志过滤 (Filter Bar)
  QToolBar *m_filterToolBar = nullptr;    ///< 过滤工具栏
  QLineEdit *m_filterInput = nullptr;     ///< 过滤输入框
  QLabel *m_filterStatusLabel = nullptr;  ///< 过滤状态标签
  QAction *m_toggleFilterAction = nullptr; ///< 切换过滤栏显示
  QToolButton *m_regexToggleBtn = nullptr; ///< 正则表达式开关

  // 书签功能
  QAction *m_toggleBookmarkAction = nullptr;  ///< 切换书签 (F2)
  QAction *m_nextBookmarkAction = nullptr;    ///< 下一个书签 (F3)

  // 最近文件菜单
  QMenu *m_recentFilesMenu = nullptr;  ///< 最近文件子菜单
  static constexpr int MAX_RECENT_FILES = 10;  ///< 最大最近文件数
};

#endif // MAINWINDOW_H

