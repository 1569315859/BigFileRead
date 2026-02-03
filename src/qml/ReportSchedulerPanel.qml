/**
 * ReportSchedulerPanel.qml
 * Report Scheduler and Template Management UI
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: qsTr("Report Scheduler")
    modal: true
    width: Math.min(900, parent.width * 0.9)
    height: Math.min(700, parent.height * 0.9)
    anchors.centerIn: parent
    standardButtons: Dialog.Close
    
    property color textColor: _themeManager ? _themeManager.textColor : palette.text
    property color panelColor: _themeManager ? _themeManager.panelBackground : palette.window
    property color bgColor: _themeManager ? _themeManager.backgroundColor : palette.base
    property color accentColor: _themeManager ? _themeManager.accentColor : palette.highlight
    property color borderColor: _themeManager ? _themeManager.borderColor : palette.mid
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }

    property var schedules: _reportScheduler ? _reportScheduler.schedules : []
    property var templates: _reportScheduler ? _reportScheduler.templates : []
    property int currentTabIndex: 0
    property string selectedScheduleId: ""
    property var editingSchedule: ({})

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true

            TabButton {
                text: qsTr("Schedules")
                width: implicitWidth
            }
            TabButton {
                text: qsTr("Templates")
                width: implicitWidth
            }
            TabButton {
                text: qsTr("History")
                width: implicitWidth
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // Schedules Tab
            Item {
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    // Schedule list
                    ColumnLayout {
                        Layout.preferredWidth: 250
                        Layout.fillHeight: true

                        Label {
                            text: qsTr("Scheduled Reports")
                            font.bold: true
                        }

                        ListView {
                            id: scheduleList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.schedules
                            spacing: 4

                            delegate: ItemDelegate {
                                width: scheduleList.width
                                height: 56
                                highlighted: root.selectedScheduleId === modelData.id
                                
                                background: Rectangle {
                                    color: parent.highlighted ? root.accentColor : 
                                           (parent.hovered ? Qt.lighter(root.panelColor, 1.1) : "transparent")
                                    border.color: parent.highlighted ? root.accentColor : root.borderColor
                                    border.width: parent.highlighted ? 2 : 1
                                    radius: 4
                                }

                                contentItem: RowLayout {
                                    spacing: 10

                                    Switch {
                                        Layout.alignment: Qt.AlignVCenter
                                        checked: modelData.enabled
                                        onToggled: _reportScheduler.setScheduleEnabled(modelData.id, checked)
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 4

                                        Label {
                                            text: modelData.name
                                            font.bold: true
                                            color: root.textColor
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Label {
                                            text: getFrequencyText(modelData.frequency)
                                            font.pixelSize: 11
                                            color: root.textColor
                                            opacity: 0.7
                                        }
                                    }
                                }

                                onClicked: {
                                    root.selectedScheduleId = modelData.id
                                    root.editingSchedule = _reportScheduler.getSchedule(modelData.id)
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                border.color: root.borderColor
                                radius: 4
                                z: -1
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                text: qsTr("Add")
                                icon.name: "list-add"
                                onClicked: createNewSchedule()
                            }

                            Button {
                                text: qsTr("Delete")
                                icon.name: "edit-delete"
                                enabled: root.selectedScheduleId !== ""
                                onClicked: deleteSelectedSchedule()
                            }
                        }
                    }

                    // Schedule editor
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ColumnLayout {
                            width: parent.width - 20
                            spacing: 15
                            visible: root.selectedScheduleId !== ""

                            GroupBox {
                                title: qsTr("Basic Settings")
                                Layout.fillWidth: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    RowLayout {
                                        Label { text: qsTr("Name:"); Layout.preferredWidth: 100 }
                                        TextField {
                                            id: scheduleNameField
                                            Layout.fillWidth: true
                                            text: root.editingSchedule.name || ""
                                            onTextChanged: root.editingSchedule.name = text
                                        }
                                    }

                                    RowLayout {
                                        Label { text: qsTr("Template:"); Layout.preferredWidth: 100 }
                                        ComboBox {
                                            id: templateCombo
                                            Layout.fillWidth: true
                                            model: root.templates
                                            textRole: "name"
                                            valueRole: "id"
                                            currentIndex: {
                                                for (var i = 0; i < templates.length; i++) {
                                                    if (templates[i].id === root.editingSchedule.templateName)
                                                        return i
                                                }
                                                return 0
                                            }
                                            onActivated: root.editingSchedule.templateName = currentValue
                                        }
                                    }

                                    RowLayout {
                                        Label { text: qsTr("Format:"); Layout.preferredWidth: 100 }
                                        ComboBox {
                                            id: formatCombo
                                            Layout.fillWidth: true
                                            model: ["HTML", "PDF", "CSV", "JSON", "Markdown"]
                                            currentIndex: root.editingSchedule.outputFormat || 0
                                            onActivated: root.editingSchedule.outputFormat = currentIndex
                                        }
                                    }
                                }
                            }

                            GroupBox {
                                title: qsTr("Schedule")
                                Layout.fillWidth: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    RowLayout {
                                        Label { text: qsTr("Frequency:"); Layout.preferredWidth: 100 }
                                        ComboBox {
                                            id: frequencyCombo
                                            Layout.fillWidth: true
                                            model: [qsTr("Once"), qsTr("Hourly"), qsTr("Daily"), 
                                                    qsTr("Weekly"), qsTr("Monthly"), qsTr("Custom")]
                                            currentIndex: root.editingSchedule.frequency || 2
                                            onActivated: root.editingSchedule.frequency = currentIndex
                                        }
                                    }

                                    RowLayout {
                                        visible: frequencyCombo.currentIndex >= 2
                                        Label { text: qsTr("Time:"); Layout.preferredWidth: 100 }
                                        SpinBox {
                                            id: hourSpin
                                            from: 0; to: 23
                                            value: root.editingSchedule.timeOfDay ? 
                                                   Qt.formatTime(root.editingSchedule.timeOfDay, "hh").valueOf() : 8
                                            onValueChanged: updateTimeOfDay()
                                        }
                                        Label { text: ":" }
                                        SpinBox {
                                            id: minuteSpin
                                            from: 0; to: 59
                                            value: root.editingSchedule.timeOfDay ?
                                                   Qt.formatTime(root.editingSchedule.timeOfDay, "mm").valueOf() : 0
                                            onValueChanged: updateTimeOfDay()
                                        }
                                    }

                                    RowLayout {
                                        visible: frequencyCombo.currentIndex === 3
                                        Label { text: qsTr("Day:"); Layout.preferredWidth: 100 }
                                        ComboBox {
                                            Layout.fillWidth: true
                                            model: [qsTr("Monday"), qsTr("Tuesday"), qsTr("Wednesday"),
                                                    qsTr("Thursday"), qsTr("Friday"), qsTr("Saturday"), qsTr("Sunday")]
                                            currentIndex: (root.editingSchedule.dayOfWeek || 1) - 1
                                            onActivated: root.editingSchedule.dayOfWeek = currentIndex + 1
                                        }
                                    }

                                    RowLayout {
                                        visible: frequencyCombo.currentIndex === 4
                                        Label { text: qsTr("Day of Month:"); Layout.preferredWidth: 100 }
                                        SpinBox {
                                            from: 1; to: 31
                                            value: root.editingSchedule.dayOfMonth || 1
                                            onValueChanged: root.editingSchedule.dayOfMonth = value
                                        }
                                    }

                                    Label {
                                        visible: root.editingSchedule && root.editingSchedule.nextRun !== undefined && root.editingSchedule.nextRun !== ""
                                        text: qsTr("Next run: %1").arg(
                                            root.editingSchedule && root.editingSchedule.nextRun ? 
                                            Qt.formatDateTime(root.editingSchedule.nextRun, "yyyy-MM-dd hh:mm") : "")
                                        font.italic: true
                                        opacity: 0.7
                                    }
                                }
                            }

                            GroupBox {
                                title: qsTr("Time Range")
                                Layout.fillWidth: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    CheckBox {
                                        id: useRelativeTimeCheck
                                        text: qsTr("Use relative time range")
                                        checked: root.editingSchedule.useRelativeTime !== false
                                        onToggled: root.editingSchedule.useRelativeTime = checked
                                    }

                                    RowLayout {
                                        visible: useRelativeTimeCheck.checked
                                        Label { text: qsTr("Last"); Layout.preferredWidth: 100 }
                                        SpinBox {
                                            from: 1; to: 720
                                            value: root.editingSchedule.timeRangeHours || 24
                                            onValueChanged: root.editingSchedule.timeRangeHours = value
                                        }
                                        Label { text: qsTr("hours") }
                                    }
                                }
                            }

                            GroupBox {
                                title: qsTr("Output")
                                Layout.fillWidth: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    RowLayout {
                                        Label { text: qsTr("Save to:"); Layout.preferredWidth: 100 }
                                        TextField {
                                            Layout.fillWidth: true
                                            text: root.editingSchedule.outputPath || ""
                                            onTextChanged: root.editingSchedule.outputPath = text
                                        }
                                        Button {
                                            text: "..."
                                            onClicked: folderDialog.open()
                                        }
                                    }

                                    RowLayout {
                                        Label { text: qsTr("Filename:"); Layout.preferredWidth: 100 }
                                        TextField {
                                            Layout.fillWidth: true
                                            text: root.editingSchedule.outputFilenamePattern || "report_{name}_{date}"
                                            placeholderText: "report_{name}_{date}_{time}"
                                            onTextChanged: root.editingSchedule.outputFilenamePattern = text
                                        }
                                    }

                                    Label {
                                        text: qsTr("Available: {name}, {date}, {time}, {datetime}")
                                        font.pixelSize: 11
                                        opacity: 0.6
                                    }
                                }
                            }

                            GroupBox {
                                title: qsTr("Distribution")
                                Layout.fillWidth: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    CheckBox {
                                        id: emailEnabledCheck
                                        text: qsTr("Send via Email")
                                        checked: root.editingSchedule.emailEnabled || false
                                        onToggled: root.editingSchedule.emailEnabled = checked
                                    }

                                    RowLayout {
                                        visible: emailEnabledCheck.checked
                                        Label { text: qsTr("Recipients:"); Layout.preferredWidth: 100 }
                                        TextField {
                                            Layout.fillWidth: true
                                            placeholderText: "email1@example.com, email2@example.com"
                                            text: (root.editingSchedule.emailRecipients || []).join(", ")
                                            onTextChanged: root.editingSchedule.emailRecipients = 
                                                text.split(",").map(s => s.trim()).filter(s => s.length > 0)
                                        }
                                    }

                                    CheckBox {
                                        id: webhookEnabledCheck
                                        text: qsTr("Send to Webhook")
                                        checked: root.editingSchedule.webhookEnabled || false
                                        onToggled: root.editingSchedule.webhookEnabled = checked
                                    }

                                    RowLayout {
                                        visible: webhookEnabledCheck.checked
                                        Label { text: qsTr("URL:"); Layout.preferredWidth: 100 }
                                        TextField {
                                            Layout.fillWidth: true
                                            placeholderText: "https://hooks.example.com/..."
                                            text: root.editingSchedule.webhookUrl || ""
                                            onTextChanged: root.editingSchedule.webhookUrl = text
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Button {
                                    text: qsTr("Save Changes")
                                    highlighted: true
                                    onClicked: saveScheduleChanges()
                                }

                                Button {
                                    text: qsTr("Run Now")
                                    onClicked: _reportScheduler.runScheduleNow(root.selectedScheduleId)
                                }

                                Button {
                                    text: qsTr("Preview")
                                    onClicked: previewReport()
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    visible: _reportScheduler && _reportScheduler.isRunning
                                    text: qsTr("Generating: %1 (%2%)")
                                        .arg(_reportScheduler?.currentReport || "")
                                        .arg(_reportScheduler?.progress || 0)
                                }
                            }

                            Item { Layout.preferredHeight: 20 }
                        }
                    }

                    Label {
                        visible: root.selectedScheduleId === ""
                        text: qsTr("Select a schedule to edit or create a new one")
                        opacity: 0.5
                        Layout.alignment: Qt.AlignCenter
                    }
                }
            }

            // Templates Tab
            Item {
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    // Template list with fixed width
                    Rectangle {
                        Layout.preferredWidth: 220
                        Layout.fillHeight: true
                        color: root.bgColor
                        border.color: root.borderColor
                        radius: 4

                        ListView {
                            id: templateList
                            anchors.fill: parent
                            anchors.margins: 1
                            clip: true
                            model: root.templates
                            spacing: 2

                            delegate: ItemDelegate {
                                width: templateList.width
                                height: 60
                                highlighted: ListView.isCurrentItem
                                
                                background: Rectangle {
                                    color: parent.highlighted ? root.accentColor : (parent.hovered ? Qt.lighter(root.panelColor, 1.1) : "transparent")
                                    radius: 4
                                }

                                contentItem: ColumnLayout {
                                    spacing: 2

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: modelData.name
                                            font.bold: true
                                            color: root.textColor
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: modelData.category
                                            font.pixelSize: 10
                                            padding: 2
                                            background: Rectangle {
                                                color: modelData.category === "Built-in" ? "#2196F3" : "#4CAF50"
                                                radius: 3
                                            }
                                            color: "white"
                                        }
                                    }

                                    Label {
                                        text: modelData.description || ""
                                        font.pixelSize: 11
                                        color: root.textColor
                                        opacity: 0.7
                                        wrapMode: Text.Wrap
                                        elide: Text.ElideRight
                                        maximumLineCount: 2
                                        Layout.fillWidth: true
                                    }
                                }
                                
                                onClicked: templateList.currentIndex = index
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ColumnLayout {
                            width: parent.width - 20
                            spacing: 15

                            Label {
                                text: qsTr("Template Sections")
                                font.bold: true
                                font.pixelSize: 16
                                color: root.textColor
                            }

                            GridLayout {
                                columns: 2
                                rowSpacing: 10
                                columnSpacing: 20
                                Layout.fillWidth: true

                                CheckBox { text: qsTr("Summary"); checked: true; palette.windowText: root.textColor }
                                CheckBox { text: qsTr("Timeline"); checked: true; palette.windowText: root.textColor }
                                CheckBox { text: qsTr("Error Distribution"); checked: true; palette.windowText: root.textColor }
                                CheckBox { text: qsTr("Top Messages"); checked: true; palette.windowText: root.textColor }
                                CheckBox { text: qsTr("Source Breakdown"); checked: true; palette.windowText: root.textColor }
                                CheckBox { text: qsTr("Keyword Hits"); checked: true; palette.windowText: root.textColor }
                                CheckBox { text: qsTr("Bookmarks"); checked: false; palette.windowText: root.textColor }
                                CheckBox { text: qsTr("Trends"); checked: true; palette.windowText: root.textColor }
                            }

                            GroupBox {
                                title: qsTr("Styling")
                                Layout.fillWidth: true
                                
                                label: Label {
                                    text: parent.title
                                    color: root.textColor
                                    font.bold: true
                                }
                                
                                background: Rectangle {
                                    y: parent.topPadding - parent.bottomPadding
                                    width: parent.width
                                    height: parent.height - parent.topPadding + parent.bottomPadding
                                    color: "transparent"
                                    border.color: root.borderColor
                                    radius: 4
                                }

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    RowLayout {
                                        Label { text: qsTr("Theme:"); Layout.preferredWidth: 100; color: root.textColor }
                                        ComboBox {
                                            model: ["Light", "Dark", "Corporate", "Custom"]
                                            Layout.fillWidth: true
                                        }
                                    }

                                    RowLayout {
                                        Label { text: qsTr("Company:"); Layout.preferredWidth: 100; color: root.textColor }
                                        TextField {
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Company name for report header")
                                        }
                                    }

                                    RowLayout {
                                        Label { text: qsTr("Logo:"); Layout.preferredWidth: 100; color: root.textColor }
                                        TextField {
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Path to logo image")
                                        }
                                        Button { text: "..." }
                                    }
                                }
                            }

                            RowLayout {
                                Button {
                                    text: qsTr("Create Template")
                                    onClicked: createNewTemplate()
                                }
                                Button {
                                    text: qsTr("Duplicate")
                                }
                                Button {
                                    text: qsTr("Export")
                                }
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }
                }
            }

            // History Tab
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("Report History")
                            font.bold: true
                            font.pixelSize: 16
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: qsTr("Clear History")
                            onClicked: _reportScheduler.clearHistory()
                        }
                    }

                    ListView {
                        id: historyList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: _reportScheduler ? _reportScheduler.getReportHistory(50) : []

                        delegate: ItemDelegate {
                            width: historyList.width
                            height: 70

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 15

                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: modelData.success ? "#4CAF50" : "#f44336"
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Label {
                                        text: modelData.scheduleName
                                        font.bold: true
                                    }

                                    Label {
                                        text: Qt.formatDateTime(modelData.timestamp, "yyyy-MM-dd hh:mm:ss")
                                        font.pixelSize: 11
                                        opacity: 0.7
                                    }

                                    Label {
                                        visible: !modelData.success
                                        text: modelData.error
                                        color: "#f44336"
                                        font.pixelSize: 11
                                    }
                                }

                                Label {
                                    visible: modelData.success && modelData.fileSize
                                    text: _reportScheduler ? _reportScheduler.formatBytes(modelData.fileSize) : ""
                                    opacity: 0.7
                                }

                                Button {
                                    text: qsTr("Open")
                                    visible: modelData.success
                                    onClicked: Qt.openUrlExternally("file:///" + modelData.filePath)
                                }

                                Button {
                                    text: qsTr("Delete")
                                    onClicked: _reportScheduler.deleteReportFromHistory(modelData.id)
                                }
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: palette.mid
                            radius: 4
                            z: -1
                        }
                    }
                }
            }
        }
    }

    function getFrequencyText(freq) {
        var texts = [qsTr("Once"), qsTr("Hourly"), qsTr("Daily"), 
                     qsTr("Weekly"), qsTr("Monthly"), qsTr("Custom")]
        return texts[freq] || texts[2]
    }

    function createNewSchedule() {
        var config = {
            name: qsTr("New Schedule"),
            frequency: 2,  // Daily
            templateName: "default",
            outputFormat: 0,  // HTML
            timeRangeHours: 24,
            useRelativeTime: true
        }
        var result = _reportScheduler.createSchedule(config)
        if (result.success) {
            root.selectedScheduleId = result.id
            root.editingSchedule = _reportScheduler.getSchedule(result.id)
        }
    }

    function deleteSelectedSchedule() {
        if (root.selectedScheduleId !== "") {
            _reportScheduler.deleteSchedule(root.selectedScheduleId)
            root.selectedScheduleId = ""
            root.editingSchedule = {}
        }
    }

    function saveScheduleChanges() {
        if (root.selectedScheduleId !== "") {
            _reportScheduler.updateSchedule(root.selectedScheduleId, root.editingSchedule)
        }
    }

    function updateTimeOfDay() {
        // Note: Would need proper time handling
    }

    function previewReport() {
        var html = _reportScheduler.previewReport(root.editingSchedule)
        previewDialog.htmlContent = html
        previewDialog.open()
    }

    function createNewTemplate() {
        var config = {
            name: qsTr("New Template"),
            description: ""
        }
        _reportScheduler.createTemplate(config)
    }

    Dialog {
        id: previewDialog
        title: qsTr("Report Preview")
        width: 800
        height: 600
        modal: true
        standardButtons: Dialog.Close

        property string htmlContent: ""

        ScrollView {
            anchors.fill: parent
            
            TextArea {
                text: previewDialog.htmlContent
                readOnly: true
                wrapMode: TextArea.Wrap
                font.family: "Consolas"
                font.pixelSize: 12
            }
        }
    }

    Connections {
        target: _reportScheduler
        function onSchedulesChanged() {
            root.schedules = _reportScheduler.schedules
        }
        function onTemplatesChanged() {
            root.templates = _reportScheduler.templates
        }
        function onReportGenerated(scheduleId, filePath) {
            console.log("Report generated:", filePath)
        }
        function onReportGenerationFailed(scheduleId, error) {
            console.error("Report generation failed:", error)
        }
    }
}
