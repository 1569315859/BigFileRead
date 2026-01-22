import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief Log Statistics Panel
 * 
 * Pro feature: Display log level distribution and statistics
 */
Dialog {
    id: root
    
    title: qsTr("Log Statistics")
    width: 550
    height: 480
    modal: true
    
    // Center in parent
    anchors.centerIn: parent
    
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
    
    contentItem: ColumnLayout {
        spacing: 12
        
        // Title bar
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: qsTr("File: %1").arg(_logModel ? _logModel.filePath : "")
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
                
                Label { text: qsTr("File Size:"); font.bold: true }
                Label { text: _logModel ? formatFileSize(_logModel.fileSize) : "0" }
                
                Label { text: qsTr("Current View:"); font.bold: true }
                Label { text: _logModel ? _logModel.lineCount.toLocaleString() + qsTr(" lines") : "0" }
                
                Label { text: qsTr("Filter Mode:"); font.bold: true }
                Label { 
                    text: _logModel && _logModel.isFilterMode ? qsTr("Yes") : qsTr("No")
                    color: _logModel && _logModel.isFilterMode ? "#2196F3" : _themeManager.textColor
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
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
    
    function formatFileSize(bytes) {
        if (!bytes) return "0 B"
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GB"
    }
    
    function getPercentage(value) {
        var total = root.statistics.error + root.statistics.warn + 
                    root.statistics.info + root.statistics.debug + 
                    root.statistics.trace + root.statistics.other
        if (total === 0) return "0.0"
        return ((value / total) * 100).toFixed(1)
    }
    
    function calculateStatistics() {
        if (!_logModel) return
        
        var errorCount = 0
        var warnCount = 0
        var infoCount = 0
        var debugCount = 0
        var traceCount = 0
        
        // Use getLogStatistics if available, otherwise fall back to line methods
        if (typeof _logModel.getLogStatistics === "function") {
            var stats = _logModel.getLogStatistics()
            errorCount = stats.error || 0
            warnCount = stats.warn || 0
            infoCount = stats.info || 0
            debugCount = stats.debug || 0
            traceCount = stats.trace || 0
        } else {
            // Try errorLines, warningLines, infoLines methods
            if (typeof _logModel.errorLines === "function") {
                var errorLines = _logModel.errorLines()
                errorCount = Array.isArray(errorLines) ? errorLines.length : 0
            }
            if (typeof _logModel.warningLines === "function") {
                var warnLines = _logModel.warningLines()
                warnCount = Array.isArray(warnLines) ? warnLines.length : 0
            }
            if (typeof _logModel.infoLines === "function") {
                var infoLines = _logModel.infoLines()
                infoCount = Array.isArray(infoLines) ? infoLines.length : 0
            }
        }
        
        var total = _logModel.totalLineCount || _logModel.lineCount || 0
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
        
        pieChart.requestPaint()
        barChart.requestPaint()
    }
    
    onOpened: {
        calculateStatistics()
    }
}
