/**
 * TextTransformPanel.qml
 * Text Transformation Panel for BigFileViewer
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    width: Math.min(900, parent ? parent.width * 0.9 : 900)
    height: Math.min(700, parent ? parent.height * 0.9 : 700)
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    
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
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: qsTr("Text Transformer")
                font.bold: true
                font.pixelSize: 14
                color: root.textColor
            }
            
            Item { Layout.fillWidth: true }
            
            ToolButton {
                text: "×"
                font.pixelSize: 16
                onClicked: root.close()
            }
        }
        
        // Transformation categories
        TabBar {
            id: categoryBar
            Layout.fillWidth: true
            
            TabButton { text: qsTr("Case") }
            TabButton { text: qsTr("Encode") }
            TabButton { text: qsTr("Format") }
            TabButton { text: qsTr("Lines") }
            TabButton { text: qsTr("Escape") }
            TabButton { text: qsTr("Regex") }
            TabButton { text: qsTr("Time") }
        }
        
        // Input/Output Areas
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical
            
            // Input
            GroupBox {
                SplitView.preferredHeight: parent.height * 0.4
                title: qsTr("Input")
                
                ColumnLayout {
                    anchors.fill: parent
                    
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        
                        TextArea {
                            id: inputArea
                            placeholderText: qsTr("Enter or paste text to transform...")
                            wrapMode: TextEdit.Wrap
                            font.family: "Consolas, monospace"
                        }
                    }
                    
                    RowLayout {
                        Button {
                            text: qsTr("Clear")
                            onClicked: inputArea.text = ""
                        }
                        Button {
                            text: qsTr("Paste")
                            onClicked: inputArea.paste()
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: qsTr("%1 chars").arg(inputArea.text.length)
                            font.pixelSize: 11
                            opacity: 0.7
                        }
                    }
                }
            }
            
            // Transformation buttons
            StackLayout {
                currentIndex: categoryBar.currentIndex
                SplitView.preferredHeight: 120
                
                // Case transformations
                GridLayout {
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4
                    
                    Button {
                        text: qsTr("UPPERCASE")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.toUpperCase(inputArea.text)
                    }
                    Button {
                        text: qsTr("lowercase")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.toLowerCase(inputArea.text)
                    }
                    Button {
                        text: qsTr("Title Case")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.toTitleCase(inputArea.text)
                    }
                    Button {
                        text: qsTr("camelCase")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.toCamelCase(inputArea.text)
                    }
                    Button {
                        text: qsTr("snake_case")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.toSnakeCase(inputArea.text)
                    }
                    Button {
                        text: qsTr("kebab-case")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.toKebabCase(inputArea.text)
                    }
                }
                
                // Encode/Decode
                GridLayout {
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 4
                    
                    Button {
                        text: qsTr("Base64 Encode")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.base64Encode(inputArea.text)
                    }
                    Button {
                        text: qsTr("Base64 Decode")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.base64Decode(inputArea.text)
                    }
                    Button {
                        text: qsTr("URL Encode")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.urlEncode(inputArea.text)
                    }
                    Button {
                        text: qsTr("URL Decode")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.urlDecode(inputArea.text)
                    }
                    Button {
                        text: qsTr("HTML Encode")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.htmlEncode(inputArea.text)
                    }
                    Button {
                        text: qsTr("HTML Decode")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.htmlDecode(inputArea.text)
                    }
                    Button {
                        text: qsTr("Hex Encode")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.hexEncode(inputArea.text)
                    }
                    Button {
                        text: qsTr("Hex Decode")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.hexDecode(inputArea.text)
                    }
                }
                
                // Format
                GridLayout {
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 4
                    
                    Button {
                        text: qsTr("Format JSON")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.formatJson(inputArea.text)
                    }
                    Button {
                        text: qsTr("Minify JSON")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.minifyJson(inputArea.text)
                    }
                    Button {
                        text: qsTr("Format XML")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.formatXml(inputArea.text)
                    }
                    Button {
                        text: qsTr("Minify XML")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.minifyXml(inputArea.text)
                    }
                }
                
                // Lines
                GridLayout {
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4
                    
                    Button {
                        text: qsTr("Sort Lines A-Z")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.sortLines(inputArea.text, true)
                    }
                    Button {
                        text: qsTr("Sort Lines Z-A")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.sortLines(inputArea.text, false)
                    }
                    Button {
                        text: qsTr("Unique Lines")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.uniqueLines(inputArea.text)
                    }
                    Button {
                        text: qsTr("Reverse Lines")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.reverseLines(inputArea.text)
                    }
                    Button {
                        text: qsTr("Shuffle Lines")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.shuffleLines(inputArea.text)
                    }
                    Button {
                        text: qsTr("Number Lines")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.numberLines(inputArea.text)
                    }
                    Button {
                        text: qsTr("Trim Lines")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.trimLines(inputArea.text)
                    }
                    Button {
                        text: qsTr("Remove Empty")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.removeEmptyLines(inputArea.text)
                    }
                    Button {
                        text: qsTr("Statistics")
                        Layout.fillWidth: true
                        onClicked: {
                            var stats = _textTransformer.getStatistics(inputArea.text)
                            outputArea.text = "Characters: " + stats.characters + "\n" +
                                             "Characters (no spaces): " + stats.charactersNoSpaces + "\n" +
                                             "Words: " + stats.words + "\n" +
                                             "Lines: " + stats.lines + "\n" +
                                             "Non-empty lines: " + stats.nonEmptyLines + "\n" +
                                             "Paragraphs: " + stats.paragraphs
                        }
                    }
                }
                
                // Escape
                GridLayout {
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 4
                    
                    Button {
                        text: qsTr("Escape JSON")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.escapeJson(inputArea.text)
                    }
                    Button {
                        text: qsTr("Unescape JSON")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.unescapeJson(inputArea.text)
                    }
                    Button {
                        text: qsTr("Escape Regex")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.escapeRegex(inputArea.text)
                    }
                    Button {
                        text: qsTr("Escape SQL")
                        Layout.fillWidth: true
                        onClicked: outputArea.text = _textTransformer.escapeSql(inputArea.text)
                    }
                }
                
                // Regex
                ColumnLayout {
                    spacing: 4
                    
                    RowLayout {
                        Label { text: qsTr("Pattern:") }
                        TextField {
                            id: regexPatternField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Regular expression pattern")
                        }
                    }
                    RowLayout {
                        Label { text: qsTr("Replace:") }
                        TextField {
                            id: regexReplaceField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Replacement text (use $1, $2 for groups)")
                        }
                    }
                    RowLayout {
                        Button {
                            text: qsTr("Replace")
                            onClicked: outputArea.text = _textTransformer.regexReplace(
                                inputArea.text, regexPatternField.text, regexReplaceField.text)
                        }
                        Button {
                            text: qsTr("Extract Matches")
                            onClicked: {
                                var matches = _textTransformer.regexExtract(inputArea.text, regexPatternField.text)
                                outputArea.text = matches.join("\n")
                            }
                        }
                        Button {
                            text: qsTr("Split")
                            onClicked: {
                                var parts = _textTransformer.regexSplit(inputArea.text, regexPatternField.text)
                                outputArea.text = parts.join("\n---\n")
                            }
                        }
                    }
                }
                
                // Timestamp
                ColumnLayout {
                    spacing: 4
                    
                    RowLayout {
                        Label { text: qsTr("Timestamp:") }
                        TextField {
                            id: timestampField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Unix timestamp (ms)")
                        }
                        Button {
                            text: qsTr("To DateTime")
                            onClicked: {
                                var ts = parseInt(timestampField.text)
                                outputArea.text = _textTransformer.timestampToDateTime(ts, "yyyy-MM-dd HH:mm:ss.zzz")
                            }
                        }
                    }
                    RowLayout {
                        Label { text: qsTr("From Format:") }
                        TextField {
                            id: fromFormatField
                            Layout.preferredWidth: 150
                            text: "yyyy-MM-dd HH:mm:ss"
                        }
                        Label { text: qsTr("To Format:") }
                        TextField {
                            id: toFormatField
                            Layout.preferredWidth: 150
                            text: "dd/MM/yyyy HH:mm"
                        }
                        Button {
                            text: qsTr("Convert")
                            onClicked: outputArea.text = _textTransformer.convertTimestampFormat(
                                inputArea.text, fromFormatField.text, toFormatField.text)
                        }
                    }
                    Label {
                        text: qsTr("Format tokens: yyyy, MM, dd, HH, mm, ss, zzz")
                        font.pixelSize: 11
                        opacity: 0.7
                    }
                }
            }
            
            // Output
            GroupBox {
                SplitView.fillHeight: true
                title: qsTr("Output")
                
                ColumnLayout {
                    anchors.fill: parent
                    
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        
                        TextArea {
                            id: outputArea
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            font.family: "Consolas, monospace"
                        }
                    }
                    
                    RowLayout {
                        Button {
                            text: qsTr("Copy")
                            onClicked: {
                                outputArea.selectAll()
                                outputArea.copy()
                                outputArea.deselect()
                            }
                        }
                        Button {
                            text: qsTr("Use as Input")
                            onClicked: inputArea.text = outputArea.text
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: qsTr("%1 chars").arg(outputArea.text.length)
                            font.pixelSize: 11
                            opacity: 0.7
                        }
                    }
                }
            }
        }
    }
    
    // Error handling
    Connections {
        target: _textTransformer
        function onTransformationError(error) {
            outputArea.text = qsTr("Error: %1").arg(error)
        }
    }
}
