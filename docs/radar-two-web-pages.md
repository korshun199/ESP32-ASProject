# Radar Two Web Pages

Branch: dev/radar-new-task

Pages:
- web/radar_virtual_mics/control.html
  - T16 control panel.
  - Loads motorcycle audio file.
  - Controls virtual sound source X/Y.
  - Controls virtual microphone gains.
  - Sends shared state to server.

- web/radar_virtual_mics/monitor.html
  - Tablet monitor.
  - Reads shared state from server.
  - Displays acoustic point X/Y, frequency, volume and mic levels.

Server:
- web/radar_virtual_mics/server.py
  - Python stdlib HTTP server.
  - GET /api/state
  - POST /api/state
