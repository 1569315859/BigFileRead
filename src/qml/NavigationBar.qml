import QtQuick
import QtQuick.Controls

/**
 * 侧边导航条
 * 类似 VS Code 的 minimap 滚动条，显示关键字/搜索结果在文档中的位置
 * 点击可快速跳转到对应位置
 */
Rectangle {
    id: navBar
    
    // 必需属性
    property int totalLines: 1           // 总行数
    property int visibleStartLine: 0     // 可见区域起始行
    property int visibleLineCount: 30    // 可见区域行数
    property var markerData: []          // 标记数据: [{line: 0, color: "#FF0000", type: "error"}, ...]
    property var bookmarks: []           // 书签行号数组
    property var searchResults: []       // 搜索结果行号数组
    property string searchColor: "#FFFF00"  // 搜索结果颜色
    property string bookmarkColor: "#007ACC" // 书签颜色
    
    // 主题颜色
    property color bgColor: _themeManager.panelBackground
    property color borderColor: _themeManager.borderColor
    property color textColor: _themeManager.textColor
    
    // 信号
    signal lineClicked(int lineNumber)
    
    color: Qt.darker(bgColor, 1.05)
    border.color: borderColor
    border.width: 1
    
    // 计算缩放比例
    readonly property real lineToPixelRatio: totalLines > 0 ? (height - 4) / totalLines : 1
    
    // 可见区域指示器
    Rectangle {
        id: visibleIndicator
        x: 2
        y: Math.max(2, visibleStartLine * navBar.lineToPixelRatio + 2)
        width: parent.width - 4
        height: Math.max(10, Math.min(visibleLineCount * navBar.lineToPixelRatio, parent.height - y - 2))
        color: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.1)
        border.color: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.3)
        border.width: 1
        radius: 2
        
        Behavior on y { NumberAnimation { duration: 50 } }
        Behavior on height { NumberAnimation { duration: 50 } }
    }
    
    // 标记容器
    Item {
        anchors.fill: parent
        anchors.margins: 2
        clip: true
        
        // 关键字标记
        Repeater {
            model: markerData
            
            Rectangle {
                x: 0
                y: modelData.line * navBar.lineToPixelRatio
                width: parent.width
                height: Math.max(2, navBar.lineToPixelRatio)
                color: modelData.color
                opacity: 0.8
                
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -2  // 增大点击区域
                    cursorShape: Qt.PointingHandCursor
                    onClicked: navBar.lineClicked(modelData.line)
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 300
                    ToolTip.text: qsTr("Line") + " " + (modelData.line + 1) + ": " + (modelData.keyword || modelData.type)
                    
                    hoverEnabled: true
                }
            }
        }
        
        // 书签标记（蓝色三角形）
        Repeater {
            model: bookmarks
            
            Canvas {
                x: 0
                y: modelData * navBar.lineToPixelRatio - 3
                width: 8
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
                
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -2
                    cursorShape: Qt.PointingHandCursor
                    onClicked: navBar.lineClicked(modelData)
                    hoverEnabled: true
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 300
                    ToolTip.text: qsTr("Bookmark at line") + " " + (modelData + 1)
                }
            }
        }
        
        // 搜索结果标记（黄色短横线）
        Repeater {
            model: searchResults
            
            Rectangle {
                x: parent.width - 6
                y: modelData * navBar.lineToPixelRatio
                width: 4
                height: Math.max(2, navBar.lineToPixelRatio)
                radius: 1
                color: searchColor
                opacity: 0.9
                
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -2
                    cursorShape: Qt.PointingHandCursor
                    onClicked: navBar.lineClicked(modelData)
                    hoverEnabled: true
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 300
                    ToolTip.text: qsTr("Search result at line") + " " + (modelData + 1)
                }
            }
        }
    }
    
    // 点击跳转
    MouseArea {
        anchors.fill: parent
        z: -1  // 放在标记后面
        onClicked: (mouse) => {
            var targetLine = Math.floor((mouse.y - 2) / navBar.lineToPixelRatio)
            targetLine = Math.max(0, Math.min(targetLine, totalLines - 1))
            navBar.lineClicked(targetLine)
        }
    }
    
    // 图例
    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 3
        visible: parent.width >= 25
        
        Row {
            spacing: 3
            Rectangle { width: 6; height: 6; color: searchColor; radius: 1 }
            Text { text: qsTr("S"); color: textColor; font.pixelSize: 8; opacity: 0.7 }
        }
        Row {
            spacing: 3
            Rectangle { width: 6; height: 6; color: bookmarkColor; radius: 1 }
            Text { text: qsTr("B"); color: textColor; font.pixelSize: 8; opacity: 0.7 }
        }
    }
}
