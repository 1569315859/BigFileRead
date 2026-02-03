import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @file ParserConfigDialog.qml
 * @brief Log Parser Configuration Dialog - Configure log format parsing rules
 * 
 * Features:
 * - Preset log formats (Generic, SpringBoot, Log4j, Syslog, Apache, JSON, XML, CSV)
 * - Custom regex pattern configuration
 * - JSON key extraction
 * - XML element/attribute paths
 * - CSV/DSV delimiter settings
 * - Multiline log merging
 */
Dialog {
    id: root
    title: qsTr("Parser Settings")
    modal: true
    width: 700
    height: 600
    anchors.centerIn: parent
    standardButtons: Dialog.Apply | Dialog.Cancel
    
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
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    // Current configuration
    property int selectedPreset: 0
    property string customPattern: ""
    property string columnHeaders: ""
    property string jsonKeys: ""
    property string xmlElements: ""
    property string xmlAttributes: ""
    property string dsvDelimiter: ","
    property bool dsvHasHeader: true
    property bool multilineEnabled: false
    property int multilineMode: 0
    property string multilineStartPattern: ""
    
    signal configurationApplied(var config)
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // Preset selection
        GroupBox {
            Layout.fillWidth: true
            title: qsTr("Log Format Preset")
            
            background: Rectangle {
                color: Qt.darker(panelColor, 1.05)
                border.color: borderColor
                radius: 4
                y: parent.topPadding - parent.bottomPadding
                height: parent.height - parent.topPadding + parent.bottomPadding
            }
            
            RowLayout {
                anchors.fill: parent
                spacing: 12
                
                ComboBox {
                    id: presetCombo
                    Layout.fillWidth: true
                    model: [
                        qsTr("Generic (Auto-detect)"),
                        qsTr("Spring Boot"),
                        qsTr("Logback"),
                        qsTr("Log4j"),
                        qsTr("Syslog (RFC 5424)"),
                        qsTr("Apache Access Log"),
                        qsTr("JSON Lines"),
                        qsTr("XML Log (Generic)"),
                        qsTr("Log4j XML"),
                        qsTr("CSV/DSV"),
                        qsTr("Custom Regex")
                    ]
                    currentIndex: root.selectedPreset
                    onCurrentIndexChanged: root.selectedPreset = currentIndex
                }
                
                Button {
                    text: qsTr("Auto Detect")
                    enabled: currentLogModel && currentLogModel.lineCount > 0
                    onClicked: {
                        // Try to auto-detect format from current file
                        if (currentLogModel && currentLogModel.filePath) {
                            var sampleContent = ""
                            for (var i = 0; i < Math.min(100, currentLogModel.lineCount); i++) {
                                sampleContent += currentLogModel.getLine(i) + "\n"
                            }
                            // This would call the backend auto-detection
                            console.log("[ParserConfig] Auto-detecting format...")
                        }
                    }
                }
            }
        }
        
        // Tab-based configuration
        TabBar {
            id: configTabBar
            Layout.fillWidth: true
            
            TabButton { text: qsTr("Regex Pattern") }
            TabButton { text: qsTr("JSON") }
            TabButton { text: qsTr("XML") }
            TabButton { text: qsTr("CSV/DSV") }
            TabButton { text: qsTr("Multiline") }
        }
        
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: configTabBar.currentIndex
            
            // ===== Regex Pattern Tab =====
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width - 20
                    spacing: 12
                    
                    Label {
                        text: qsTr("Custom Regex Pattern:")
                        font.bold: true
                    }
                    
                    TextArea {
                        id: patternInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        text: root.customPattern
                        placeholderText: qsTr("Example: ^\\[(.*?)\\]\\s+(\\w+)\\s+(.*)$")
                        font.family: "Consolas"
                        color: textColor
                        wrapMode: TextArea.Wrap
                        onTextChanged: root.customPattern = text
                        
                        background: Rectangle {
                            color: bgColor
                            border.color: borderColor
                            radius: 4
                        }
                    }
                    
                    Label {
                        text: qsTr("Column Headers (comma-separated):")
                        font.bold: true
                    }
                    
                    TextField {
                        id: headersInput
                        Layout.fillWidth: true
                        text: root.columnHeaders
                        placeholderText: qsTr("Time, Level, Message")
                        color: textColor
                        onTextChanged: root.columnHeaders = text
                        
                        background: Rectangle {
                            color: bgColor
                            border.color: borderColor
                            radius: 4
                        }
                    }
                    
                    Label {
                        text: qsTr("Tips:")
                        color: Qt.darker(textColor, 1.3)
                    }
                    
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("• Use capture groups () to extract fields\n• Number of groups should match column headers\n• Test your pattern with sample data below")
                        color: Qt.darker(textColor, 1.3)
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // ===== JSON Tab =====
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width - 20
                    spacing: 12
                    
                    CheckBox {
                        id: jsonEnabledCheck
                        text: qsTr("Enable JSON Parsing")
                        checked: true
                    }
                    
                    Label {
                        text: qsTr("JSON Keys to Extract (comma-separated):")
                        font.bold: true
                        enabled: jsonEnabledCheck.checked
                    }
                    
                    TextField {
                        id: jsonKeysInput
                        Layout.fillWidth: true
                        text: root.jsonKeys
                        placeholderText: qsTr("timestamp, level, message, logger")
                        color: textColor
                        enabled: jsonEnabledCheck.checked
                        onTextChanged: root.jsonKeys = text
                        
                        background: Rectangle {
                            color: bgColor
                            border.color: borderColor
                            radius: 4
                        }
                    }
                    
                    Label {
                        text: qsTr("For nested keys, use dot notation: data.value, error.code")
                        color: Qt.darker(textColor, 1.3)
                        font.pixelSize: 12
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // ===== XML Tab =====
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width - 20
                    spacing: 12
                    
                    CheckBox {
                        id: xmlEnabledCheck
                        text: qsTr("Enable XML Parsing")
                        checked: false
                    }
                    
                    CheckBox {
                        id: log4jXmlCheck
                        text: qsTr("Log4j XML Format")
                        enabled: xmlEnabledCheck.checked
                    }
                    
                    Label {
                        text: qsTr("Element Paths (comma-separated):")
                        font.bold: true
                        enabled: xmlEnabledCheck.checked && !log4jXmlCheck.checked
                    }
                    
                    TextField {
                        id: xmlElementsInput
                        Layout.fillWidth: true
                        text: root.xmlElements
                        placeholderText: qsTr("message, data/value, error/code")
                        color: textColor
                        enabled: xmlEnabledCheck.checked && !log4jXmlCheck.checked
                        onTextChanged: root.xmlElements = text
                        
                        background: Rectangle {
                            color: bgColor
                            border.color: borderColor
                            radius: 4
                        }
                    }
                    
                    Label {
                        text: qsTr("Attribute Paths (element@attribute):")
                        font.bold: true
                        enabled: xmlEnabledCheck.checked && !log4jXmlCheck.checked
                    }
                    
                    TextField {
                        id: xmlAttributesInput
                        Layout.fillWidth: true
                        text: root.xmlAttributes
                        placeholderText: qsTr("event@timestamp, log@level")
                        color: textColor
                        enabled: xmlEnabledCheck.checked && !log4jXmlCheck.checked
                        onTextChanged: root.xmlAttributes = text
                        
                        background: Rectangle {
                            color: bgColor
                            border.color: borderColor
                            radius: 4
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // ===== CSV/DSV Tab =====
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width - 20
                    spacing: 12
                    
                    CheckBox {
                        id: dsvEnabledCheck
                        text: qsTr("Enable DSV Parsing")
                        checked: false
                    }
                    
                    RowLayout {
                        Layout.fillWidth: true
                        enabled: dsvEnabledCheck.checked
                        
                        Label {
                            text: qsTr("Delimiter:")
                            font.bold: true
                        }
                        
                        ComboBox {
                            id: delimiterCombo
                            model: [
                                { text: qsTr("Comma (,)"), value: "," },
                                { text: qsTr("Tab (\\t)"), value: "\t" },
                                { text: qsTr("Semicolon (;)"), value: ";" },
                                { text: qsTr("Pipe (|)"), value: "|" },
                                { text: qsTr("Space"), value: " " }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            onCurrentValueChanged: root.dsvDelimiter = currentValue
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        CheckBox {
                            id: hasHeaderCheck
                            text: qsTr("First row is header")
                            checked: root.dsvHasHeader
                            onCheckedChanged: root.dsvHasHeader = checked
                        }
                    }
                    
                    CheckBox {
                        text: qsTr("Trim whitespace from fields")
                        checked: true
                        enabled: dsvEnabledCheck.checked
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // ===== Multiline Tab =====
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width - 20
                    spacing: 12
                    
                    CheckBox {
                        id: multilineCheck
                        text: qsTr("Enable Multiline Merging")
                        checked: root.multilineEnabled
                        onCheckedChanged: root.multilineEnabled = checked
                    }
                    
                    Label {
                        text: qsTr("Merge Mode:")
                        font.bold: true
                        enabled: multilineCheck.checked
                    }
                    
                    ComboBox {
                        id: multilineModeCombo
                        Layout.fillWidth: true
                        enabled: multilineCheck.checked
                        model: [
                            qsTr("Indent-based (continuation lines start with whitespace)"),
                            qsTr("Regex-based (lines NOT matching pattern are continuations)"),
                            qsTr("Both (either condition)")
                        ]
                        currentIndex: root.multilineMode
                        onCurrentIndexChanged: root.multilineMode = currentIndex
                    }
                    
                    Label {
                        text: qsTr("Start Pattern (for Regex mode):")
                        font.bold: true
                        enabled: multilineCheck.checked && multilineModeCombo.currentIndex >= 1
                    }
                    
                    TextField {
                        id: multilinePatternInput
                        Layout.fillWidth: true
                        text: root.multilineStartPattern
                        placeholderText: qsTr("^\\d{4}-\\d{2}-\\d{2} (matches date at line start)")
                        color: textColor
                        enabled: multilineCheck.checked && multilineModeCombo.currentIndex >= 1
                        onTextChanged: root.multilineStartPattern = text
                        
                        background: Rectangle {
                            color: bgColor
                            border.color: borderColor
                            radius: 4
                        }
                    }
                    
                    RowLayout {
                        Layout.fillWidth: true
                        enabled: multilineCheck.checked
                        
                        Label {
                            text: qsTr("Max lines to merge:")
                        }
                        
                        SpinBox {
                            id: maxLinesSpin
                            from: 2
                            to: 1000
                            value: 100
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
        }
        
        // Test area
        GroupBox {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            title: qsTr("Test Preview")
            
            background: Rectangle {
                color: Qt.darker(panelColor, 1.05)
                border.color: borderColor
                radius: 4
                y: parent.topPadding - parent.bottomPadding
                height: parent.height - parent.topPadding + parent.bottomPadding
            }
            
            RowLayout {
                anchors.fill: parent
                spacing: 8
                
                TextField {
                    id: testInput
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    placeholderText: qsTr("Paste a sample log line here to test parsing...")
                    color: textColor
                    
                    background: Rectangle {
                        color: bgColor
                        border.color: borderColor
                        radius: 4
                    }
                }
                
                Button {
                    text: qsTr("Test")
                    Layout.preferredWidth: 80
                    onClicked: {
                        console.log("[ParserConfig] Testing pattern with:", testInput.text)
                        // TODO: Call backend to test parsing
                    }
                }
            }
        }
    }
    
    onApplied: {
        var config = {
            preset: selectedPreset,
            customPattern: customPattern,
            columnHeaders: columnHeaders.split(",").map(function(s) { return s.trim() }),
            jsonEnabled: jsonEnabledCheck.checked,
            jsonKeys: jsonKeys.split(",").map(function(s) { return s.trim() }),
            xmlEnabled: xmlEnabledCheck.checked,
            log4jXmlMode: log4jXmlCheck.checked,
            xmlElements: xmlElements.split(",").map(function(s) { return s.trim() }),
            xmlAttributes: xmlAttributes.split(",").map(function(s) { return s.trim() }),
            dsvEnabled: dsvEnabledCheck.checked,
            dsvDelimiter: dsvDelimiter,
            dsvHasHeader: dsvHasHeader,
            multilineEnabled: multilineEnabled,
            multilineMode: multilineMode,
            multilineStartPattern: multilineStartPattern,
            maxLinesToMerge: maxLinesSpin.value
        }
        
        root.configurationApplied(config)
        console.log("[ParserConfig] Configuration applied:", JSON.stringify(config))
    }
}
