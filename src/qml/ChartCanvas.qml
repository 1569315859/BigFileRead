import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * ChartCanvas.qml - 通用 Canvas 自绘图表组件
 * 
 * 支持图表类型:
 * - bar: 柱状图
 * - pie: 饼图  
 * - line: 折线图
 * - area: 面积图
 * - histogram: 直方图
 * - scatter: 散点图
 */
Item {
    id: root
    
    // ========== 公共属性 ==========
    
    /// 图表类型
    property string chartType: "bar"  // bar, pie, line, area, histogram, scatter
    
    /// 图表标题
    property string title: ""
    
    /// 数据数组
    /// 格式取决于图表类型:
    /// - bar/pie: [{ label: "A", value: 10, color: "#FF0000" }, ...]
    /// - line/area: [{ x: 0, y: 10 }, { x: 1, y: 20 }, ...]
    /// - histogram: [10, 20, 15, 8, ...] 原始数值数组
    /// - scatter: [{ x: 1, y: 2 }, { x: 3, y: 5 }, ...]
    property var data: []
    
    /// 配置
    property int bins: 10  // 直方图分箱数
    property bool showGrid: true
    property bool showLabels: true
    property bool showValues: true
    property bool showLegend: false
    property bool animate: true
    
    /// 边距
    property int marginLeft: 50
    property int marginRight: 20
    property int marginTop: 40
    property int marginBottom: 40
    
    /// 颜色
    property color bgColor: "transparent"
    property color gridColor: "#3C3C3C"
    property color textColor: "#D4D4D4"
    property color accentColor: "#007ACC"
    property var colorPalette: [
        "#4CAF50", "#2196F3", "#FF9800", "#E91E63", 
        "#9C27B0", "#00BCD4", "#FFEB3B", "#795548",
        "#607D8B", "#8BC34A", "#3F51B5", "#FF5722"
    ]
    
    /// 散点图配置
    property int pointRadius: 4
    property color pointColor: accentColor
    property real pointOpacity: 0.7
    
    /// 直方图配置
    property color barColor: accentColor
    property real barOpacity: 0.8
    property int barSpacing: 2
    
    // ========== 信号 ==========
    signal pointClicked(int index, var dataPoint)
    signal barClicked(int index, var dataPoint)
    
    // ========== 内部状态 ==========
    property var processedData: []
    property real animationProgress: 0
    
    onDataChanged: {
        processData()
        if (animate) {
            animationProgress = 0
            animationTimer.start()
        } else {
            animationProgress = 1
        }
        chartCanvas.requestPaint()
    }
    
    onChartTypeChanged: {
        processData()
        chartCanvas.requestPaint()
    }
    
    Timer {
        id: animationTimer
        interval: 16
        repeat: true
        onTriggered: {
            animationProgress += 0.05
            if (animationProgress >= 1) {
                animationProgress = 1
                stop()
            }
            chartCanvas.requestPaint()
        }
    }
    
    // ========== 数据处理 ==========
    function processData() {
        if (!data || data.length === 0) {
            processedData = []
            return
        }
        
        switch (chartType) {
            case "histogram":
                processedData = calculateHistogram(data, bins)
                break
            case "scatter":
            case "line":
            case "area":
                // 计算数据范围
                processedData = {
                    points: data,
                    xMin: Math.min(...data.map(p => p.x)),
                    xMax: Math.max(...data.map(p => p.x)),
                    yMin: Math.min(...data.map(p => p.y)),
                    yMax: Math.max(...data.map(p => p.y))
                }
                // 添加一些边距
                var yRange = processedData.yMax - processedData.yMin
                processedData.yMin -= yRange * 0.05
                processedData.yMax += yRange * 0.05
                break
            default:
                processedData = data
        }
    }
    
    function calculateHistogram(values, numBins) {
        if (values.length === 0) return []
        
        var min = Math.min(...values)
        var max = Math.max(...values)
        var binWidth = (max - min) / numBins
        
        if (binWidth === 0) binWidth = 1
        
        var bins = []
        for (var i = 0; i < numBins; i++) {
            bins.push({
                start: min + i * binWidth,
                end: min + (i + 1) * binWidth,
                count: 0
            })
        }
        
        for (var j = 0; j < values.length; j++) {
            var val = values[j]
            var binIndex = Math.floor((val - min) / binWidth)
            if (binIndex >= numBins) binIndex = numBins - 1
            if (binIndex < 0) binIndex = 0
            bins[binIndex].count++
        }
        
        return {
            bins: bins,
            maxCount: Math.max(...bins.map(b => b.count)),
            min: min,
            max: max,
            binWidth: binWidth
        }
    }
    
    // ========== Canvas 绑定 ==========
    Canvas {
        id: chartCanvas
        anchors.fill: parent
        
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            
            // 背景
            if (bgColor !== "transparent") {
                ctx.fillStyle = bgColor
                ctx.fillRect(0, 0, width, height)
            }
            
            // 图表区域
            var chartX = marginLeft
            var chartY = marginTop
            var chartWidth = width - marginLeft - marginRight
            var chartHeight = height - marginTop - marginBottom
            
            if (chartWidth <= 0 || chartHeight <= 0) return
            
            // 标题
            if (title) {
                ctx.fillStyle = textColor
                ctx.font = "bold 14px sans-serif"
                ctx.textAlign = "center"
                ctx.fillText(title, width / 2, 24)
            }
            
            // 根据类型绘制
            switch (chartType) {
                case "bar":
                    drawBarChart(ctx, chartX, chartY, chartWidth, chartHeight)
                    break
                case "pie":
                    drawPieChart(ctx, chartX, chartY, chartWidth, chartHeight)
                    break
                case "line":
                    drawLineChart(ctx, chartX, chartY, chartWidth, chartHeight)
                    break
                case "area":
                    drawAreaChart(ctx, chartX, chartY, chartWidth, chartHeight)
                    break
                case "histogram":
                    drawHistogram(ctx, chartX, chartY, chartWidth, chartHeight)
                    break
                case "scatter":
                    drawScatterPlot(ctx, chartX, chartY, chartWidth, chartHeight)
                    break
            }
        }
        
        // ========== 柱状图 ==========
        function drawBarChart(ctx, x, y, w, h) {
            if (!processedData || processedData.length === 0) return
            
            var maxVal = Math.max(...processedData.map(d => d.value))
            if (maxVal === 0) maxVal = 1
            
            var barCount = processedData.length
            var totalSpacing = (barCount - 1) * barSpacing
            var barWidth = (w - totalSpacing) / barCount
            
            // 网格
            if (showGrid) {
                drawGrid(ctx, x, y, w, h, maxVal)
            }
            
            // 柱子
            for (var i = 0; i < barCount; i++) {
                var item = processedData[i]
                var barHeight = (item.value / maxVal) * h * animationProgress
                var barX = x + i * (barWidth + barSpacing)
                var barY = y + h - barHeight
                
                ctx.fillStyle = item.color || colorPalette[i % colorPalette.length]
                ctx.globalAlpha = barOpacity
                ctx.fillRect(barX, barY, barWidth, barHeight)
                ctx.globalAlpha = 1.0
                
                // 标签
                if (showLabels) {
                    ctx.fillStyle = textColor
                    ctx.font = "10px sans-serif"
                    ctx.textAlign = "center"
                    var label = item.label || ""
                    if (label.length > 10) label = label.substring(0, 10) + "..."
                    ctx.fillText(label, barX + barWidth / 2, y + h + 15)
                }
                
                // 值
                if (showValues && barHeight > 15) {
                    ctx.fillStyle = "#FFFFFF"
                    ctx.font = "bold 10px sans-serif"
                    ctx.textAlign = "center"
                    ctx.fillText(formatValue(item.value), barX + barWidth / 2, barY + 12)
                }
            }
        }
        
        // ========== 饼图 ==========
        function drawPieChart(ctx, x, y, w, h) {
            if (!processedData || processedData.length === 0) return
            
            var total = processedData.reduce((a, b) => a + b.value, 0)
            if (total === 0) return
            
            var centerX = x + w / 2
            var centerY = y + h / 2
            var radius = Math.min(w, h) / 2 - 10
            
            var startAngle = -Math.PI / 2
            
            for (var i = 0; i < processedData.length; i++) {
                var item = processedData[i]
                var sliceAngle = (item.value / total) * 2 * Math.PI * animationProgress
                
                ctx.beginPath()
                ctx.moveTo(centerX, centerY)
                ctx.arc(centerX, centerY, radius, startAngle, startAngle + sliceAngle)
                ctx.closePath()
                
                ctx.fillStyle = item.color || colorPalette[i % colorPalette.length]
                ctx.fill()
                
                // 标签
                if (showLabels && sliceAngle > 0.2) {
                    var midAngle = startAngle + sliceAngle / 2
                    var labelRadius = radius * 0.7
                    var lx = centerX + Math.cos(midAngle) * labelRadius
                    var ly = centerY + Math.sin(midAngle) * labelRadius
                    
                    ctx.fillStyle = "#FFFFFF"
                    ctx.font = "bold 10px sans-serif"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    var percent = Math.round(item.value / total * 100)
                    ctx.fillText(percent + "%", lx, ly)
                }
                
                startAngle += sliceAngle
            }
        }
        
        // ========== 折线图 ==========
        function drawLineChart(ctx, x, y, w, h) {
            if (!processedData || !processedData.points || processedData.points.length === 0) return
            
            var points = processedData.points
            var xMin = processedData.xMin
            var xMax = processedData.xMax
            var yMin = processedData.yMin
            var yMax = processedData.yMax
            
            // 网格
            if (showGrid) {
                drawGridXY(ctx, x, y, w, h, xMin, xMax, yMin, yMax)
            }
            
            // 线条
            ctx.strokeStyle = accentColor
            ctx.lineWidth = 2
            ctx.beginPath()
            
            for (var i = 0; i < points.length; i++) {
                var px = x + ((points[i].x - xMin) / (xMax - xMin)) * w
                var py = y + h - ((points[i].y - yMin) / (yMax - yMin)) * h * animationProgress
                
                if (i === 0) {
                    ctx.moveTo(px, py)
                } else {
                    ctx.lineTo(px, py)
                }
            }
            ctx.stroke()
            
            // 点
            for (var j = 0; j < points.length; j++) {
                var px2 = x + ((points[j].x - xMin) / (xMax - xMin)) * w
                var py2 = y + h - ((points[j].y - yMin) / (yMax - yMin)) * h * animationProgress
                
                ctx.beginPath()
                ctx.arc(px2, py2, 3, 0, Math.PI * 2)
                ctx.fillStyle = accentColor
                ctx.fill()
            }
        }
        
        // ========== 面积图 ==========
        function drawAreaChart(ctx, x, y, w, h) {
            if (!processedData || !processedData.points || processedData.points.length === 0) return
            
            var points = processedData.points
            var xMin = processedData.xMin
            var xMax = processedData.xMax
            var yMin = processedData.yMin
            var yMax = processedData.yMax
            
            // 网格
            if (showGrid) {
                drawGridXY(ctx, x, y, w, h, xMin, xMax, yMin, yMax)
            }
            
            // 面积
            ctx.beginPath()
            var firstPx = x + ((points[0].x - xMin) / (xMax - xMin)) * w
            ctx.moveTo(firstPx, y + h)
            
            for (var i = 0; i < points.length; i++) {
                var px = x + ((points[i].x - xMin) / (xMax - xMin)) * w
                var py = y + h - ((points[i].y - yMin) / (yMax - yMin)) * h * animationProgress
                ctx.lineTo(px, py)
            }
            
            var lastPx = x + ((points[points.length - 1].x - xMin) / (xMax - xMin)) * w
            ctx.lineTo(lastPx, y + h)
            ctx.closePath()
            
            ctx.fillStyle = Qt.rgba(0, 122/255, 204/255, 0.3)
            ctx.fill()
            
            // 边线
            ctx.strokeStyle = accentColor
            ctx.lineWidth = 2
            ctx.beginPath()
            for (var j = 0; j < points.length; j++) {
                var px2 = x + ((points[j].x - xMin) / (xMax - xMin)) * w
                var py2 = y + h - ((points[j].y - yMin) / (yMax - yMin)) * h * animationProgress
                if (j === 0) ctx.moveTo(px2, py2)
                else ctx.lineTo(px2, py2)
            }
            ctx.stroke()
        }
        
        // ========== 直方图 ==========
        function drawHistogram(ctx, x, y, w, h) {
            if (!processedData || !processedData.bins || processedData.bins.length === 0) return
            
            var bins = processedData.bins
            var maxCount = processedData.maxCount
            if (maxCount === 0) maxCount = 1
            
            var binCount = bins.length
            var barWidth = w / binCount - barSpacing
            
            // 网格
            if (showGrid) {
                drawGrid(ctx, x, y, w, h, maxCount, true)
            }
            
            // 柱子
            for (var i = 0; i < binCount; i++) {
                var bin = bins[i]
                var barHeight = (bin.count / maxCount) * h * animationProgress
                var barX = x + i * (barWidth + barSpacing)
                var barY = y + h - barHeight
                
                ctx.fillStyle = barColor
                ctx.globalAlpha = barOpacity
                ctx.fillRect(barX, barY, barWidth, barHeight)
                ctx.globalAlpha = 1.0
                
                // X轴标签（只显示部分）
                if (showLabels && (i === 0 || i === binCount - 1 || i === Math.floor(binCount / 2))) {
                    ctx.fillStyle = textColor
                    ctx.font = "9px sans-serif"
                    ctx.textAlign = "center"
                    ctx.fillText(formatValue(bin.start), barX + barWidth / 2, y + h + 12)
                }
                
                // 值标签
                if (showValues && barHeight > 15) {
                    ctx.fillStyle = "#FFFFFF"
                    ctx.font = "bold 9px sans-serif"
                    ctx.textAlign = "center"
                    ctx.fillText(bin.count.toString(), barX + barWidth / 2, barY + 12)
                }
            }
            
            // X轴标题
            ctx.fillStyle = textColor
            ctx.font = "10px sans-serif"
            ctx.textAlign = "center"
            ctx.fillText("Value Range", x + w / 2, y + h + 30)
        }
        
        // ========== 散点图 ==========
        function drawScatterPlot(ctx, x, y, w, h) {
            if (!processedData || !processedData.points || processedData.points.length === 0) return
            
            var points = processedData.points
            var xMin = processedData.xMin
            var xMax = processedData.xMax
            var yMin = processedData.yMin
            var yMax = processedData.yMax
            
            // 网格
            if (showGrid) {
                drawGridXY(ctx, x, y, w, h, xMin, xMax, yMin, yMax)
            }
            
            // 点
            ctx.fillStyle = pointColor
            ctx.globalAlpha = pointOpacity
            
            for (var i = 0; i < points.length * animationProgress; i++) {
                var px = x + ((points[i].x - xMin) / (xMax - xMin)) * w
                var py = y + h - ((points[i].y - yMin) / (yMax - yMin)) * h
                
                ctx.beginPath()
                ctx.arc(px, py, pointRadius, 0, Math.PI * 2)
                ctx.fill()
            }
            
            ctx.globalAlpha = 1.0
        }
        
        // ========== 辅助函数 ==========
        function drawGrid(ctx, x, y, w, h, maxVal, isCount) {
            ctx.strokeStyle = gridColor
            ctx.lineWidth = 1
            
            // 水平线
            var numLines = 5
            for (var i = 0; i <= numLines; i++) {
                var lineY = y + (h / numLines) * i
                ctx.beginPath()
                ctx.moveTo(x, lineY)
                ctx.lineTo(x + w, lineY)
                ctx.stroke()
                
                // Y轴标签
                var val = maxVal * (1 - i / numLines)
                ctx.fillStyle = textColor
                ctx.font = "9px sans-serif"
                ctx.textAlign = "right"
                ctx.fillText(isCount ? Math.round(val) : formatValue(val), x - 5, lineY + 3)
            }
        }
        
        function drawGridXY(ctx, x, y, w, h, xMin, xMax, yMin, yMax) {
            ctx.strokeStyle = gridColor
            ctx.lineWidth = 1
            
            var numLines = 5
            
            // 水平线
            for (var i = 0; i <= numLines; i++) {
                var lineY = y + (h / numLines) * i
                ctx.beginPath()
                ctx.moveTo(x, lineY)
                ctx.lineTo(x + w, lineY)
                ctx.stroke()
                
                var yVal = yMax - (yMax - yMin) * (i / numLines)
                ctx.fillStyle = textColor
                ctx.font = "9px sans-serif"
                ctx.textAlign = "right"
                ctx.fillText(formatValue(yVal), x - 5, lineY + 3)
            }
            
            // 垂直线
            for (var j = 0; j <= numLines; j++) {
                var lineX = x + (w / numLines) * j
                ctx.beginPath()
                ctx.moveTo(lineX, y)
                ctx.lineTo(lineX, y + h)
                ctx.stroke()
                
                var xVal = xMin + (xMax - xMin) * (j / numLines)
                ctx.fillStyle = textColor
                ctx.font = "9px sans-serif"
                ctx.textAlign = "center"
                ctx.fillText(formatValue(xVal), lineX, y + h + 12)
            }
        }
        
        function formatValue(val) {
            if (Math.abs(val) >= 1000000) {
                return (val / 1000000).toFixed(1) + "M"
            } else if (Math.abs(val) >= 1000) {
                return (val / 1000).toFixed(1) + "K"
            } else if (Math.abs(val) < 1 && val !== 0) {
                return val.toFixed(2)
            }
            return Math.round(val).toString()
        }
        
        // 鼠标交互
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            
            property int hoveredIndex: -1
            
            onPositionChanged: function(mouse) {
                // 计算悬停的数据点
                if (chartType === "scatter" && processedData && processedData.points) {
                    var chartX = marginLeft
                    var chartY = marginTop
                    var chartW = width - marginLeft - marginRight
                    var chartH = height - marginTop - marginBottom
                    
                    var points = processedData.points
                    for (var i = 0; i < points.length; i++) {
                        var px = chartX + ((points[i].x - processedData.xMin) / (processedData.xMax - processedData.xMin)) * chartW
                        var py = chartY + chartH - ((points[i].y - processedData.yMin) / (processedData.yMax - processedData.yMin)) * chartH
                        
                        var dist = Math.sqrt(Math.pow(mouse.x - px, 2) + Math.pow(mouse.y - py, 2))
                        if (dist <= pointRadius + 3) {
                            hoveredIndex = i
                            return
                        }
                    }
                }
                hoveredIndex = -1
            }
            
            onClicked: function(mouse) {
                if (hoveredIndex >= 0) {
                    pointClicked(hoveredIndex, processedData.points[hoveredIndex])
                }
            }
        }
    }
    
    // 空状态
    Text {
        anchors.centerIn: parent
        text: qsTr("No data")
        color: Qt.darker(textColor, 1.5)
        font.pixelSize: 14
        visible: !data || data.length === 0
    }
}
