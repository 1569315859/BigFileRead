import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform // For FileDialog (native)

ApplicationWindow {
    id: window
    width: 1200
    height: 800
    visible: true
    title: "BigFileViewer"
    
    color: _themeManager.backgroundColor

    // Define theme colors for easy binding
    property color bgColor: _themeManager.backgroundColor
    property color panelColor: _themeManager.panelBackground
    property color textColor: _themeManager.textColor
    property color accentColor: _themeManager.accentColor
    property color borderColor: _themeManager.borderColor

    // UI State
    property bool isSearchVisible: false
    property bool isFilterVisible: false
    property bool isDetailPanelVisible: false
    property bool isFollowMode: false  // 实时加载模式
    property bool isTableViewMode: false  // 表格视图模式
    property int rowHeight: 24
    property int selectedRow: -1
    property string selectedLineText: ""
    property string formattedJsonText: ""  // JSON 格式化后的文本
    
    // ========== 列可见性配置（Grid 模式下使用）==========
    property bool showLineColumn: true      // 显示行号列（内置，始终显示）
    property bool showTimeColumn: true      // 显示时间列
    property bool showLevelColumn: true     // 显示日志级别列
    property bool showMessageColumn: true   // 显示消息列（始终显示）

    // 拖拽文件打开
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0) {
                var filePath = drop.urls[0].toString()
                // 移除 file:/// 前缀
                if (filePath.startsWith("file:///")) {
                    filePath = filePath.substring(8)
                }
                _logModel.loadFile(filePath)
                _appController.addRecentFile(filePath)
            }
        }
        
        // 拖拽悬停提示
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
            visible: parent.containsDrag
            z: 1000
            
            Rectangle {
                anchors.centerIn: parent
                width: 300
                height: 100
                radius: 10
                color: panelColor
                border.color: accentColor
                border.width: 2
                
                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    
                    Text {
                        text: "📂"
                        font.pixelSize: 32
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: qsTr("Drop file to open")
                        color: textColor
                        font.pixelSize: 16
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
    }

    // 监听日志追加，自动滚动到底部
    Connections {
        target: _logModel
        function onLogAppended() {
            if (isFollowMode) {
                tableView.positionViewAtRow(_logModel.lineCount - 1, TableView.AlignBottom)
            }
        }
    }

    // 快捷键
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            isSearchVisible = true
            searchInput.forceActiveFocus()
        }
    }
    Shortcut {
        sequence: "Ctrl+G"
        onActivated: {
            isFilterVisible = true
            filterInput.forceActiveFocus()
        }
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: isDetailPanelVisible = !isDetailPanelVisible
    }
    Shortcut {
        sequence: "Ctrl+L"
        onActivated: goToLineDialog.open()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (isSearchVisible) isSearchVisible = false
            else if (isFilterVisible) isFilterVisible = false
            else if (isDetailPanelVisible) isDetailPanelVisible = false
        }
    }
    Shortcut {
        sequence: "Ctrl+C"
        onActivated: {
            if (selectedLineText) {
                _appController.copyToClipboard(selectedLineText)
            }
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+V"
        onActivated: _logModel.loadFromClipboard()
    }

    // Format file size with appropriate unit (B/KB/MB/GB)
    function formatFileSize(bytes) {
        if (bytes < 1024) {
            return bytes + " B"
        } else if (bytes < 1024 * 1024) {
            return (bytes / 1024).toFixed(2) + " KB"
        } else if (bytes < 1024 * 1024 * 1024) {
            return (bytes / 1024 / 1024).toFixed(2) + " MB"
        } else {
            return (bytes / 1024 / 1024 / 1024).toFixed(2) + " GB"
        }
    }
    
    // ========== 关键词高亮配置（从 KeywordConfigManager 获取）==========
    // 默认关键字颜色（如果 _keywordConfig 未加载）
    property var defaultKeywordColors: ({
        "FATAL": "#FF0000",
        "CRITICAL": "#FF0000",
        "ERROR": "#FF6B6B",
        "FAIL": "#FF6B6B",
        "EXCEPTION": "#FF4444",
        "WARN": "#FFA500",
        "WARNING": "#FFA500",
        "INFO": "#00BFFF",
        "DEBUG": "#DA70D6",
        "TRACE": "#20B2AA",
        "SUCCESS": "#32CD32",
        "OK": "#32CD32"
    })
    
    property var keywordColors: {
        var cfg = _keywordConfig ? _keywordConfig.keywords : null
        if (cfg && Object.keys(cfg).length > 0) {
            return cfg
        }
        return defaultKeywordColors
    }
    
    // 当配置改变时更新
    Connections {
        target: _keywordConfig
        function onKeywordsChanged() {
            var cfg = _keywordConfig.keywords
            if (cfg && Object.keys(cfg).length > 0) {
                keywordColors = cfg
            }
        }
    }
    
    property string currentSearchTerm: ""
    property string currentFilterTerm: ""
    
    // 侧边导航条的标记数据
    property var navMarkerData: []
    property bool isNavBarVisible: true  // 滚动条标记可见性
    
    // 将文本转换为带高亮的富文本 HTML
    function highlightText(text) {
        if (!text) return ""
        
        // HTML 转义
        var escaped = text.replace(/&/g, "&amp;")
                         .replace(/</g, "&lt;")
                         .replace(/>/g, "&gt;")
        
        var result = escaped
        
        // 1. 关键词高亮（不区分大小写）
        for (var keyword in keywordColors) {
            var regex = new RegExp("\\b(" + keyword + ")\\b", "gi")
            var color = keywordColors[keyword]
            result = result.replace(regex, '<font color="' + color + '"><b>$1</b></font>')
        }
        
        // 2. 搜索词高亮（黄色背景）
        if (currentSearchTerm && currentSearchTerm.length > 0) {
            try {
                var searchRegex = new RegExp("(" + escapeRegex(currentSearchTerm) + ")", "gi")
                result = result.replace(searchRegex, '<span style="background-color:#FFFF00;color:#000000;">$1</span>')
            } catch (e) {
                // 正则表达式无效，忽略
            }
        }
        
        // 3. 过滤词高亮（绿色背景，与搜索词区分）
        if (currentFilterTerm && currentFilterTerm.length > 0 && currentFilterTerm !== currentSearchTerm) {
            try {
                var filterRegex = new RegExp("(" + escapeRegex(currentFilterTerm) + ")", "gi")
                result = result.replace(filterRegex, '<span style="background-color:#90EE90;color:#000000;">$1</span>')
            } catch (e) {
                // 正则表达式无效，忽略
            }
        }
        
        return result
    }
    
    // 转义正则表达式特殊字符
    function escapeRegex(str) {
        return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
    }
    
    // JSON 格式化函数
    function formatJson(text) {
        if (!text) return ""
        
        // 尝试查找 JSON 对象或数组
        var jsonMatch = text.match(/(\{[\s\S]*\}|\[[\s\S]*\])/g)
        if (!jsonMatch) return text
        
        var result = text
        for (var i = 0; i < jsonMatch.length; i++) {
            try {
                var parsed = JSON.parse(jsonMatch[i])
                var formatted = JSON.stringify(parsed, null, 2)
                result = result.replace(jsonMatch[i], "\n" + formatted + "\n")
            } catch (e) {
                // 解析失败，保持原样
            }
        }
        return result
    }
    
    // 检测文本中是否包含 JSON
    function containsJson(text) {
        if (!text) return false
        return /\{[\s\S]*\}|\[[\s\S]*\]/.test(text)
    }
    
    // 提取并格式化所有 JSON
    function extractAndFormatJson(text) {
        if (!text) return ""
        
        var jsonMatch = text.match(/(\{[\s\S]*\}|\[[\s\S]*\])/g)
        if (!jsonMatch) return ""
        
        var result = []
        for (var i = 0; i < jsonMatch.length; i++) {
            try {
                var parsed = JSON.parse(jsonMatch[i])
                result.push(JSON.stringify(parsed, null, 2))
            } catch (e) {
                // 解析失败，跳过
            }
        }
        return result.join("\n\n")
    }

    // Unified button style component
    component ToolBtn: ToolButton {
        id: toolBtn
        implicitWidth: Math.max(60, contentItem.implicitWidth + 16)
        implicitHeight: 28
        
        contentItem: Text {
            text: toolBtn.text
            color: textColor
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        
        background: Rectangle {
            color: toolBtn.checked ? accentColor : 
                   (toolBtn.down ? Qt.darker(panelColor, 1.2) : 
                   (toolBtn.hovered ? Qt.lighter(panelColor, 1.1) : "transparent"))
            radius: 4
            border.color: toolBtn.hovered ? borderColor : "transparent"
            border.width: 1
        }
    }

    header: ToolBar {
        height: 36
        
        background: Rectangle {
            color: panelColor
            border.color: borderColor
            border.width: 1
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 2
            
            // ========== 文件 ==========
            ToolBtn {
                text: qsTr("File")
                onClicked: fileMenu.open()
                
                Menu {
                    id: fileMenu
                    
                    MenuItem {
                        text: qsTr("Open File...")
                        onTriggered: fileDialog.open()
                    }
                    
                    Menu {
                        id: recentFilesMenu
                        title: qsTr("Recent Files")
                        enabled: _appController.recentFiles.length > 0
                        
                        Instantiator {
                            model: _appController.recentFiles
                            MenuItem {
                                text: _appController.getFileName(modelData)
                                ToolTip.visible: hovered
                                ToolTip.text: modelData
                                ToolTip.delay: 500
                                onTriggered: {
                                    _logModel.loadFile(modelData)
                                    _appController.addRecentFile(modelData)
                                }
                            }
                            onObjectAdded: (index, object) => recentFilesMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => recentFilesMenu.removeItem(object)
                        }
                        
                        MenuSeparator { }
                        
                        MenuItem {
                            text: qsTr("Clear Recent Files")
                            onTriggered: _appController.clearRecentFiles()
                        }
                    }
                    
                    MenuSeparator {}
                    
                    MenuItem {
                        text: qsTr("Paste from Clipboard") + " (Ctrl+Shift+V)"
                        onTriggered: _logModel.loadFromClipboard()
                    }
                    
                    MenuItem {
                        text: qsTr("Reload File")
                        enabled: _logModel.filePath !== ""
                        onTriggered: _logModel.loadFile(_logModel.filePath)
                    }
                    
                    MenuSeparator {}
                    
                    MenuItem {
                        text: qsTr("Export...")
                        enabled: _logModel.lineCount > 0
                        onTriggered: exportDialog.open()
                    }
                    
                    MenuItem {
                        text: qsTr("Monitor Directory...")
                        onTriggered: directoryMonitorDialog.open()
                    }
                }
            }

            Rectangle { width: 1; height: 20; color: borderColor }

            // ========== 视图 ==========
            ToolBtn {
                id: searchBtn
                text: qsTr("Search")
                checkable: true
                checked: isSearchVisible
                onClicked: {
                    isSearchVisible = !isSearchVisible
                    if (isSearchVisible) searchInput.forceActiveFocus()
                }
            }

            ToolBtn {
                id: filterBtn
                text: qsTr("Filter")
                checkable: true
                checked: isFilterVisible
                onClicked: {
                    isFilterVisible = !isFilterVisible
                    if (isFilterVisible) filterInput.forceActiveFocus()
                }
            }
            
            ToolBtn {
                text: qsTr("Details")
                checkable: true
                checked: isDetailPanelVisible
                onClicked: isDetailPanelVisible = !isDetailPanelVisible
            }

            Rectangle { width: 1; height: 20; color: borderColor }

            // ========== 导航 ==========
            ToolBtn {
                text: qsTr("Go To")
                onClicked: goToLineDialog.open()
            }
            
            ToolBtn {
                text: qsTr("Bookmark")
                onClicked: bookmarkMenu.open()
                
                Menu {
                    id: bookmarkMenu
                    
                    MenuItem {
                        text: qsTr("Add/Remove Bookmark")
                        enabled: selectedRow >= 0
                        onTriggered: _logModel.toggleBookmark(selectedRow)
                    }
                    MenuItem {
                        text: qsTr("Edit Bookmark Comment...")
                        enabled: selectedRow >= 0 && _logModel.isBookmarked(selectedRow)
                        onTriggered: {
                            var lineContent = _logModel.data(_logModel.index(selectedRow, 0), Qt.DisplayRole) || ""
                            bookmarkCommentDialog.openForRow(selectedRow, lineContent)
                        }
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Next Bookmark")
                        onTriggered: {
                            var currentRow = Math.floor(tableView.contentY / rowHeight)
                            var nextRow = _logModel.getNextBookmark(currentRow)
                            if (nextRow >= 0) {
                                tableView.contentY = nextRow * rowHeight
                            }
                        }
                    }
                    MenuItem {
                        text: qsTr("Previous Bookmark")
                        onTriggered: {
                            var currentRow = Math.floor(tableView.contentY / rowHeight)
                            var prevRow = _logModel.getPrevBookmark(currentRow)
                            if (prevRow >= 0) {
                                tableView.contentY = prevRow * rowHeight
                            }
                        }
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Save Bookmarks...")
                        enabled: _logModel.bookmarkLines.length > 0
                        onTriggered: _logModel.autoSaveBookmarks()
                    }
                    MenuItem {
                        text: qsTr("Load Bookmarks...")
                        onTriggered: _logModel.autoLoadBookmarks()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Clear All Bookmarks")
                        onTriggered: _logModel.clearAllBookmarks()
                    }
                }
            }

            Rectangle { width: 1; height: 20; color: borderColor }

            // ========== 视图菜单 ==========
            ToolBtn {
                text: qsTr("View")
                onClicked: viewMenu.open()
                
                Menu {
                    id: viewMenu
                    title: qsTr("View")
                    
                    // 视图模式切换
                    MenuItem {
                        text: qsTr("Text View (Raw)")
                        checkable: true
                        checked: !isTableViewMode
                        onTriggered: {
                            isTableViewMode = false
                            _logModel.setTableModeEnabled(false)
                        }
                    }
                    MenuItem {
                        text: qsTr("Grid View (Parsed)")
                        checkable: true
                        checked: isTableViewMode
                        onTriggered: {
                            isTableViewMode = true
                            _logModel.setTableModeEnabled(true)
                        }
                    }
                    
                    MenuSeparator {}
                    
                    // 列显示配置子菜单
                    Menu {
                        id: columnsMenu
                        title: qsTr("Columns")
                        enabled: isTableViewMode
                        
                        MenuItem {
                            text: qsTr("Line Number")
                            checkable: true
                            checked: showLineColumn
                            enabled: false  // 行号始终显示
                        }
                        MenuItem {
                            text: qsTr("Time")
                            checkable: true
                            checked: showTimeColumn
                            onTriggered: showTimeColumn = checked
                        }
                        MenuItem {
                            text: qsTr("Level")
                            checkable: true
                            checked: showLevelColumn
                            onTriggered: showLevelColumn = checked
                        }
                        MenuItem {
                            text: qsTr("Message")
                            checkable: true
                            checked: showMessageColumn
                            enabled: false  // 消息始终显示
                        }
                        
                        MenuSeparator {}
                        
                        MenuItem {
                            text: qsTr("Show All Columns")
                            onTriggered: {
                                showTimeColumn = true
                                showLevelColumn = true
                            }
                        }
                        MenuItem {
                            text: qsTr("Hide Optional Columns")
                            onTriggered: {
                                showTimeColumn = false
                                showLevelColumn = false
                            }
                        }
                    }
                    
                    MenuSeparator {}
                    
                    // 导航条可见性
                    MenuItem {
                        text: qsTr("Navigation Markers")
                        checkable: true
                        checked: isNavBarVisible
                        onTriggered: isNavBarVisible = checked
                    }
                }
            }

            // ========== 工具 ==========
            ToolBtn {
                text: qsTr("Tools")
                onClicked: toolsMenu.open()
                
                Menu {
                    id: toolsMenu
                    
                    MenuItem {
                        text: qsTr("Advanced Filter...")
                        onTriggered: advancedFilterDialog.open()
                    }
                    MenuItem {
                        text: qsTr("Export Visible Lines...")
                        onTriggered: exportFileDialog.open()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Follow Mode (Live)")
                        checkable: true
                        checked: isFollowMode
                        onTriggered: {
                            isFollowMode = checked
                            if (isFollowMode && _logModel.lineCount > 0) {
                                tableView.positionViewAtRow(_logModel.lineCount - 1, TableView.AlignBottom)
                            }
                        }
                    }
                    MenuItem {
                        text: qsTr("Reload File")
                        onTriggered: {
                            if (_logModel.filePath) {
                                _logModel.loadFile(_logModel.filePath)
                            }
                        }
                    }
                }
            }

            // ========== 设置 ==========
            ToolBtn {
                text: qsTr("Settings")
                onClicked: settingsMenu.open()
                
                Menu {
                    id: settingsMenu
                    
                    Menu {
                        title: qsTr("Theme")
                        MenuItem {
                            text: "Dark"
                            checkable: true
                            checked: _themeManager.currentTheme === "Dark"
                            onTriggered: _themeManager.setTheme("Dark")
                        }
                        MenuItem {
                            text: "Light"
                            checkable: true
                            checked: _themeManager.currentTheme === "Light"
                            onTriggered: _themeManager.setTheme("Light")
                        }
                        MenuItem {
                            text: "Warm"
                            checkable: true
                            checked: _themeManager.currentTheme === "Warm"
                            onTriggered: _themeManager.setTheme("Warm")
                        }
                    }
                    
                    Menu {
                        title: qsTr("Language")
                        
                        MenuItem {
                            text: "English"
                            checkable: true
                            checked: _languageManager.currentLanguage === "en_US"
                            onTriggered: _languageManager.loadLanguage("en_US")
                        }
                        MenuItem {
                            text: "简体中文"
                            checkable: true
                            checked: _languageManager.currentLanguage === "zh_CN"
                            onTriggered: _languageManager.loadLanguage("zh_CN")
                        }
                        MenuItem {
                            text: "繁體中文"
                            checkable: true
                            checked: _languageManager.currentLanguage === "zh_TW"
                            onTriggered: _languageManager.loadLanguage("zh_TW")
                        }
                        MenuItem {
                            text: "日本語"
                            checkable: true
                            checked: _languageManager.currentLanguage === "ja_JP"
                            onTriggered: _languageManager.loadLanguage("ja_JP")
                        }
                        MenuItem {
                            text: "한국어"
                            checkable: true
                            checked: _languageManager.currentLanguage === "ko_KR"
                            onTriggered: _languageManager.loadLanguage("ko_KR")
                        }
                        MenuItem {
                            text: "Русский"
                            checkable: true
                            checked: _languageManager.currentLanguage === "ru_RU"
                            onTriggered: _languageManager.loadLanguage("ru_RU")
                        }
                        MenuItem {
                            text: "Deutsch"
                            checkable: true
                            checked: _languageManager.currentLanguage === "de_DE"
                            onTriggered: _languageManager.loadLanguage("de_DE")
                        }
                        MenuItem {
                            text: "Français"
                            checkable: true
                            checked: _languageManager.currentLanguage === "fr_FR"
                            onTriggered: _languageManager.loadLanguage("fr_FR")
                        }
                        MenuItem {
                            text: "Español"
                            checkable: true
                            checked: _languageManager.currentLanguage === "es_ES"
                            onTriggered: _languageManager.loadLanguage("es_ES")
                        }
                        MenuItem {
                            text: "Português"
                            checkable: true
                            checked: _languageManager.currentLanguage === "pt_BR"
                            onTriggered: _languageManager.loadLanguage("pt_BR")
                        }
                        MenuItem {
                            text: "Italiano"
                            checkable: true
                            checked: _languageManager.currentLanguage === "it_IT"
                            onTriggered: _languageManager.loadLanguage("it_IT")
                        }
                        MenuItem {
                            text: "العربية"
                            checkable: true
                            checked: _languageManager.currentLanguage === "ar_SA"
                            onTriggered: _languageManager.loadLanguage("ar_SA")
                        }
                        MenuItem {
                            text: "हिन्दी"
                            checkable: true
                            checked: _languageManager.currentLanguage === "hi_IN"
                            onTriggered: _languageManager.loadLanguage("hi_IN")
                        }
                    }
                    
                    Menu {
                        title: qsTr("Encoding")
                        MenuItem { 
                            text: "UTF-8"
                            checkable: true
                            onTriggered: _logModel.setEncoding(0) 
                        }
                        MenuItem { 
                            text: "System"
                            checkable: true
                            onTriggered: _logModel.setEncoding(1) 
                        }
                        MenuItem { 
                            text: "GBK/GB2312"
                            checkable: true
                            onTriggered: _logModel.setEncoding(2) 
                        }
                    }
                    
                    MenuSeparator {}
                    
                    MenuItem {
                        text: qsTr("Keyword Colors...")
                        onTriggered: keywordConfigDialog.open()
                    }
                    
                    MenuItem {
                        text: isNavBarVisible ? qsTr("Hide Scroll Markers") : qsTr("Show Scroll Markers")
                        onTriggered: isNavBarVisible = !isNavBarVisible
                    }
                    
                    MenuSeparator {}
                    
                    MenuItem {
                        text: qsTr("Register...")
                        onTriggered: registrationDialog.open()
                    }
                    MenuItem {
                        text: qsTr("About")
                        onTriggered: aboutDialog.open()
                    }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            // ========== 状态指示器 ==========
            Text {
                text: isFollowMode ? "● LIVE" : ""
                color: "#FF5722"
                font.pixelSize: 11
                font.bold: true
                visible: isFollowMode
            }
        }
    }
    
    // About Dialog - Using Popup instead of Dialog (Dialog is abstract in Qt6)
    Popup {
        id: aboutDialog
        modal: true
        width: 320
        height: 200
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        background: Rectangle {
            color: panelColor
            border.color: borderColor
            radius: 6
        }
        
        contentItem: ColumnLayout {
            spacing: 12
            
            Text {
                text: "BigFileViewer v1.0"
                font.bold: true
                font.pixelSize: 18
                color: textColor
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: qsTr("High-performance large file viewer")
                color: textColor
                font.pixelSize: 13
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: qsTr("Supports files over 10GB")
                color: Qt.darker(textColor, 1.3)
                font.pixelSize: 11
                Layout.alignment: Qt.AlignHCenter
            }
            
            Item { Layout.fillHeight: true }
            
            Button {
                text: "OK"
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 80
                implicitHeight: 30
                onClicked: aboutDialog.close()
                background: Rectangle {
                    color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    RegistrationDialog { id: registrationDialog }
    GoToLineDialog { 
        id: goToLineDialog 
        onAccepted: {
            if (targetLine > 0) {
                tableView.contentY = (targetLine - 1) * rowHeight
            }
        }
    }
    BookmarkCommentDialog {
        id: bookmarkCommentDialog
    }
    ExportDialog {
        id: exportDialog
        onExportCompleted: function(path) {
            console.log("Export completed:", path)
        }
    }
    DirectoryMonitorDialog {
        id: directoryMonitorDialog
        onFileSelected: function(filePath) {
            _logModel.loadFile(filePath)
            _appController.addRecentFile(filePath)
        }
    }
    AdvancedFilterDialog { id: advancedFilterDialog }
    KeywordConfigDialog { id: keywordConfigDialog }

    Platform.FileDialog {
        id: fileDialog
        title: qsTr("Open Log File")
        nameFilters: ["Log files (*.log *.txt)", "All files (*)"]
        
        Component.onCompleted: {
            console.log("FileDialog initialized, platform:", Qt.platform.os)
        }
        
        onAccepted: {
            console.log("FileDialog accepted, file:", file)
            // 使用 C++ 端的 urlToLocalPath 方法进行更可靠的路径转换
            var path = _appController.urlToLocalPath(file)
            console.log("Processed path:", path)
            _logModel.loadFile(path)
            _appController.addRecentFile(path)
        }
        
        onRejected: {
            console.log("FileDialog rejected/cancelled")
        }
    }

    Platform.FileDialog {
        id: exportFileDialog
        title: qsTr("Export Visible Lines")
        fileMode: Platform.FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Text files (*.txt)", "Log files (*.log)", "All files (*)"]
        onAccepted: {
            var path = _appController.urlToLocalPath(file)
            _logModel.exportToFile(path)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Search Bar - Optimized layout
        Rectangle {
            id: searchBar
            Layout.fillWidth: true
            Layout.preferredHeight: isSearchVisible ? 40 : 0
            opacity: isSearchVisible ? 1 : 0
            color: panelColor
            clip: true
            
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 150 } }
            Behavior on opacity { NumberAnimation { duration: 150 } }
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 8
                
                Text { 
                    text: qsTr("Find:") 
                    color: textColor 
                    font.pixelSize: 12
                }
                
                TextField {
                    id: searchInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    placeholderText: qsTr("Enter search text (supports regex)...")
                    font.pixelSize: 12
                    color: textColor
                    background: Rectangle { 
                        color: bgColor
                        border.color: searchInput.activeFocus ? accentColor : borderColor
                        border.width: searchInput.activeFocus ? 2 : 1
                        radius: 4 
                    }
                    onAccepted: {
                        currentSearchTerm = text
                        _logModel.search(text, regexCheck.checked)
                    }
                }
                
                CheckBox {
                    id: regexCheck
                    text: qsTr("Regex")
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 16
                        implicitHeight: 16
                        x: 0
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        
                        Text {
                            text: "✓"
                            color: accentColor
                            anchors.centerIn: parent
                            font.pixelSize: 12
                            font.bold: true
                            visible: regexCheck.checked
                        }
                    }
                    contentItem: Text { 
                        text: regexCheck.text
                        color: textColor
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: regexCheck.indicator.width + regexCheck.spacing
                    }
                }
                
                Button {
                    text: qsTr("Find")
                    implicitHeight: 28
                    implicitWidth: 60
                    font.pixelSize: 12
                    onClicked: {
                        currentSearchTerm = searchInput.text
                        _logModel.search(searchInput.text, regexCheck.checked)
                    }
                    background: Rectangle {
                        color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Button {
                    text: "▲"
                    implicitHeight: 28
                    implicitWidth: 32
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Previous")
                    onClicked: {
                        var currentRow = Math.floor(tableView.contentY / rowHeight)
                        var prevRow = _logModel.prevSearchResult(currentRow)
                        if (prevRow >= 0) {
                            tableView.contentY = prevRow * rowHeight
                        }
                    }
                    background: Rectangle {
                        color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Button {
                    text: "▼"
                    implicitHeight: 28
                    implicitWidth: 32
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Next")
                    onClicked: {
                        var currentRow = Math.floor(tableView.contentY / rowHeight)
                        var nextRow = _logModel.nextSearchResult(currentRow)
                        if (nextRow >= 0) {
                            tableView.contentY = nextRow * rowHeight
                        }
                    }
                    background: Rectangle {
                        color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Text {
                    text: _logModel.searchResultCount > 0 ? (_logModel.searchResultCount + " " + qsTr("matches")) : ""
                    color: Qt.darker(textColor, 1.3)
                    font.pixelSize: 11
                }
                
                Button {
                    text: "✕"
                    implicitHeight: 28
                    implicitWidth: 28
                    onClicked: {
                        currentSearchTerm = ""
                        isSearchVisible = false
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#e81123" : "transparent"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.hovered ? "white" : textColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // Filter Bar - Optimized layout
        Rectangle {
            id: filterBar
            Layout.fillWidth: true
            Layout.preferredHeight: isFilterVisible ? 40 : 0
            opacity: isFilterVisible ? 1 : 0
            color: panelColor
            clip: true
            
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 150 } }
            Behavior on opacity { NumberAnimation { duration: 150 } }
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 8
                
                Text { 
                    text: qsTr("Filter:") 
                    color: textColor 
                    font.pixelSize: 12
                }
                
                TextField {
                    id: filterInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    placeholderText: qsTr("Enter filter text (supports regex)...")
                    font.pixelSize: 12
                    color: textColor
                    background: Rectangle { 
                        color: bgColor
                        border.color: filterInput.activeFocus ? accentColor : borderColor
                        border.width: filterInput.activeFocus ? 2 : 1
                        radius: 4 
                    }
                    onAccepted: {
                        currentFilterTerm = text
                        _logModel.applyFilter(text, filterRegexCheck.checked)
                    }
                }
                
                CheckBox {
                    id: filterRegexCheck
                    text: qsTr("Regex")
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 16
                        implicitHeight: 16
                        x: 0
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        
                        Text {
                            text: "✓"
                            color: accentColor
                            anchors.centerIn: parent
                            font.pixelSize: 12
                            font.bold: true
                            visible: filterRegexCheck.checked
                        }
                    }
                    contentItem: Text { 
                        text: filterRegexCheck.text
                        color: textColor
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: filterRegexCheck.indicator.width + filterRegexCheck.spacing
                    }
                }
                
                Button {
                    text: qsTr("Apply")
                    implicitHeight: 28
                    implicitWidth: 60
                    font.pixelSize: 12
                    onClicked: {
                        currentFilterTerm = filterInput.text
                        _logModel.applyFilter(filterInput.text, filterRegexCheck.checked)
                    }
                    background: Rectangle {
                        color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Button {
                    text: qsTr("Clear")
                    implicitHeight: 28
                    implicitWidth: 60
                    font.pixelSize: 12
                    onClicked: {
                        filterInput.text = ""
                        currentFilterTerm = ""
                        _logModel.clearFilter()
                    }
                    background: Rectangle {
                        color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Text {
                    text: _logModel.isFilterMode ? (qsTr("Showing") + " " + _logModel.lineCount + " / " + _logModel.totalLineCount) : ""
                    color: Qt.darker(textColor, 1.3)
                    font.pixelSize: 11
                }
                
                Button {
                    text: "✕"
                    implicitHeight: 28
                    implicitWidth: 28
                    onClicked: isFilterVisible = false
                    background: Rectangle {
                        color: parent.hovered ? "#e81123" : "transparent"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.hovered ? "white" : textColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // ========== 日志视图区域 ==========
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            
            // ========== 固定的行号列 ==========
            ColumnLayout {
                Layout.preferredWidth: 60
                Layout.minimumWidth: 60
                Layout.maximumWidth: 60
                Layout.fillHeight: true
                spacing: 0
                
                // 行号列标题
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: isTableViewMode ? 28 : 0
                    visible: isTableViewMode
                    color: panelColor
                    
                    Text {
                        text: qsTr("Line")
                        color: textColor
                        font.bold: true
                        font.pixelSize: 12
                        anchors.centerIn: parent
                    }
                    
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: borderColor
                    }
                    Rectangle {
                        anchors.right: parent.right
                        width: 1
                        height: parent.height
                        color: borderColor
                    }
                }
                
                // 行号列表 - 与 TableView 同步滚动
                ListView {
                    id: lineNumberListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    model: _logModel
                    
                    // 与 tableView 同步滚动
                    contentY: tableView.contentY
                    onContentYChanged: {
                        if (lineNumberListView.moving) {
                            tableView.contentY = contentY
                        }
                    }
                    
                    interactive: false  // 禁用独立滚动，由 tableView 控制
                    
                    delegate: Rectangle {
                        width: lineNumberListView.width
                        height: rowHeight
                        color: {
                            if (index === selectedRow) return Qt.tint(Qt.darker(bgColor, 1.1), "#40007ACC")
                            return Qt.darker(bgColor, 1.1)
                        }
                        
                        // 行号文字
                        Text {
                            text: (realRow !== undefined ? realRow + 1 : index + 1)
                            color: index === selectedRow ? textColor : "#858585"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        
                        // 书签指示器
                        Rectangle {
                            visible: isBookmarked
                            width: 8
                            height: 8
                            radius: 4
                            color: "#3794ff"
                            anchors.left: parent.left
                            anchors.leftMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        
                        // 右边框
                        Rectangle {
                            width: 1
                            height: parent.height
                            color: borderColor
                            anchors.right: parent.right
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                selectedRow = index
                                selectedLineText = display
                            }
                        }
                    }
                }
            }
            
            // ========== 主内容区域 ==========
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
            
                // 列标题（仅表格模式显示）- 不包含 Line 列
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: isTableViewMode ? 28 : 0
                    visible: isTableViewMode
                    color: panelColor
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    spacing: 1
                    
                    // 消息列标题（始终显示）
                    Text {
                        text: qsTr("Message")
                        color: textColor
                        font.bold: true
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                }
                
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: borderColor
                }
            }

            TableView {
                id: tableView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                
                model: _logModel
                
                columnSpacing: 0
                rowSpacing: 0
                
                // 强制单列布局 - 只显示第一列（原始内容）
                // 多列解析模式在未来版本中实现
                columnWidthProvider: function(column) {
                    if (column === 0) {
                        return tableView.width
                    }
                    return 0  // 隐藏其他列
                }
                
                delegate: Rectangle {
                    implicitWidth: tableView.width
                    implicitHeight: rowHeight
                    color: {
                        if (row === selectedRow) return Qt.tint(bgColor, "#40007ACC")
                        if (isBookmarked) return Qt.tint(row % 2 === 0 ? bgColor : Qt.darker(bgColor, 1.05), "#40FFFF00")
                        return row % 2 === 0 ? bgColor : Qt.darker(bgColor, 1.05)
                    }
                    
                    // ========== 内容区域 ==========
                    Text {
                        text: highlightText(display)
                        textFormat: Text.RichText
                        color: textColor
                        font.family: "Consolas"
                        font.pixelSize: 13
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: rowMouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            selectedRow = row
                            selectedLineText = display
                            if (mouse.button === Qt.RightButton) {
                                globalContextMenu.popup(rowMouseArea, mouse.x, mouse.y)
                            }
                        }
                        onDoubleClicked: {
                            selectedRow = row
                            selectedLineText = display
                            isDetailPanelVisible = true
                        }
                    }
                }
                
                // 隐藏默认滚动条，使用增强型滚动条
                ScrollBar.vertical: ScrollBar { 
                    id: vbar
                    visible: false  // 隐藏，由 EnhancedScrollBar 替代
                }
                ScrollBar.horizontal: ScrollBar { }
            }
            }  // End of inner ColumnLayout
            
            // ========== 增强型滚动条（集成导航标记）==========
            EnhancedScrollBar {
                id: enhancedScrollBar
                Layout.fillHeight: true
                Layout.preferredWidth: 16
                
                flickable: tableView
                totalLines: _logModel.lineCount
                rowHeight: window.rowHeight
                showMarkers: isNavBarVisible
                
                // 各类标记数据
                bookmarks: _logModel.bookmarkLines || []
                searchResults: _logModel.searchResultLines || []
                errorLines: _logModel.errorLines || []
                warningLines: _logModel.warningLines || []
                infoLines: _logModel.infoLines || []
                
                onLineClicked: function(lineNumber) {
                    tableView.contentY = lineNumber * rowHeight
                    selectedRow = lineNumber
                }
            }
        }  // End of RowLayout
        
        // Detail Panel - Expandable bottom panel for selected line
        Rectangle {
            id: detailPanel
            Layout.fillWidth: true
            Layout.preferredHeight: isDetailPanelVisible ? 200 : 0
            opacity: isDetailPanelVisible ? 1 : 0
            color: panelColor
            clip: true
            
            property bool showFormattedJson: false
            
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 200 } }
            Behavior on opacity { NumberAnimation { duration: 200 } }
            
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: borderColor
            }
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                
                // Header with buttons
                RowLayout {
                    Layout.fillWidth: true
                    
                    Text {
                        text: "📄 " + qsTr("Line Details") + (selectedRow >= 0 ? " - " + qsTr("Line") + " " + (selectedRow + 1) : "")
                        color: textColor
                        font.bold: true
                        font.pixelSize: 13
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    // View mode toggle
                    Button {
                        id: jsonToggleBtn
                        text: detailPanel.showFormattedJson ? qsTr("Raw") : qsTr("JSON")
                        implicitHeight: 26
                        implicitWidth: 60
                        font.pixelSize: 11
                        visible: containsJson(selectedLineText)
                        onClicked: {
                            detailPanel.showFormattedJson = !detailPanel.showFormattedJson
                            if (detailPanel.showFormattedJson) {
                                formattedJsonText = extractAndFormatJson(selectedLineText)
                            }
                        }
                        background: Rectangle {
                            color: detailPanel.showFormattedJson ? accentColor : 
                                   (parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor))
                            border.color: borderColor
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: detailPanel.showFormattedJson ? "white" : textColor
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    
                    Button {
                        text: qsTr("Copy")
                        implicitHeight: 26
                        implicitWidth: 60
                        font.pixelSize: 11
                        onClicked: _appController.copyToClipboard(detailPanel.showFormattedJson ? formattedJsonText : selectedLineText)
                        background: Rectangle {
                            color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    
                    Button {
                        text: "✕"
                        implicitHeight: 26
                        implicitWidth: 26
                        font.pixelSize: 14
                        onClicked: isDetailPanelVisible = false
                        background: Rectangle {
                            color: parent.hovered ? "#e81123" : "transparent"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? "white" : textColor
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                
                // Scrollable text area
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    TextArea {
                        id: detailTextArea
                        text: {
                            if (!selectedLineText) return qsTr("(Select a line to view details)")
                            if (detailPanel.showFormattedJson && formattedJsonText) return formattedJsonText
                            return selectedLineText
                        }
                        color: selectedLineText ? textColor : Qt.darker(textColor, 1.5)
                        font.family: "Consolas"
                        font.pixelSize: 13
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextArea.Wrap
                        background: Rectangle {
                            color: bgColor
                            border.color: borderColor
                            radius: 4
                        }
                    }
                }
            }
        }
    }
    
    footer: Rectangle {
        height: 26
        color: panelColor
        border.color: borderColor
        border.width: 1
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 20
            
            Text {
                text: _logModel.filePath ? _logModel.filePath : qsTr("No file opened")
                color: _logModel.filePath ? textColor : Qt.darker(textColor, 1.5)
                font.pixelSize: 12
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            
            Rectangle { width: 1; height: 16; color: borderColor }
            
            Text {
                text: qsTr("Size:") + " " + formatFileSize(_logModel.fileSize)
                color: textColor
                font.pixelSize: 12
            }
            
            Rectangle { width: 1; height: 16; color: borderColor }
            
            Text {
                text: _logModel.isFilterMode 
                    ? (qsTr("Lines:") + " " + _logModel.lineCount.toLocaleString() + " / " + _logModel.totalLineCount.toLocaleString())
                    : (qsTr("Lines:") + " " + _logModel.lineCount.toLocaleString())
                color: textColor
                font.pixelSize: 12
            }
            
            Rectangle { width: 1; height: 16; color: borderColor }
            
            Text {
                text: _appController.isRegistered ? qsTr("Registered") : (qsTr("Trial:") + " " + _appController.trialDaysRemaining + " " + qsTr("days left"))
                color: _appController.isRegistered ? "#4CAF50" : "#FF9800"
                font.pixelSize: 12
            }
        }
    }
    
    // ========== 右键上下文菜单（ApplicationWindow 级别） ==========
    Menu {
        id: globalContextMenu
        
        MenuItem {
            text: qsTr("Copy Line")
            onTriggered: _appController.copyToClipboard(selectedLineText)
        }
        MenuItem {
            text: qsTr("Copy Line Number")
            onTriggered: _appController.copyToClipboard((selectedRow + 1).toString())
        }
        
        MenuSeparator {}
        
        MenuItem {
            text: qsTr("Toggle Bookmark")
            onTriggered: _logModel.toggleBookmark(selectedRow)
        }
        MenuItem {
            text: qsTr("Next Bookmark")
            onTriggered: {
                var nextRow = _logModel.getNextBookmark(selectedRow)
                if (nextRow >= 0) {
                    tableView.contentY = nextRow * rowHeight
                    selectedRow = nextRow
                }
            }
        }
        MenuItem {
            text: qsTr("Previous Bookmark")
            onTriggered: {
                var prevRow = _logModel.getPrevBookmark(selectedRow)
                if (prevRow >= 0) {
                    tableView.contentY = prevRow * rowHeight
                    selectedRow = prevRow
                }
            }
        }
        
        MenuSeparator {}
        
        MenuItem {
            text: qsTr("Show Details")
            onTriggered: isDetailPanelVisible = true
        }
        MenuItem {
            text: qsTr("Format JSON")
            enabled: containsJson(selectedLineText)
            onTriggered: {
                formattedJsonText = extractAndFormatJson(selectedLineText)
                isDetailPanelVisible = true
            }
        }
        
        MenuSeparator {}
        
        MenuItem {
            text: qsTr("Filter This Level")
            enabled: {
                if (!selectedLineText) return false
                var levelMatch = selectedLineText.match(/\b(INFO|DEBUG|WARN|WARNING|ERROR|FATAL|TRACE)\b/i)
                return levelMatch !== null
            }
            onTriggered: {
                var levelMatch = selectedLineText.match(/\b(INFO|DEBUG|WARN|WARNING|ERROR|FATAL|TRACE)\b/i)
                if (levelMatch) {
                    _logModel.applyAdvancedFilter(levelMatch[1].toUpperCase(), [], true, false)
                }
            }
        }
        MenuItem {
            text: qsTr("Filter This Keyword...")
            onTriggered: {
                isFilterVisible = true
                filterInput.forceActiveFocus()
            }
        }
        
        MenuSeparator {}
        
        MenuItem {
            text: qsTr("Go To Line...")
            onTriggered: goToLineDialog.open()
        }
        MenuItem {
            text: qsTr("Advanced Filter...")
            onTriggered: advancedFilterDialog.open()
        }
    }
    
    // ========== 升级提示对话框 ==========
    UpgradePromptDialog {
        id: upgradePromptDialog
        parent: Overlay.overlay
    }
    
    // ========== FeatureGate 信号连接 ==========
    Connections {
        target: _featureGate
        function onUpgradePromptRequested(featureInfo) {
            upgradePromptDialog.showForFeature(featureInfo)
        }
    }
}
