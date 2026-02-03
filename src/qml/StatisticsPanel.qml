import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.platform as Platform

/**
 * @brief Log Statistics Panel
 * 
 * Pro feature: Display log level distribution and statistics
 */
Dialog {
    id: root
    
    title: qsTr("Log Statistics")
    width: 550
    height: 520
    modal: true
    closePolicy: Popup.CloseOnEscape
    
    // Center in parent
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    
    // Close on reject
    onRejected: close()
    
    // Statistics data
    property var statistics: ({
        total: 0,
        error: 0,
        warn: 0,
        info: 0,
        debug: 0,
        trace: 0,
        other: 0
    })

    // 支持动态绑定不同的模型，默认为全局 _logModel
    property var targetLogModel: _logModel

    // 加载状态
    property bool isLoading: false
    
    // 计算百分比
    function getPercentage(val) {
        return statistics.total > 0 ? ((val / statistics.total) * 100).toFixed(1) : "0.0"
    }
    
    contentItem: ColumnLayout {
        spacing: 12
        
        // Title bar
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: qsTr("File: %1").arg(targetLogModel ? targetLogModel.filePath : "")
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                font.pixelSize: 12
            }
            
            Button {
                text: qsTr("Refresh")
                onClicked: calculateStatistics()
            }
        }
        
        // Overview
        GroupBox {
            title: qsTr("Overview")
            Layout.fillWidth: true
            
            GridLayout {
                anchors.fill: parent
                columns: 4
                columnSpacing: 16
                rowSpacing: 4
                
                Label { text: qsTr("Total Lines:"); font.bold: true }
                Label { text: root.statistics.total.toLocaleString() }
                Label { text: targetLogModel ? formatFileSize(targetLogModel.fileSize) : "0" }
                
                Label { text: qsTr("Current View:"); font.bold: true }
                Label { text: targetLogModel ? targetLogModel.lineCount.toLocaleString() + qsTr(" lines") : "0" }
                
                Label { text: qsTr("Filter Mode:"); font.bold: true }
                Label { 
                    text: targetLogModel && targetLogModel.isFilterMode ? qsTr("Yes") : qsTr("No")
                    color: targetLogModel && targetLogModel.isFilterMode ? "#2196F3" : _themeManager.textColor
                }

            }
        }
        
        // Log level distribution
        GroupBox {
            title: qsTr("Log Level Distribution")
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            RowLayout {
                anchors.fill: parent
                spacing: 16
                
                // Pie chart container
                Rectangle {
                    id: pieChartContainer
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 140
                    color: "transparent"
                    
                    Canvas {
                        id: pieChart
                        anchors.fill: parent
                        
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            
                            var total = root.statistics.error + root.statistics.warn + 
                                        root.statistics.info + root.statistics.debug + 
                                        root.statistics.trace + root.statistics.other
                            
                            var centerX = width / 2
                            var centerY = height / 2
                            var radius = Math.min(width, height) / 2 - 5
                            
                            if (total === 0) {
                                ctx.beginPath()
                                ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI)
                                ctx.fillStyle = "#E0E0E0"
                                ctx.fill()
                                return
                            }
                            
                            var data = [
                                { value: root.statistics.error, color: "#F44336" },
                                { value: root.statistics.warn, color: "#FF9800" },
                                { value: root.statistics.info, color: "#4CAF50" },
                                { value: root.statistics.debug, color: "#2196F3" },
                                { value: root.statistics.trace, color: "#9E9E9E" },
                                { value: root.statistics.other, color: "#607D8B" }
                            ]
                            
                            var startAngle = -Math.PI / 2
                            
                            for (var i = 0; i < data.length; i++) {
                                if (data[i].value === 0) continue
                                
                                var sliceAngle = (data[i].value / total) * 2 * Math.PI
                                
                                ctx.beginPath()
                                ctx.moveTo(centerX, centerY)
                                ctx.arc(centerX, centerY, radius, startAngle, startAngle + sliceAngle)
                                ctx.closePath()
                                ctx.fillStyle = data[i].color
                                ctx.fill()
                                
                                startAngle += sliceAngle
                            }
                        }
                    }
                }
                
                // Legend
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    
                    Repeater {
                        model: [
                            { label: "ERROR", color: "#F44336", key: "error" },
                            { label: "WARN", color: "#FF9800", key: "warn" },
                            { label: "INFO", color: "#4CAF50", key: "info" },
                            { label: "DEBUG", color: "#2196F3", key: "debug" },
                            { label: "TRACE", color: "#9E9E9E", key: "trace" },
                            { label: "OTHER", color: "#607D8B", key: "other" }
                        ]
                        
                        delegate: RowLayout {
                            spacing: 6
                            
                            Rectangle { 
                                width: 12
                                height: 12
                                color: modelData.color
                                radius: 2 
                            }
                            
                            Label { 
                                text: modelData.label
                                font.bold: true
                                font.pixelSize: 12
                                Layout.preferredWidth: 45
                            }
                            
                            Label { 
                                text: {
                                    var val = root.statistics[modelData.key] || 0
                                    return val.toLocaleString() + " (" + getPercentage(val) + "%)"
                                }
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
        
        // Bar chart
        GroupBox {
            id: barChartGroup
            title: qsTr("Level Comparison")
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            
            Canvas {
                id: barChart
                anchors.fill: parent
                
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    
                    var data = [
                        { value: root.statistics.error, color: "#F44336", label: "E" },
                        { value: root.statistics.warn, color: "#FF9800", label: "W" },
                        { value: root.statistics.info, color: "#4CAF50", label: "I" },
                        { value: root.statistics.debug, color: "#2196F3", label: "D" },
                        { value: root.statistics.trace, color: "#9E9E9E", label: "T" }
                    ]
                    
                    var maxValue = Math.max(root.statistics.error, root.statistics.warn,
                                            root.statistics.info, root.statistics.debug,
                                            root.statistics.trace, 1)
                    
                    var barWidth = (width - 20) / data.length - 6
                    var maxHeight = height - 20
                    
                    for (var i = 0; i < data.length; i++) {
                        var barHeight = Math.max((data[i].value / maxValue) * maxHeight, 2)
                        var x = 10 + i * (barWidth + 6)
                        var y = maxHeight - barHeight
                        
                        ctx.fillStyle = data[i].color
                        ctx.fillRect(x, y, barWidth, barHeight)
                        
                        ctx.fillStyle = _themeManager.isDarkTheme ? "#CCC" : "#666"
                        ctx.font = "10px sans-serif"
                        ctx.textAlign = "center"
                        ctx.fillText(data[i].label, x + barWidth / 2, height - 4)
                    }
                }
            }
        }
        
        // Bottom buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            
            Button {
                text: qsTr("Export Pie Chart")
                onClicked: pieChartSaveDialog.open()
            }
            
            Button {
                text: qsTr("Export Bar Chart")
                onClicked: barChartSaveDialog.open()
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }

    function calculateStatistics() {
        if (!targetLogModel) return
        
        var errorCount = 0
        var warnCount = 0
        var infoCount = 0
        var debugCount = 0
        var traceCount = 0
        
        // Use getLogStatistics if available, otherwise fall back to line methods
        if (typeof targetLogModel.getLogStatistics === "function") {
            var stats = targetLogModel.getLogStatistics()
            errorCount = stats.error || 0
            warnCount = stats.warn || 0
            infoCount = stats.info || 0
            debugCount = stats.debug || 0
            traceCount = stats.trace || 0
        } else {
            // Try errorLines, warningLines, infoLines methods
            if (typeof targetLogModel.errorLines === "function") {
                var errorLines = targetLogModel.errorLines()
                errorCount = Array.isArray(errorLines) ? errorLines.length : 0
            }
            if (typeof targetLogModel.warningLines === "function") {
                var warnLines = targetLogModel.warningLines()
                warnCount = Array.isArray(warnLines) ? warnLines.length : 0
            }
            if (typeof targetLogModel.infoLines === "function") {
                var infoLines = targetLogModel.infoLines()
                infoCount = Array.isArray(infoLines) ? infoLines.length : 0
            }
        }
        
        var total = targetLogModel.totalLineCount || targetLogModel.lineCount || 0
        var otherCount = Math.max(0, total - errorCount - warnCount - infoCount - debugCount - traceCount)
        
        root.statistics = {
            total: total,
            error: errorCount,
            warn: warnCount,
            info: infoCount,
            debug: debugCount,
            trace: traceCount,
            other: otherCount
        }
        
        isLoading = false
        pieChart.requestPaint()
        barChart.requestPaint()
    }
    
    // 延迟加载定时器
    Timer {
        id: loadTimer
        interval: 150  // 延迟150ms让弹出动画完成
        running: false
        repeat: false
        onTriggered: calculateStatistics()
    }
    
    onOpened: {
        isLoading = true
        loadTimer.start()
    }
    
    // 导出饼状图为图片
    function exportPieChart(filePath) {
        // 创建临时画布来绘制带背景和图例的完整图片
        var exportWidth = 320
        var exportHeight = 200
        
        pieChart.grabToImage(function(result) {
            // 使用canvas自带的toDataURL或直接保存
            result.saveToFile(filePath)
            console.log("Pie chart exported to:", filePath)
        }, Qt.size(exportWidth, exportHeight))
    }
    
    // 导出柱状图为图片
    function exportBarChart(filePath) {
        var exportWidth = 400
        var exportHeight = 150
        
        barChart.grabToImage(function(result) {
            result.saveToFile(filePath)
            console.log("Bar chart exported to:", filePath)
        }, Qt.size(exportWidth, exportHeight))
    }
}
