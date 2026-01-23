import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * AI 日志分析对话框
 * 支持选择日志、自动脱敏预览、输入问题并获取 AI 分析结果
 */
Popup {
    id: root
    
    // 外部需要传入的属性
    property var selectedLines: []  // 选中的行号数组
    property var logModel: null     // BigFileModel 实例
    
    width: Math.min(parent.width * 0.85, 900)
    height: Math.min(parent.height * 0.9, 720)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    modal: true
    closePolicy: Popup.CloseOnEscape
    
    background: Rectangle {
        color: _themeManager.backgroundColor
        border.color: _themeManager.borderColor
        border.width: 1
        radius: 8
    }
    
    // 当前服务的可用模型列表
    property var currentModels: []
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10
        
        // ========== 标题栏 ==========
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            spacing: 12
            
            Label {
                text: qsTr("AI Log Analysis")
                font.pixelSize: 18
                font.bold: true
                color: _themeManager.textColor
            }
            
            Item { Layout.fillWidth: true }
            
            // AI 服务选择
            Label {
                text: qsTr("Service:")
                color: _themeManager.textColor
                font.pixelSize: 12
            }
            
            ComboBox {
                id: serviceCombo
                Layout.preferredWidth: 160
                font.pixelSize: 12
                model: _aiManager ? _aiManager.getAvailableServices() : []
                textRole: "name"
                valueRole: "id"
                
                currentIndex: {
                    if (!_aiManager) return 0
                    let services = _aiManager.getAvailableServices()
                    let current = _aiManager.currentServiceId
                    for (let i = 0; i < services.length; i++) {
                        if (services[i].id === current) return i
                    }
                    return 0
                }
                
                onActivated: {
                    if (_aiManager) {
                        _aiManager.setCurrentService(currentValue)
                        updateModelList()
                    }
                }
                
                Component.onCompleted: updateModelList()
            }
            
            // 模型选择
            Label {
                text: qsTr("Model:")
                color: _themeManager.textColor
                font.pixelSize: 12
            }
            
            ComboBox {
                id: modelCombo
                Layout.preferredWidth: 180
                font.pixelSize: 12
                model: root.currentModels
                
                onActivated: {
                    if (_aiManager && currentText) {
                        _aiManager.setModel(serviceCombo.currentValue, currentText)
                    }
                }
            }
            
            // 设置按钮
            Button {
                text: qsTr("Settings")
                font.pixelSize: 12
                implicitHeight: 28
                onClicked: aiSettingsDialog.open()
            }
            
            // 关闭按钮
            Button {
                text: "×"
                flat: true
                font.pixelSize: 16
                implicitWidth: 28
                implicitHeight: 28
                onClicked: root.close()
            }
        }
        
        // ========== 原始日志内容区域 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: originalExpanded.checked ? 130 : 32
            color: Qt.darker(_themeManager.backgroundColor, 1.03)
            border.color: _themeManager.borderColor
            radius: 4
            
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 150 } }
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                
                // 标题行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    CheckBox {
                        id: originalExpanded
                        checked: true
                        implicitWidth: 20
                        implicitHeight: 20
                    }
                    
                    Label {
                        text: qsTr("Original Log Content (%1 lines)").arg(selectedLines.length)
                        color: _themeManager.textColor
                        font.pixelSize: 13
                        font.bold: true
                    }
                    
                    Item { Layout.fillWidth: true }
                }
                
                // 内容区域
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: originalExpanded.checked
                    clip: true
                    
                    TextArea {
                        id: originalLogArea
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        font.family: "Consolas, Monaco, monospace"
                        font.pixelSize: 12
                        color: _themeManager.textColor
                        
                        background: Rectangle {
                            color: "transparent"
                        }
                    }
                }
            }
        }
        
        // ========== 脱敏后的日志内容区域 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: sanitizedExpanded.checked ? 130 : 32
            color: Qt.darker(_themeManager.backgroundColor, 1.03)
            border.color: _themeManager.borderColor
            radius: 4
            
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 150 } }
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                
                // 标题行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    CheckBox {
                        id: sanitizedExpanded
                        checked: true
                        implicitWidth: 20
                        implicitHeight: 20
                    }
                    
                    Label {
                        text: qsTr("Sanitized Content (sent to AI)")
                        color: _themeManager.textColor
                        font.pixelSize: 13
                        font.bold: true
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    // 脱敏级别选择
                    Label {
                        text: qsTr("Level:")
                        color: _themeManager.secondaryTextColor
                        font.pixelSize: 11
                    }
                    
                    ComboBox {
                        id: sanitizeLevel
                        Layout.preferredWidth: 100
                        font.pixelSize: 11
                        implicitHeight: 24
                        model: [
                            { text: qsTr("Standard"), value: "standard" },
                            { text: qsTr("Strict"), value: "strict" },
                            { text: qsTr("Minimal"), value: "minimal" }
                        ]
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: 0
                        
                        onCurrentValueChanged: updateSanitizedPreview()
                    }
                }
                
                // 内容区域
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: sanitizedExpanded.checked
                    clip: true
                    
                    TextArea {
                        id: sanitizedLogArea
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        font.family: "Consolas, Monaco, monospace"
                        font.pixelSize: 12
                        color: _themeManager.textColor
                        
                        background: Rectangle {
                            color: "transparent"
                        }
                    }
                }
            }
        }
        
        // ========== 问题输入区域 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: Qt.darker(_themeManager.backgroundColor, 1.03)
            border.color: _themeManager.borderColor
            radius: 4
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6
                
                Label {
                    text: qsTr("Your Question")
                    color: _themeManager.textColor
                    font.pixelSize: 13
                    font.bold: true
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    TextField {
                        id: questionInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        placeholderText: qsTr("Enter your question about this log...")
                        font.pixelSize: 12
                        color: _themeManager.textColor
                        
                        background: Rectangle {
                            color: _themeManager.backgroundColor
                            border.color: questionInput.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                            radius: 4
                        }
                        
                        onAccepted: sendQuery()
                    }
                    
                    // 快捷问题按钮
                    Button {
                        text: qsTr("Presets")
                        font.pixelSize: 12
                        implicitHeight: 32
                        onClicked: presetMenu.open()
                        
                        Menu {
                            id: presetMenu
                            
                            MenuItem {
                                text: qsTr("What does this error mean?")
                                onTriggered: questionInput.text = text
                            }
                            MenuItem {
                                text: qsTr("What is the root cause?")
                                onTriggered: questionInput.text = text
                            }
                            MenuItem {
                                text: qsTr("How to fix this issue?")
                                onTriggered: questionInput.text = text
                            }
                            MenuItem {
                                text: qsTr("Summarize this log")
                                onTriggered: questionInput.text = text
                            }
                            MenuItem {
                                text: qsTr("Find anomalies or patterns")
                                onTriggered: questionInput.text = text
                            }
                        }
                    }
                    
                    Button {
                        id: sendButton
                        text: _aiManager && _aiManager.isAnalyzing ? qsTr("Cancel") : qsTr("Ask AI")
                        font.pixelSize: 12
                        implicitHeight: 32
                        highlighted: true
                        enabled: questionInput.text.trim().length > 0 || (_aiManager && _aiManager.isAnalyzing)
                        
                        onClicked: {
                            if (_aiManager && _aiManager.isAnalyzing) {
                                _aiManager.cancelAnalysis()
                            } else {
                                sendQuery()
                            }
                        }
                    }
                }
            }
        }
        
        // ========== AI 响应区域 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.darker(_themeManager.backgroundColor, 1.03)
            border.color: _themeManager.borderColor
            radius: 4
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6
                
                // 标题行
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: qsTr("AI Response")
                        color: _themeManager.textColor
                        font.pixelSize: 13
                        font.bold: true
                    }
                    
                    // 加载指示器
                    BusyIndicator {
                        running: _aiManager && _aiManager.isAnalyzing
                        visible: running
                        implicitWidth: 20
                        implicitHeight: 20
                    }
                    
                    Label {
                        text: qsTr("Analyzing...")
                        color: _themeManager.secondaryTextColor
                        font.pixelSize: 11
                        visible: _aiManager && _aiManager.isAnalyzing
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    Label {
                        id: statusLabel
                        color: _themeManager.secondaryTextColor
                        font.pixelSize: 11
                    }
                }
                
                // 响应内容
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    TextArea {
                        id: responseArea
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        font.pixelSize: 13
                        color: _themeManager.textColor
                        textFormat: TextEdit.MarkdownText
                        
                        background: Rectangle {
                            color: "transparent"
                        }
                    }
                }
                
                // 底部操作栏
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    Item { Layout.fillWidth: true }
                    
                    Button {
                        text: qsTr("Copy Response")
                        font.pixelSize: 11
                        implicitHeight: 26
                        enabled: responseArea.text.length > 0
                        onClicked: {
                            responseArea.selectAll()
                            responseArea.copy()
                            responseArea.deselect()
                            statusLabel.text = qsTr("Copied to clipboard")
                        }
                    }
                    
                    Button {
                        text: qsTr("Clear")
                        font.pixelSize: 11
                        implicitHeight: 26
                        onClicked: {
                            responseArea.text = ""
                            statusLabel.text = ""
                        }
                    }
                }
            }
        }
    }
    
    // AI 设置对话框
    AISettingsDialog {
        id: aiSettingsDialog
    }
    
    // 信号连接
    Connections {
        target: _aiManager
        
        function onAnalysisResponse(response, isComplete) {
            responseArea.text = response
        }
        
        function onAnalysisCompleted(response) {
            statusLabel.text = qsTr("Analysis completed")
        }
        
        function onAnalysisError(error) {
            responseArea.text = qsTr("Error: %1").arg(error)
            statusLabel.text = qsTr("Analysis failed")
        }
        
        function onCurrentServiceChanged() {
            updateModelList()
        }
    }
    
    // 打开时加载数据
    onOpened: {
        loadSelectedLogs()
        updateModelList()
    }
    
    // 更新模型列表
    function updateModelList() {
        if (!_aiManager) {
            root.currentModels = []
            return
        }
        
        let serviceId = serviceCombo.currentValue || _aiManager.currentServiceId
        root.currentModels = _aiManager.getModelsForService(serviceId)
        
        // 选择当前模型
        let currentModel = _aiManager.getCurrentModel(serviceId)
        for (let i = 0; i < root.currentModels.length; i++) {
            if (root.currentModels[i] === currentModel) {
                modelCombo.currentIndex = i
                break
            }
        }
    }
    
    // 加载选中的日志内容
    function loadSelectedLogs() {
        if (!logModel || selectedLines.length === 0) {
            originalLogArea.text = qsTr("No lines selected")
            return
        }
        
        let content = []
        for (let i = 0; i < selectedLines.length && i < 100; i++) {
            let row = selectedLines[i]
            let line = logModel.data(logModel.index(row, 0), 0)
            if (line) {
                content.push(line)
            }
        }
        
        originalLogArea.text = content.join("\n")
        updateSanitizedPreview()
    }
    
    // 更新脱敏预览
    function updateSanitizedPreview() {
        if (!_dataSanitizer) {
            sanitizedLogArea.text = originalLogArea.text
            return
        }
        
        sanitizedLogArea.text = _dataSanitizer.sanitize(originalLogArea.text)
    }
    
    // 发送查询
    function sendQuery() {
        if (!_aiManager) {
            responseArea.text = qsTr("AI Manager not available")
            return
        }
        
        let question = questionInput.text.trim()
        if (question.length === 0) return
        
        let sanitizedContent = sanitizedLogArea.text
        if (sanitizedContent.length === 0) {
            responseArea.text = qsTr("No content to analyze")
            return
        }
        
        // 检查 API Key
        let currentService = _aiManager.currentServiceId
        if (!_aiManager.hasApiKey(currentService)) {
            responseArea.text = qsTr("Please configure API Key in Settings first")
            return
        }
        
        responseArea.text = ""
        statusLabel.text = qsTr("Sending request...")
        
        _aiManager.analyzeLog(sanitizedContent, question)
    }
}
