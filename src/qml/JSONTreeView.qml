import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * JSONTreeView.qml - JSON树形视图组件
 * 提供可折叠的树形结构展示JSON数据，支持搜索和路径导航
 */
Item {
    id: root
    
    // 数据源
    property var treeModel: null   // JSONTreeModel 实例
    property var tableModel: null  // JSONLTableModel 实例 (用于JSONL模式)
    
    // 主题颜色
    property color bgColor: "#1E1E1E"
    property color panelColor: "#252526"
    property color textColor: "#D4D4D4"
    property color accentColor: "#007ACC"
    property color borderColor: "#3C3C3C"
    property color keyColor: "#9CDCFE"      // JSON键名颜色
    property color stringColor: "#CE9178"   // 字符串值颜色
    property color numberColor: "#B5CEA8"   // 数字值颜色
    property color boolColor: "#569CD6"     // 布尔值颜色
    property color nullColor: "#569CD6"     // null值颜色
    property color bracketColor: "#FFD700"  // 括号颜色
    property color selectedBgColor: "#094771"
    property color hoverBgColor: "#2A2D2E"
    
    // 配置
    property int nodeHeight: 24
    property int indentWidth: 20
    property bool showLineNumbers: true
    property bool wordWrap: false
    
    // 状态
    property var selectedNode: null
    property string selectedPath: ""
    property var expandedNodes: ({})
    
    // 信号
    signal nodeClicked(var node, string path)
    signal nodeDoubleClicked(var node, string path)
    signal pathCopied(string path)
    signal valueCopied(string value)
    
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
                    text: treeModel ? treeModel.filePath.split('/').pop().split('\\').pop() : ""
                    color: textColor
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: 200
                }
                
                Rectangle { width: 1; height: 20; color: borderColor }
                
                // 节点统计
                Text {
                    text: treeModel ? qsTr("%1 nodes").arg(treeModel.totalNodes) : ""
                    color: Qt.darker(textColor, 1.3)
                    font.pixelSize: 11
                }
                
                // JSONL标识
                Rectangle {
                    visible: treeModel && treeModel.isJsonl
                    color: accentColor
                    radius: 3
                    implicitWidth: jsonlLabel.width + 12
                    implicitHeight: 18
                    
                    Text {
                        id: jsonlLabel
                        text: "JSONL"
                        color: "white"
                        font.pixelSize: 10
                        font.bold: true
                        anchors.centerIn: parent
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // 搜索框
                Rectangle {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 26
                    color: bgColor
                    border.color: searchInput.activeFocus ? accentColor : borderColor
                    radius: 3
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 4
                        
                        Text {
                            text: "🔍"
                            font.pixelSize: 12
                        }
                        
                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            color: textColor
                            font.pixelSize: 11
                            clip: true
                            selectByMouse: true
                            
                            property string placeholder: qsTr("Search... ($.path or value)")
                            
                            Text {
                                text: parent.placeholder
                                color: Qt.darker(textColor, 1.5)
                                font.pixelSize: 11
                                visible: !parent.text && !parent.activeFocus
                            }
                            
                            onAccepted: performSearch()
                        }
                        
                        // 搜索类型选择
                        ComboBox {
                            id: searchTypeCombo
                            model: [
                                { text: qsTr("Path"), value: "path" },
                                { text: qsTr("Value"), value: "value" },
                                { text: qsTr("Key"), value: "key" }
                            ]
                            textRole: "text"
                            implicitWidth: 70
                            implicitHeight: 22
                            
                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Text {
                                text: searchTypeCombo.displayText
                                color: accentColor
                                font.pixelSize: 10
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
                
                // 搜索结果导航
                Row {
                    visible: searchResults.length > 0
                    spacing: 4
                    
                    Text {
                        text: qsTr("%1/%2").arg(currentSearchIndex + 1).arg(searchResults.length)
                        color: textColor
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    Button {
                        text: "▲"
                        implicitWidth: 24
                        implicitHeight: 22
                        onClicked: navigateSearchResult(-1)
                        background: Rectangle {
                            color: parent.hovered ? hoverBgColor : "transparent"
                            radius: 2
                        }
                        contentItem: Text {
                            text: parent.text
                            color: textColor
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    
                    Button {
                        text: "▼"
                        implicitWidth: 24
                        implicitHeight: 22
                        onClicked: navigateSearchResult(1)
                        background: Rectangle {
                            color: parent.hovered ? hoverBgColor : "transparent"
                            radius: 2
                        }
                        contentItem: Text {
                            text: parent.text
                            color: textColor
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                
                Rectangle { width: 1; height: 20; color: borderColor }
                
                // 展开/折叠按钮
                Button {
                    text: qsTr("Expand All")
                    implicitHeight: 26
                    implicitWidth: 80
                    onClicked: expandAllNodes()
                    background: Rectangle {
                        color: parent.down ? Qt.darker(panelColor, 1.2) : (parent.hovered ? hoverBgColor : "transparent")
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
                }
                
                Button {
                    text: qsTr("Collapse")
                    implicitHeight: 26
                    implicitWidth: 70
                    onClicked: collapseAllNodes()
                    background: Rectangle {
                        color: parent.down ? Qt.darker(panelColor, 1.2) : (parent.hovered ? hoverBgColor : "transparent")
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
                }
            }
        }
        
        // ========== 路径面包屑 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: selectedPath ? 28 : 0
            color: Qt.darker(panelColor, 1.1)
            visible: selectedPath !== ""
            
            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 150 }
            }
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4
                
                Text {
                    text: "📍"
                    font.pixelSize: 11
                }
                
                // 路径分段显示
                Flow {
                    Layout.fillWidth: true
                    spacing: 2
                    
                    Repeater {
                        model: selectedPath.split('.').filter(function(s) { return s !== '$' && s !== '' })
                        
                        Row {
                            spacing: 2
                            
                            Text {
                                text: index > 0 ? "›" : ""
                                color: Qt.darker(textColor, 1.5)
                                font.pixelSize: 11
                                visible: index > 0
                            }
                            
                            Text {
                                text: modelData
                                color: accentColor
                                font.pixelSize: 11
                                
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        // 导航到该路径
                                        var pathParts = selectedPath.split('.').slice(0, index + 2)
                                        navigateToPath(pathParts.join('.'))
                                    }
                                }
                            }
                        }
                    }
                }
                
                // 复制路径按钮
                Button {
                    text: "📋"
                    implicitWidth: 24
                    implicitHeight: 22
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Copy Path")
                    
                    onClicked: {
                        // 复制到剪贴板（通过C++实现）
                        pathCopied(selectedPath)
                    }
                    
                    background: Rectangle {
                        color: parent.hovered ? hoverBgColor : "transparent"
                        radius: 2
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        
        // ========== 树形视图区域 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: bgColor
            clip: true
            
            ScrollView {
                anchors.fill: parent
                
                TreeView {
                    id: treeView
                    model: treeModel
                    
                    delegate: Item {
                        id: nodeDelegate
                        implicitWidth: treeView.width
                        implicitHeight: nodeHeight
                        
                        required property int depth
                        required property bool expanded
                        required property bool hasChildren
                        required property var model
                        required property int row
                        required property bool current
                        
                        // 悬停状态
                        property bool isHovered: nodeMouseArea.containsMouse
                        
                        Rectangle {
                            anchors.fill: parent
                            color: {
                                if (current) return selectedBgColor
                                if (isHovered) return hoverBgColor
                                return "transparent"
                            }
                        }
                        
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: depth * indentWidth + 8
                            spacing: 4
                            
                            // 展开/折叠图标
                            Text {
                                text: hasChildren ? (expanded ? "▼" : "▶") : " "
                                color: textColor
                                font.pixelSize: 10
                                width: 16
                                anchors.verticalCenter: parent.verticalCenter
                                
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        if (hasChildren) {
                                            treeView.toggleExpanded(row)
                                        }
                                    }
                                }
                            }
                            
                            // 类型图标
                            Text {
                                text: {
                                    switch (model.type) {
                                        case 4: return "[]"  // Array
                                        case 5: return "{}"  // Object
                                        case 1: return "☑"   // Bool
                                        case 2: return "#"   // Number
                                        case 3: return "\""  // String
                                        default: return "∅"  // Null
                                    }
                                }
                                color: {
                                    switch (model.type) {
                                        case 4:
                                        case 5: return bracketColor
                                        default: return Qt.darker(textColor, 1.3)
                                    }
                                }
                                font.pixelSize: 10
                                font.bold: true
                                width: 18
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            
                            // 键名
                            Text {
                                text: model.key ? model.key + ":" : ""
                                color: keyColor
                                font.family: "Consolas"
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                                visible: model.key !== undefined && model.key !== ""
                            }
                            
                            // 值
                            Text {
                                text: model.value || ""
                                color: {
                                    switch (model.type) {
                                        case 3: return stringColor   // String
                                        case 2: return numberColor   // Number
                                        case 1: return boolColor     // Bool
                                        case 0: return nullColor     // Null
                                        case 4:
                                        case 5: return Qt.darker(textColor, 1.3)  // Array/Object
                                        default: return textColor
                                    }
                                }
                                font.family: "Consolas"
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                
                                Layout.fillWidth: true
                            }
                            
                            // 子节点数量（仅数组和对象）
                            Text {
                                text: model.childCount > 0 ? "(" + model.childCount + ")" : ""
                                color: Qt.darker(textColor, 1.5)
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                                visible: model.type === 4 || model.type === 5
                            }
                        }
                        
                        MouseArea {
                            id: nodeMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            
                            onClicked: function(mouse) {
                                treeView.selectionModel.select(
                                    treeView.model.index(row, 0),
                                    ItemSelectionModel.ClearAndSelect
                                )
                                selectedPath = model.path || ""
                                selectedNode = model
                                
                                if (mouse.button === Qt.RightButton) {
                                    nodeContextMenu.popup()
                                } else {
                                    nodeClicked(model, selectedPath)
                                }
                            }
                            
                            onDoubleClicked: {
                                if (hasChildren) {
                                    treeView.toggleExpanded(row)
                                }
                                nodeDoubleClicked(model, selectedPath)
                            }
                        }
                    }
                }
            }
            
            // 加载中遮罩
            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(0, 0, 0, 0.7)
                visible: treeModel && treeModel.isLoading
                
                Column {
                    anchors.centerIn: parent
                    spacing: 16
                    
                    BusyIndicator {
                        running: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    
                    Text {
                        text: qsTr("Loading JSON...")
                        color: textColor
                        font.pixelSize: 14
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            
            // 空状态
            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: !treeModel || (treeModel.totalNodes === 0 && !treeModel.isLoading)
                
                Text {
                    text: "{ }"
                    color: Qt.darker(textColor, 1.5)
                    font.pixelSize: 48
                    font.family: "Consolas"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                
                Text {
                    text: qsTr("No JSON data loaded.\nDrag and drop a JSON file here.")
                    color: Qt.darker(textColor, 1.5)
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
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
                
                // 选中节点路径
                Text {
                    text: selectedPath || qsTr("No selection")
                    color: selectedPath ? textColor : Qt.darker(textColor, 1.5)
                    font.family: "Consolas"
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                
                // 节点类型
                Text {
                    text: selectedNode ? selectedNode.typeName : ""
                    color: accentColor
                    font.pixelSize: 11
                    visible: selectedNode !== null
                }
            }
        }
    }
    
    // ========== 右键菜单 ==========
    Menu {
        id: nodeContextMenu
        
        MenuItem {
            text: qsTr("Copy Value")
            onTriggered: {
                if (selectedNode) {
                    valueCopied(selectedNode.value || "")
                }
            }
        }
        MenuItem {
            text: qsTr("Copy Path")
            onTriggered: {
                pathCopied(selectedPath)
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Expand All Children")
            enabled: selectedNode && selectedNode.hasChildren
            onTriggered: {
                // 展开所有子节点
            }
        }
        MenuItem {
            text: qsTr("Collapse All Children")
            enabled: selectedNode && selectedNode.hasChildren
            onTriggered: {
                // 折叠所有子节点
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Filter by this value")
            enabled: selectedNode && (selectedNode.type === 2 || selectedNode.type === 3)
            onTriggered: {
                if (selectedNode) {
                    searchInput.text = selectedNode.value || ""
                    performSearch()
                }
            }
        }
    }
    
    // ========== 搜索功能 ==========
    property var searchResults: []
    property int currentSearchIndex: -1
    
    function performSearch() {
        if (!treeModel || !searchInput.text) {
            searchResults = []
            currentSearchIndex = -1
            return
        }
        
        var type = searchTypeCombo.model[searchTypeCombo.currentIndex].value
        var query = searchInput.text
        
        switch (type) {
            case "path":
                searchResults = treeModel.searchByPath(query)
                break
            case "value":
                searchResults = treeModel.searchByValue(query)
                break
            case "key":
                searchResults = treeModel.searchByKey(query)
                break
        }
        
        if (searchResults.length > 0) {
            currentSearchIndex = 0
            navigateToSearchResult(0)
        } else {
            currentSearchIndex = -1
        }
    }
    
    function navigateSearchResult(delta) {
        if (searchResults.length === 0) return
        
        currentSearchIndex += delta
        if (currentSearchIndex < 0) {
            currentSearchIndex = searchResults.length - 1
        } else if (currentSearchIndex >= searchResults.length) {
            currentSearchIndex = 0
        }
        
        navigateToSearchResult(currentSearchIndex)
    }
    
    function navigateToSearchResult(index) {
        if (index < 0 || index >= searchResults.length) return
        
        var result = searchResults[index]
        selectedPath = result.path || ""
        
        // 展开到该路径并滚动到可见
        // (需要通过TreeView的API实现)
    }
    
    function navigateToPath(path) {
        var results = treeModel.searchByPath(path)
        if (results.length > 0) {
            selectedPath = results[0].path
        }
    }
    
    function expandAllNodes() {
        // 通过模型的expandAll方法
        if (treeModel) {
            treeModel.expandAll()
        }
    }
    
    function collapseAllNodes() {
        if (treeModel) {
            treeModel.collapseAll()
        }
    }
}
