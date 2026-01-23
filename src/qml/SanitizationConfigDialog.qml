import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * 数据脱敏配置对话框
 * 用于配置导出和AI分析时的敏感数据脱敏规则
 */
Popup {
    id: root
    modal: true
    width: 550
    height: 500
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape

    property color textColor: _themeManager.textColor
    property color panelColor: _themeManager.panelBackground
    property color bgColor: _themeManager.backgroundColor
    property color accentColor: _themeManager.accentColor
    property color borderColor: _themeManager.borderColor

    background: Rectangle {
        color: panelColor
        border.color: borderColor
        border.width: 1
        radius: 8
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        // Title
        Text {
            text: qsTr("Data Sanitization Settings")
            color: textColor
            font.pixelSize: 16
            font.bold: true
        }

        Text {
            text: qsTr("Configure which sensitive data patterns to detect and mask when exporting or using AI analysis.")
            color: Qt.darker(textColor, 1.2)
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: borderColor
        }

        // Scrollable rules list
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 8

                // Built-in Rules Section
                Text {
                    text: qsTr("Built-in Rules")
                    color: accentColor
                    font.pixelSize: 13
                    font.bold: true
                }

                // IP Address
                CheckBox {
                    id: ipCheck
                    text: qsTr("IP Addresses (IPv4 & IPv6)")
                    checked: _dataSanitizer.ipEnabled
                    onCheckedChanged: _dataSanitizer.ipEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // Email
                CheckBox {
                    id: emailCheck
                    text: qsTr("Email Addresses")
                    checked: _dataSanitizer.emailEnabled
                    onCheckedChanged: _dataSanitizer.emailEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // Phone
                CheckBox {
                    id: phoneCheck
                    text: qsTr("Phone Numbers (CN & International)")
                    checked: _dataSanitizer.phoneEnabled
                    onCheckedChanged: _dataSanitizer.phoneEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // ID Card
                CheckBox {
                    id: idCardCheck
                    text: qsTr("ID Card Numbers (China)")
                    checked: _dataSanitizer.idCardEnabled
                    onCheckedChanged: _dataSanitizer.idCardEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // Domain
                CheckBox {
                    id: domainCheck
                    text: qsTr("Domain Names")
                    checked: _dataSanitizer.domainEnabled
                    onCheckedChanged: _dataSanitizer.domainEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // Port
                CheckBox {
                    id: portCheck
                    text: qsTr("Port Numbers")
                    checked: _dataSanitizer.portEnabled
                    onCheckedChanged: _dataSanitizer.portEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // API Key
                CheckBox {
                    id: apiKeyCheck
                    text: qsTr("API Keys & Tokens")
                    checked: _dataSanitizer.apiKeyEnabled
                    onCheckedChanged: _dataSanitizer.apiKeyEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // Username
                CheckBox {
                    id: usernameCheck
                    text: qsTr("Usernames (user=xxx patterns)")
                    checked: _dataSanitizer.usernameEnabled
                    onCheckedChanged: _dataSanitizer.usernameEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // File Paths
                CheckBox {
                    id: pathCheck
                    text: qsTr("File Paths (Windows & Unix)")
                    checked: _dataSanitizer.pathEnabled
                    onCheckedChanged: _dataSanitizer.pathEnabled = checked
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 12
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }

                // Separator
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: borderColor
                    Layout.topMargin: 10
                    Layout.bottomMargin: 5
                }

                // Level explanation
                Text {
                    text: qsTr("Sanitization Levels Explanation")
                    color: accentColor
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    text: qsTr("• Minimal: Passwords, Tokens, API Keys only\n• Standard: + IP, Email, Phone, Username\n• Strict: All enabled rules above")
                    color: Qt.darker(textColor, 1.3)
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    lineHeight: 1.4
                }
            }
        }

        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: borderColor
        }

        // Buttons
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10

            Button {
                text: qsTr("Reset to Defaults")
                implicitHeight: 32
                implicitWidth: 120
                onClicked: {
                    _dataSanitizer.resetToDefaults()
                    // Refresh checkboxes
                    ipCheck.checked = _dataSanitizer.ipEnabled
                    emailCheck.checked = _dataSanitizer.emailEnabled
                    phoneCheck.checked = _dataSanitizer.phoneEnabled
                    idCardCheck.checked = _dataSanitizer.idCardEnabled
                    domainCheck.checked = _dataSanitizer.domainEnabled
                    portCheck.checked = _dataSanitizer.portEnabled
                    apiKeyCheck.checked = _dataSanitizer.apiKeyEnabled
                    usernameCheck.checked = _dataSanitizer.usernameEnabled
                    pathCheck.checked = _dataSanitizer.pathEnabled
                }
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: borderColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: qsTr("Save")
                implicitHeight: 32
                implicitWidth: 80
                onClicked: {
                    _dataSanitizer.saveSettings()
                    root.close()
                }
                background: Rectangle {
                    color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: qsTr("Cancel")
                implicitHeight: 32
                implicitWidth: 80
                onClicked: {
                    _dataSanitizer.loadSettings()  // Revert changes
                    root.close()
                }
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: borderColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    onOpened: {
        // Refresh from saved settings
        _dataSanitizer.loadSettings()
        ipCheck.checked = _dataSanitizer.ipEnabled
        emailCheck.checked = _dataSanitizer.emailEnabled
        phoneCheck.checked = _dataSanitizer.phoneEnabled
        idCardCheck.checked = _dataSanitizer.idCardEnabled
        domainCheck.checked = _dataSanitizer.domainEnabled
        portCheck.checked = _dataSanitizer.portEnabled
        apiKeyCheck.checked = _dataSanitizer.apiKeyEnabled
        usernameCheck.checked = _dataSanitizer.usernameEnabled
        pathCheck.checked = _dataSanitizer.pathEnabled
    }
}
