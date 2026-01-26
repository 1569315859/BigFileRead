import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @file RuleWizard.qml
 * @brief Rule creation wizard - step-by-step guided rule creation
 * 
 * Steps:
 * 1. Basic Info - Name, description
 * 2. Conditions - Define matching conditions
 * 3. Actions - Select notification actions
 * 4. Schedule - Optional time scheduling
 * 5. Test & Save - Test with sample data and save
 */
Dialog {
    id: root
    title: qsTr("Rule Wizard")
    width: 700
    height: 550
    modal: true
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
    palette.buttonText: textColor
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    property int currentStep: 0
    property var ruleData: ({
        name: "",
        description: "",
        enabled: true,
        conditions: [],
        logic: 0,  // And
        actions: [],
        cooldownSeconds: 60,
        scheduleEnabled: false,
        scheduleStart: "",
        scheduleEnd: "",
        scheduleDays: []
    })
    
    property string editRuleId: ""  // Empty for new rule
    
    signal ruleCreated(var rule)
    signal ruleUpdated(string ruleId, var rule)
    
    header: ColumnLayout {
        spacing: 0
        
        // Progress indicator
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 15
            spacing: 5
            
            Repeater {
                model: [qsTr("Basic Info"), qsTr("Conditions"), qsTr("Actions"), qsTr("Schedule"), qsTr("Test & Save")]
                
                RowLayout {
                    spacing: 5
                    
                    Rectangle {
                        width: 24
                        height: 24
                        radius: 12
                        color: index <= currentStep ? "#2196f3" : "#e0e0e0"
                        
                        Label {
                            anchors.centerIn: parent
                            text: index + 1
                            color: index <= currentStep ? "white" : "#666"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                    
                    Label {
                        text: modelData
                        color: index <= currentStep ? "#2196f3" : "#666"
                        font.bold: index === currentStep
                        font.pixelSize: 12
                    }
                    
                    Rectangle {
                        visible: index < 4
                        width: 30
                        height: 2
                        color: index < currentStep ? "#2196f3" : "#e0e0e0"
                    }
                }
            }
        }
        
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#e0e0e0"
        }
    }
    
    StackLayout {
        anchors.fill: parent
        anchors.margins: 15
        currentIndex: currentStep
        
        // Step 1: Basic Info
        ColumnLayout {
            spacing: 15
            
            Label {
                text: qsTr("Step 1: Basic Information")
                font.bold: true
                font.pixelSize: 16
            }
            
            Label {
                text: qsTr("Enter a name and description for your rule.")
                color: "#666"
            }
            
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 10
                Layout.fillWidth: true
                
                Label { text: qsTr("Rule Name:") }
                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    text: ruleData.name
                    placeholderText: qsTr("e.g., Error Alert, Critical Warning")
                    onTextChanged: ruleData.name = text
                }
                
                Label { text: qsTr("Description:") }
                TextArea {
                    id: descField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    text: ruleData.description
                    placeholderText: qsTr("Describe what this rule does...")
                    onTextChanged: ruleData.description = text
                }
                
                Label { text: qsTr("Enabled:") }
                Switch {
                    checked: ruleData.enabled
                    onCheckedChanged: ruleData.enabled = checked
                }
            }
            
            Item { Layout.fillHeight: true }
        }
        
        // Step 2: Conditions
        ColumnLayout {
            spacing: 15
            
            Label {
                text: qsTr("Step 2: Define Conditions")
                font.bold: true
                font.pixelSize: 16
            }
            
            RowLayout {
                Label { text: qsTr("Match:") }
                ComboBox {
                    id: logicCombo
                    model: [qsTr("All conditions (AND)"), qsTr("Any condition (OR)")]
                    currentIndex: ruleData.logic
                    onCurrentIndexChanged: ruleData.logic = currentIndex
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("+ Add Condition")
                    onClicked: addCondition()
                }
            }
            
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                ListView {
                    id: conditionsList
                    model: ruleData.conditions
                    spacing: 10
                    
                    delegate: Rectangle {
                        width: conditionsList.width - 20
                        height: 60
                        color: "#f5f5f5"
                        radius: 4
                        border.color: "#ddd"
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10
                            
                            ComboBox {
                                id: fieldCombo
                                model: _ruleEngine ? _ruleEngine.getAvailableFields() : []
                                textRole: "name"
                                currentIndex: modelData.field || 0
                                Layout.preferredWidth: 120
                                onCurrentIndexChanged: {
                                    var conds = ruleData.conditions
                                    conds[index].field = currentIndex
                                    ruleData.conditions = conds
                                }
                            }
                            
                            ComboBox {
                                id: opCombo
                                model: _ruleEngine ? _ruleEngine.getAvailableOperators() : []
                                textRole: "name"
                                currentIndex: modelData.operator || 0
                                Layout.preferredWidth: 140
                                onCurrentIndexChanged: {
                                    var conds = ruleData.conditions
                                    conds[index].operator = currentIndex
                                    ruleData.conditions = conds
                                }
                            }
                            
                            TextField {
                                Layout.fillWidth: true
                                text: modelData.value || ""
                                placeholderText: qsTr("Value...")
                                onTextChanged: {
                                    var conds = ruleData.conditions
                                    conds[index].value = text
                                    ruleData.conditions = conds
                                }
                            }
                            
                            CheckBox {
                                text: qsTr("Case")
                                checked: modelData.caseSensitive || false
                                onCheckedChanged: {
                                    var conds = ruleData.conditions
                                    conds[index].caseSensitive = checked
                                    ruleData.conditions = conds
                                }
                            }
                            
                            Button {
                                text: "×"
                                flat: true
                                onClicked: removeCondition(index)
                            }
                        }
                    }
                }
            }
            
            Label {
                visible: ruleData.conditions.length === 0
                text: qsTr("No conditions defined. Add at least one condition.")
                color: "#f44336"
            }
        }
        
        // Step 3: Actions
        ColumnLayout {
            spacing: 15
            
            Label {
                text: qsTr("Step 3: Select Actions")
                font.bold: true
                font.pixelSize: 16
            }
            
            Label {
                text: qsTr("Choose what happens when this rule is triggered.")
                color: "#666"
            }
            
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                GridLayout {
                    columns: 2
                    columnSpacing: 15
                    rowSpacing: 15
                    width: parent.width
                    
                    Repeater {
                        model: _ruleEngine ? _ruleEngine.getAvailableActions() : []
                        
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: isActionSelected(modelData.type) ? "#e3f2fd" : "#f5f5f5"
                            border.color: isActionSelected(modelData.type) ? "#2196f3" : "#ddd"
                            border.width: isActionSelected(modelData.type) ? 2 : 1
                            radius: 8
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: toggleAction(modelData.type)
                            }
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12
                                
                                CheckBox {
                                    checked: isActionSelected(modelData.type)
                                    onCheckedChanged: {
                                        if (checked !== isActionSelected(modelData.type)) {
                                            toggleAction(modelData.type)
                                        }
                                    }
                                }
                                
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                    }
                                    
                                    Label {
                                        text: modelData.description
                                        color: "#666"
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Action configuration (shown when action selected)
            GroupBox {
                visible: ruleData.actions.length > 0
                title: qsTr("Action Configuration")
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    
                    Label {
                        text: qsTr("Cooldown (seconds between triggers):")
                    }
                    
                    SpinBox {
                        from: 0
                        to: 3600
                        value: ruleData.cooldownSeconds
                        onValueChanged: ruleData.cooldownSeconds = value
                    }
                }
            }
        }
        
        // Step 4: Schedule
        ColumnLayout {
            spacing: 15
            
            Label {
                text: qsTr("Step 4: Schedule (Optional)")
                font.bold: true
                font.pixelSize: 16
            }
            
            CheckBox {
                id: scheduleEnabled
                text: qsTr("Enable time-based scheduling")
                checked: ruleData.scheduleEnabled
                onCheckedChanged: ruleData.scheduleEnabled = checked
            }
            
            GroupBox {
                enabled: scheduleEnabled.checked
                title: qsTr("Active Time Range")
                Layout.fillWidth: true
                
                RowLayout {
                    Label { text: qsTr("From:") }
                    TextField {
                        text: ruleData.scheduleStart
                        placeholderText: "HH:mm"
                        inputMask: "99:99"
                        onTextChanged: ruleData.scheduleStart = text
                    }
                    Label { text: qsTr("To:") }
                    TextField {
                        text: ruleData.scheduleEnd
                        placeholderText: "HH:mm"
                        inputMask: "99:99"
                        onTextChanged: ruleData.scheduleEnd = text
                    }
                }
            }
            
            GroupBox {
                enabled: scheduleEnabled.checked
                title: qsTr("Active Days")
                Layout.fillWidth: true
                
                RowLayout {
                    Repeater {
                        model: [qsTr("Sun"), qsTr("Mon"), qsTr("Tue"), qsTr("Wed"), qsTr("Thu"), qsTr("Fri"), qsTr("Sat")]
                        
                        CheckBox {
                            text: modelData
                            checked: ruleData.scheduleDays.indexOf(index) >= 0
                            onCheckedChanged: {
                                var days = ruleData.scheduleDays.slice()
                                var idx = days.indexOf(index)
                                if (checked && idx < 0) {
                                    days.push(index)
                                } else if (!checked && idx >= 0) {
                                    days.splice(idx, 1)
                                }
                                ruleData.scheduleDays = days
                            }
                        }
                    }
                }
            }
            
            Item { Layout.fillHeight: true }
        }
        
        // Step 5: Test & Save
        ColumnLayout {
            spacing: 15
            
            Label {
                text: qsTr("Step 5: Test & Save")
                font.bold: true
                font.pixelSize: 16
            }
            
            GroupBox {
                title: qsTr("Test Your Rule")
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    
                    Label {
                        text: qsTr("Enter sample log lines to test:")
                    }
                    
                    TextArea {
                        id: testInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        placeholderText: qsTr("Paste sample log lines here...")
                        font.family: "Consolas, Monaco, monospace"
                    }
                    
                    Button {
                        text: qsTr("Test Rule")
                        enabled: testInput.text.length > 0 && ruleData.conditions.length > 0
                        onClicked: runTest()
                    }
                    
                    ListView {
                        id: testResults
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: []
                        clip: true
                        
                        delegate: Rectangle {
                            width: testResults.width
                            height: 30
                            color: modelData.matched ? "#e8f5e9" : "#ffebee"
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 5
                                
                                Label {
                                    text: modelData.matched ? "✓" : "✗"
                                    color: modelData.matched ? "#4caf50" : "#f44336"
                                    font.bold: true
                                }
                                
                                Label {
                                    text: qsTr("Line %1").arg(modelData.lineNumber)
                                    Layout.preferredWidth: 60
                                }
                                
                                Label {
                                    text: modelData.line
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
            
            // Summary
            GroupBox {
                title: qsTr("Rule Summary")
                Layout.fillWidth: true
                
                GridLayout {
                    columns: 2
                    columnSpacing: 20
                    
                    Label { text: qsTr("Name:"); font.bold: true }
                    Label { text: ruleData.name || qsTr("(not set)") }
                    
                    Label { text: qsTr("Conditions:"); font.bold: true }
                    Label { text: ruleData.conditions.length + " " + qsTr("condition(s)") }
                    
                    Label { text: qsTr("Actions:"); font.bold: true }
                    Label { text: ruleData.actions.length + " " + qsTr("action(s)") }
                    
                    Label { text: qsTr("Schedule:"); font.bold: true }
                    Label { text: ruleData.scheduleEnabled ? qsTr("Enabled") : qsTr("Always active") }
                }
            }
        }
    }
    
    footer: DialogButtonBox {
        Button {
            text: qsTr("Back")
            enabled: currentStep > 0
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: currentStep--
        }
        
        Button {
            text: currentStep < 4 ? qsTr("Next") : qsTr("Save Rule")
            enabled: canProceed()
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: {
                if (currentStep < 4) {
                    currentStep++
                } else {
                    saveRule()
                }
            }
        }
        
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.reject()
        }
    }
    
    function canProceed() {
        switch (currentStep) {
        case 0: return ruleData.name.length > 0
        case 1: return ruleData.conditions.length > 0
        case 2: return ruleData.actions.length > 0
        default: return true
        }
    }
    
    function addCondition() {
        var conds = ruleData.conditions.slice()
        conds.push({
            field: 0,
            operator: 0,
            value: "",
            caseSensitive: false
        })
        ruleData.conditions = conds
    }
    
    function removeCondition(index) {
        var conds = ruleData.conditions.slice()
        conds.splice(index, 1)
        ruleData.conditions = conds
    }
    
    function isActionSelected(actionType) {
        for (var i = 0; i < ruleData.actions.length; i++) {
            if (ruleData.actions[i].type === actionType) return true
        }
        return false
    }
    
    function toggleAction(actionType) {
        var actions = ruleData.actions.slice()
        var found = -1
        for (var i = 0; i < actions.length; i++) {
            if (actions[i].type === actionType) {
                found = i
                break
            }
        }
        
        if (found >= 0) {
            actions.splice(found, 1)
        } else {
            actions.push({ type: actionType, config: {} })
        }
        
        ruleData.actions = actions
    }
    
    function runTest() {
        if (_ruleEngine) {
            var lines = testInput.text.split(/\r?\n/).filter(function(l) { return l.length > 0 })
            var results = _ruleEngine.testRule(ruleData, lines)
            testResults.model = results
        }
    }
    
    function saveRule() {
        if (_ruleEngine) {
            if (editRuleId) {
                _ruleEngine.updateRule(editRuleId, ruleData)
                root.ruleUpdated(editRuleId, ruleData)
            } else {
                _ruleEngine.addRule(ruleData)
                root.ruleCreated(ruleData)
            }
        }
        root.accept()
    }
    
    function loadRule(ruleId) {
        if (_ruleEngine) {
            var rule = _ruleEngine.getRule(ruleId)
            if (rule) {
                editRuleId = ruleId
                ruleData = rule
            }
        }
    }
    
    function resetForm() {
        currentStep = 0
        editRuleId = ""
        ruleData = {
            name: "",
            description: "",
            enabled: true,
            conditions: [],
            logic: 0,
            actions: [],
            cooldownSeconds: 60,
            scheduleEnabled: false,
            scheduleStart: "",
            scheduleEnd: "",
            scheduleDays: []
        }
        testResults.model = []
    }
    
    Component.onCompleted: {
        resetForm()
    }
}
