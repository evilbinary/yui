function onTermCommand(layerId) {
  var term = YUI.find(typeof layerId === "string" ? layerId : "term");
  var status = YUI.find("status");
  if (!term) return;

  var text = term.text ? String(term.text) : "";
  var parts = text.trim().split(/\s+/);
  var command = parts[0] || "";
  var output = "";

  YUI.log("input cmd: " + text);
  YUI.log("command: " + command);

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
      output = isNaN(a) || isNaN(b) ? "Usage: add <a> <b>" : String(a + b);
      break;
    case "clear":
      YUI.update(JSON.stringify({
        target: "term",
        change: { data: [] }
      }));
      if (status) status.text = "Cleared";
      return;
    case "":
      return;
    default:
      output = "Unknown: " + command;
  }

  if (output) {
    YUI.update(JSON.stringify({
      target: "term",
      change: { data: [{ text: output }] }
    }));
  }
  if (status) status.text = text;
}
