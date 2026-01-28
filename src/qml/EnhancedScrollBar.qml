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
    
    width: 24  // 默认宽度增加
    
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
        readonly property real markerHeight: Math.max(1.5, Math.min(lineToPixelRatio * 1.5, 4)) // Reduced marker height for dense logs
        
        Canvas {
            id: markerCanvas
            anchors.fill: parent
            
            // 依赖数据变化自动重绘
            property var errorData: (enhancedScrollBar.showMarkers && enhancedScrollBar.showErrors) ? enhancedScrollBar.errorLines : []
            property var warningData: (enhancedScrollBar.showMarkers && enhancedScrollBar.showWarnings) ? enhancedScrollBar.warningLines : []
            property var infoData: (enhancedScrollBar.showMarkers && enhancedScrollBar.showInfo) ? enhancedScrollBar.infoLines : []
            property var bookmarkData: (enhancedScrollBar.showMarkers && enhancedScrollBar.showBookmarks) ? enhancedScrollBar.bookmarks : []
            property var searchData: (enhancedScrollBar.showMarkers && enhancedScrollBar.showSearchResults) ? enhancedScrollBar.searchResults : []
            
            // 监听所有依赖数据的变化
            onErrorDataChanged: requestPaint()
            onWarningDataChanged: requestPaint()
            onInfoDataChanged: requestPaint()
            onBookmarkDataChanged: requestPaint()
            onSearchDataChanged: requestPaint()
            
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                var ratio = markerTrack.lineToPixelRatio
                var mh = markerTrack.markerHeight
                // 最小绘制高度为1像素，避免过细看不见
                var drawH = Math.max(1, mh)
                
                // 辅助绘制函数：优化大量数据的绘制，避免重复绘制同一像素
                // x: x坐标, w: 宽度, color: 颜色, data: 行号数组
                function drawStrip(x, w, color, data) {
                    if (!data || data.length === 0) return
                    
                    ctx.fillStyle = color
                    var lastY = -100
                    var count = data.length
                    
                    for (var i = 0; i < count; i++) {
                        // 计算像素Y坐标
                        var y = Math.floor(data[i] * ratio)
                        
                        // 简单去重：如果当前行映射的像素位置与上一次相同（或者非常接近），跳过
                        // 对于非常密集的错误，这能显著提高性能
                        if (y > lastY) {
                            ctx.fillRect(x, y, w, drawH)
                            lastY = y
                        }
                    }
                }
                
                // 1. 错误标记（红色，最左侧）
                drawStrip(0, 8, enhancedScrollBar.errorColor, errorData)
                
                // 2. 警告标记（橙色，左侧偏中）
                drawStrip(6, 8, enhancedScrollBar.warningColor, warningData)
                
                // 3. 信息标记（绿色，中间）
                drawStrip(10, 6, enhancedScrollBar.infoColor, infoData)
                
                // 4. 搜索结果标记（黄色，右侧）
                drawStrip(width - 7, 7, enhancedScrollBar.searchColor, searchData)
                
                // 5. 书签标记（蓝色三角形，右侧，需要特殊绘制）
                if (bookmarkData && bookmarkData.length > 0) {
                    ctx.fillStyle = enhancedScrollBar.bookmarkColor
                    var bData = bookmarkData
                    var lastBY = -100
                    
                    for (var j = 0; j < bData.length; j++) {
                        var by = Math.floor(bData[j] * ratio) - 2
                        if (by > lastBY + 4) { // 书签稍微稀疏一点绘制，避免重叠太难看
                            ctx.beginPath()
                            ctx.moveTo(width - 9, by)
                            ctx.lineTo(width, by + 3.5)
                            ctx.lineTo(width - 9, by + 7)
                            ctx.fill()
                            lastBY = by
                        }
                    }
                }
            }
        }
        
        // 点击处理 - 需要根据点击位置在 Canvas 上查找对应的行
        MouseArea {
            anchors.fill: parent
            
            // 二分查找或近似查找点击位置附近的行
            function findOriginalLine(yPos) {
                var total = enhancedScrollBar.totalLines
                var ratio = markerTrack.lineToPixelRatio
                if (ratio <= 0) return -1
                
                var clickedLine = Math.floor(yPos / ratio)
                var searchRange = Math.floor(5 / ratio) // 搜索点击位置上下5像素范围内的标记
                
                // 搜索优先顺序：书签 > 错误 > 搜索 > 警告 > 信息
                var allTypes = [
                    { data: enhancedScrollBar.bookmarks, name: qsTr("Bookmark"), enabled: enhancedScrollBar.showBookmarks },
                    { data: enhancedScrollBar.errorLines, name: qsTr("Error"), enabled: enhancedScrollBar.showErrors },
                    { data: enhancedScrollBar.searchResults, name: qsTr("Search result"), enabled: enhancedScrollBar.showSearchResults },
                    { data: enhancedScrollBar.warningLines, name: qsTr("Warning"), enabled: enhancedScrollBar.showWarnings },
                    { data: enhancedScrollBar.infoLines, name: qsTr("Info"), enabled: enhancedScrollBar.showInfo }
                ]
                
                var bestLine = -1
                var minDist = Number.MAX_VALUE
                var bestType = ""
                
                for (var t = 0; t < allTypes.length; t++) {
                    var type = allTypes[t]
                    if (!type.enabled || !type.data || type.data.length === 0) continue
                    
                    var arr = type.data
                    // 简单的线性搜索优化（实际可以用二分，但这里只有点击时触发，性能要求不高）
                    // 既然数组是有序的，我们可以二分查找到 clickedLine 附近
                    
                    // 简单遍历：由于我们不知道数组大小，但通常我们只需要找附近的
                    // 我们可以直接根据 clickLine 在数组中 lower_bound
                    
                    // 二分查找 lower_bound
                    var low = 0, high = arr.length - 1
                    var idx = -1
                    while (low <= high) {
                        var mid = Math.floor((low + high) / 2)
                        if (arr[mid] >= clickedLine - searchRange) {
                            idx = mid
                            high = mid - 1
                        } else {
                            low = mid + 1
                        }
                    }
                    
                    if (idx !== -1) {
                        // Check forward from idx
                        for (var k = idx; k < arr.length; k++) {
                            var line = arr[k]
                            if (line > clickedLine + searchRange) break
                            
                            var dist = Math.abs(line - clickedLine)
                            if (dist < minDist) {
                                minDist = dist
                                bestLine = line
                                bestType = type.name
                            }
                        }
                    }
                }
                
                return { line: bestLine, type: bestType }
            }
            
            onClicked: (mouse) => {
                var result = findOriginalLine(mouse.y)
                if (result.line >= 0) {
                     enhancedScrollBar.lineClicked(result.line)
                }
            }
            
            // 用于 Tooltip
            hoverEnabled: true
            property string tooltipText: ""
            
            onPositionChanged: (mouse) => {
                var result = findOriginalLine(mouse.y)
                if (result.line >= 0) {
                    tooltipText = result.type + " " + qsTr("at line") + " " + (result.line + 1)
                } else {
                    tooltipText = ""
                }
            }
            
            ToolTip.visible: tooltipText !== "" && containsMouse
            ToolTip.delay: 200
            ToolTip.text: tooltipText
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
