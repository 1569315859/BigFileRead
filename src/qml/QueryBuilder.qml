import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * QueryBuilder.qml - 可视化查询构建器
 * 
 * 提供类SQL的可视化过滤条件构建界面，无需用户编写SQL
 * 支持:
 * - 多条件组合 (AND/OR)
 * - 多种操作符 (等于、包含、大于、小于等)
 * - 实时预览匹配行数
 */
Item {
    id: root
    
    // 数据源 (CSVDataSource 或其他支持过滤的模型)
    property var dataSource: null
    
    // 可用的列列表
    property var columns: dataSource ? dataSource.getColumns() : []
    
    // 当前查询条件
    property var conditions: []
    
    // 条件组合方式
    property string combineMode: "AND"  // "AND" 或 "OR"
    
    // 预览匹配数
    property int matchCount: 0
    property int totalCount: dataSource ? dataSource.totalRowCount : 0
    
    // 主题颜色
    property color bgColor: "#1E1E1E"
    property color panelColor: "#252526"
    property color textColor: "#D4D4D4"
    property color accentColor: "#007ACC"
    property color borderColor: "#3C3C3C"
    property color errorColor: "#F44336"
    property color successColor: "#4CAF50"
    
    // 信号
    signal queryApplied(var conditions)
    signal queryCleared()
    
    // 操作符定义
    readonly property var operatorList: [
        { value: "equals", label: qsTr("Equals"), icon: "=" },
        { value: "notEquals", label: qsTr("Not Equals"), icon: "≠" },
        { value: "contains", label: qsTr("Contains"), icon: "∋" },
        { value: "notContains", label: qsTr("Not Contains"), icon: "∌" },
        { value: "startsWith", label: qsTr("Starts With"), icon: "^" },
        { value: "endsWith", label: qsTr("Ends With"), icon: "$" },
        { value: "greaterThan", label: qsTr("Greater Than"), icon: ">" },
        { value: "lessThan", label: qsTr("Less Than"), icon: "<" },
        { value: "greaterOrEqual", label: qsTr("Greater or Equal"), icon: "≥" },
        { value: "lessOrEqual", label: qsTr("Less or Equal"), icon: "≤" },
        { value: "isEmpty", label: qsTr("Is Empty"), icon: "∅" },
        { value: "isNotEmpty", label: qsTr("Is Not Empty"), icon: "≢∅" },
        { value: "regex", label: qsTr("Regex Match"), icon: ".*" }
    ]
    
    // 根据列类型过滤可用操作符
    function getOperatorsForColumn(columnIndex) {
        if (!dataSource || columnIndex < 0) return operatorList
        
        var meta = dataSource.getColumnMeta(columnIndex)
        if (!meta) return operatorList
        
        // 数字类型列可以使用比较操作符
        if (meta.type === 1) {  // NumberType
            return operatorList
        }
        
        // 文本类型列主要使用字符串操作符
        return operatorList.filter(function(op) {
            return ["equals", "notEquals", "contains", "notContains", 
                    "startsWith", "endsWith", "isEmpty", "isNotEmpty", "regex"].indexOf(op.value) >= 0
        })
    }
    
    // 添加条件
    function addCondition() {
        var newCondition = {
            id: Date.now(),
            columnIndex: 0,
            operator: "contains",
            value: "",
            enabled: true
        }
        conditions = conditions.concat([newCondition])
        updatePreview()
    }
    
    // 移除条件
    function removeCondition(conditionId) {
        conditions = conditions.filter(function(c) { return c.id !== conditionId })
        updatePreview()
    }
    
    // 更新条件
    function updateCondition(conditionId, field, value) {
        conditions = conditions.map(function(c) {
            if (c.id === conditionId) {
                var updated = Object.assign({}, c)
                updated[field] = value
                return updated
            }
            return c
        })
        updatePreview()
    }
    
    // 清空所有条件
    function clearAllConditions() {
        conditions = []
        matchCount = totalCount
        queryCleared()
    }
    
    // 应用查询
    function applyQuery() {
        if (!dataSource) return
        
        // 清除之前的过滤
        dataSource.clearAllFilters()
        
        // 应用每个条件
        var enabledConditions = conditions.filter(function(c) { return c.enabled && c.value !== "" })
        
        if (enabledConditions.length === 0) {
            matchCount = totalCount
            queryApplied([])
            return
        }
        
        // 对于单个条件，直接应用
        // 对于多个条件，需要在数据源层面实现组合逻辑
        for (var i = 0; i < enabledConditions.length; i++) {
            var cond = enabledConditions[i]
            dataSource.applyColumnFilter(cond.columnIndex, cond.value, cond.operator)
        }
        
        matchCount = dataSource.totalRowCount
        queryApplied(enabledConditions)
    }
    
    // 更新预览
    function updatePreview() {
        if (!dataSource) {
            matchCount = 0
            return
        }
        
        var enabledConditions = conditions.filter(function(c) { 
            return c.enabled && c.value !== "" 
        })
        
        if (enabledConditions.length === 0) {
            matchCount = totalCount
            return
        }
        
        // 简单预览：只计算第一个条件的匹配数
        // 完整实现需要在数据源层面支持多条件组合查询
        var firstCond = enabledConditions[0]
        var results = dataSource.searchInColumn(firstCond.columnIndex, firstCond.value, false)
        matchCount = results.length
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // ========== 标题栏 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: panelColor
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12
                
                Text {
                    text: qsTr("Query Builder")
                    color: textColor
                    font.bold: true
                    font.pixelSize: 14
                }
                
                Rectangle { width: 1; height: 24; color: borderColor }
                
                // 组合模式选择
                Row {
                    spacing: 8
                    
                    Text {
                        text: qsTr("Combine:")
                        color: Qt.darker(textColor, 1.3)
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    ButtonGroup { id: combineModeGroup }
                    
                    RadioButton {
                        text: "AND"
                        checked: combineMode === "AND"
                        ButtonGroup.group: combineModeGroup
                        onCheckedChanged: if (checked) combineMode = "AND"
                        
                        indicator: Rectangle {
                            implicitWidth: 14
                            implicitHeight: 14
                            radius: 7
                            border.color: borderColor
                            color: bgColor
                            
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                anchors.centerIn: parent
                                color: accentColor
                                visible: parent.parent.checked
                            }
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.checked ? accentColor : textColor
                            font.pixelSize: 11
                            font.bold: parent.checked
                            leftPadding: parent.indicator.width + 4
                        }
                    }
                    
                    RadioButton {
                        text: "OR"
                        checked: combineMode === "OR"
                        ButtonGroup.group: combineModeGroup
                        onCheckedChanged: if (checked) combineMode = "OR"
                        
                        indicator: Rectangle {
                            implicitWidth: 14
                            implicitHeight: 14
                            radius: 7
                            border.color: borderColor
                            color: bgColor
                            
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                anchors.centerIn: parent
                                color: accentColor
                                visible: parent.parent.checked
                            }
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.checked ? accentColor : textColor
                            font.pixelSize: 11
                            font.bold: parent.checked
                            leftPadding: parent.indicator.width + 4
                        }
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // 匹配预览
                Rectangle {
                    implicitWidth: previewText.width + 16
                    implicitHeight: 24
                    radius: 4
                    color: matchCount > 0 ? Qt.rgba(successColor.r, successColor.g, successColor.b, 0.2) : Qt.rgba(errorColor.r, errorColor.g, errorColor.b, 0.2)
                    border.color: matchCount > 0 ? successColor : errorColor
                    border.width: 1
                    
                    Text {
                        id: previewText
                        text: qsTr("%1 / %2 rows").arg(matchCount).arg(totalCount)
                        color: matchCount > 0 ? successColor : errorColor
                        font.pixelSize: 11
                        font.bold: true
                        anchors.centerIn: parent
                    }
                }
            }
        }
        
        // ========== 条件列表 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: bgColor
            
            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                
                ColumnLayout {
                    width: parent.width
                    spacing: 8
                    
                    // 条件项
                    Repeater {
                        model: conditions
                        
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            color: panelColor
                            radius: 4
                            border.color: modelData.enabled ? borderColor : Qt.darker(borderColor, 1.5)
                            opacity: modelData.enabled ? 1.0 : 0.6
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                
                                // 启用开关
                                CheckBox {
                                    checked: modelData.enabled
                                    onCheckedChanged: updateCondition(modelData.id, "enabled", checked)
                                    
                                    indicator: Rectangle {
                                        implicitWidth: 16
                                        implicitHeight: 16
                                        radius: 3
                                        border.color: borderColor
                                        color: bgColor
                                        
                                        Text {
                                            text: "✓"
                                            color: accentColor
                                            font.pixelSize: 12
                                            anchors.centerIn: parent
                                            visible: parent.parent.checked
                                        }
                                    }
                                }
                                
                                // 列选择
                                ComboBox {
                                    id: columnCombo
                                    model: columns
                                    textRole: "name"
                                    currentIndex: modelData.columnIndex
                                    implicitWidth: 150
                                    implicitHeight: 28
                                    
                                    onCurrentIndexChanged: {
                                        if (currentIndex >= 0) {
                                            updateCondition(modelData.id, "columnIndex", currentIndex)
                                        }
                                    }
                                    
                                    background: Rectangle {
                                        color: bgColor
                                        border.color: borderColor
                                        radius: 3
                                    }
                                    contentItem: Text {
                                        text: columnCombo.displayText
                                        color: textColor
                                        font.pixelSize: 11
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 6
                                        elide: Text.ElideRight
                                    }
                                }
                                
                                // 操作符选择
                                ComboBox {
                                    id: operatorCombo
                                    model: getOperatorsForColumn(modelData.columnIndex)
                                    textRole: "label"
                                    implicitWidth: 130
                                    implicitHeight: 28
                                    
                                    currentIndex: {
                                        var ops = getOperatorsForColumn(modelData.columnIndex)
                                        for (var i = 0; i < ops.length; i++) {
                                            if (ops[i].value === modelData.operator) return i
                                        }
                                        return 0
                                    }
                                    
                                    onCurrentIndexChanged: {
                                        var ops = getOperatorsForColumn(modelData.columnIndex)
                                        if (currentIndex >= 0 && currentIndex < ops.length) {
                                            updateCondition(modelData.id, "operator", ops[currentIndex].value)
                                        }
                                    }
                                    
                                    background: Rectangle {
                                        color: bgColor
                                        border.color: borderColor
                                        radius: 3
                                    }
                                    contentItem: Row {
                                        spacing: 4
                                        leftPadding: 6
                                        Text {
                                            text: {
                                                var ops = getOperatorsForColumn(modelData.columnIndex)
                                                if (operatorCombo.currentIndex >= 0 && operatorCombo.currentIndex < ops.length) {
                                                    return ops[operatorCombo.currentIndex].icon
                                                }
                                                return ""
                                            }
                                            color: accentColor
                                            font.pixelSize: 11
                                            font.bold: true
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Text {
                                            text: operatorCombo.displayText
                                            color: textColor
                                            font.pixelSize: 11
                                            anchors.verticalCenter: parent.verticalCenter
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                                
                                // 值输入
                                TextField {
                                    id: valueInput
                                    text: modelData.value
                                    placeholderText: qsTr("Enter value...")
                                    Layout.fillWidth: true
                                    implicitHeight: 28
                                    
                                    // 对于 isEmpty/isNotEmpty 操作符禁用输入
                                    enabled: modelData.operator !== "isEmpty" && modelData.operator !== "isNotEmpty"
                                    
                                    onTextChanged: {
                                        updateCondition(modelData.id, "value", text)
                                    }
                                    
                                    background: Rectangle {
                                        color: bgColor
                                        border.color: valueInput.activeFocus ? accentColor : borderColor
                                        radius: 3
                                    }
                                    color: textColor
                                    font.pixelSize: 11
                                }
                                
                                // 删除按钮
                                Button {
                                    text: "✕"
                                    implicitWidth: 28
                                    implicitHeight: 28
                                    
                                    onClicked: removeCondition(modelData.id)
                                    
                                    background: Rectangle {
                                        color: parent.hovered ? errorColor : "transparent"
                                        radius: 3
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: parent.hovered ? "white" : textColor
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                    
                    // 添加条件按钮
                    Button {
                        text: qsTr("+ Add Condition")
                        Layout.alignment: Qt.AlignHCenter
                        implicitHeight: 32
                        implicitWidth: 150
                        
                        onClicked: addCondition()
                        
                        background: Rectangle {
                            color: parent.hovered ? accentColor : "transparent"
                            border.color: accentColor
                            border.width: 1
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? "white" : accentColor
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    
                    // 空状态提示
                    Text {
                        text: qsTr("No conditions defined.\nClick '+ Add Condition' to start filtering.")
                        color: Qt.darker(textColor, 1.5)
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 20
                        visible: conditions.length === 0
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
        }
        
        // ========== 底部操作栏 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: panelColor
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12
                
                // 快速过滤提示
                Text {
                    text: conditions.length > 0 
                        ? qsTr("%1 condition(s)").arg(conditions.filter(function(c) { return c.enabled }).length)
                        : qsTr("No active filters")
                    color: Qt.darker(textColor, 1.3)
                    font.pixelSize: 11
                }
                
                Item { Layout.fillWidth: true }
                
                // 清空按钮
                Button {
                    text: qsTr("Clear All")
                    implicitHeight: 32
                    implicitWidth: 90
                    enabled: conditions.length > 0
                    
                    onClicked: clearAllConditions()
                    
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? Qt.darker(panelColor, 1.2) : panelColor) : Qt.darker(panelColor, 1.5)
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? textColor : Qt.darker(textColor, 1.5)
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                // 应用按钮
                Button {
                    text: qsTr("Apply Filter")
                    implicitHeight: 32
                    implicitWidth: 110
                    enabled: conditions.some(function(c) { return c.enabled && c.value !== "" })
                    
                    onClicked: applyQuery()
                    
                    background: Rectangle {
                        color: parent.enabled 
                            ? (parent.hovered ? Qt.lighter(accentColor, 1.1) : accentColor)
                            : Qt.darker(panelColor, 1.3)
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "white" : Qt.darker(textColor, 1.5)
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
    
    // 初始化
    Component.onCompleted: {
        matchCount = totalCount
    }
    
    // 数据源变化时更新
    onDataSourceChanged: {
        conditions = []
        matchCount = totalCount
    }
}
