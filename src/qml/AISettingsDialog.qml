import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * AI 服务设置对话框
 * 配置 API Keys、代理、自定义服务等
 */
Popup {
    id: root
    
    width: Math.min(parent.width * 0.75, 700)
    height: Math.min(parent.height * 0.85, 600)
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
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // 标题栏
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: qsTr("AI Service Settings")
                font.pixelSize: 18
                font.bold: true
                color: _themeManager.textColor
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: "×"
                flat: true
                font.pixelSize: 18
                onClicked: root.close()
            }
        }
        
        // 选项卡
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            background: Rectangle {
                color: "transparent"
            }
            
            TabButton {
                text: qsTr("API Keys")
                contentItem: Text {
                    text: parent.text
                    color: _themeManager.textColor
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    opacity: parent.checked ? 1.0 : 0.6
                }
                background: Rectangle {
                    color: parent.checked ? Qt.darker(_themeManager.accentColor, 1.3) : "transparent"
                    border.color: parent.checked ? _themeManager.accentColor : _themeManager.borderColor
                    border.width: parent.checked ? 0 : 1
                    radius: 4
                }
            }
            TabButton {
                text: qsTr("Proxy")
                contentItem: Text {
                    text: parent.text
                    color: _themeManager.textColor
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    opacity: parent.checked ? 1.0 : 0.6
                }
                background: Rectangle {
                    color: parent.checked ? Qt.darker(_themeManager.accentColor, 1.3) : "transparent"
                    border.color: parent.checked ? _themeManager.accentColor : _themeManager.borderColor
                    border.width: parent.checked ? 0 : 1
                    radius: 4
                }
            }
            TabButton {
                text: qsTr("Custom Services")
                contentItem: Text {
                    text: parent.text
                    color: _themeManager.textColor
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    opacity: parent.checked ? 1.0 : 0.6
                }
                background: Rectangle {
                    color: parent.checked ? Qt.darker(_themeManager.accentColor, 1.3) : "transparent"
                    border.color: parent.checked ? _themeManager.accentColor : _themeManager.borderColor
                    border.width: parent.checked ? 0 : 1
                    radius: 4
                }
            }
        }
        
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // API Keys 页面
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width
                    spacing: 12
                    
                    Label {
                        text: qsTr("Configure API keys for each AI service. Keys are stored encrypted locally.")
                        color: _themeManager.secondaryTextColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    
                    Repeater {
                        id: apiKeyRepeater
                        model: _aiManager ? _aiManager.getAvailableServices().filter(s => !s.isCustom) : []
                        
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70
                            color: Qt.darker(_themeManager.backgroundColor, 1.02)
                            border.color: _themeManager.borderColor
                            radius: 4
                            
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6
                                
                                // 服务名称
                                Label {
                                    text: modelData.name
                                    font.bold: true
                                    font.pixelSize: 12
                                    color: _themeManager.textColor
                                }
                                
                                // API Key 输入行
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    
                                    TextField {
                                        id: apiKeyField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        font.pixelSize: 11
                                        echoMode: showKeyCheck.checked ? TextInput.Normal : TextInput.Password
                                        placeholderText: modelData.hasApiKey ? qsTr("Key: %1").arg(_aiManager.getMaskedApiKey(modelData.id)) : qsTr("Enter API Key...")
                                        color: _themeManager.textColor
                                        
                                        background: Rectangle {
                                            color: _themeManager.backgroundColor
                                            border.color: apiKeyField.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                            radius: 3
                                        }
                                    }
                                    
                                    CheckBox {
                                        id: showKeyCheck
                                        text: qsTr("Show")
                                        font.pixelSize: 11
                                        
                                        contentItem: Text {
                                            text: showKeyCheck.text
                                            color: _themeManager.textColor
                                            font.pixelSize: 11
                                            leftPadding: showKeyCheck.indicator.width + 4
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                    
                                    Button {
                                        text: qsTr("Save")
                                        font.pixelSize: 11
                                        implicitHeight: 26
                                        enabled: apiKeyField.text.length > 0
                                        onClicked: {
                                            _aiManager.setApiKey(modelData.id, apiKeyField.text)
                                            apiKeyField.text = ""
                                            apiKeyField.placeholderText = qsTr("Key: %1").arg(_aiManager.getMaskedApiKey(modelData.id))
                                        }
                                    }
                                    
                                    Button {
                                        text: qsTr("Test")
                                        font.pixelSize: 11
                                        implicitHeight: 26
                                        enabled: modelData.hasApiKey
                                        onClicked: {
                                            _aiManager.testConnection(modelData.id)
                                        }
                                    }
                                    
                                    Button {
                                        text: qsTr("Clear")
                                        font.pixelSize: 11
                                        implicitHeight: 26
                                        enabled: modelData.hasApiKey
                                        onClicked: {
                                            _aiManager.setApiKey(modelData.id, "")
                                            apiKeyField.placeholderText = qsTr("Enter API Key...")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // 代理设置页面
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width
                    spacing: 16
                    
                    Label {
                        text: qsTr("Configure network proxy for AI API requests.")
                        color: _themeManager.secondaryTextColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    
                    // Proxy Type 区域
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: proxyTypeColumn.implicitHeight + 24
                        color: Qt.darker(_themeManager.backgroundColor, 1.02)
                        border.color: _themeManager.borderColor
                        radius: 4
                        
                        ColumnLayout {
                            id: proxyTypeColumn
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8
                            
                            Label {
                                text: qsTr("Proxy Type")
                                font.bold: true
                                font.pixelSize: 13
                                color: _themeManager.textColor
                            }
                            
                            RadioButton {
                                id: noProxyRadio
                                text: qsTr("No Proxy (Direct Connection)")
                                checked: _aiManager && _aiManager.proxyType === "none"
                                onClicked: if (_aiManager) _aiManager.setProxyType("none")
                                
                                contentItem: Text {
                                    text: noProxyRadio.text
                                    color: _themeManager.textColor
                                    font.pixelSize: 12
                                    leftPadding: noProxyRadio.indicator.width + 6
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            
                            RadioButton {
                                id: systemProxyRadio
                                text: qsTr("Use System Proxy")
                                checked: _aiManager && _aiManager.proxyType === "system"
                                onClicked: if (_aiManager) _aiManager.setProxyType("system")
                                
                                contentItem: Text {
                                    text: systemProxyRadio.text
                                    color: _themeManager.textColor
                                    font.pixelSize: 12
                                    leftPadding: systemProxyRadio.indicator.width + 6
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            
                            RadioButton {
                                id: customProxyRadio
                                text: qsTr("Custom Proxy")
                                checked: _aiManager && _aiManager.proxyType === "custom"
                                onClicked: if (_aiManager) _aiManager.setProxyType("custom")
                                
                                contentItem: Text {
                                    text: customProxyRadio.text
                                    color: _themeManager.textColor
                                    font.pixelSize: 12
                                    leftPadding: customProxyRadio.indicator.width + 6
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                    
                    // Custom Proxy Settings 区域
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.minimumHeight: 160
                        implicitHeight: customProxyColumn.implicitHeight + 30
                        color: Qt.darker(_themeManager.backgroundColor, 1.02)
                        border.color: _themeManager.borderColor
                        radius: 4
                        opacity: customProxyRadio.checked ? 1.0 : 0.5
                        
                        ColumnLayout {
                            id: customProxyColumn
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10
                            enabled: customProxyRadio.checked
                            
                            Label {
                                text: qsTr("Custom Proxy Settings")
                                font.bold: true
                                font.pixelSize: 13
                                color: _themeManager.textColor
                            }
                            
                            // 使用 GridLayout 保证对齐
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 4
                                columnSpacing: 10
                                rowSpacing: 8
                                
                                // 第一行: Type + Host
                                Label { 
                                    text: qsTr("Type:") 
                                    color: _themeManager.textColor 
                                    font.pixelSize: 12 
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                ComboBox {
                                    id: proxyTypeCombo
                                    Layout.preferredWidth: 100
                                    font.pixelSize: 12
                                    model: ["HTTP", "SOCKS5"]
                                    currentIndex: _aiManager && _aiManager.getCustomProxy().isSocks5 ? 1 : 0
                                }
                                
                                Label { 
                                    text: qsTr("Host:") 
                                    color: _themeManager.textColor 
                                    font.pixelSize: 12 
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                TextField {
                                    id: proxyHostField
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    font.pixelSize: 12
                                    placeholderText: "127.0.0.1"
                                    text: _aiManager ? _aiManager.getCustomProxy().host : ""
                                    color: _themeManager.textColor
                                    
                                    background: Rectangle {
                                        color: _themeManager.backgroundColor
                                        border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                        radius: 4
                                    }
                                }
                                
                                // 第二行: Username + Port
                                Label { 
                                    text: qsTr("Username:") 
                                    color: _themeManager.textColor 
                                    font.pixelSize: 12 
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                TextField {
                                    id: proxyUserField
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    font.pixelSize: 12
                                    placeholderText: qsTr("Optional")
                                    text: _aiManager ? _aiManager.getCustomProxy().username : ""
                                    color: _themeManager.textColor
                                    
                                    background: Rectangle {
                                        color: _themeManager.backgroundColor
                                        border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                        radius: 4
                                    }
                                }
                                
                                Label { 
                                    text: qsTr("Port:") 
                                    color: _themeManager.textColor 
                                    font.pixelSize: 12 
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                TextField {
                                    id: proxyPortField
                                    Layout.preferredWidth: 80
                                    Layout.preferredHeight: 28
                                    font.pixelSize: 12
                                    placeholderText: "7890"
                                    text: _aiManager && _aiManager.getCustomProxy().port > 0 ? _aiManager.getCustomProxy().port.toString() : ""
                                    validator: IntValidator { bottom: 1; top: 65535 }
                                    color: _themeManager.textColor
                                    
                                    background: Rectangle {
                                        color: _themeManager.backgroundColor
                                        border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                        radius: 4
                                    }
                                }
                                
                                // 第三行: Password + 保存按钮
                                Label { 
                                    text: qsTr("Password:") 
                                    color: _themeManager.textColor 
                                    font.pixelSize: 12 
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                }
                                TextField {
                                    id: proxyPassField
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    font.pixelSize: 12
                                    echoMode: TextInput.Password
                                    placeholderText: qsTr("Optional")
                                    color: _themeManager.textColor
                                    
                                    background: Rectangle {
                                        color: _themeManager.backgroundColor
                                        border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                        radius: 4
                                    }
                                }
                                
                                Item { Layout.fillWidth: true } // 占位
                                
                                Button {
                                    text: qsTr("Save Proxy Settings")
                                    Layout.alignment: Qt.AlignRight
                                    font.pixelSize: 12
                                    onClicked: {
                                        if (_aiManager) {
                                            _aiManager.setCustomProxy(
                                                proxyHostField.text,
                                                parseInt(proxyPortField.text) || 0,
                                                proxyTypeCombo.currentIndex === 1,
                                                proxyUserField.text,
                                                proxyPassField.text
                                            )
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // 自定义服务页面
            ColumnLayout {
                spacing: 12
                
                Label {
                    text: qsTr("Add custom AI services compatible with OpenAI API format.")
                    color: _themeManager.secondaryTextColor
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                
                // 已有自定义服务列表
                GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Custom Services")
                    
                    background: Rectangle {
                        color: _themeManager.backgroundColor
                        border.color: _themeManager.borderColor
                        radius: 4
                    }
                    
                    ListView {
                        id: customServiceList
                        anchors.fill: parent
                        clip: true
                        model: _aiManager ? _aiManager.getAvailableServices().filter(s => s.isCustom) : []
                        
                        delegate: Rectangle {
                            width: customServiceList.width
                            height: 60
                            color: index % 2 === 0 ? "transparent" : Qt.rgba(_themeManager.borderColor.r, _themeManager.borderColor.g, _themeManager.borderColor.b, 0.1)
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                        color: _themeManager.textColor
                                    }
                                    
                                    Label {
                                        text: modelData.endpoint
                                        font.pixelSize: 11
                                        color: _themeManager.secondaryTextColor
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }
                                
                                Button {
                                    text: qsTr("Remove")
                                    onClicked: {
                                        if (_aiManager) {
                                            _aiManager.removeCustomService(modelData.id)
                                            // 刷新列表
                                            customServiceList.model = _aiManager.getAvailableServices().filter(s => s.isCustom)
                                        }
                                    }
                                }
                            }
                        }
                        
                        Label {
                            anchors.centerIn: parent
                            visible: customServiceList.count === 0
                            text: qsTr("No custom services configured")
                            color: _themeManager.secondaryTextColor
                        }
                    }
                }
                
                // 添加新服务
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: newServiceColumn.implicitHeight + 24
                    color: Qt.darker(_themeManager.backgroundColor, 1.02)
                    border.color: _themeManager.borderColor
                    radius: 4
                    
                    ColumnLayout {
                        id: newServiceColumn
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8
                        
                        Label {
                            text: qsTr("Add New Service")
                            font.bold: true
                            font.pixelSize: 13
                            color: _themeManager.textColor
                        }
                        
                        // Name
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: qsTr("Name:"); color: _themeManager.textColor; Layout.preferredWidth: 100 }
                            TextField {
                                id: newServiceName
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                font.pixelSize: 12
                                placeholderText: qsTr("My AI Service")
                                color: _themeManager.textColor
                                background: Rectangle {
                                    color: _themeManager.backgroundColor
                                    border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                    radius: 4
                                }
                            }
                        }
                        
                        // Endpoint
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: qsTr("Endpoint:"); color: _themeManager.textColor; Layout.preferredWidth: 100 }
                            TextField {
                                id: newServiceEndpoint
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                font.pixelSize: 12
                                placeholderText: "https://api.example.com/v1/chat/completions"
                                color: _themeManager.textColor
                                background: Rectangle {
                                    color: _themeManager.backgroundColor
                                    border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                    radius: 4
                                }
                            }
                        }
                        
                        // Model
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: qsTr("Model:"); color: _themeManager.textColor; Layout.preferredWidth: 100 }
                            TextField {
                                id: newServiceModel
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                font.pixelSize: 12
                                placeholderText: "gpt-4"
                                color: _themeManager.textColor
                                background: Rectangle {
                                    color: _themeManager.backgroundColor
                                    border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                    radius: 4
                                }
                            }
                        }
                        
                        // API Key Header
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: qsTr("API Key Header:"); color: _themeManager.textColor; Layout.preferredWidth: 100 }
                            TextField {
                                id: newServiceHeader
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                font.pixelSize: 12
                                placeholderText: "Authorization"
                                text: "Authorization"
                                color: _themeManager.textColor
                                background: Rectangle {
                                    color: _themeManager.backgroundColor
                                    border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                    radius: 4
                                }
                            }
                        }
                        
                        // API Key Prefix
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: qsTr("API Key Prefix:"); color: _themeManager.textColor; Layout.preferredWidth: 100 }
                            TextField {
                                id: newServicePrefix
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                font.pixelSize: 12
                                placeholderText: "Bearer "
                                text: "Bearer "
                                color: _themeManager.textColor
                                background: Rectangle {
                                    color: _themeManager.backgroundColor
                                    border.color: parent.activeFocus ? _themeManager.accentColor : _themeManager.borderColor
                                    radius: 4
                                }
                            }
                        }
                        
                        // Add button
                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Button {
                                text: qsTr("Add Service")
                                enabled: newServiceName.text.length > 0 && newServiceEndpoint.text.length > 0 && newServiceModel.text.length > 0
                                
                                onClicked: {
                                    if (_aiManager) {
                                        _aiManager.addCustomService(
                                            newServiceName.text,
                                            newServiceEndpoint.text,
                                            newServiceModel.text,
                                            newServiceHeader.text,
                                            newServicePrefix.text
                                        )
                                        
                                        // 清空输入并刷新列表
                                        newServiceName.text = ""
                                        newServiceEndpoint.text = ""
                                        newServiceModel.text = ""
                                        customServiceList.model = _aiManager.getAvailableServices().filter(s => s.isCustom)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // 底部按钮
        RowLayout {
            Layout.fillWidth: true
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
    
    // 连接测试结果信号
    Connections {
        target: _aiManager
        
        function onConnectionTestResult(serviceId, success, message) {
            // 显示测试结果
            testResultDialog.serviceId = serviceId
            testResultDialog.success = success
            testResultDialog.message = message
            testResultDialog.open()
        }
    }
    
    // 测试结果对话框
    Dialog {
        id: testResultDialog
        
        property string serviceId: ""
        property bool success: false
        property string message: ""
        
        title: qsTr("Connection Test Result")
        standardButtons: Dialog.Ok
        modal: true
        closePolicy: Popup.CloseOnEscape
        
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        
        ColumnLayout {
            spacing: 12
            
            RowLayout {
                spacing: 8
                
                Rectangle {
                    width: 24
                    height: 24
                    radius: 12
                    color: testResultDialog.success ? "#4CAF50" : "#F44336"
                    
                    Label {
                        anchors.centerIn: parent
                        text: testResultDialog.success ? "✓" : "✗"
                        color: "white"
                        font.bold: true
                    }
                }
                
                Label {
                    text: testResultDialog.success ? qsTr("Connection successful!") : qsTr("Connection failed")
                    font.bold: true
                    color: _themeManager.textColor
                }
            }
            
            Label {
                text: testResultDialog.message
                wrapMode: Text.Wrap
                color: _themeManager.secondaryTextColor
            }
        }
    }
}
