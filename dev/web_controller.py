#!/usr/bin/env python3
"""
Brush-n-Snack Robot — Web Controller
=====================================
Bridges laptop <-> Bluetooth (via Communication Bridge Pro) <-> Robot.

Usage:
    python web_controller.py                          # auto-detect port
    python web_controller.py --port /dev/rfcomm0      # Linux / macOS
    python web_controller.py --port COM5              # Windows
    python web_controller.py --port /dev/tty.usbmodem1 --baud 115200

Then open http://localhost:5000 in your browser.

Master command protocol (sent over serial):
    F<steps>          forward
    B<steps>          backward
    L<steps>          turn left
    R<steps>          turn right
    x                 stop drive
  V<speed>          set drive speed (80..600)
  J                 quick dance
    A<slot><delta>    arm move (slot 1-3, delta encoder steps)
    Ax                arm stop all
    Go / Gc / Gs      gripper open / close / stop
    C<c>,<R>,<G>,<B>  LED fade  (c = 1 or 2)
    D<c>,<R>,<G>,<B>  LED instant set
    U0 / U1            ultrasonic safety stop disable / enable
"""

import argparse
import threading
import time
from collections import deque
from datetime import datetime

import serial
import serial.tools.list_ports
from flask import Flask, jsonify, render_template_string, request

# ── Defaults ──────────────────────────────────────────────────────────────────
DEFAULT_BAUD      = 115200
DEFAULT_STEPS     = 360     # encoder steps per drive button press
DEFAULT_ARM_DELTA = 150     # encoder steps per arm button press

# ── App / state ───────────────────────────────────────────────────────────────
app  = Flask(__name__)
ser  = None
_lock = threading.Lock()   # guards serial port access
_log  = deque(maxlen=200)  # rolling log shown in the UI


# ── Logging ───────────────────────────────────────────────────────────────────
def _log_add(direction: str, text: str):
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    _log.append({"ts": ts, "dir": direction, "text": text})


# ── Serial helpers ────────────────────────────────────────────────────────────
def open_port(port: str, baud: int = DEFAULT_BAUD):
    global ser
    with _lock:
        if ser and ser.is_open:
            try:
                ser.close()
            except Exception:
                pass
        ser = serial.Serial(port, baud, timeout=0)   # non-blocking read
    _log_add("sys", f"Connected to {port} @ {baud} baud")


def send_cmd(cmd: str):
    """Send one newline-terminated command over serial."""
    with _lock:
        if ser is None or not ser.is_open:
            raise RuntimeError("Serial port not open")
        ser.write((cmd + "\n").encode("ascii"))
    _log_add("tx", cmd)


def _reader_thread():
    """Background thread: drain incoming bytes, split into lines, log them."""
    buf = b""
    while True:
        try:
            if ser and ser.is_open:
                with _lock:
                    n = ser.in_waiting
                    chunk = ser.read(n) if n else b""
                if chunk:
                    buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    text = line.decode(errors="replace").strip()
                    if text:
                        _log_add("rx", text)
        except Exception:
            buf = b""
        time.sleep(0.02)


# ── Flask routes ──────────────────────────────────────────────────────────────
@app.route("/")
def index():
    return render_template_string(_HTML)


@app.route("/ports")
def list_ports():
    """Return list of available serial ports."""
    ports = [p.device for p in serial.tools.list_ports.comports()]
    return jsonify(ports)


@app.route("/connect", methods=["POST"])
def connect():
    data = request.get_json(force=True)
    port = str(data.get("port", "")).strip()
    baud = int(data.get("baud", DEFAULT_BAUD))
    if not port:
        return jsonify({"ok": False, "error": "No port specified"}), 400
    try:
        open_port(port, baud)
        return jsonify({"ok": True})
    except Exception as exc:
        _log_add("err", str(exc))
        return jsonify({"ok": False, "error": str(exc)}), 500


@app.route("/disconnect", methods=["POST"])
def disconnect():
    global ser
    with _lock:
        if ser and ser.is_open:
            ser.close()
    _log_add("sys", "Disconnected")
    return jsonify({"ok": True})


@app.route("/cmd", methods=["POST"])
def cmd():
    data = request.get_json(force=True)
    command = str(data.get("cmd", "")).strip()
    if not command:
        return jsonify({"ok": False, "error": "Empty command"}), 400
    # Validate: printable ASCII only, max 64 chars — no shell injection possible
    if len(command) > 64 or not all(32 <= ord(c) < 127 for c in command):
        return jsonify({"ok": False, "error": "Invalid characters in command"}), 400
    try:
        send_cmd(command)
        return jsonify({"ok": True})
    except RuntimeError as exc:
        return jsonify({"ok": False, "error": str(exc)}), 503


@app.route("/status")
def status():
    connected = bool(ser and ser.is_open)
    return jsonify({
        "connected": connected,
        "port": ser.port if connected else None,
        "log": list(_log),
    })


# ── HTML + JS (single-file UI) ────────────────────────────────────────────────
_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Brush-n-Snack Controller</title>
<style>
  :root{--bg:#0f111a;--card:#1a1d2e;--border:#2d3250;--accent:#7c83fd;--green:#4caf7d;--red:#e05c5c;--text:#e2e4f0;--muted:#6b7194;--btn:#252841;--btn-hover:#2e3357;}
  *{box-sizing:border-box;margin:0;padding:0;}
  body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;padding:16px;}
  h1{text-align:center;font-size:1.4rem;letter-spacing:.06em;color:var(--accent);margin-bottom:16px;}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(270px,1fr));gap:14px;}
  .card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px;}
  .card h2{font-size:.75rem;text-transform:uppercase;letter-spacing:.1em;color:var(--muted);margin-bottom:12px;}
  /* Connection bar */
  .conn-bar{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:14px;}
  select,input[type=text],input[type=number]{
    background:var(--btn);border:1px solid var(--border);color:var(--text);
    border-radius:8px;padding:6px 10px;font-size:.85rem;outline:none;flex:1;min-width:0;}
  .dot{width:10px;height:10px;border-radius:50%;background:var(--red);flex-shrink:0;}
  .dot.on{background:var(--green);}
  /* Buttons */
  button{
    background:var(--btn);border:1px solid var(--border);color:var(--text);
    border-radius:8px;padding:8px 14px;font-size:.85rem;cursor:pointer;transition:background .15s;}
  button:hover{background:var(--btn-hover);}
  button:active{transform:scale(.95);}
  button.accent{background:var(--accent);border-color:var(--accent);color:#fff;}
  button.red{background:var(--red);border-color:var(--red);color:#fff;}
  /* D-pad */
  .dpad{display:grid;grid-template-columns:repeat(3,52px);grid-template-rows:repeat(3,52px);gap:5px;justify-content:center;}
  .dpad button{width:52px;height:52px;padding:0;font-size:1.2rem;}
  .dpad .mid{display:flex;gap:5px;}
  /* Arm grid */
  .arm-grid{display:grid;grid-template-columns:1fr auto auto auto;gap:6px;align-items:center;}
  .arm-grid label{font-size:.82rem;color:var(--muted);}
  /* Gripper row */
  .row{display:flex;gap:8px;flex-wrap:wrap;}
  /* LED pickers */
  .led-row{display:grid;grid-template-columns:auto auto 1fr auto;gap:8px;align-items:center;margin-bottom:6px;}
  input[type=color]{width:36px;height:28px;border:1px solid var(--border);border-radius:6px;cursor:pointer;background:none;padding:0;}
  /* Log */
  #log{background:#0a0c14;border:1px solid var(--border);border-radius:8px;height:160px;overflow-y:auto;
       padding:8px;font-family:monospace;font-size:.78rem;line-height:1.55;}
  .rx{color:#7ec8e3;} .tx{color:#a8e6a3;} .sys{color:var(--muted);} .err{color:var(--red);}
  /* Custom cmd */
  .cmd-row{display:flex;gap:8px;}
  .cmd-row input{flex:1;}
</style>
</head>
<body>
<h1>Brush-n-Snack Controller</h1>

<!-- Connection -->
<div class="card" style="margin-bottom:14px">
  <h2>Connection</h2>
  <div class="conn-bar">
    <div class="dot" id="dot"></div>
    <select id="portSel" style="flex:2"></select>
    <button onclick="refreshPorts()" title="Refresh ports">&#x21BB;</button>
    <input type="number" id="baudIn" value="115200" style="width:90px;flex:none">
    <button class="accent" onclick="connect()" id="connBtn">Connect</button>
    <button onclick="disconnect()">Disconnect</button>
  </div>
</div>

<div class="grid">

  <!-- Drive -->
  <div class="card">
    <h2>Drive</h2>
    <div style="margin-bottom:10px">
      <label style="font-size:.8rem;color:var(--muted)">Steps per press: </label>
      <input type="number" id="driveSteps" value="360" style="width:80px">
    </div>
    <div style="margin-bottom:10px;display:flex;gap:8px;align-items:center;flex-wrap:wrap">
      <label style="font-size:.8rem;color:var(--muted)">Drive speed: </label>
      <input type="number" id="driveSpeed" value="300" min="80" max="600" style="width:90px">
      <button onclick="setDriveSpeed()" style="font-size:.8rem">Apply</button>
    </div>
    <div class="dpad">
      <div></div>
      <button onclick="drive('F')" title="Forward">&#x25B2;</button>
      <div></div>
      <button onclick="drive('L')" title="Left">&#x25C4;</button>
      <button class="red" onclick="send('x')" title="Stop">&#x25FC;</button>
      <button onclick="drive('R')" title="Right">&#x25BA;</button>
      <div></div>
      <button onclick="drive('B')" title="Backward">&#x25BC;</button>
      <div></div>
    </div>
    <div style="margin-top:10px;display:flex;gap:8px;justify-content:center">
      <button class="accent" onclick="startDance()">Dance</button>
    </div>
  </div>

  <!-- Arm -->
  <div class="card">
    <h2>Arm (encoder steps)</h2>
    <div style="margin-bottom:10px">
      <label style="font-size:.8rem;color:var(--muted)">Delta per press: </label>
      <input type="number" id="armDelta" value="150" style="width:80px">
    </div>
    <div class="arm-grid">
      <label>Base (A1)</label>
      <button onclick="arm(1,-1)">&#x2212;</button>
      <button onclick="arm(1, 1)">&#x2B;</button>
      <button class="red" onclick="send('Ax')" style="font-size:.7rem">Stop all</button>

      <label>Shoulder (A2)</label>
      <button onclick="arm(2,-1)">&#x2212;</button>
      <button onclick="arm(2, 1)">&#x2B;</button>
      <div></div>

      <label>Elbow (A3)</label>
      <button onclick="arm(3,-1)">&#x2212;</button>
      <button onclick="arm(3, 1)">&#x2B;</button>
      <div></div>
    </div>
  </div>

  <!-- Gripper -->
  <div class="card">
    <h2>Gripper</h2>
    <div class="row" style="margin-top:8px">
      <button onclick="send('Go')" style="flex:1">&#x25C1; Open</button>
      <button onclick="send('Gc')" style="flex:1">Close &#x25B7;</button>
      <button class="red" onclick="send('Gs')">Stop</button>
    </div>
  </div>

  <!-- LEDs -->
  <div class="card">
    <h2>LEDs</h2>
    <div class="led-row">
      <label style="font-size:.82rem">Left&nbsp;(1)</label>
      <input type="color" id="led1" value="#0000ff">
      <div></div>
      <div style="display:flex;gap:6px">
        <button onclick="ledFade(1)" title="Smooth fade">Fade</button>
        <button onclick="ledSet(1)"  title="Instant set">Set</button>
      </div>
    </div>
    <div class="led-row">
      <label style="font-size:.82rem">Right&nbsp;(2)</label>
      <input type="color" id="led2" value="#0000ff">
      <div></div>
      <div style="display:flex;gap:6px">
        <button onclick="ledFade(2)" title="Smooth fade">Fade</button>
        <button onclick="ledSet(2)"  title="Instant set">Set</button>
      </div>
    </div>
    <div style="margin-top:6px">
      <label style="font-size:.8rem;color:var(--muted)">Turn-signal colour: </label>
      <input type="color" id="turnColor" value="#ffaa00" style="vertical-align:middle">
      <button onclick="setTurnColor()" style="margin-left:6px;font-size:.8rem">Apply</button>
    </div>
    <div style="margin-top:10px;display:flex;gap:8px;flex-wrap:wrap">
      <button onclick="send('U1')" class="accent" style="font-size:.8rem">Ultrasonic ON</button>
      <button onclick="send('U0')" style="font-size:.8rem">Ultrasonic OFF</button>
    </div>
  </div>

  <!-- Custom command -->
  <div class="card">
    <h2>Custom command</h2>
    <div class="cmd-row" style="margin-top:4px">
      <input type="text" id="customCmd" placeholder="e.g. F360 or A1150" maxlength="64"
             onkeydown="if(event.key==='Enter')sendCustom()">
      <button class="accent" onclick="sendCustom()">Send</button>
    </div>
  </div>

  <!-- Log -->
  <div class="card" style="grid-column:1/-1">
    <h2>Serial log &nbsp;<button onclick="clearLog()" style="font-size:.7rem;padding:3px 8px">Clear</button></h2>
    <div id="log"></div>
  </div>

</div>

<script>
// ── Helpers ──────────────────────────────────────────────────────────────────
async function send(cmd) {
  try {
    const r = await fetch('/cmd', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({cmd})
    });
    if (!r.ok) {
      const j = await r.json();
      appendLog('err', 'Error: ' + j.error);
    }
  } catch(e) {
    appendLog('err', 'Fetch error: ' + e.message);
  }
}

function drive(dir) {
  const steps = parseInt(document.getElementById('driveSteps').value) || 360;
  send(dir + steps);
}

function arm(slot, sign) {
  const delta = parseInt(document.getElementById('armDelta').value) || 150;
  send('A' + slot + (sign * delta));
}

function setDriveSpeed() {
  let speed = parseInt(document.getElementById('driveSpeed').value);
  if (!Number.isFinite(speed)) speed = 300;
  if (speed < 80) speed = 80;
  if (speed > 600) speed = 600;
  document.getElementById('driveSpeed').value = speed;
  send('V' + speed);
}

function startDance() {
  send('J');
}

function hexToRgb(hex) {
  const n = parseInt(hex.slice(1), 16);
  return [(n>>16)&255, (n>>8)&255, n&255];
}

function ledFade(conn) {
  const [r,g,b] = hexToRgb(document.getElementById('led'+conn).value);
  send('C' + conn + ',' + r + ',' + g + ',' + b);
}
function ledSet(conn) {
  const [r,g,b] = hexToRgb(document.getElementById('led'+conn).value);
  send('D' + conn + ',' + r + ',' + g + ',' + b);
}
function setTurnColor() {
  const [r,g,b] = hexToRgb(document.getElementById('turnColor').value);
  send('T' + r + ',' + g + ',' + b);
}

function sendCustom() {
  const v = document.getElementById('customCmd').value.trim();
  if (v) send(v);
}

// ── Connection ───────────────────────────────────────────────────────────────
async function refreshPorts() {
  const r = await fetch('/ports');
  const ports = await r.json();
  const sel = document.getElementById('portSel');
  const cur = sel.value;
  sel.innerHTML = '';
  if (ports.length === 0) {
    sel.innerHTML = '<option value="">— no ports found —</option>';
  } else {
    ports.forEach(p => {
      const o = document.createElement('option');
      o.value = o.textContent = p;
      if (p === cur) o.selected = true;
      sel.appendChild(o);
    });
  }
}

async function connect() {
  const port = document.getElementById('portSel').value;
  const baud = parseInt(document.getElementById('baudIn').value) || 115200;
  if (!port) { appendLog('err','Select a port first'); return; }
  const r = await fetch('/connect', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({port, baud})
  });
  const j = await r.json();
  if (!j.ok) appendLog('err', 'Connect failed: ' + j.error);
}

async function disconnect() {
  await fetch('/disconnect', {method:'POST'});
}

// ── Log ──────────────────────────────────────────────────────────────────────
let _lastLogLen = 0;

function appendLog(dir, text) {
  const el = document.getElementById('log');
  const div = document.createElement('div');
  div.className = dir;
  div.textContent = text;
  el.appendChild(div);
  el.scrollTop = el.scrollHeight;
}

function clearLog() {
  document.getElementById('log').innerHTML = '';
  _lastLogLen = 0;
}

async function pollStatus() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    // Connection dot
    const dot = document.getElementById('dot');
    dot.classList.toggle('on', j.connected);
    document.getElementById('connBtn').textContent = j.connected ? 'Reconnect' : 'Connect';
    // Incremental log update
    if (j.log.length > _lastLogLen) {
      const newEntries = j.log.slice(_lastLogLen);
      newEntries.forEach(e => appendLog(e.dir, '[' + e.ts + '] ' + e.text));
      _lastLogLen = j.log.length;
    }
  } catch(_) {}
}

// ── Keyboard shortcuts ───────────────────────────────────────────────────────
document.addEventListener('keydown', e => {
  if (document.activeElement.tagName === 'INPUT') return;
  switch(e.key) {
    case 'ArrowUp':    case 'w': drive('F'); break;
    case 'ArrowDown':  case 's': drive('B'); break;
    case 'ArrowLeft':  case 'a': drive('L'); break;
    case 'ArrowRight': case 'd': drive('R'); break;
    case ' ': send('x'); break;
    case 'o': send('Go'); break;
    case 'c': send('Gc'); break;
    case 'j': startDance(); break;
  }
});

// ── Boot ─────────────────────────────────────────────────────────────────────
refreshPorts();
setInterval(pollStatus, 500);
</script>
</body>
</html>"""


# ── Entry point ───────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Brush-n-Snack web controller")
    parser.add_argument("--port",  default=None,           help="Serial port (e.g. /dev/rfcomm0 or COM5)")
    parser.add_argument("--baud",  type=int, default=DEFAULT_BAUD, help="Baud rate (default 115200)")
    parser.add_argument("--host",  default="0.0.0.0",      help="Web server host (default 0.0.0.0)")
    parser.add_argument("--web-port", type=int, default=5000, help="Web server port (default 5000)")
    args = parser.parse_args()

    # Start background reader
    t = threading.Thread(target=_reader_thread, daemon=True)
    t.start()

    # Auto-open serial port if provided
    if args.port:
        try:
            open_port(args.port, args.baud)
        except Exception as exc:
            print(f"[warn] Could not open {args.port}: {exc}")
            print("[info] You can connect manually in the web UI.")

    print(f"[web] Brush-n-Snack controller running at http://{args.host}:{args.web_port}")
    print("[web] Open http://localhost:5000 in your browser")
    app.run(host=args.host, port=args.web_port, debug=False, use_reloader=False)


if __name__ == "__main__":
    main()
