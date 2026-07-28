function onTermCommand(cmd) {
  var term = YUI.find("term");
  var status = YUI.find("status");
  var parts = cmd.trim().split(/\s+/);
  var command = parts[0];
  var output = "";
  switch (command) {
    case "help":
      output = "Available: help, echo <text>, date, clear, add <a> <b>";
      break;
    case "echo":
      output = parts.slice(1).join(" ");
      break;
    case "date":
      output = new Date().toString();
      break;
    case "add":
      var a = parseInt(parts[1], 10);
      var b = parseInt(parts[2], 10);
      output = isNaN(a) || isNaN(b) ? "Usage: add <a> <b>" : (a + b).toString();
      break;
    case "clear":
      YUI.update('{"target":"term","change":{"data":[]}}');
      status.text = "Cleared";
      return;
    default:
      output = "Unknown: " + command;
  }
  if (output) {
    var current = YUI.update('{"target":"term","get":"data"}');
    var arr = current || [];
    arr.push({text: output});
    YUI.update('{"target":"term","change":{"data":' + JSON.stringify(arr) + '}}');
  }
  status.text = cmd;
}
