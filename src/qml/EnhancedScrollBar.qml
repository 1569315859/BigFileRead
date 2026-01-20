import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * 增强型滚动条
 * 将导航标记集成到滚动条内部，类似 VS Code 的 minimap 滚动条
 * 支持书签、搜索结果、错误、警告等多种标记类型
 * 用户可勾选显示/隐藏各类标记
 */
Item {
    id: enhancedScrollBar
    
    // 必需属性
    required property Flickable flickable
    property int totalLines: 1           // 总行数
    property real rowHeight: 22          // 每行高度
    
    // 标记数据
    property var bookmarks: []           // 书签行号数组
    property var searchResults: []       // 搜索结果行号数组
    property var errorLines: []          // 错误行号数组
    property var warningLines: []        // 警告行号数组
    property var infoLines: []           // 信息行号数组
    
    // 显示控制（用户可配置）
    property bool showMarkers: true           // 总开关
    property bool showBookmarks: true         // 显示书签
    property bool showSearchResults: true     // 显示搜索结果
    property bool showErrors: true            // 显示错误
    property bool showWarnings: true          // 显示警告
    property bool showInfo: false             // 显示信息（默认关闭，因为通常太多）
    
    // 高对比度颜色配置（适配深色和浅色主题）
    property bool isDarkTheme: _themeManager.isDarkTheme
    
    // 书签颜色 - 使用亮蓝色，深浅主题都清晰
    property color bookmarkColor: isDarkTheme ? "#00D4FF" : "#0066CC"
    // 搜索结果颜色 - 使用亮黄色/橙色
    property color searchColor: isDarkTheme ? "#FFD700" : "#FF8C00"
    // 错误颜色 - 鲜红色
    property color errorColor: isDarkTheme ? "#FF4757" : "#DC3545"
    // 警告颜色 - 鲜橙色
    property color warningColor: isDarkTheme ? "#FFA502" : "#E67E00"
    // 信息颜色 - 绿色
    property color infoColor: isDarkTheme ? "#2ED573" : "#28A745"
    
    // 主题颜色
    property color bgColor: _themeManager.panelBackground
    property color borderColor: _themeManager.borderColor
    property color textColor: _themeManager.textColor
    property color handleColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.4)
    property color handleHoverColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.6)
    
    // 信号
    signal lineClicked(int lineNumber)
    signal settingsRequested()  // 请求打开设置
    
    width: 16
    
    // 背景
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(bgColor.r, bgColor.g, bgColor.b, 0.5)
        
        // 左边框线
        Rectangle {
            anchors.left: parent.left
            width: 1
            height: parent.height
            color: borderColor
            opacity: 0.5
        }
    }
    
    // 标记轨道（在滚动条背景上绘制标记）
    Item {
        id: markerTrack
        anchors.fill: parent
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        visible: showMarkers
        clip: true
        
        // 计算缩放比例
        readonly property real lineToPixelRatio: totalLines > 0 ? height / totalLines : 1
        readonly property real markerHeight: Math.max(3, Math.min(lineToPixelRatio * 1.5, 6))
        
        // 错误标记（红色，最左侧，最优先显示）
        Repeater {
            model: (showMarkers && showErrors) ? errorLines : []
            
            Rectangle {
                x: 0
                y: Math.max(0, Math.min(modelData * markerTrack.lineToPixelRatio, markerTrack.height - markerTrack.markerHeight))
                width: 5
                height: markerTrack.markerHeight
                radius: 1
                color: errorColor
                
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -3
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: enhancedScrollBar.lineClicked(modelData)
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 200
                    ToolTip.text: qsTr("Error at line") + " " + (modelData + 1)
                }
            }
        }
        
        // 警告标记（橙色，左侧偏中）
        Repeater {
            model: (showMarkers && showWarnings) ? warningLines : []
            
            Rectangle {
                x: 3
                y: Math.max(0, Math.min(modelData * markerTrack.lineToPixelRatio, markerTrack.height - markerTrack.markerHeight))
                width: 5
                height: markerTrack.markerHeight
                radius: 1
                color: warningColor
                opacity: 0.9
                
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -3
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: enhancedScrollBar.lineClicked(modelData)
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 200
                    ToolTip.text: qsTr("Warning at line") + " " + (modelData + 1)
                }
            }
        }
        
        // 信息标记（绿色，中间）
        Repeater {
            model: (showMarkers && showInfo) ? infoLines : []
            
            Rectangle {
                x: 5
                y: Math.max(0, Math.min(modelData * markerTrack.lineToPixelRatio, markerTrack.height - markerTrack.markerHeight))
                width: 4
                height: markerTrack.markerHeight
                radius: 1
                color: infoColor
                opacity: 0.8
                
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -3
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: enhancedScrollBar.lineClicked(modelData)
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 200
                    ToolTip.text: qsTr("Info at line") + " " + (modelData + 1)
                }
            }
        }
        
        // 书签标记（蓝色三角形，右侧）
        Repeater {
            model: (showMarkers && showBookmarks) ? bookmarks : []
            
            Canvas {
                x: parent.width - 7
                y: Math.max(0, Math.min(modelData * markerTrack.lineToPixelRatio - 2, markerTrack.height - 6))
                width: 7
                height: 6
                
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = bookmarkColor
                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(width, height / 2)
                    ctx.lineTo(0, height)
                    ctx.closePath()
                    ctx.fill()
                }
                
                // 主题变化时重绘
                Connections {
                    target: enhancedScrollBar
                    function onBookmarkColorChanged() { parent.requestPaint() }
                }
                
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -3
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: enhancedScrollBar.lineClicked(modelData)
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 200
                    ToolTip.text: qsTr("Bookmark at line") + " " + (modelData + 1)
                }
            }
        }
        
        // 搜索结果标记（黄色/橙色，右侧）
        Repeater {
            model: (showMarkers && showSearchResults) ? searchResults : []
            
            Rectangle {
                x: parent.width - 5
                y: Math.max(0, Math.min(modelData * markerTrack.lineToPixelRatio, markerTrack.height - markerTrack.markerHeight))
                width: 5
                height: markerTrack.markerHeight
                radius: 1
                color: searchColor
                
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -3
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: enhancedScrollBar.lineClicked(modelData)
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 200
                    ToolTip.text: qsTr("Search result at line") + " " + (modelData + 1)
                }
            }
        }
    }
    
    // 滚动条滑块
    Rectangle {
        id: scrollHandle
        
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 4
        
        // 计算滑块位置和大小
        property real viewportRatio: flickable.height / Math.max(1, flickable.contentHeight)
        property real handleHeight: Math.max(30, parent.height * viewportRatio)
        property real availableHeight: parent.height - handleHeight
        property real scrollRatio: flickable.contentY / Math.max(1, flickable.contentHeight - flickable.height)
        
        y: Math.max(0, Math.min(scrollRatio * availableHeight, availableHeight))
        height: handleHeight
        
        radius: 3
        color: handleMouseArea.containsMouse || handleMouseArea.pressed ? handleHoverColor : handleColor
        border.color: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.2)
        border.width: 1
        
        Behavior on color { ColorAnimation { duration: 100 } }
        
        // 滑块拖动
        MouseArea {
            id: handleMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            
            property real dragStartY: 0
            property real dragStartScrollY: 0
            
            onPressed: (mouse) => {
                dragStartY = mouse.y + scrollHandle.y
                dragStartScrollY = flickable.contentY
            }
            
            onPositionChanged: (mouse) => {
                if (pressed) {
                    var deltaY = (mouse.y + scrollHandle.y) - dragStartY
                    var scrollRange = flickable.contentHeight - flickable.height
                    var newScrollY = dragStartScrollY + (deltaY / scrollHandle.availableHeight) * scrollRange
                    flickable.contentY = Math.max(0, Math.min(newScrollY, scrollRange))
                }
            }
        }
    }
    
    // 轨道点击跳转
    MouseArea {
        anchors.fill: parent
        z: -1  // 放在滑块后面
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                // 右键打开设置菜单
                markerSettingsMenu.popup()
            } else {
                // 左键跳转
                var clickRatio = mouse.y / height
                var maxScroll = flickable.contentHeight - flickable.height
                flickable.contentY = Math.max(0, Math.min(clickRatio * maxScroll, maxScroll))
            }
        }
        
        onWheel: (wheel) => {
            // 支持滚轮
            var delta = wheel.angleDelta.y > 0 ? -rowHeight * 3 : rowHeight * 3
            flickable.contentY = Math.max(0, Math.min(flickable.contentY + delta, flickable.contentHeight - flickable.height))
        }
    }
    
    // 标记设置右键菜单
    Menu {
        id: markerSettingsMenu
        
        MenuItem {
            text: qsTr("Show Markers")
            checkable: true
            checked: showMarkers
            onTriggered: showMarkers = !showMarkers
        }
        
        MenuSeparator {}
        
        MenuItem {
            text: "  ● " + qsTr("Errors")
            checkable: true
            checked: showErrors
            enabled: showMarkers
            onTriggered: showErrors = !showErrors
        }
        
        MenuItem {
            text: "  ● " + qsTr("Warnings")
            checkable: true
            checked: showWarnings
            enabled: showMarkers
            onTriggered: showWarnings = !showWarnings
        }
        
        MenuItem {
            text: "  ● " + qsTr("Info")
            checkable: true
            checked: showInfo
            enabled: showMarkers
            onTriggered: showInfo = !showInfo
        }
        
        MenuItem {
            text: "  ▶ " + qsTr("Bookmarks")
            checkable: true
            checked: showBookmarks
            enabled: showMarkers
            onTriggered: showBookmarks = !showBookmarks
        }
        
        MenuItem {
            text: "  ■ " + qsTr("Search Results")
            checkable: true
            checked: showSearchResults
            enabled: showMarkers
            onTriggered: showSearchResults = !showSearchResults
        }
    }
}
