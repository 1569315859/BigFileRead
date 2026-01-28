import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief 文件标签栏组件
 * 
 * 功能特性：
 * - 显示多个打开的文件标签
 * - 支持拖拽排序
 * - 右键菜单（关闭/关闭其他/关闭全部）
 * - 中键点击关闭
 * - 显示文件修改状态
 * - 标签宽度自适应
 */
Rectangle {
    id: root
    
    property var tabManager  // TabManager 实例
    
    // 主题颜色 - 从 ThemeManager 获取
    property color textColor: _themeManager ? _themeManager.textColor : "#CCCCCC"
    property color bgColor: _themeManager ? _themeManager.panelBackground : "#252526"
    property color accentColor: _themeManager ? _themeManager.accentColor : "#007ACC"
    property color borderColor: _themeManager ? _themeManager.borderColor : "#3C3C3C"
    property color hoverColor: _themeManager ? Qt.lighter(_themeManager.panelBackground, 1.2) : "#2D2D2D"
    property color activeColor: _themeManager ? _themeManager.backgroundColor : "#1E1E1E"
    
    // 标签配置
    property int tabMinWidth: 100
    property int tabMaxWidth: 200
    property int tabHeight: 32
    
    color: bgColor
    height: tabHeight + 2
    
    // 标签滚动区域
    Flickable {
        id: tabFlickable
        anchors.fill: parent
        anchors.rightMargin: newTabButton.width + 4
        contentWidth: tabRow.width
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        
        // 鼠标滚轮水平滚动
        MouseArea {
            anchors.fill: parent
            onWheel: (wheel) => {
                tabFlickable.contentX = Math.max(0, 
                    Math.min(tabFlickable.contentWidth - tabFlickable.width,
                             tabFlickable.contentX - wheel.angleDelta.y))
            }
        }
        
        Row {
            id: tabRow
            height: parent.height
            spacing: 1
            
            Repeater {
                id: tabRepeater
                model: tabManager ? tabManager.tabs : []
                
                delegate: Rectangle {
                    id: tabItem
                    
                    property bool isActive: tabManager && tabManager.currentTabIndex === index
                    property bool isHovered: tabMouseArea.containsMouse
                    property bool isDragging: false
                    
                    width: calculateTabWidth()
                    height: root.tabHeight
                    
                    color: {
                        if (isActive) return activeColor
                        if (isHovered) return hoverColor
                        return "transparent"
                    }
                    
                    // 底部高亮线（活动标签）
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 2
                        color: accentColor
                        visible: isActive
                    }
                    
                    // 顶部边框
                    Rectangle {
                        anchors.top: parent.top
                        width: parent.width
                        height: 1
                        color: isActive ? accentColor : "transparent"
                    }
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 24  // 为关闭按钮留出空间
                        spacing: 4
                        
                        // 文件图标
                        Text {
                            text: modelData.isModified ? "●" : "📄"
                            color: modelData.isModified ? "#E8AB53" : textColor
                            font.pixelSize: modelData.isModified ? 8 : 12
                            Layout.alignment: Qt.AlignVCenter
                        }
                        
                        // 文件名
                        Text {
                            text: modelData.fileName || qsTr("Untitled")
                            color: textColor
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            
                            ToolTip.visible: tabMouseArea.containsMouse && text !== modelData.filePath
                            ToolTip.text: modelData.filePath || ""
                            ToolTip.delay: 800
                        }
                    }
                    
                    // 主鼠标区域 - 不覆盖关闭按钮区域
                    MouseArea {
                        id: tabMouseArea
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: closeBtn.left  // 停在关闭按钮之前
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                        
                        // 拖拽支持
                        drag.target: tabItem
                        drag.axis: Drag.XAxis
                        
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.LeftButton) {
                                console.log("[FileTabBar] Tab clicked, index:", index, "current:", tabManager ? tabManager.currentTabIndex : -1)
                                if (tabManager) {
                                    console.log("[FileTabBar] Calling setCurrentTabIndex(", index, ")")
                                    tabManager.setCurrentTabIndex(index)
                                }
                            } else if (mouse.button === Qt.MiddleButton) {
                                // 中键关闭
                                if (tabManager) {
                                    tabManager.closeTab(index)
                                }
                            } else if (mouse.button === Qt.RightButton) {
                                // 右键菜单
                                tabContextMenu.targetIndex = index
                                tabContextMenu.popup()
                            }
                        }
                        
                        onDoubleClicked: {
                            // 双击可以重命名或其他操作
                        }
                    }
                    
                    // 关闭按钮 - 独立的鼠标区域，不受 tabMouseArea 影响
                    Rectangle {
                        id: closeBtn
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18
                        height: 18
                        radius: 3
                        color: closeBtnMouseArea.containsMouse ? "#E81123" : "transparent"
                        visible: tabMouseArea.containsMouse || closeBtnMouseArea.containsMouse || isActive
                        
                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: closeBtnMouseArea.containsMouse ? "white" : textColor
                            font.pixelSize: 10
                        }
                        
                        MouseArea {
                            id: closeBtnMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                console.log("[FileTabBar] Close button clicked, index:", index)
                                if (tabManager) {
                                    tabManager.closeTab(index)
                                }
                            }
                        }
                    }
                    
                    // 计算标签宽度
                    function calculateTabWidth() {
                        if (!tabManager || tabManager.tabCount === 0) return tabMinWidth
                        
                        var availableWidth = tabFlickable.width
                        var tabCount = tabManager.tabCount
                        var idealWidth = availableWidth / tabCount
                        
                        return Math.max(tabMinWidth, Math.min(tabMaxWidth, idealWidth))
                    }
                    
                    // 拖拽状态处理
                    Drag.active: tabMouseArea.drag.active
                    Drag.source: tabItem
                    Drag.hotSpot.x: width / 2
                    Drag.hotSpot.y: height / 2
                    
                    states: State {
                        when: tabMouseArea.drag.active
                        PropertyChanges {
                            target: tabItem
                            opacity: 0.8
                            z: 100
                        }
                    }
                }
            }
        }
    }
    
    // 新建标签按钮
    Rectangle {
        id: newTabButton
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        radius: 4
        color: newTabMouseArea.containsMouse ? hoverColor : "transparent"
        
        Text {
            anchors.centerIn: parent
            text: "+"
            color: textColor
            font.pixelSize: 16
            font.bold: true
        }
        
        MouseArea {
            id: newTabMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                // 触发打开文件对话框
                root.openFileRequested()
            }
        }
        
        ToolTip.visible: newTabMouseArea.containsMouse
        ToolTip.text: qsTr("Open New File")
        ToolTip.delay: 500
    }
    
    // 底部边框
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: borderColor
    }
    
    // 右键上下文菜单
    Menu {
        id: tabContextMenu
        
        property int targetIndex: -1
        
        MenuItem {
            text: qsTr("Close")
            onTriggered: {
                if (tabManager && tabContextMenu.targetIndex >= 0) {
                    tabManager.closeTab(tabContextMenu.targetIndex)
                }
            }
        }
        
        MenuItem {
            text: qsTr("Close Others")
            enabled: tabManager && tabManager.tabCount > 1
            onTriggered: {
                if (tabManager && tabContextMenu.targetIndex >= 0) {
                    tabManager.closeOtherTabs(tabContextMenu.targetIndex)
                }
            }
        }
        
        MenuItem {
            text: qsTr("Close All")
            onTriggered: {
                if (tabManager) {
                    tabManager.closeAllTabs()
                }
            }
        }
        
        MenuItem {
            text: qsTr("Close to the Right")
            enabled: tabManager && tabContextMenu.targetIndex < tabManager.tabCount - 1
            onTriggered: {
                if (tabManager && tabContextMenu.targetIndex >= 0) {
                    tabManager.closeTabsToRight(tabContextMenu.targetIndex)
                }
            }
        }
        
        MenuSeparator {}
        
        MenuItem {
            text: qsTr("Duplicate Tab")
            onTriggered: {
                if (tabManager && tabContextMenu.targetIndex >= 0) {
                    tabManager.duplicateTab(tabContextMenu.targetIndex)
                }
            }
        }
        
        MenuSeparator {}
        
        MenuItem {
            text: qsTr("Copy Path")
            onTriggered: {
                if (tabManager && tabContextMenu.targetIndex >= 0) {
                    var info = tabManager.getTabInfo(tabContextMenu.targetIndex)
                    if (info && info.filePath) {
                        _appController.copyToClipboard(info.filePath)
                    }
                }
            }
        }
        
        MenuItem {
            text: qsTr("Open Containing Folder")
            onTriggered: {
                if (tabManager && tabContextMenu.targetIndex >= 0) {
                    var info = tabManager.getTabInfo(tabContextMenu.targetIndex)
                    if (info && info.filePath) {
                        // 打开文件所在目录
                        var folderPath = info.filePath.substring(0, info.filePath.lastIndexOf('/'))
                        Qt.openUrlExternally("file:///" + folderPath)
                    }
                }
            }
        }
    }
    
    // 信号
    signal openFileRequested()
    signal tabChanged(int index)
    
    // 连接 TabManager 信号
    Connections {
        target: tabManager
        
        function onCurrentTabChanged() {
            console.log("[FileTabBar] onCurrentTabChanged, new index:", tabManager ? tabManager.currentTabIndex : -1)
            root.tabChanged(tabManager.currentTabIndex)
            // 强制刷新 Repeater 以更新 isActive 状态
            var oldModel = tabRepeater.model
            tabRepeater.model = null
            tabRepeater.model = oldModel
        }
        
        function onTabsChanged() {
            console.log("[FileTabBar] onTabsChanged, tab count:", tabManager ? tabManager.tabCount : 0)
            // 标签列表变化，重新布局
            tabRepeater.model = tabManager ? tabManager.tabs : []
        }
        
        function onTabClosed(index) {
            console.log("[FileTabBar] onTabClosed, index:", index)
        }
    }
}
