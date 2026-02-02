/**
 * DiffView.qml
 * 分屏差异对比视图
 * 支持：左右分屏显示、差异高亮、同步滚动、逐个差异跳转
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    
    // 公开属性
    property var diffs: []          // 差异数据列表
    property var stats: ({})        // 统计信息
    property string leftTitle: qsTr("Original")
    property string rightTitle: qsTr("Modified")
    property bool syncScroll: true  // 同步滚动
    property bool showLineNumbers: true
    property int currentDiffIndex: -1
    
    // 主题颜色（从父组件传入）
    property color bgColor: "#1E1E1E"
    property color panelColor: "#252526"
    property color textColor: "#D4D4D4"
    property color borderColor: "#3C3C3C"
    
    // 信号
    signal diffSelected(int index)
    signal exportRequested(string format)
    
    // 差异颜色定义（自动适应深浅主题）
    readonly property bool isDarkTheme: bgColor.r < 0.5
    readonly property color addedColor: isDarkTheme ? "#1d4428" : "#e6ffec"
    readonly property color deletedColor: isDarkTheme ? "#5c2323" : "#ffebe9"
    readonly property color modifiedColor: isDarkTheme ? "#5c4a1d" : "#fff3cd"
    readonly property color unchangedColor: "transparent"
    readonly property color addedTextColor: isDarkTheme ? "#7ee787" : "#1a7f37"
    readonly property color deletedTextColor: isDarkTheme ? "#f97583" : "#cf222e"
    readonly property color modifiedTextColor: isDarkTheme ? "#e3b341" : "#9a6700"
    
    // 工具栏
    Rectangle {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 40
        color: panelColor
        border.color: borderColor
        border.width: 1
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 12
            
            // 统计信息
            Label {
                text: {
                    if (!stats) return ""
                    return qsTr("Added: %1 | Deleted: %2 | Modified: %3 | Similarity: %4%")
                        .arg(stats.addedLines || 0)
                        .arg(stats.deletedLines || 0)
                        .arg(stats.modifiedLines || 0)
                        .arg(((stats.similarity || 0) * 100).toFixed(1))
                }
                font.pixelSize: 12
                color: textColor
            }
            
            Item { Layout.fillWidth: true }
            
            // 导航按钮
            Button {
                text: "◀"
                implicitWidth: 32
                implicitHeight: 28
                onClicked: navigateDiff(-1)
                ToolTip.text: qsTr("Previous difference")
                ToolTip.visible: hovered
                enabled: currentDiffIndex > 0
            }
            
            Label {
                text: currentDiffIndex >= 0 ? 
                    qsTr("%1 / %2").arg(currentDiffIndex + 1).arg(getDiffCount()) : 
                    qsTr("No diff")
                font.pixelSize: 12
                color: textColor
            }
            
            Button {
                text: "▶"
                implicitWidth: 32
                implicitHeight: 28
                onClicked: navigateDiff(1)
                ToolTip.text: qsTr("Next difference")
                ToolTip.visible: hovered
                enabled: currentDiffIndex < getDiffCount() - 1
            }
            
            Rectangle { width: 1; height: 20; color: borderColor }
            
            // 同步滚动开关
            CheckBox {
                id: syncScrollCheck
                text: qsTr("Sync Scroll")
                checked: syncScroll
                onCheckedChanged: syncScroll = checked
            }
            
            // 导出按钮
            Button {
                text: qsTr("Export")
                implicitHeight: 28
                onClicked: exportMenu.open()
                
                Menu {
                    id: exportMenu
                    MenuItem { 
                        text: qsTr("Export as HTML")
                        onTriggered: exportRequested("html")
                    }
                    MenuItem { 
                        text: qsTr("Export as Patch")
                        onTriggered: exportRequested("patch")
                    }
                    MenuItem { 
                        text: qsTr("Export as Text")
                        onTriggered: exportRequested("text")
                    }
                }
            }
        }
    }
    
    // 标题栏
    Row {
        id: headerRow
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 32
        
        Rectangle {
            width: parent.width / 2
            height: parent.height
            color: panelColor
            border.color: borderColor
            
            Label {
                anchors.centerIn: parent
                text: leftTitle
                font.bold: true
                color: textColor
            }
        }
        
        Rectangle {
            width: parent.width / 2
            height: parent.height
            color: panelColor
            border.color: borderColor
            
            Label {
                anchors.centerIn: parent
                text: rightTitle
                font.bold: true
                color: textColor
            }
        }
    }
    
    // 分屏内容
    Row {
        id: contentRow
        anchors.top: headerRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        
        // 左侧面板
        DiffPanel {
            id: leftPanel
            width: parent.width / 2
            height: parent.height
            side: "left"
            diffs: root.diffs
            showLineNumbers: root.showLineNumbers
            currentDiffIndex: root.currentDiffIndex
            bgColor: root.bgColor
            textColor: root.textColor
            borderColor: root.borderColor
            addedColor: root.addedColor
            deletedColor: root.deletedColor
            modifiedColor: root.modifiedColor
            addedTextColor: root.addedTextColor
            deletedTextColor: root.deletedTextColor
            modifiedTextColor: root.modifiedTextColor
            
            onScrollPositionChanged: {
                if (syncScroll && !rightPanel.isScrolling) {
                    rightPanel.setScrollPosition(position)
                }
            }
            
            onDiffClicked: function(index) {
                root.currentDiffIndex = index
                root.diffSelected(index)
            }
        }
        
        // 分隔线
        Rectangle {
            width: 2
            height: parent.height
            color: borderColor
        }
        
        // 右侧面板
        DiffPanel {
            id: rightPanel
            width: parent.width / 2 - 2
            height: parent.height
            side: "right"
            diffs: root.diffs
            showLineNumbers: root.showLineNumbers
            currentDiffIndex: root.currentDiffIndex
            bgColor: root.bgColor
            textColor: root.textColor
            borderColor: root.borderColor
            addedColor: root.addedColor
            deletedColor: root.deletedColor
            modifiedColor: root.modifiedColor
            addedTextColor: root.addedTextColor
            deletedTextColor: root.deletedTextColor
            modifiedTextColor: root.modifiedTextColor
            
            onScrollPositionChanged: {
                if (syncScroll && !leftPanel.isScrolling) {
                    leftPanel.setScrollPosition(position)
                }
            }
            
            onDiffClicked: function(index) {
                root.currentDiffIndex = index
                root.diffSelected(index)
            }
        }
    }
    
    // 辅助函数
    function getDiffCount() {
        var count = 0
        for (var i = 0; i < diffs.length; i++) {
            if (diffs[i].type !== 0) count++  // 0 = Unchanged
        }
        return count
    }
    
    function navigateDiff(direction) {
        var diffIndices = []
        for (var i = 0; i < diffs.length; i++) {
            if (diffs[i].type !== 0) {
                diffIndices.push(i)
            }
        }
        
        if (diffIndices.length === 0) return
        
        var currentPos = diffIndices.indexOf(currentDiffIndex)
        var newPos = currentPos + direction
        
        if (newPos < 0) newPos = 0
        if (newPos >= diffIndices.length) newPos = diffIndices.length - 1
        
        currentDiffIndex = diffIndices[newPos]
        
        // 滚动到差异位置
        leftPanel.scrollToLine(diffs[currentDiffIndex].leftLine)
        rightPanel.scrollToLine(diffs[currentDiffIndex].rightLine)
    }
    
    function scrollToLine(lineNumber) {
        leftPanel.scrollToLine(lineNumber)
        rightPanel.scrollToLine(lineNumber)
    }

    /**
     * 单侧差异面板组件
     */
    component DiffPanel: Rectangle {
    id: panel
    
    property string side: "left"  // "left" or "right"
    property var diffs: []
    property bool showLineNumbers: true
    property int currentDiffIndex: -1
    property bool isScrolling: false
    
    // 主题颜色
    property color bgColor: "#1E1E1E"
    property color textColor: "#D4D4D4"
    property color borderColor: "#3C3C3C"
    property color addedColor: "#1d4428"
    property color deletedColor: "#5c2323"
    property color modifiedColor: "#5c4a1d"
    property color addedTextColor: "#7ee787"
    property color deletedTextColor: "#f97583"
    property color modifiedTextColor: "#e3b341"
    
    signal scrollPositionChanged(real position)
    signal diffClicked(int index)
    
    color: bgColor
    border.color: borderColor
    
    ListView {
        id: listView
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        
        model: diffs
        
        delegate: Rectangle {
            id: lineDelegate
            width: listView.width
            height: lineText.implicitHeight + 8
            
            property var diffData: modelData
            property int diffType: diffData ? diffData.type : 0
            property bool isCurrentDiff: index === currentDiffIndex
            
            // 背景色根据差异类型
            color: {
                if (isCurrentDiff) return Qt.lighter(getTypeColor(), 1.1)
                return getTypeColor()
            }
            
            border.color: isCurrentDiff ? "#0969da" : "transparent"
            border.width: isCurrentDiff ? 2 : 0
            
            function getTypeColor() {
                switch (diffType) {
                    case 1: return panel.addedColor      // Added
                    case 2: return panel.deletedColor    // Deleted
                    case 3: return panel.modifiedColor   // Modified
                    default: return panel.bgColor        // Unchanged
                }
            }
            
            function getTextColor() {
                switch (diffType) {
                    case 1: return panel.addedTextColor
                    case 2: return panel.deletedTextColor
                    case 3: return panel.modifiedTextColor
                    default: return panel.textColor
                }
            }
            
            Row {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: 8
                
                // 行号
                Label {
                    visible: showLineNumbers
                    width: 50
                    height: parent.height
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    text: {
                        var lineNum = side === "left" ? diffData.leftLine : diffData.rightLine
                        return lineNum >= 0 ? (lineNum + 1).toString() : ""
                    }
                    font.family: "Consolas, Monaco, monospace"
                    font.pixelSize: 12
                    color: Qt.darker(panel.textColor, 1.3)
                }
                
                // 差异标记
                Label {
                    width: 16
                    height: parent.height
                    horizontalAlignment: Text.AlignCenter
                    verticalAlignment: Text.AlignVCenter
                    text: {
                        switch (diffType) {
                            case 1: return "+"
                            case 2: return "-"
                            case 3: return "~"
                            default: return ""
                        }
                    }
                    font.bold: true
                    color: lineDelegate.getTextColor()
                }
                
                // 内容文本
                Label {
                    id: lineText
                    width: parent.width - (showLineNumbers ? 74 : 24)
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: {
                        if (!diffData) return ""
                        if (side === "left") {
                            return diffData.leftContent || ""
                        } else {
                            return diffData.rightContent || ""
                        }
                    }
                    font.family: "Consolas, Monaco, monospace"
                    font.pixelSize: 13
                    color: lineDelegate.getTextColor()
                    elide: Text.ElideRight
                }
            }
            
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (diffType !== 0) {
                        diffClicked(index)
                    }
                }
            }
        }
        
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }
        
        onContentYChanged: {
            if (!isScrolling) {
                isScrolling = true
                var position = contentY / (contentHeight - height)
                scrollPositionChanged(position)
                isScrolling = false
            }
        }
    }
    
    function setScrollPosition(position) {
        if (!isScrolling) {
            isScrolling = true
            listView.contentY = position * (listView.contentHeight - listView.height)
            isScrolling = false
        }
    }
    
    function scrollToLine(lineNumber) {
        if (lineNumber >= 0 && lineNumber < diffs.length) {
            listView.positionViewAtIndex(lineNumber, ListView.Center)
        }
    }
}

} // End of root Item
