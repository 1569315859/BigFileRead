import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief Email Alert Settings Dialog
 * 
 * Enterprise feature: Configure SMTP settings and alert rules
 */
Dialog {
    id: root
    
    title: qsTr("Email Alert Settings")
    width: 600
    height: 550
    modal: true
    anchors.centerIn: parent
    
    property var alertManager: null
    
    contentItem: ColumnLayout {
        spacing: 12
        
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            TabButton { text: qsTr("SMTP Settings") }
            TabButton { text: qsTr("Alert Rules") }
        }
        
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // ===== SMTP Settings Tab =====
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width
                    spacing: 12
                    
                    // Enable switch
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Label { 
                            text: qsTr("Enable Email Alerts")
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Switch {
                            id: enableSwitch
                            checked: root.alertManager ? root.alertManager.enabled : false
                            onCheckedChanged: {
                                if (root.alertManager) {
                                    root.alertManager.enabled = checked
                                }
                            }
                        }
                    }
                    
                    GroupBox {
                        title: qsTr("SMTP Server")
                        Layout.fillWidth: true
                        
                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            
                            Label { text: qsTr("Server:") }
                            TextField {
                                id: smtpHostField
                                Layout.fillWidth: true
                                placeholderText: "smtp.gmail.com"
                                text: root.alertManager ? root.alertManager.smtpHost : ""
                            }
                            
                            Label { text: qsTr("Port:") }
                            RowLayout {
                                SpinBox {
                                    id: smtpPortField
                                    from: 1
                                    to: 65535
                                    value: root.alertManager ? root.alertManager.smtpPort : 587
                                    editable: true
                                }
                                
                                ComboBox {
                                    id: portPreset
                                    model: ["587 (TLS)", "465 (SSL)", "25 (Plain)"]
                                    currentIndex: smtpPortField.value === 587 ? 0 : 
                                                  (smtpPortField.value === 465 ? 1 : 2)
                                    onCurrentIndexChanged: {
                                        if (currentIndex === 0) smtpPortField.value = 587
                                        else if (currentIndex === 1) smtpPortField.value = 465
                                        else smtpPortField.value = 25
                                    }
                                }
                            }
                            
                            Label { text: qsTr("Encryption:") }
                            CheckBox {
                                id: useTlsCheck
                                text: qsTr("Use TLS/SSL")
                                checked: root.alertManager ? root.alertManager.useTls : true
                            }
                        }
                    }
                    
                    GroupBox {
                        title: qsTr("Authentication")
                        Layout.fillWidth: true
                        
                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            
                            Label { text: qsTr("Username:") }
                            TextField {
                                id: usernameField
                                Layout.fillWidth: true
                                placeholderText: qsTr("your-email@gmail.com")
                                text: root.alertManager ? root.alertManager.username : ""
                            }
                            
                            Label { text: qsTr("Password:") }
                            RowLayout {
                                Layout.fillWidth: true
                                
                                TextField {
                                    id: passwordField
                                    Layout.fillWidth: true
                                    echoMode: TextInput.Password
                                    placeholderText: qsTr("App password or SMTP password")
                                }
                                
                                Button {
                                    text: passwordField.echoMode === TextInput.Password ? "👁" : "🔒"
                                    flat: true
                                    implicitWidth: 36
                                    onClicked: {
                                        passwordField.echoMode = passwordField.echoMode === TextInput.Password 
                                            ? TextInput.Normal : TextInput.Password
                                    }
                                }
                            }
                        }
                    }
                    
                    GroupBox {
                        title: qsTr("Email Addresses")
                        Layout.fillWidth: true
                        
                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            
                            Label { text: qsTr("From:") }
                            TextField {
                                id: fromAddressField
                                Layout.fillWidth: true
                                placeholderText: "alerts@yourcompany.com"
                                text: root.alertManager ? root.alertManager.fromAddress : ""
                            }
                            
                            Label { text: qsTr("To:") }
                            TextField {
                                id: toAddressesField
                                Layout.fillWidth: true
                                placeholderText: qsTr("admin@company.com, dev@company.com")
                                text: root.alertManager ? root.alertManager.toAddresses : ""
                            }
                            
                            Label { }
                            Label {
                                text: qsTr("Separate multiple addresses with commas")
                                font.pixelSize: 11
                                opacity: 0.6
                            }
                        }
                    }
                    
                    // Test button
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        
                        Item { Layout.fillWidth: true }
                        
                        Button {
                            text: qsTr("Send Test Email")
                            enabled: smtpHostField.text.length > 0 && 
                                     fromAddressField.text.length > 0 &&
                                     toAddressesField.text.length > 0
                            onClicked: {
                                saveSettings()
                                if (root.alertManager) {
                                    statusLabel.text = qsTr("Sending test email...")
                                    root.alertManager.sendTestEmail()
                                }
                            }
                        }
                    }
                    
                    // Status label
                    Label {
                        id: statusLabel
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignCenter
                        font.pixelSize: 12
                        color: statusLabel.text.includes("failed") ? "#F44336" : "#4CAF50"
                    }
                }
            }
            
            // ===== Alert Rules Tab =====
            ColumnLayout {
                spacing: 8
                
                // Rules list
                ListView {
                    id: rulesList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    model: root.alertManager ? root.alertManager.getRules() : []
                    
                    delegate: Rectangle {
                        width: rulesList.width
                        height: 60
                        color: index % 2 === 0 ? "transparent" : Qt.rgba(0, 0, 0, 0.03)
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 12
                            
                            CheckBox {
                                checked: modelData.enabled
                                onCheckedChanged: {
                                    if (root.alertManager) {
                                        root.alertManager.setRuleEnabled(modelData.id, checked)
                                    }
                                }
                            }
                            
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                
                                Label {
                                    text: modelData.name
                                    font.bold: true
                                }
                                
                                Label {
                                    text: modelData.keywords
                                    font.pixelSize: 11
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                            
                            Label {
                                text: modelData.cooldownMinutes + qsTr(" min cooldown")
                                font.pixelSize: 11
                                opacity: 0.5
                            }
                            
                            Button {
                                text: "✏"
                                flat: true
                                implicitWidth: 32
                                onClicked: editRule(modelData)
                            }
                            
                            Button {
                                text: "🗑"
                                flat: true
                                implicitWidth: 32
                                onClicked: {
                                    if (root.alertManager) {
                                        root.alertManager.removeRule(modelData.id)
                                        refreshRules()
                                    }
                                }
                            }
                        }
                    }
                    
                    // Empty state
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("No alert rules defined.\nClick 'Add Rule' to create one.")
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.5
                        visible: rulesList.count === 0
                    }
                }
                
                // Add rule button
                Button {
                    text: qsTr("+ Add Rule")
                    Layout.alignment: Qt.AlignRight
                    onClicked: addRuleDialog.open()
                }
            }
        }
        
        // Bottom buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            
            Button {
                text: qsTr("Save")
                highlighted: true
                onClicked: {
                    saveSettings()
                    root.close()
                }
            }
        }
    }
    
    // ===== Add/Edit Rule Dialog =====
    Dialog {
        id: addRuleDialog
        title: editingRuleId ? qsTr("Edit Rule") : qsTr("Add Alert Rule")
        width: 400
        height: 350
        modal: true
        anchors.centerIn: parent
        
        property string editingRuleId: ""
        
        contentItem: ColumnLayout {
            spacing: 12
            
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8
                
                Label { text: qsTr("Rule Name:") }
                TextField {
                    id: ruleNameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("e.g., Critical Errors")
                }
                
                Label { text: qsTr("Keywords:") }
                TextField {
                    id: ruleKeywordsField
                    Layout.fillWidth: true
                    placeholderText: qsTr("FATAL, CRITICAL, ERROR")
                }
                
                Label { }
                Label {
                    text: qsTr("Comma-separated keywords to trigger alert")
                    font.pixelSize: 11
                    opacity: 0.6
                }
                
                Label { text: qsTr("Options:") }
                ColumnLayout {
                    spacing: 4
                    
                    CheckBox {
                        id: ruleCaseSensitive
                        text: qsTr("Case sensitive")
                    }
                    
                    CheckBox {
                        id: ruleUseRegex
                        text: qsTr("Use regular expressions")
                    }
                }
                
                Label { text: qsTr("Cooldown:") }
                RowLayout {
                    SpinBox {
                        id: ruleCooldown
                        from: 1
                        to: 1440
                        value: 5
                        editable: true
                    }
                    Label { text: qsTr("minutes") }
                }
                
                Label { }
                Label {
                    text: qsTr("Minimum time between alerts for this rule")
                    font.pixelSize: 11
                    opacity: 0.6
                }
            }
            
            Item { Layout.fillHeight: true }
            
            RowLayout {
                Layout.fillWidth: true
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: qsTr("Cancel")
                    onClicked: addRuleDialog.close()
                }
                
                Button {
                    text: addRuleDialog.editingRuleId ? qsTr("Update") : qsTr("Add")
                    highlighted: true
                    enabled: ruleNameField.text.length > 0 && ruleKeywordsField.text.length > 0
                    onClicked: {
                        if (root.alertManager) {
                            if (addRuleDialog.editingRuleId) {
                                root.alertManager.updateRule(
                                    addRuleDialog.editingRuleId,
                                    ruleNameField.text,
                                    ruleKeywordsField.text,
                                    ruleCaseSensitive.checked,
                                    ruleUseRegex.checked,
                                    ruleCooldown.value
                                )
                            } else {
                                root.alertManager.addRule(
                                    ruleNameField.text,
                                    ruleKeywordsField.text,
                                    ruleCaseSensitive.checked,
                                    ruleUseRegex.checked,
                                    ruleCooldown.value
                                )
                            }
                            refreshRules()
                        }
                        addRuleDialog.close()
                    }
                }
            }
        }
        
        onOpened: {
            if (!editingRuleId) {
                ruleNameField.text = ""
                ruleKeywordsField.text = ""
                ruleCaseSensitive.checked = false
                ruleUseRegex.checked = false
                ruleCooldown.value = 5
            }
        }
    }
    
    function editRule(rule) {
        addRuleDialog.editingRuleId = rule.id
        ruleNameField.text = rule.name
        ruleKeywordsField.text = rule.keywords
        ruleCaseSensitive.checked = rule.caseSensitive
        ruleUseRegex.checked = rule.useRegex
        ruleCooldown.value = rule.cooldownMinutes
        addRuleDialog.open()
    }
    
    function refreshRules() {
        if (root.alertManager) {
            rulesList.model = root.alertManager.getRules()
        }
    }
    
    function saveSettings() {
        if (!root.alertManager) return
        
        root.alertManager.smtpHost = smtpHostField.text
        root.alertManager.smtpPort = smtpPortField.value
        root.alertManager.useTls = useTlsCheck.checked
        root.alertManager.username = usernameField.text
        root.alertManager.setPassword(passwordField.text)
        root.alertManager.fromAddress = fromAddressField.text
        root.alertManager.toAddresses = toAddressesField.text
        root.alertManager.saveSettings()
    }
    
    Connections {
        target: root.alertManager
        
        function onEmailSent(count) {
            statusLabel.text = qsTr("Email sent successfully!")
        }
        
        function onEmailFailed(error) {
            statusLabel.text = qsTr("Send failed: %1").arg(error)
        }
    }
    
    onOpened: {
        refreshRules()
        statusLabel.text = ""
    }
}
