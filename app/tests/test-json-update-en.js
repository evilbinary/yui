// JSON Update Test - English version

// Test single property update
function testSingleUpdate() {
    YUI.log("Test single update");
    
    var update1 = '{"target":"statusLabel","change":{"text":"Status: Updated","color":"#4caf50"}}';
    YUI.update(update1);
}

// Test batch update
function testBatchUpdate() {
    YUI.log("Test batch update");
    
    var updates = '[{"target":"statusLabel","change":{"text":"Status: Batch","color":"#2196f3"}},{"target":"item1","change":{"text":"Item1","bgColor":"#2196f3"}}]';
    YUI.update(updates);
}

// Test delete operation
function testDelete() {
    YUI.log("Test delete");
    
    var hideBtn = '{"target":"updateBtn3","change":{"visible":null}}';
    YUI.update(hideBtn);
}

// Page load
function onPageLoad() {
    YUI.log("Page loaded");
}
