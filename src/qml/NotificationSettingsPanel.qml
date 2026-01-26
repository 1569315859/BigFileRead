import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * NotificationSettingsPanel.qml
 * 通知设置面板 - 配置Webhook、弹窗、命令行等通知方式
 */
Popup {
    id: root
    width: 700
    height: 600
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    
    property var notificationManager
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
    
    signal configurationSaved()
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        border.width: 1
        radius: 8
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: qsTr("通知设置")
                font.pixelSize: 18
                font.bold: true
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: "×"
                flat: true
                onClicked: root.close()
            }
        }
        
        // Tab bar
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            TabButton { text: qsTr("Webhook") }
            TabButton { text: qsTr("弹窗/托盘") }
            TabButton { text: qsTr("命令行") }
            TabButton { text: qsTr("声音") }
            TabButton { text: qsTr("历史") }
        }
        
        // Content
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // ===== Webhook Tab =====
            ScrollView {
                ColumnLayout {
                    width: parent.width - 20
                    spacing: 12
                    
                    // Enable toggle
                    CheckBox {
                        id: webhookEnabled
                        text: qsTr("启用Webhook通知")
                        checked: notificationManager ? notificationManager.webhookEnabled : true
                        onCheckedChanged: if (notificationManager) notificationManager.webhookEnabled = checked
                    }
                    
                    GroupBox {
                        title: qsTr("新建Webhook配置")
                        Layout.fillWidth: true
                        enabled: webhookEnabled.checked
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            RowLayout {
                                Label { text: qsTr("名称:"); Layout.preferredWidth: 80 }
                                TextField {
                                    id: webhookName
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("配置名称")
                                }
                            }
                            
                            RowLayout {
                                Label { text: qsTr("URL:"); Layout.preferredWidth: 80 }
                                TextField {
                                    id: webhookUrl
                                    Layout.fillWidth: true
                                    placeholderText: "https://hooks.slack.com/services/..."
                                }
                            }
                            
                            RowLayout {
                                Label { text: qsTr("方法:"); Layout.preferredWidth: 80 }
                                ComboBox {
                                    id: webhookMethod
                                    model: ["POST", "PUT", "PATCH"]
                                    Layout.preferredWidth: 120
                                }
                            }
                            
                            RowLayout {
                                Label { text: qsTr("Headers:"); Layout.preferredWidth: 80 }
                                TextArea {
                                    id: webhookHeaders
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 60
                                    placeholderText: "Content-Type: application/json"
                                    wrapMode: TextArea.Wrap
                                }
                            }
                            
                            RowLayout {
                                Label { text: qsTr("Body模板:"); Layout.preferredWidth: 80 }
                                TextArea {
                                    id: webhookBody
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 80
                                    placeholderText: '{"text": "Alert: {{ruleName}} at line {{lineNumber}}"}'
                                    wrapMode: TextArea.Wrap
                                }
                            }
                            
                            // Template variables help
                            Label {
                                text: qsTr("可用变量: {{ruleName}}, {{lineNumber}}, {{lineContent}}, {{timestamp}}, {{hostname}}")
                                font.pixelSize: 11
                                color: palette.placeholderText
                            }
                            
                            RowLayout {
                                Button {
                                    text: qsTr("保存配置")
                                    onClicked: {
                                        if (webhookName.text && webhookUrl.text) {
                                            var headers = {};
                                            webhookHeaders.text.split('\n').forEach(function(line) {
                                                var parts = line.split(':');
                                                if (parts.length >= 2) {
                                                    headers[parts[0].trim()] = parts.slice(1).join(':').trim();
                                                }
                                            });
                                            
                                            notificationManager.saveWebhookConfig(webhookName.text, {
                                                url: webhookUrl.text,
                                                method: webhookMethod.currentIndex,
                                                headers: headers,
                                                bodyTemplate: webhookBody.text
                                            });
                                            
                                            webhookConfigList.model = notificationManager.getWebhookConfigs();
                                        }
                                    }
                                }
                                
                                Button {
                                    text: qsTr("测试")
                                    onClicked: {
                                        var headers = {};
                                        webhookHeaders.text.split('\n').forEach(function(line) {
                                            var parts = line.split(':');
                                            if (parts.length >= 2) {
                                                headers[parts[0].trim()] = parts.slice(1).join(':').trim();
                                            }
                                        });
                                        
                                        notificationManager.testWebhook(
                                            webhookUrl.text,
                                            webhookMethod.currentIndex,
                                            headers,
                                            webhookBody.text.replace(/\{\{(\w+)\}\}/g, "TEST")
                                        );
                                    }
                                }
                            }
                        }
                    }
                    
                    // Saved webhook configs
                    GroupBox {
                        title: qsTr("已保存的Webhook配置")
                        Layout.fillWidth: true
                        visible: webhookConfigList.count > 0
                        
                        ListView {
                            id: webhookConfigList
                            width: parent.width
                            height: Math.min(150, contentHeight)
                            model: notificationManager ? notificationManager.getWebhookConfigs() : []
                            clip: true
                            
                            delegate: ItemDelegate {
                                width: webhookConfigList.width
                                
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                    }
                                    
                                    Label {
                                        text: modelData.url
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                        color: palette.placeholderText
                                    }
                                    
                                    Button {
                                        text: qsTr("删除")
                                        flat: true
                                        onClicked: {
                                            notificationManager.deleteWebhookConfig(modelData.name);
                                            webhookConfigList.model = notificationManager.getWebhookConfigs();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // ===== Popup/Tray Tab =====
            ColumnLayout {
                spacing: 12
                
                CheckBox {
                    id: popupEnabled
                    text: qsTr("启用桌面弹窗通知")
                    checked: notificationManager ? notificationManager.popupEnabled : true
                    onCheckedChanged: if (notificationManager) notificationManager.popupEnabled = checked
                }
                
                GroupBox {
                    title: qsTr("弹窗设置")
                    Layout.fillWidth: true
                    enabled: popupEnabled.checked
                    
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        
                        RowLayout {
                            Label { text: qsTr("显示时长:"); Layout.preferredWidth: 100 }
                            SpinBox {
                                id: popupDuration
                                from: 1000
                                to: 30000
                                value: 5000
                                stepSize: 1000
                                
                                textFromValue: function(value) { return (value / 1000) + " 秒"; }
                            }
                        }
                        
                        Button {
                            text: qsTr("测试弹窗")
                            onClicked: {
                                notificationManager.showPopup(
                                    qsTr("测试通知"),
                                    qsTr("这是一条测试弹窗消息"),
                                    popupDuration.value
                                );
                            }
                        }
                    }
                }
                
                Item { Layout.fillHeight: true }
            }
            
            // ===== Command Line Tab =====
            ScrollView {
                ColumnLayout {
                    width: parent.width - 20
                    spacing: 12
                    
                    CheckBox {
                        id: commandEnabled
                        text: qsTr("启用命令行通知")
                        checked: notificationManager ? notificationManager.commandEnabled : true
                        onCheckedChanged: if (notificationManager) notificationManager.commandEnabled = checked
                    }
                    
                    Label {
                        text: qsTr("警告: 命令行通知可能执行任意命令，请谨慎使用")
                        color: "orange"
                        visible: commandEnabled.checked
                    }
                    
                    GroupBox {
                        title: qsTr("新建命令配置")
                        Layout.fillWidth: true
                        enabled: commandEnabled.checked
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            RowLayout {
                                Label { text: qsTr("名称:"); Layout.preferredWidth: 80 }
                                TextField {
                                    id: commandName
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("配置名称")
                                }
                            }
                            
                            RowLayout {
                                Label { text: qsTr("命令:"); Layout.preferredWidth: 80 }
                                TextArea {
                                    id: commandText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 60
                                    placeholderText: "echo \"Alert: {{ruleName}} at {{lineNumber}}\" >> alerts.log"
                                    wrapMode: TextArea.Wrap
                                }
                            }
                            
                            Label {
                                text: qsTr("环境变量: ALERT_RULE, ALERT_LINE_NUMBER, ALERT_LINE_CONTENT, ALERT_TIMESTAMP")
                                font.pixelSize: 11
                                color: palette.placeholderText
                            }
                            
                            RowLayout {
                                Button {
                                    text: qsTr("保存配置")
                                    onClicked: {
                                        if (commandName.text && commandText.text) {
                                            notificationManager.saveCommandConfig(commandName.text, {
                                                command: commandText.text
                                            });
                                            
                                            commandConfigList.model = notificationManager.getCommandConfigs();
                                        }
                                    }
                                }
                                
                                Button {
                                    text: qsTr("测试")
                                    onClicked: {
                                        notificationManager.testCommand(
                                            commandText.text.replace(/\{\{(\w+)\}\}/g, "TEST")
                                        );
                                    }
                                }
                            }
                        }
                    }
                    
                    // Command output
                    GroupBox {
                        title: qsTr("命令输出")
                        Layout.fillWidth: true
                        
                        TextArea {
                            id: commandOutput
                            width: parent.width
                            height: 100
                            readOnly: true
                            wrapMode: TextArea.Wrap
                            placeholderText: qsTr("命令执行输出将显示在这里")
                            
                            Connections {
                                target: notificationManager
                                function onCommandOutput(stdOut, stdErr, exitCode) {
                                    commandOutput.text = "Exit code: " + exitCode + "\n" +
                                                         "stdout:\n" + stdOut + "\n" +
                                                         "stderr:\n" + stdErr;
                                }
                            }
                        }
                    }
                    
                    // Saved command configs
                    GroupBox {
                        title: qsTr("已保存的命令配置")
                        Layout.fillWidth: true
                        visible: commandConfigList.count > 0
                        
                        ListView {
                            id: commandConfigList
                            width: parent.width
                            height: Math.min(100, contentHeight)
                            model: notificationManager ? notificationManager.getCommandConfigs() : []
                            clip: true
                            
                            delegate: ItemDelegate {
                                width: commandConfigList.width
                                
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                    }
                                    
                                    Label {
                                        text: modelData.command
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                        color: palette.placeholderText
                                    }
                                    
                                    Button {
                                        text: qsTr("删除")
                                        flat: true
                                        onClicked: {
                                            notificationManager.deleteCommandConfig(modelData.name);
                                            commandConfigList.model = notificationManager.getCommandConfigs();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // ===== Sound Tab =====
            ColumnLayout {
                spacing: 12
                
                CheckBox {
                    id: soundEnabled
                    text: qsTr("启用声音提醒")
                    checked: notificationManager ? notificationManager.soundEnabled : true
                    onCheckedChanged: if (notificationManager) notificationManager.soundEnabled = checked
                }
                
                GroupBox {
                    title: qsTr("声音设置")
                    Layout.fillWidth: true
                    enabled: soundEnabled.checked
                    
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        
                        RowLayout {
                            Label { text: qsTr("声音文件:"); Layout.preferredWidth: 80 }
                            ComboBox {
                                id: soundFileCombo
                                Layout.fillWidth: true
                                model: notificationManager ? notificationManager.getAvailableSounds() : []
                                editable: true
                                
                                Component.onCompleted: {
                                    model = ["(默认系统声音)"].concat(
                                        notificationManager ? notificationManager.getAvailableSounds() : []
                                    );
                                }
                            }
                        }
                        
                        RowLayout {
                            Button {
                                text: qsTr("测试声音")
                                onClicked: {
                                    if (soundFileCombo.currentIndex === 0) {
                                        notificationManager.playDefaultAlert();
                                    } else {
                                        notificationManager.playSound(soundFileCombo.currentText);
                                    }
                                }
                            }
                            
                            Button {
                                text: qsTr("浏览...")
                                onClicked: soundFileDialog.open()
                            }
                        }
                    }
                }
                
                Item { Layout.fillHeight: true }
            }
            
            // ===== History Tab =====
            ColumnLayout {
                spacing: 12
                
                RowLayout {
                    Label { text: qsTr("通知历史") }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: qsTr("清除历史")
                        flat: true
                        onClicked: {
                            notificationManager.clearNotificationHistory();
                            historyList.model = [];
                        }
                    }
                    Button {
                        text: qsTr("刷新")
                        flat: true
                        onClicked: {
                            historyList.model = notificationManager.getNotificationHistory(100);
                        }
                    }
                }
                
                ListView {
                    id: historyList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: notificationManager ? notificationManager.getNotificationHistory(100) : []
                    clip: true
                    
                    delegate: ItemDelegate {
                        width: historyList.width
                        height: 40
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 8
                            
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: modelData.success ? "green" : "red"
                            }
                            
                            Label {
                                text: {
                                    var types = ["Webhook", qsTr("弹窗"), qsTr("命令"), qsTr("声音"), qsTr("日志")];
                                    return types[modelData.type] || "?";
                                }
                                font.bold: true
                                Layout.preferredWidth: 60
                            }
                            
                            Label {
                                text: modelData.details
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            
                            Label {
                                text: modelData.timestamp
                                color: palette.placeholderText
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
        
        // Rate limit setting
        RowLayout {
            Layout.fillWidth: true
            
            Label { text: qsTr("速率限制:") }
            SpinBox {
                id: rateLimitSpin
                from: 1
                to: 1000
                value: 60
                
                onValueModified: {
                    if (notificationManager) {
                        notificationManager.setRateLimit(value);
                    }
                }
            }
            Label { text: qsTr("次/分钟") }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("关闭")
                onClicked: root.close()
            }
        }
    }
    
    // Sound file dialog would be Platform.FileDialog in real app
    // Simplified for now
    
    Component.onCompleted: {
        if (notificationManager) {
            webhookConfigList.model = notificationManager.getWebhookConfigs();
            commandConfigList.model = notificationManager.getCommandConfigs();
            historyList.model = notificationManager.getNotificationHistory(100);
        }
    }
}
