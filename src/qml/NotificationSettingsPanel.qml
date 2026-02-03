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
                text: qsTr("Notification Settings")
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
            TabButton { text: qsTr("Popup/Tray") }
            TabButton { text: qsTr("Command") }
            TabButton { text: qsTr("Sound") }
            TabButton { text: qsTr("History") }
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
                        text: qsTr("Enable Webhook Notifications")
                        checked: notificationManager ? notificationManager.webhookEnabled : true
                        onCheckedChanged: if (notificationManager) notificationManager.webhookEnabled = checked
                    }

                    GroupBox {
                        title: qsTr("New Webhook Configuration")
                        Layout.fillWidth: true
                        enabled: webhookEnabled.checked
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            RowLayout {
                                Label { text: qsTr("Name:"); Layout.preferredWidth: 80 }
                                TextField {
                                    id: webhookName
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Configuration Name")
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
                                Label { text: qsTr("Method:"); Layout.preferredWidth: 80 }
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
                                Label { text: qsTr("Body Template:"); Layout.preferredWidth: 80 }
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
                                text: qsTr("Available variables: {{ruleName}}, {{lineNumber}}, {{lineContent}}, {{timestamp}}, {{hostname}}")
                                font.pixelSize: 11
                                color: palette.placeholderText
                            }

                            RowLayout {
                                Button {
                                    text: qsTr("Save Configuration")
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
                                    text: qsTr("Test")
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
                        title: qsTr("Saved Webhook Configurations")
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
                                        text: qsTr("Delete")
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
                    text: qsTr("Enable Desktop Popup Notifications")
                    checked: notificationManager ? notificationManager.popupEnabled : true
                    onCheckedChanged: if (notificationManager) notificationManager.popupEnabled = checked
                }

                GroupBox {
                    title: qsTr("Popup Settings")
                    Layout.fillWidth: true
                    enabled: popupEnabled.checked

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        RowLayout {
                            Label { text: qsTr("Display Duration:"); Layout.preferredWidth: 100 }
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
                            text: qsTr("Test Popup")
                            onClicked: {
                                notificationManager.showPopup(
                                    qsTr("Test Notification"),
                                    qsTr("This is a test popup message"),
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
                        text: qsTr("Enable Command Line Notifications")
                        checked: notificationManager ? notificationManager.commandEnabled : true
                        onCheckedChanged: if (notificationManager) notificationManager.commandEnabled = checked
                    }

                    Label {
                        text: qsTr("Warning: Command line notifications may execute arbitrary commands, use with caution")
                        color: "orange"
                        visible: commandEnabled.checked
                    }

                    GroupBox {
                        title: qsTr("New Command Configuration")
                        Layout.fillWidth: true
                        enabled: commandEnabled.checked
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            RowLayout {
                                Label { text: qsTr("Name:"); Layout.preferredWidth: 80 }
                                TextField {
                                    id: commandName
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Configuration Name")
                                }
                            }

                            RowLayout {
                                Label { text: qsTr("Command:"); Layout.preferredWidth: 80 }
                                TextArea {
                                    id: commandText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 60
                                    placeholderText: "echo \"Alert: {{ruleName}} at {{lineNumber}}\" >> alerts.log"
                                    wrapMode: TextArea.Wrap
                                }
                            }
                            
                            Label {
                                text: qsTr("Environment Variables: ALERT_RULE, ALERT_LINE_NUMBER, ALERT_LINE_CONTENT, ALERT_TIMESTAMP")
                                font.pixelSize: 11
                                color: palette.placeholderText
                            }

                            RowLayout {
                                Button {
                                    text: qsTr("Save Configuration")
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
                                    text: qsTr("Test")
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
                        title: qsTr("Command Output")
                        Layout.fillWidth: true

                        TextArea {
                            id: commandOutput
                            width: parent.width
                            height: 100
                            readOnly: true
                            wrapMode: TextArea.Wrap
                            placeholderText: qsTr("Command execution output will be displayed here")
                            Connections {
                                target: _notificationManager || null
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
                        title: qsTr("Saved Command Configurations")
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
                                        text: qsTr("Delete")
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
                    text: qsTr("Enable Sound Alerts")
                    checked: notificationManager ? notificationManager.soundEnabled : true
                    onCheckedChanged: if (notificationManager) notificationManager.soundEnabled = checked
                }

                GroupBox {
                    title: qsTr("Sound Settings")
                    Layout.fillWidth: true
                    enabled: soundEnabled.checked

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        RowLayout {
                            Label { text: qsTr("Sound File:"); Layout.preferredWidth: 80 }
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
                                text: qsTr("Test Sound")
                                onClicked: {
                                    if (soundFileCombo.currentIndex === 0) {
                                        notificationManager.playDefaultAlert();
                                    } else {
                                        notificationManager.playSound(soundFileCombo.currentText);
                                    }
                                }
                            }

                            Button {
                                text: qsTr("Browse...")
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
                    Label { text: qsTr("Notification History") }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: qsTr("Clear History")
                        flat: true
                        onClicked: {
                            notificationManager.clearNotificationHistory();
                            historyList.model = [];
                        }
                    }
                    Button {
                        text: qsTr("Refresh")
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
                                    var types = ["Webhook", qsTr("Popup"), qsTr("Command"), qsTr("Sound"), qsTr("Log")];
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

            Label { text: qsTr("Rate Limit:") }
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
            Label { text: qsTr("times/minute") }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Close")
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
