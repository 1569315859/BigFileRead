import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.platform as Platform

/**
 * DashboardPanel.qml - 日志分析仪表盘面板
 * 
 * 功能:
 * - 按时间段统计日志（每分钟/小时/天）
 * - 多种图表类型切换（堆叠柱状图、分组柱状图、面积图）
 * - 日志级别分布概览
 * - 时间范围显示
 */
Popup {
    id: root
    modal: true
    width: Math.min(parent.width * 0.9, 1000)
    height: Math.min(parent.height * 0.85, 700)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape
    
    // 主题颜色
    property color textColor: _themeManager ? _themeManager.textColor : "#ffffff"
    property color panelColor: _themeManager ? _themeManager.panelBackground : "#252526"
    property color bgColor: _themeManager ? _themeManager.backgroundColor : "#1e1e1e"
    property color accentColor: _themeManager ? _themeManager.accentColor : "#007ACC"
    property color borderColor: _themeManager ? _themeManager.borderColor : "#3c3c3c"
    
    // 数据
    property var timeData: []
    property var levelStats: ({})
    property var timeRange: ({})
    
    // 支持动态绑定不同的模型，默认为全局 _logModel
    property var targetLogModel: _logModel

    // 当前设置
    property string currentInterval: "hour"  // "minute", "hour", "day"
    property string currentChartType: "stacked"  // "stacked", "grouped", "area"
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        border.width: 1
        radius: 8
    }
    
    // 加载数据
    function loadData() {
        if (!targetLogModel || !targetLogModel.filePath) return
        
        isLoading = false
        
        // 获取时间统计
        timeData = targetLogModel.getTimeBasedStatistics(currentInterval, 50)
        
        // 获取级别统计
        levelStats = targetLogModel.getLogStatistics()
        
        // 获取时间范围
        timeRange = targetLogModel.getLogTimeRange()
    }
    
    // 加载状态
    property bool isLoading: false
    
    // 延迟加载定时器
    Timer {
        id: loadTimer
        interval: 150  // 延迟150ms让弹出动画完全完成
        running: false
        repeat: false
        onTriggered: loadData()
    }
    
    onOpened: {
        isLoading = true
        loadTimer.start()
    }
    
    contentItem: ColumnLayout {
        spacing: 12
        
        // ============ 标题栏 ============
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            
            Text {
                text: "📊 " + qsTr("Log Analytics Dashboard")
                font.bold: true
                font.pixelSize: 18
                color: textColor
            }
            
            Item { Layout.fillWidth: true }
            
            // 时间范围显示
            Text {
                text: {
                    if (timeRange && timeRange.startTime && timeRange.endTime) {
                        var start = timeRange.startTime
                        var end = timeRange.endTime
                        if (start && end) {
                            return qsTr("Time Range: ") + 
                                   Qt.formatDateTime(start, "yyyy-MM-dd HH:mm") + " ~ " +
                                   Qt.formatDateTime(end, "yyyy-MM-dd HH:mm")
                        }
                    }
                    return ""
                }
                color: Qt.darker(textColor, 1.3)
                font.pixelSize: 12
                visible: text.length > 0
            }
            
            Button {
                text: "✕"
                implicitWidth: 30
                implicitHeight: 30
                onClicked: root.close()
                background: Rectangle {
                    color: parent.hovered ? "#e81123" : "transparent"
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.hovered ? "white" : textColor
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        
        // ============ 控制栏 ============
        RowLayout {
            Layout.fillWidth: true
            spacing: 20
            
            // 时间间隔选择
            RowLayout {
                spacing: 8
                
                Label {
                    text: qsTr("Time Interval:")
                    color: textColor
                    font.pixelSize: 13
                }
                
                ComboBox {
                    id: intervalCombo
                    model: [
                        { text: qsTr("Per Minute"), value: "minute" },
                        { text: qsTr("Per Hour"), value: "hour" },
                        { text: qsTr("Per Day"), value: "day" }
                    ]
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: 1
                    implicitWidth: 130
                    implicitHeight: 30
                    
                    onCurrentValueChanged: {
                        currentInterval = currentValue
                        loadData()
                    }
                    
                    background: Rectangle {
                        color: bgColor
                        border.color: borderColor
                        radius: 4
                    }
                    
                    contentItem: Text {
                        text: intervalCombo.displayText
                        color: textColor
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                }
            }
            
            // 图表类型选择
            RowLayout {
                spacing: 8
                
                Label {
                    text: qsTr("Chart Type:")
                    color: textColor
                    font.pixelSize: 13
                }
                
                ButtonGroup {
                    id: chartTypeGroup
                }
                
                Button {
                    text: qsTr("Stacked")
                    checkable: true
                    checked: currentChartType === "stacked"
                    ButtonGroup.group: chartTypeGroup
                    implicitHeight: 28
                    implicitWidth: 70
                    onClicked: currentChartType = "stacked"
                    
                    background: Rectangle {
                        color: parent.checked ? accentColor : (parent.hovered ? Qt.lighter(panelColor, 1.2) : panelColor)
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "white" : textColor
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Button {
                    text: qsTr("Grouped")
                    checkable: true
                    checked: currentChartType === "grouped"
                    ButtonGroup.group: chartTypeGroup
                    implicitHeight: 28
                    implicitWidth: 70
                    onClicked: currentChartType = "grouped"
                    
                    background: Rectangle {
                        color: parent.checked ? accentColor : (parent.hovered ? Qt.lighter(panelColor, 1.2) : panelColor)
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "white" : textColor
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Button {
                    text: qsTr("Area")
                    checkable: true
                    checked: currentChartType === "area"
                    ButtonGroup.group: chartTypeGroup
                    implicitHeight: 28
                    implicitWidth: 70
                    onClicked: currentChartType = "area"
                    
                    background: Rectangle {
                        color: parent.checked ? accentColor : (parent.hovered ? Qt.lighter(panelColor, 1.2) : panelColor)
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "white" : textColor
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            // 导出按钮
            Button {
                text: "📷 " + qsTr("Export")
                implicitHeight: 30
                implicitWidth: 90
                onClicked: chartExportDialog.open()
                
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.lighter(panelColor, 1.2) : panelColor)
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
            
            // 刷新按钮
            Button {
                text: "🔄 " + qsTr("Refresh")
                implicitHeight: 30
                implicitWidth: 90
                onClicked: loadData()
                
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
        }
        
        // ============ 分隔线 ============
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: borderColor
        }
        
        // ============ 主内容区域 ============
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            
            // 时间序列图表（左侧，占主要空间）
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: bgColor
                border.color: borderColor
                radius: 6
                
                TimeSeriesChart {
                    id: mainChart
                    anchors.fill: parent
                    anchors.margins: 10
                    chartData: timeData
                    chartType: currentChartType
                    title: qsTr("Log Volume Over Time")
                    visible: !isLoading
                    
                    onBarClicked: (index, dataItem) => {
                        console.log("Clicked:", dataItem.timeLabel, "Total:", dataItem.total)
                    }
                }
                
                // 加载中提示
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12
                    visible: isLoading
                    
                    BusyIndicator {
                        running: isLoading
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 48
                        implicitHeight: 48
                    }
                    
                    Text {
                        text: qsTr("Loading data...")
                        color: Qt.darker(textColor, 1.3)
                        font.pixelSize: 13
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
            
            // 右侧统计面板
            Rectangle {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                color: bgColor
                border.color: borderColor
                radius: 6
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    
                    // 标题
                    Text {
                        text: qsTr("Level Distribution")
                        font.bold: true
                        font.pixelSize: 14
                        color: textColor
                    }
                    
                    // 级别统计条
                    Repeater {
                        model: [
                            { name: "Error", key: "error", color: "#F44336" },
                            { name: "Warn", key: "warn", color: "#FF9800" },
                            { name: "Info", key: "info", color: "#2196F3" },
                            { name: "Debug", key: "debug", color: "#9C27B0" },
                            { name: "Trace", key: "trace", color: "#4CAF50" },
                            { name: "Other", key: "other", color: "#607D8B" }
                        ]
                        
                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            
                            property int levelCount: levelStats ? (levelStats[modelData.key] || 0) : 0
                            property real percentage: {
                                if (!levelStats) return 0
                                var total = (levelStats.error || 0) + (levelStats.warn || 0) + 
                                           (levelStats.info || 0) + (levelStats.debug || 0) + 
                                           (levelStats.trace || 0) + (levelStats.other || 0)
                                return total > 0 ? (levelCount / total * 100) : 0
                            }
                            
                            RowLayout {
                                Layout.fillWidth: true
                                
                                Rectangle {
                                    width: 12
                                    height: 12
                                    radius: 2
                                    color: modelData.color
                                }
                                
                                Text {
                                    text: modelData.name
                                    color: textColor
                                    font.pixelSize: 12
                                }
                                
                                Item { Layout.fillWidth: true }
                                
                                Text {
                                    text: levelCount.toLocaleString() + " (" + percentage.toFixed(1) + "%)"
                                    color: Qt.darker(textColor, 1.3)
                                    font.pixelSize: 11
                                }
                            }
                            
                            // 进度条
                            Rectangle {
                                Layout.fillWidth: true
                                height: 6
                                radius: 3
                                color: Qt.darker(panelColor, 1.2)
                                
                                Rectangle {
                                    width: parent.width * (percentage / 100)
                                    height: parent.height
                                    radius: 3
                                    color: modelData.color
                                    
                                    Behavior on width {
                                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                                    }
                                }
                            }
                        }
                    }
                    
                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: borderColor
                    }
                    
                    // 总计
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Text {
                            text: qsTr("Total Lines:")
                            font.bold: true
                            color: textColor
                            font.pixelSize: 13
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Text {
                            text: targetLogModel ? targetLogModel.totalLineCount.toLocaleString() : "0"
                            font.bold: true
                            color: accentColor
                            font.pixelSize: 14
                        }
                    }
                    
                    // 当前过滤
                    RowLayout {
                        Layout.fillWidth: true
                        visible: targetLogModel && targetLogModel.isFilterMode
                        
                        Text {
                            text: qsTr("Filtered:")
                            color: textColor
                            font.pixelSize: 12
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Text {
                            text: targetLogModel ? targetLogModel.lineCount.toLocaleString() : "0"
                            color: "#FF9800"
                            font.pixelSize: 13
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                    
                    // 数据点数量提示
                    Text {
                        text: qsTr("Data Points: ") + timeData.length
                        color: Qt.darker(textColor, 1.5)
                        font.pixelSize: 10
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }
    }
    
    // 图表导出保存对话框
    FileDialog {
        id: chartExportDialog
        title: qsTr("Export Dashboard Chart")
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG Image (*.png)"]
        currentFolder: Platform.StandardPaths.writableLocation(Platform.StandardPaths.PicturesLocation)
        
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            if (!path.toLowerCase().endsWith(".png")) {
                path += ".png"
            }
            exportChart(path)
        }
    }
    
    // 导出图表为图片
    function exportChart(filePath) {
        mainChart.grabToImage(function(result) {
            result.saveToFile(filePath)
            console.log("Dashboard chart exported to:", filePath)
        }, Qt.size(800, 500))
    }
}
