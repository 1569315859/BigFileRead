import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * CSVTableView.qml - 专用CSV表格视图组件
 * 提供类似Excel的表格体验，支持列排序、冻结行列、列宽调整等功能
 */
Item {
    id: root
    
    // 数据源
    property var dataSource: null  // CSVDataSource 实例
    
    // 主题颜色
    property color bgColor: "#1E1E1E"
    property color panelColor: "#252526"
    property color textColor: "#D4D4D4"
    property color accentColor: "#007ACC"
    property color borderColor: "#3C3C3C"
    property color headerBgColor: "#2D2D2D"
    property color selectedBgColor: "#094771"
    property color hoverBgColor: "#2A2D2E"
    property color frozenBgColor: "#1A3A4A"
    
    // 配置
    property int rowHeight: 28
    property int headerHeight: 32
    property int defaultColumnWidth: 120
    property int minColumnWidth: 50
    property int maxColumnWidth: 500
    
    // 状态
    property int selectedRow: -1
    property int selectedColumn: -1
    property var selectedCells: []  // 多选支持
    
    // 信号
    signal cellClicked(int row, int column, var value)
    signal cellDoubleClicked(int row, int column, var value)
    signal headerClicked(int column)
    signal sortRequested(int column, int order)
    signal contextMenuRequested(int row, int column, var mousePos)
    
    // 列宽存储
    property var columnWidths: ({})
    
    // 获取列宽
    function getColumnWidth(index) {
        if (columnWidths[index] !== undefined) {
            return columnWidths[index]
        }
        if (dataSource) {
            var meta = dataSource.getColumnMeta(index)
            if (meta && meta.width) {
                return meta.width
            }
        }
        return defaultColumnWidth
    }
    
    // 设置列宽
    function setColumnWidth(index, width) {
        columnWidths[index] = Math.max(minColumnWidth, Math.min(maxColumnWidth, width))
        columnWidthsChanged()
    }
    
    // 计算总宽度
    function getTotalWidth() {
        var total = 60  // 行号列宽度
        var colCount = dataSource ? dataSource.totalColumnCount : 0
        for (var i = 0; i < colCount; i++) {
            total += getColumnWidth(i)
        }
        return total
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // ========== 工具栏 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: panelColor
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8
                
                // 文件信息
                Text {
                    text: dataSource ? dataSource.filePath.split('/').pop().split('\\').pop() : ""
                    color: textColor
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: 200
                }
                
                Rectangle { width: 1; height: 20; color: borderColor }
                
                // 统计信息
                Text {
                    text: dataSource ? qsTr("%1 rows × %2 columns").arg(dataSource.totalRowCount).arg(dataSource.totalColumnCount) : ""
                    color: Qt.darker(textColor, 1.3)
                    font.pixelSize: 11
                }
                
                Item { Layout.fillWidth: true }
                
                // 分隔符选择
                Text {
                    text: qsTr("Delimiter:")
                    color: textColor
                    font.pixelSize: 11
                }
                
                ComboBox {
                    id: delimiterCombo
                    model: [
                        { text: qsTr("Comma (,)"), value: "," },
                        { text: qsTr("Tab"), value: "\t" },
                        { text: qsTr("Semicolon (;)"), value: ";" },
                        { text: qsTr("Pipe (|)"), value: "|" }
                    ]
                    textRole: "text"
                    implicitWidth: 120
                    implicitHeight: 26
                    
                    currentIndex: {
                        if (!dataSource) return 0
                        var d = dataSource.delimiter
                        if (d === ',') return 0
                        if (d === '\t') return 1
                        if (d === ';') return 2
                        if (d === '|') return 3
                        return 0
                    }
                    
                    onActivated: function(index) {
                        if (dataSource) {
                            dataSource.setDelimiter(model[index].value)
                        }
                    }
                    
                    background: Rectangle {
                        color: bgColor
                        border.color: borderColor
                        radius: 3
                    }
                    contentItem: Text {
                        text: delimiterCombo.displayText
                        color: textColor
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 6
                    }
                }
                
                // 表头开关
                CheckBox {
                    id: headerCheck
                    text: qsTr("Has Header")
                    checked: dataSource ? dataSource.hasHeader : true
                    onCheckedChanged: {
                        if (dataSource && dataSource.hasHeader !== checked) {
                            dataSource.setHasHeader(checked)
                        }
                    }
                    
                    indicator: Rectangle {
                        implicitWidth: 16
                        implicitHeight: 16
                        x: 0
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        
                        Text {
                            text: "✓"
                            color: accentColor
                            anchors.centerIn: parent
                            font.pixelSize: 12
                            visible: headerCheck.checked
                        }
                    }
                    contentItem: Text {
                        text: headerCheck.text
                        color: textColor
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: headerCheck.indicator.width + 4
                    }
                }
                
                Rectangle { width: 1; height: 20; color: borderColor }
                
                // 导出按钮
                Button {
                    text: qsTr("Export")
                    implicitHeight: 26
                    implicitWidth: 60
                    onClicked: exportMenu.open()
                    
                    background: Rectangle {
                        color: parent.down ? Qt.darker(accentColor, 1.2) : (parent.hovered ? accentColor : panelColor)
                        border.color: borderColor
                        radius: 3
                    }
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    Menu {
                        id: exportMenu
                        MenuItem { text: qsTr("Export to CSV...") }
                        MenuItem { text: qsTr("Export Selected Rows...") }
                        MenuItem { text: qsTr("Copy to Clipboard") }
                    }
                }
            }
        }
        
        // ========== 表格区域 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: bgColor
            clip: true
            
            // 列头
            Rectangle {
                id: headerRow
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: headerHeight
                color: headerBgColor
                z: 10
                
                Row {
                    id: headerContent
                    x: -tableFlickable.contentX
                    height: parent.height
                    
                    // 行号列头
                    Rectangle {
                        width: 60
                        height: parent.height
                        color: headerBgColor
                        
                        Text {
                            text: "#"
                            color: textColor
                            font.bold: true
                            font.pixelSize: 12
                            anchors.centerIn: parent
                        }
                        
                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: borderColor
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: borderColor
                        }
                    }
                    
                    // 数据列头
                    Repeater {
                        model: dataSource ? dataSource.totalColumnCount : 0
                        
                        Rectangle {
                            id: headerCell
                            width: getColumnWidth(index)
                            height: headerHeight
                            color: headerMouseArea.containsMouse ? hoverBgColor : headerBgColor
                            
                            property int columnIndex: index
                            property var columnMeta: dataSource ? dataSource.getColumnMeta(index) : null
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 20
                                spacing: 4
                                
                                // 列类型图标
                                Text {
                                    text: {
                                        if (!columnMeta) return ""
                                        switch (columnMeta.type) {
                                            case 1: return "123"  // Number
                                            case 2: return "📅"   // DateTime
                                            case 3: return "☑"    // Boolean
                                            default: return "Abc" // Text
                                        }
                                    }
                                    color: Qt.darker(textColor, 1.5)
                                    font.pixelSize: 9
                                }
                                
                                // 列名
                                Text {
                                    text: columnMeta ? columnMeta.name : ""
                                    color: textColor
                                    font.bold: true
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                
                                // 排序指示器
                                Text {
                                    text: {
                                        if (dataSource && dataSource.sortColumn === (columnMeta ? columnMeta.name : "")) {
                                            return dataSource.sortOrder === Qt.AscendingOrder ? "▲" : "▼"
                                        }
                                        return ""
                                    }
                                    color: accentColor
                                    font.pixelSize: 10
                                }
                            }
                            
                            // 底部边框
                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: borderColor
                            }
                            
                            // 右侧边框 + 拖拽调整宽度
                            Rectangle {
                                id: resizeHandle
                                anchors.right: parent.right
                                width: 4
                                height: parent.height
                                color: resizeMouseArea.containsMouse ? accentColor : borderColor
                                
                                MouseArea {
                                    id: resizeMouseArea
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    hoverEnabled: true
                                    cursorShape: Qt.SizeHorCursor
                                    
                                    property real startX: 0
                                    property real startWidth: 0
                                    
                                    onPressed: function(mouse) {
                                        startX = mouse.x
                                        startWidth = headerCell.width
                                    }
                                    
                                    onPositionChanged: function(mouse) {
                                        if (pressed) {
                                            var delta = mouse.x - startX
                                            setColumnWidth(headerCell.columnIndex, startWidth + delta)
                                        }
                                    }
                                }
                            }
                            
                            MouseArea {
                                id: headerMouseArea
                                anchors.fill: parent
                                anchors.rightMargin: 6
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.LeftButton) {
                                        // 点击排序
                                        var newOrder = Qt.AscendingOrder
                                        if (dataSource && dataSource.sortColumn === columnMeta.name) {
                                            newOrder = dataSource.sortOrder === Qt.AscendingOrder ? Qt.DescendingOrder : Qt.AscendingOrder
                                        }
                                        if (dataSource) {
                                            dataSource.sortByColumn(headerCell.columnIndex, newOrder)
                                        }
                                        sortRequested(headerCell.columnIndex, newOrder)
                                    } else if (mouse.button === Qt.RightButton) {
                                        headerContextMenu.columnIndex = headerCell.columnIndex
                                        headerContextMenu.popup()
                                    }
                                }
                                
                                onDoubleClicked: {
                                    // 双击自动调整宽度
                                    if (dataSource) {
                                        dataSource.autoFitColumnWidth(headerCell.columnIndex)
                                        var meta = dataSource.getColumnMeta(headerCell.columnIndex)
                                        if (meta) {
                                            setColumnWidth(headerCell.columnIndex, meta.width)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // 数据区域
            Flickable {
                id: tableFlickable
                anchors.top: headerRow.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                
                contentWidth: getTotalWidth()
                contentHeight: (dataSource ? dataSource.totalRowCount : 0) * rowHeight
                
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                
                // 使用 ListView 实现虚拟化
                ListView {
                    id: rowListView
                    anchors.fill: parent
                    model: dataSource
                    
                    cacheBuffer: rowHeight * 20
                    
                    delegate: Rectangle {
                        id: rowDelegate
                        width: getTotalWidth()
                        height: rowHeight
                        color: {
                            if (index === selectedRow) return selectedBgColor
                            if (rowMouseArea.containsMouse) return hoverBgColor
                            return index % 2 === 0 ? bgColor : Qt.darker(bgColor, 1.05)
                        }
                        
                        property int rowIndex: index
                        
                        Row {
                            anchors.fill: parent
                            
                            // 行号
                            Rectangle {
                                width: 60
                                height: rowHeight
                                color: Qt.darker(bgColor, 1.1)
                                
                                Text {
                                    text: rowIndex + 1
                                    color: "#858585"
                                    font.family: "Consolas"
                                    font.pixelSize: 11
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                
                                Rectangle {
                                    anchors.right: parent.right
                                    width: 1
                                    height: parent.height
                                    color: borderColor
                                }
                            }
                            
                            // 数据单元格
                            Repeater {
                                model: dataSource ? dataSource.totalColumnCount : 0
                                
                                Rectangle {
                                    id: cellRect
                                    width: getColumnWidth(index)
                                    height: rowHeight
                                    color: "transparent"
                                    
                                    property int colIndex: index
                                    property var cellValue: dataSource ? dataSource.getCellValue(rowDelegate.rowIndex, index) : ""
                                    property int colType: dataSource ? (dataSource.getColumnMeta(index).type || 0) : 0
                                    
                                    Text {
                                        text: cellValue || ""
                                        color: {
                                            // 根据列类型着色
                                            switch (colType) {
                                                case 1: return "#B5CEA8"  // Number - 绿色
                                                case 2: return "#CE9178"  // DateTime - 橙色
                                                case 3: return "#569CD6"  // Boolean - 蓝色
                                                default: return textColor // Text
                                            }
                                        }
                                        font.family: "Consolas"
                                        font.pixelSize: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        anchors.leftMargin: 8
                                        anchors.right: parent.right
                                        anchors.rightMargin: 8
                                        elide: Text.ElideRight
                                        
                                        // 数字右对齐
                                        horizontalAlignment: colType === 1 ? Text.AlignRight : Text.AlignLeft
                                    }
                                    
                                    // 右边框
                                    Rectangle {
                                        anchors.right: parent.right
                                        width: 1
                                        height: parent.height
                                        color: borderColor
                                        opacity: 0.3
                                    }
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        
                                        onClicked: {
                                            selectedRow = rowDelegate.rowIndex
                                            selectedColumn = cellRect.colIndex
                                            cellClicked(rowDelegate.rowIndex, cellRect.colIndex, cellRect.cellValue)
                                        }
                                        
                                        onDoubleClicked: {
                                            cellDoubleClicked(rowDelegate.rowIndex, cellRect.colIndex, cellRect.cellValue)
                                        }
                                    }
                                }
                            }
                        }
                        
                        MouseArea {
                            id: rowMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.RightButton
                            propagateComposedEvents: true
                            
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    selectedRow = rowDelegate.rowIndex
                                    contextMenuRequested(rowDelegate.rowIndex, -1, Qt.point(mouse.x, mouse.y))
                                    rowContextMenu.popup()
                                }
                            }
                        }
                    }
                }
            }
            
            // 加载中遮罩
            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(0, 0, 0, 0.7)
                visible: dataSource && dataSource.isLoading
                
                Column {
                    anchors.centerIn: parent
                    spacing: 16
                    
                    BusyIndicator {
                        running: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    
                    Text {
                        text: qsTr("Loading...")
                        color: textColor
                        font.pixelSize: 14
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            
            // 空状态
            Text {
                anchors.centerIn: parent
                text: qsTr("No data loaded.\nDrag and drop a CSV file here.")
                color: Qt.darker(textColor, 1.5)
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                visible: !dataSource || (dataSource.totalRowCount === 0 && !dataSource.isLoading)
            }
        }
        
        // ========== 状态栏 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: panelColor
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 16
                
                Text {
                    text: selectedRow >= 0 ? qsTr("Row: %1").arg(selectedRow + 1) : ""
                    color: textColor
                    font.pixelSize: 11
                }
                
                Text {
                    text: selectedColumn >= 0 ? qsTr("Column: %1").arg(selectedColumn + 1) : ""
                    color: textColor
                    font.pixelSize: 11
                }
                
                Item { Layout.fillWidth: true }
                
                Text {
                    text: dataSource ? qsTr("File size: %1").arg(formatFileSize(dataSource.fileSize)) : ""
                    color: Qt.darker(textColor, 1.3)
                    font.pixelSize: 11
                }
            }
        }
    }
    
    // ========== 右键菜单 ==========
    Menu {
        id: headerContextMenu
        property int columnIndex: -1
        
        MenuItem {
            text: qsTr("Sort Ascending")
            onTriggered: {
                if (dataSource) {
                    dataSource.sortByColumn(headerContextMenu.columnIndex, Qt.AscendingOrder)
                }
            }
        }
        MenuItem {
            text: qsTr("Sort Descending")
            onTriggered: {
                if (dataSource) {
                    dataSource.sortByColumn(headerContextMenu.columnIndex, Qt.DescendingOrder)
                }
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Auto Fit Width")
            onTriggered: {
                if (dataSource) {
                    dataSource.autoFitColumnWidth(headerContextMenu.columnIndex)
                }
            }
        }
        MenuItem {
            text: qsTr("Hide Column")
            onTriggered: {
                if (dataSource) {
                    dataSource.setColumnVisible(headerContextMenu.columnIndex, false)
                }
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Column Statistics...")
            onTriggered: {
                columnStatsDialog.columnIndex = headerContextMenu.columnIndex
                columnStatsDialog.open()
            }
        }
    }
    
    Menu {
        id: rowContextMenu
        
        MenuItem {
            text: qsTr("Copy Row")
            onTriggered: {
                if (dataSource && selectedRow >= 0) {
                    var text = dataSource.getRawRowText(selectedRow)
                    // 复制到剪贴板（需要通过C++实现）
                }
            }
        }
        MenuItem {
            text: qsTr("Copy Cell")
            onTriggered: {
                if (dataSource && selectedRow >= 0 && selectedColumn >= 0) {
                    var value = dataSource.getCellValue(selectedRow, selectedColumn)
                    // 复制到剪贴板
                }
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Filter by this value")
            onTriggered: {
                if (dataSource && selectedRow >= 0 && selectedColumn >= 0) {
                    var value = dataSource.getCellValue(selectedRow, selectedColumn)
                    dataSource.applyColumnFilter(selectedColumn, value, "equals")
                }
            }
        }
    }
    
    // 列统计对话框
    Popup {
        id: columnStatsDialog
        modal: true
        width: 300
        height: 280
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        
        property int columnIndex: -1
        property var stats: dataSource && columnIndex >= 0 ? dataSource.getColumnStatistics(columnIndex) : null
        
        background: Rectangle {
            color: panelColor
            border.color: borderColor
            radius: 6
        }
        
        contentItem: ColumnLayout {
            spacing: 12
            
            Text {
                text: qsTr("Column Statistics")
                font.bold: true
                font.pixelSize: 14
                color: textColor
                Layout.alignment: Qt.AlignHCenter
            }
            
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: borderColor
            }
            
            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 8
                Layout.fillWidth: true
                
                Text { text: qsTr("Column:"); color: Qt.darker(textColor, 1.3); font.pixelSize: 11 }
                Text { text: columnStatsDialog.stats ? columnStatsDialog.stats.name : ""; color: textColor; font.pixelSize: 11 }
                
                Text { text: qsTr("Total Rows:"); color: Qt.darker(textColor, 1.3); font.pixelSize: 11 }
                Text { text: columnStatsDialog.stats ? columnStatsDialog.stats.totalCount : ""; color: textColor; font.pixelSize: 11 }
                
                Text { text: qsTr("Unique Values:"); color: Qt.darker(textColor, 1.3); font.pixelSize: 11 }
                Text { text: columnStatsDialog.stats ? columnStatsDialog.stats.uniqueCount : ""; color: textColor; font.pixelSize: 11 }
                
                Text { text: qsTr("Empty Values:"); color: Qt.darker(textColor, 1.3); font.pixelSize: 11 }
                Text { text: columnStatsDialog.stats ? columnStatsDialog.stats.nullCount : ""; color: textColor; font.pixelSize: 11 }
                
                Text { 
                    text: qsTr("Min:"); 
                    color: Qt.darker(textColor, 1.3); 
                    font.pixelSize: 11
                    visible: columnStatsDialog.stats && columnStatsDialog.stats.type === 1
                }
                Text { 
                    text: columnStatsDialog.stats ? columnStatsDialog.stats.minValue : ""
                    color: textColor
                    font.pixelSize: 11
                    visible: columnStatsDialog.stats && columnStatsDialog.stats.type === 1
                }
                
                Text { 
                    text: qsTr("Max:"); 
                    color: Qt.darker(textColor, 1.3); 
                    font.pixelSize: 11
                    visible: columnStatsDialog.stats && columnStatsDialog.stats.type === 1
                }
                Text { 
                    text: columnStatsDialog.stats ? columnStatsDialog.stats.maxValue : ""
                    color: textColor
                    font.pixelSize: 11
                    visible: columnStatsDialog.stats && columnStatsDialog.stats.type === 1
                }
                
                Text { 
                    text: qsTr("Mean:"); 
                    color: Qt.darker(textColor, 1.3); 
                    font.pixelSize: 11
                    visible: columnStatsDialog.stats && columnStatsDialog.stats.mean !== undefined
                }
                Text { 
                    text: columnStatsDialog.stats && columnStatsDialog.stats.mean !== undefined ? columnStatsDialog.stats.mean.toFixed(2) : ""
                    color: textColor
                    font.pixelSize: 11
                    visible: columnStatsDialog.stats && columnStatsDialog.stats.mean !== undefined
                }
            }
            
            Item { Layout.fillHeight: true }
            
            Button {
                text: qsTr("Close")
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 80
                implicitHeight: 28
                onClicked: columnStatsDialog.close()
                background: Rectangle {
                    color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 11
                }
            }
        }
    }
    
    // 辅助函数
    function formatFileSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / 1024 / 1024).toFixed(2) + " MB"
        return (bytes / 1024 / 1024 / 1024).toFixed(2) + " GB"
    }
}
