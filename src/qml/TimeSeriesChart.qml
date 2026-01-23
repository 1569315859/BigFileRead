import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * TimeSeriesChart.qml - Canvas 自绘时间序列图表组件
 * 
 * 支持三种图表类型:
 * - stacked: 堆叠柱状图（各级别叠加显示）
 * - grouped: 分组柱状图（各级别并排显示）
 * - area: 面积图（适合展示趋势）
 */
Item {
    id: root
    
    // ============ 属性 ============
    property var data: []  // [{timeLabel, error, warn, info, debug, trace, other, total}, ...]
    property string chartType: "stacked"  // "stacked", "grouped", "area"
    property string title: ""
    
    // 主题颜色
    property color textColor: _themeManager ? _themeManager.textColor : "#ffffff"
    property color bgColor: _themeManager ? _themeManager.backgroundColor : "#1e1e1e"
    property color borderColor: _themeManager ? _themeManager.borderColor : "#3c3c3c"
    property color panelColor: _themeManager ? _themeManager.panelBackground : "#252526"
    
    // 日志级别颜色
    property color errorColor: "#F44336"
    property color warnColor: "#FF9800"
    property color infoColor: "#2196F3"
    property color debugColor: "#9C27B0"
    property color traceColor: "#4CAF50"
    property color otherColor: "#607D8B"
    
    // 图表边距
    property int marginLeft: 50
    property int marginRight: 20
    property int marginTop: 40
    property int marginBottom: 40
    
    // 显示控制
    property bool showLegend: true
    property bool showValues: true
    property bool showGrid: true
    
    // 交互
    property int hoveredIndex: -1
    signal barClicked(int index, var dataItem)
    
    // ============ 图表画布 ============
    Canvas {
        id: chartCanvas
        anchors.fill: parent
        
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            
            // 清空背景
            ctx.fillStyle = bgColor
            ctx.fillRect(0, 0, width, height)
            
            if (!data || data.length === 0) {
                drawNoData(ctx)
                return
            }
            
            // 绘制标题
            if (title) {
                ctx.font = "bold 14px sans-serif"
                ctx.fillStyle = textColor
                ctx.textAlign = "center"
                ctx.fillText(title, width / 2, 20)
            }
            
            // 计算图表区域
            var chartX = marginLeft
            var chartY = marginTop
            var chartWidth = width - marginLeft - marginRight
            var chartHeight = height - marginTop - marginBottom - (showLegend ? 30 : 0)
            
            // 计算最大值
            var maxValue = 0
            for (var i = 0; i < data.length; i++) {
                var item = data[i]
                var total = item.error + item.warn + item.info + item.debug + item.trace + item.other
                maxValue = Math.max(maxValue, total)
            }
            if (maxValue === 0) maxValue = 1
            
            // 绘制网格
            if (showGrid) {
                drawGrid(ctx, chartX, chartY, chartWidth, chartHeight, maxValue)
            }
            
            // 根据类型绘制图表
            if (chartType === "stacked") {
                drawStackedChart(ctx, chartX, chartY, chartWidth, chartHeight, maxValue)
            } else if (chartType === "grouped") {
                drawGroupedChart(ctx, chartX, chartY, chartWidth, chartHeight, maxValue)
            } else if (chartType === "area") {
                drawAreaChart(ctx, chartX, chartY, chartWidth, chartHeight, maxValue)
            }
            
            // 绘制图例
            if (showLegend) {
                drawLegend(ctx, chartX, height - 25, chartWidth)
            }
            
            // 绘制悬停提示
            if (hoveredIndex >= 0 && hoveredIndex < data.length) {
                drawTooltip(ctx, hoveredIndex)
            }
        }
        
        function drawNoData(ctx) {
            ctx.font = "14px sans-serif"
            ctx.fillStyle = Qt.darker(textColor, 1.5)
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            ctx.fillText(qsTr("No data available"), width / 2, height / 2)
        }
        
        function drawGrid(ctx, x, y, w, h, maxValue) {
            ctx.strokeStyle = Qt.rgba(borderColor.r, borderColor.g, borderColor.b, 0.3)
            ctx.lineWidth = 1
            
            // 水平网格线和Y轴标签
            var gridLines = 5
            ctx.font = "11px sans-serif"
            ctx.fillStyle = Qt.darker(textColor, 1.3)
            ctx.textAlign = "right"
            ctx.textBaseline = "middle"
            
            for (var i = 0; i <= gridLines; i++) {
                var yPos = y + h - (i / gridLines) * h
                var value = Math.round(maxValue * i / gridLines)
                
                // 网格线
                ctx.beginPath()
                ctx.moveTo(x, yPos)
                ctx.lineTo(x + w, yPos)
                ctx.stroke()
                
                // Y轴标签
                ctx.fillText(formatNumber(value), x - 5, yPos)
            }
            
            // 绘制坐标轴
            ctx.strokeStyle = borderColor
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(x, y)
            ctx.lineTo(x, y + h)
            ctx.lineTo(x + w, y + h)
            ctx.stroke()
        }
        
        function drawStackedChart(ctx, x, y, w, h, maxValue) {
            var barWidth = (w / data.length) * 0.7
            var barSpacing = (w / data.length) * 0.3
            
            for (var i = 0; i < data.length; i++) {
                var item = data[i]
                var barX = x + i * (barWidth + barSpacing) + barSpacing / 2
                var currentY = y + h
                
                var levels = [
                    { value: item.other, color: otherColor },
                    { value: item.trace, color: traceColor },
                    { value: item.debug, color: debugColor },
                    { value: item.info, color: infoColor },
                    { value: item.warn, color: warnColor },
                    { value: item.error, color: errorColor }
                ]
                
                var total = 0
                for (var j = 0; j < levels.length; j++) {
                    total += levels[j].value
                }
                
                // 绘制堆叠的各层
                for (j = 0; j < levels.length; j++) {
                    if (levels[j].value > 0) {
                        var barHeight = (levels[j].value / maxValue) * h
                        currentY -= barHeight
                        
                        ctx.fillStyle = i === hoveredIndex 
                            ? Qt.lighter(levels[j].color, 1.2) 
                            : levels[j].color
                        ctx.fillRect(barX, currentY, barWidth, barHeight)
                    }
                }
                
                // 绘制顶部数值
                if (showValues && total > 0) {
                    ctx.font = "10px sans-serif"
                    ctx.fillStyle = textColor
                    ctx.textAlign = "center"
                    ctx.textBaseline = "bottom"
                    ctx.fillText(formatNumber(total), barX + barWidth / 2, currentY - 2)
                }
                
                // 绘制X轴标签
                ctx.font = "10px sans-serif"
                ctx.fillStyle = Qt.darker(textColor, 1.2)
                ctx.textAlign = "center"
                ctx.textBaseline = "top"
                
                // 旋转文字避免重叠
                ctx.save()
                ctx.translate(barX + barWidth / 2, y + h + 5)
                if (data.length > 12) {
                    ctx.rotate(-Math.PI / 4)
                    ctx.textAlign = "right"
                }
                ctx.fillText(item.timeLabel || "", 0, 0)
                ctx.restore()
            }
        }
        
        function drawGroupedChart(ctx, x, y, w, h, maxValue) {
            var groupWidth = w / data.length
            var barCount = 6
            var barWidth = (groupWidth * 0.8) / barCount
            var groupSpacing = groupWidth * 0.2
            
            for (var i = 0; i < data.length; i++) {
                var item = data[i]
                var groupX = x + i * groupWidth + groupSpacing / 2
                
                var levels = [
                    { value: item.error, color: errorColor },
                    { value: item.warn, color: warnColor },
                    { value: item.info, color: infoColor },
                    { value: item.debug, color: debugColor },
                    { value: item.trace, color: traceColor },
                    { value: item.other, color: otherColor }
                ]
                
                for (var j = 0; j < levels.length; j++) {
                    var barHeight = (levels[j].value / maxValue) * h
                    var barX = groupX + j * barWidth
                    
                    ctx.fillStyle = i === hoveredIndex 
                        ? Qt.lighter(levels[j].color, 1.2) 
                        : levels[j].color
                    ctx.fillRect(barX, y + h - barHeight, barWidth - 1, barHeight)
                }
                
                // X轴标签
                ctx.font = "10px sans-serif"
                ctx.fillStyle = Qt.darker(textColor, 1.2)
                ctx.textAlign = "center"
                ctx.fillText(item.timeLabel || "", groupX + groupWidth * 0.4, y + h + 15)
            }
        }
        
        function drawAreaChart(ctx, x, y, w, h, maxValue) {
            if (data.length < 2) return
            
            var stepX = w / (data.length - 1)
            
            var levels = [
                { key: "other", color: otherColor },
                { key: "trace", color: traceColor },
                { key: "debug", color: debugColor },
                { key: "info", color: infoColor },
                { key: "warn", color: warnColor },
                { key: "error", color: errorColor }
            ]
            
            // 计算累积值
            var accumulated = []
            for (var i = 0; i < data.length; i++) {
                accumulated.push({ values: [0, 0, 0, 0, 0, 0] })
                var cumulative = 0
                for (var j = 0; j < levels.length; j++) {
                    cumulative += data[i][levels[j].key] || 0
                    accumulated[i].values[j] = cumulative
                }
            }
            
            // 从后往前绘制（先绘制最上层）
            for (j = levels.length - 1; j >= 0; j--) {
                ctx.beginPath()
                ctx.moveTo(x, y + h)
                
                for (i = 0; i < data.length; i++) {
                    var px = x + i * stepX
                    var py = y + h - (accumulated[i].values[j] / maxValue) * h
                    if (i === 0) {
                        ctx.lineTo(px, py)
                    } else {
                        ctx.lineTo(px, py)
                    }
                }
                
                ctx.lineTo(x + w, y + h)
                ctx.closePath()
                
                ctx.fillStyle = Qt.rgba(levels[j].color.r, levels[j].color.g, levels[j].color.b, 0.7)
                ctx.fill()
            }
            
            // 绘制X轴标签（仅绘制部分避免重叠）
            var labelStep = Math.ceil(data.length / 10)
            ctx.font = "10px sans-serif"
            ctx.fillStyle = Qt.darker(textColor, 1.2)
            ctx.textAlign = "center"
            
            for (i = 0; i < data.length; i += labelStep) {
                ctx.fillText(data[i].timeLabel || "", x + i * stepX, y + h + 15)
            }
        }
        
        function drawLegend(ctx, x, y, w) {
            var items = [
                { label: qsTr("Error"), color: errorColor },
                { label: qsTr("Warn"), color: warnColor },
                { label: qsTr("Info"), color: infoColor },
                { label: qsTr("Debug"), color: debugColor },
                { label: qsTr("Trace"), color: traceColor },
                { label: qsTr("Other"), color: otherColor }
            ]
            
            var itemWidth = w / items.length
            ctx.font = "11px sans-serif"
            ctx.textBaseline = "middle"
            
            for (var i = 0; i < items.length; i++) {
                var itemX = x + i * itemWidth
                
                // 色块
                ctx.fillStyle = items[i].color
                ctx.fillRect(itemX, y, 12, 12)
                
                // 标签
                ctx.fillStyle = textColor
                ctx.textAlign = "left"
                ctx.fillText(items[i].label, itemX + 16, y + 6)
            }
        }
        
        function drawTooltip(ctx, index) {
            var item = data[index]
            if (!item) return
            
            var tooltipWidth = 150
            var tooltipHeight = 120
            var padding = 10
            
            // 计算位置
            var barWidth = (width - marginLeft - marginRight) / data.length
            var tooltipX = marginLeft + index * barWidth + barWidth / 2
            var tooltipY = marginTop + 20
            
            // 防止超出边界
            if (tooltipX + tooltipWidth > width - padding) {
                tooltipX = tooltipX - tooltipWidth - 10
            }
            
            // 背景
            ctx.fillStyle = Qt.rgba(0, 0, 0, 0.85)
            ctx.strokeStyle = borderColor
            ctx.lineWidth = 1
            
            roundRect(ctx, tooltipX, tooltipY, tooltipWidth, tooltipHeight, 6)
            ctx.fill()
            ctx.stroke()
            
            // 内容
            ctx.font = "bold 11px sans-serif"
            ctx.fillStyle = textColor
            ctx.textAlign = "left"
            ctx.textBaseline = "top"
            ctx.fillText(item.timeLabel || "", tooltipX + 10, tooltipY + 8)
            
            var lines = [
                { label: "Error:", value: item.error, color: errorColor },
                { label: "Warn:", value: item.warn, color: warnColor },
                { label: "Info:", value: item.info, color: infoColor },
                { label: "Debug:", value: item.debug, color: debugColor },
                { label: "Trace:", value: item.trace, color: traceColor }
            ]
            
            ctx.font = "11px sans-serif"
            for (var i = 0; i < lines.length; i++) {
                var lineY = tooltipY + 28 + i * 16
                ctx.fillStyle = lines[i].color
                ctx.fillText(lines[i].label, tooltipX + 10, lineY)
                ctx.fillStyle = textColor
                ctx.fillText(formatNumber(lines[i].value), tooltipX + 60, lineY)
            }
        }
        
        function roundRect(ctx, x, y, w, h, r) {
            ctx.beginPath()
            ctx.moveTo(x + r, y)
            ctx.lineTo(x + w - r, y)
            ctx.quadraticCurveTo(x + w, y, x + w, y + r)
            ctx.lineTo(x + w, y + h - r)
            ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
            ctx.lineTo(x + r, y + h)
            ctx.quadraticCurveTo(x, y + h, x, y + h - r)
            ctx.lineTo(x, y + r)
            ctx.quadraticCurveTo(x, y, x + r, y)
            ctx.closePath()
        }
        
        function formatNumber(num) {
            if (num >= 1000000) return (num / 1000000).toFixed(1) + "M"
            if (num >= 1000) return (num / 1000).toFixed(1) + "K"
            return num.toString()
        }
    }
    
    // ============ 鼠标交互 ============
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        
        onPositionChanged: (mouse) => {
            if (!data || data.length === 0) return
            
            var chartX = marginLeft
            var chartWidth = width - marginLeft - marginRight
            
            if (mouse.x >= chartX && mouse.x <= chartX + chartWidth) {
                var barWidth = chartWidth / data.length
                var index = Math.floor((mouse.x - chartX) / barWidth)
                if (index >= 0 && index < data.length && index !== hoveredIndex) {
                    hoveredIndex = index
                    chartCanvas.requestPaint()
                }
            } else if (hoveredIndex !== -1) {
                hoveredIndex = -1
                chartCanvas.requestPaint()
            }
        }
        
        onExited: {
            if (hoveredIndex !== -1) {
                hoveredIndex = -1
                chartCanvas.requestPaint()
            }
        }
        
        onClicked: (mouse) => {
            if (hoveredIndex >= 0 && hoveredIndex < data.length) {
                barClicked(hoveredIndex, data[hoveredIndex])
            }
        }
    }
    
    // ============ 数据变化时重绘 ============
    onDataChanged: chartCanvas.requestPaint()
    onChartTypeChanged: chartCanvas.requestPaint()
    onHoveredIndexChanged: chartCanvas.requestPaint()
    
    // 窗口大小变化时重绘
    onWidthChanged: chartCanvas.requestPaint()
    onHeightChanged: chartCanvas.requestPaint()
}
