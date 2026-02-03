/**
 * LocalLLMSettings.qml
 * 本地 LLM 模型设置界面
 * 
 * 功能：
 * - 选择本地 GGUF 模型文件
 * - 配置模型参数（上下文长度、GPU层数等）
 * - 测试模型连接
 * - 管理多个模型配置
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    title: qsTr("本地 AI 模型设置")
    width: 700
    height: 600
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok | Dialog.Cancel | Dialog.Apply
    
    // 主题颜色
    property color textColor: _themeManager ? _themeManager.textColor : palette.text
    property color panelColor: _themeManager ? _themeManager.panelBackground : palette.window
    property color bgColor: _themeManager ? _themeManager.backgroundColor : palette.base
    property color accentColor: _themeManager ? _themeManager.accentColor : palette.highlight
    property color borderColor: _themeManager ? _themeManager.borderColor : palette.mid
    
    palette.text: textColor
    palette.windowText: textColor
    palette.window: panelColor
    palette.base: bgColor
    palette.highlight: accentColor
    palette.buttonText: textColor
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    // 信号
    signal settingsApplied()
    
    // 属性
    property var localLLMEngine: null
    property bool modelLoaded: localLLMEngine ? localLLMEngine.isModelLoaded : false
    property bool isGenerating: localLLMEngine ? localLLMEngine.isGenerating : false
    
    // 当前配置
    property string modelPath: ""
    property int contextLength: 4096
    property int gpuLayers: 0
    property double temperature: 0.7
    property double topP: 0.9
    property int topK: 40
    property int threads: 4
    property string systemPrompt: qsTr("你是一个数据分析助手，帮助用户理解和分析日志文件、CSV数据等。")
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16
        
        // 标签页
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            TabButton {
                text: qsTr("模型配置")
                width: implicitWidth
            }
            TabButton {
                text: qsTr("生成参数")
                width: implicitWidth
            }
            TabButton {
                text: qsTr("系统提示词")
                width: implicitWidth
            }
            TabButton {
                text: qsTr("测试")
                width: implicitWidth
            }
        }
        
        // 标签页内容
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // ========================================
            // 模型配置页
            // ========================================
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                
                ColumnLayout {
                    width: parent.width
                    spacing: 16
                    
                    // 模型文件选择
                    GroupBox {
                        title: qsTr("模型文件")
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            RowLayout {
                                Layout.fillWidth: true
                                
                                TextField {
                                    id: modelPathField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("选择 GGUF 模型文件...")
                                    text: root.modelPath
                                    readOnly: true
                                    
                                    onTextChanged: root.modelPath = text
                                }
                                
                                Button {
                                    text: qsTr("浏览...")
                                    onClicked: fileDialog.open()
                                }
                            }
                            
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("支持的模型格式: GGUF (llama.cpp 格式)")
                                font.pixelSize: 11
                                color: "#666"
                            }
                            
                            // 模型状态
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                
                                Rectangle {
                                    width: 12
                                    height: 12
                                    radius: 6
                                    color: root.modelLoaded ? "#4CAF50" : "#F44336"
                                }
                                
                                Label {
                                    text: root.modelLoaded ? 
                                          qsTr("模型已加载") : 
                                          qsTr("模型未加载")
                                    font.bold: true
                                }
                                
                                Item { Layout.fillWidth: true }
                                
                                Button {
                                    text: root.modelLoaded ? qsTr("卸载模型") : qsTr("加载模型")
                                    enabled: root.modelPath !== ""
                                    
                                    onClicked: {
                                        if (root.modelLoaded) {
                                            localLLMEngine.unloadModel()
                                        } else {
                                            applySettings()
                                            localLLMEngine.loadModel(root.modelPath)
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // llama.cpp 路径
                    GroupBox {
                        title: qsTr("llama.cpp 可执行文件")
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            RowLayout {
                                Layout.fillWidth: true
                                
                                TextField {
                                    id: llamaCppPathField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("自动检测或手动指定 llama-cli 路径...")
                                    text: localLLMEngine ? localLLMEngine.llamaCppPath : ""
                                }
                                
                                Button {
                                    text: qsTr("浏览...")
                                    onClicked: llamaCppDialog.open()
                                }
                                
                                Button {
                                    text: qsTr("自动检测")
                                    onClicked: {
                                        if (localLLMEngine) {
                                            localLLMEngine.detectLlamaCpp()
                                        }
                                    }
                                }
                            }
                            
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("下载地址: https://github.com/ggerganov/llama.cpp/releases")
                                font.pixelSize: 11
                                color: "#2196F3"
                                
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally("https://github.com/ggerganov/llama.cpp/releases")
                                }
                            }
                        }
                    }
                    
                    // 硬件配置
                    GroupBox {
                        title: qsTr("硬件配置")
                        Layout.fillWidth: true
                        
                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            rowSpacing: 8
                            columnSpacing: 16
                            
                            Label { text: qsTr("GPU 加速层数:") }
                            RowLayout {
                                SpinBox {
                                    id: gpuLayersSpinBox
                                    from: 0
                                    to: 100
                                    value: root.gpuLayers
                                    onValueChanged: root.gpuLayers = value
                                }
                                Label {
                                    text: qsTr("(0 = 仅 CPU，增加此值可加速)")
                                    font.pixelSize: 11
                                    color: "#666"
                                }
                            }
                            
                            Label { text: qsTr("CPU 线程数:") }
                            RowLayout {
                                SpinBox {
                                    id: threadsSpinBox
                                    from: 1
                                    to: 64
                                    value: root.threads
                                    onValueChanged: root.threads = value
                                }
                                Label {
                                    text: qsTr("(建议: 物理核心数)")
                                    font.pixelSize: 11
                                    color: "#666"
                                }
                            }
                            
                            Label { text: qsTr("上下文长度:") }
                            RowLayout {
                                SpinBox {
                                    id: contextLengthSpinBox
                                    from: 512
                                    to: 32768
                                    stepSize: 512
                                    value: root.contextLength
                                    onValueChanged: root.contextLength = value
                                    
                                    textFromValue: function(value, locale) {
                                        return value.toLocaleString(locale, 'f', 0)
                                    }
                                }
                                Label {
                                    text: qsTr("(更长 = 更多内存)")
                                    font.pixelSize: 11
                                    color: "#666"
                                }
                            }
                        }
                    }
                    
                    // 推荐模型
                    GroupBox {
                        title: qsTr("推荐模型")
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4
                            
                            Repeater {
                                model: [
                                    { name: "Phi-3 Mini (3.8B)", size: "2.3GB", url: "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf" },
                                    { name: "Qwen2 1.5B", size: "1.1GB", url: "https://huggingface.co/Qwen/Qwen2-1.5B-Instruct-GGUF" },
                                    { name: "Gemma 2B", size: "1.5GB", url: "https://huggingface.co/google/gemma-2b-it-GGUF" },
                                    { name: "Llama 3.2 3B", size: "2.0GB", url: "https://huggingface.co/meta-llama/Llama-3.2-3B-Instruct-GGUF" }
                                ]
                                
                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                    }
                                    Label {
                                        text: "(" + modelData.size + ")"
                                        color: "#666"
                                    }
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        text: qsTr("下载")
                                        flat: true
                                        onClicked: Qt.openUrlExternally(modelData.url)
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // ========================================
            // 生成参数页
            // ========================================
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                
                ColumnLayout {
                    width: parent.width
                    spacing: 16
                    
                    GroupBox {
                        title: qsTr("采样参数")
                        Layout.fillWidth: true
                        
                        GridLayout {
                            anchors.fill: parent
                            columns: 3
                            rowSpacing: 12
                            columnSpacing: 16
                            
                            // Temperature
                            Label { text: qsTr("Temperature:") }
                            Slider {
                                id: temperatureSlider
                                Layout.fillWidth: true
                                from: 0.0
                                to: 2.0
                                stepSize: 0.1
                                value: root.temperature
                                onValueChanged: root.temperature = value
                            }
                            Label {
                                text: root.temperature.toFixed(1)
                                Layout.preferredWidth: 40
                            }
                            
                            // Top P
                            Label { text: qsTr("Top P:") }
                            Slider {
                                id: topPSlider
                                Layout.fillWidth: true
                                from: 0.0
                                to: 1.0
                                stepSize: 0.05
                                value: root.topP
                                onValueChanged: root.topP = value
                            }
                            Label {
                                text: root.topP.toFixed(2)
                                Layout.preferredWidth: 40
                            }
                            
                            // Top K
                            Label { text: qsTr("Top K:") }
                            Slider {
                                id: topKSlider
                                Layout.fillWidth: true
                                from: 1
                                to: 100
                                stepSize: 1
                                value: root.topK
                                onValueChanged: root.topK = value
                            }
                            Label {
                                text: root.topK.toString()
                                Layout.preferredWidth: 40
                            }
                        }
                    }
                    
                    // 参数说明
                    GroupBox {
                        title: qsTr("参数说明")
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("• Temperature: 控制输出的随机性。低值(0.1-0.5)更确定性，高值(0.8-1.5)更创意。")
                                wrapMode: Text.Wrap
                                font.pixelSize: 12
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("• Top P: 核采样。只考虑累积概率达到此值的 token。0.9 是常用值。")
                                wrapMode: Text.Wrap
                                font.pixelSize: 12
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("• Top K: 只考虑概率最高的 K 个 token。40 是常用值。")
                                wrapMode: Text.Wrap
                                font.pixelSize: 12
                            }
                        }
                    }
                    
                    // 预设
                    GroupBox {
                        title: qsTr("快速预设")
                        Layout.fillWidth: true
                        
                        RowLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            Button {
                                text: qsTr("精确分析")
                                onClicked: {
                                    root.temperature = 0.1
                                    root.topP = 0.9
                                    root.topK = 10
                                }
                            }
                            Button {
                                text: qsTr("平衡")
                                onClicked: {
                                    root.temperature = 0.7
                                    root.topP = 0.9
                                    root.topK = 40
                                }
                            }
                            Button {
                                text: qsTr("创意")
                                onClicked: {
                                    root.temperature = 1.2
                                    root.topP = 0.95
                                    root.topK = 80
                                }
                            }
                            
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
            
            // ========================================
            // 系统提示词页
            // ========================================
            ColumnLayout {
                spacing: 12
                
                Label {
                    text: qsTr("系统提示词定义 AI 的角色和行为方式：")
                }
                
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    TextArea {
                        id: systemPromptArea
                        text: root.systemPrompt
                        wrapMode: TextArea.Wrap
                        placeholderText: qsTr("输入系统提示词...")
                        
                        onTextChanged: root.systemPrompt = text
                    }
                }
                
                // 预设提示词
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label { text: qsTr("预设:") }
                    
                    ComboBox {
                        Layout.fillWidth: true
                        model: [
                            qsTr("数据分析助手"),
                            qsTr("日志分析专家"),
                            qsTr("代码解释器"),
                            qsTr("通用助手")
                        ]
                        
                        onCurrentIndexChanged: {
                            switch (currentIndex) {
                                case 0:
                                    root.systemPrompt = qsTr("你是一个数据分析助手，帮助用户理解和分析日志文件、CSV数据等。请给出简洁、准确的分析结果。")
                                    break
                                case 1:
                                    root.systemPrompt = qsTr("你是一个日志分析专家，专注于识别错误模式、异常行为和性能问题。请分析日志内容并提供可操作的建议。")
                                    break
                                case 2:
                                    root.systemPrompt = qsTr("你是一个代码解释器，帮助用户理解日志中的技术信息、错误堆栈和代码片段。请用通俗易懂的语言解释技术细节。")
                                    break
                                case 3:
                                    root.systemPrompt = qsTr("你是一个有帮助的 AI 助手。请尽力回答用户的问题。")
                                    break
                            }
                        }
                    }
                }
            }
            
            // ========================================
            // 测试页
            // ========================================
            ColumnLayout {
                spacing: 12
                
                // 测试输入
                GroupBox {
                    title: qsTr("测试输入")
                    Layout.fillWidth: true
                    
                    ColumnLayout {
                        anchors.fill: parent
                        
                        TextField {
                            id: testInput
                            Layout.fillWidth: true
                            placeholderText: qsTr("输入测试问题...")
                            text: qsTr("分析这行日志: [ERROR] 2024-01-15 10:30:45 - Connection timeout after 30s")
                        }
                        
                        RowLayout {
                            Button {
                                text: root.isGenerating ? qsTr("停止") : qsTr("发送测试")
                                enabled: root.modelLoaded
                                
                                onClicked: {
                                    if (root.isGenerating) {
                                        localLLMEngine.stopGeneration()
                                    } else {
                                        testOutput.text = ""
                                        localLLMEngine.generate(testInput.text)
                                    }
                                }
                            }
                            
                            Button {
                                text: qsTr("清空")
                                onClicked: testOutput.text = ""
                            }
                            
                            Item { Layout.fillWidth: true }
                            
                            BusyIndicator {
                                running: root.isGenerating
                                visible: root.isGenerating
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                            }
                        }
                    }
                }
                
                // 测试输出
                GroupBox {
                    title: qsTr("模型输出")
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    ScrollView {
                        anchors.fill: parent
                        
                        TextArea {
                            id: testOutput
                            readOnly: true
                            wrapMode: TextArea.Wrap
                            placeholderText: qsTr("模型输出将显示在这里...")
                            font.family: "Consolas, Monaco, monospace"
                        }
                    }
                }
                
                // 性能信息
                GroupBox {
                    title: qsTr("性能信息")
                    Layout.fillWidth: true
                    visible: localLLMEngine && localLLMEngine.lastTokensPerSecond > 0
                    
                    RowLayout {
                        anchors.fill: parent
                        
                        Label {
                            text: qsTr("生成速度: %1 tokens/s").arg(
                                localLLMEngine ? localLLMEngine.lastTokensPerSecond.toFixed(1) : "0"
                            )
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Label {
                            text: qsTr("总 tokens: %1").arg(
                                localLLMEngine ? localLLMEngine.lastTotalTokens : "0"
                            )
                        }
                    }
                }
            }
        }
    }
    
    // 文件对话框
    FileDialog {
        id: fileDialog
        title: qsTr("选择 GGUF 模型文件")
        nameFilters: ["GGUF 模型 (*.gguf)", "所有文件 (*)"]
        
        onAccepted: {
            root.modelPath = selectedFile.toString().replace("file:///", "")
        }
    }
    
    FileDialog {
        id: llamaCppDialog
        title: qsTr("选择 llama-cli 可执行文件")
        nameFilters: ["可执行文件 (*.exe llama-cli llama-cli-*)", "所有文件 (*)"]
        
        onAccepted: {
            llamaCppPathField.text = selectedFile.toString().replace("file:///", "")
            if (localLLMEngine) {
                localLLMEngine.llamaCppPath = llamaCppPathField.text
            }
        }
    }
    
    // 连接信号
    Connections {
        target: localLLMEngine
        
        function onTokenGenerated(token) {
            testOutput.text += token
        }
        
        function onGenerationFinished(fullText) {
            // 生成完成
        }
        
        function onErrorOccurred(error) {
            testOutput.text = qsTr("错误: ") + error
        }
    }
    
    // 应用设置
    function applySettings() {
        if (localLLMEngine) {
            localLLMEngine.contextLength = root.contextLength
            localLLMEngine.gpuLayers = root.gpuLayers
            localLLMEngine.temperature = root.temperature
            localLLMEngine.topP = root.topP
            localLLMEngine.topK = root.topK
            localLLMEngine.threads = root.threads
            localLLMEngine.systemPrompt = root.systemPrompt
            localLLMEngine.llamaCppPath = llamaCppPathField.text
        }
        settingsApplied()
    }
    
    // 加载设置
    function loadSettings() {
        if (localLLMEngine) {
            root.modelPath = localLLMEngine.modelPath
            root.contextLength = localLLMEngine.contextLength
            root.gpuLayers = localLLMEngine.gpuLayers
            root.temperature = localLLMEngine.temperature
            root.topP = localLLMEngine.topP
            root.topK = localLLMEngine.topK
            root.threads = localLLMEngine.threads
            root.systemPrompt = localLLMEngine.systemPrompt
            llamaCppPathField.text = localLLMEngine.llamaCppPath
        }
    }
    
    onOpened: loadSettings()
    
    onAccepted: applySettings()
    
    onApplied: applySettings()
}
