import http.server
import socketserver
import os
import sys
import threading
import time
import webbrowser

PORT = 8080

# Detect web directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if os.path.exists(os.path.join(SCRIPT_DIR, "index.html")):
    WEB_DIR = SCRIPT_DIR
elif os.path.exists(os.path.join(SCRIPT_DIR, "web", "index.html")):
    WEB_DIR = os.path.join(SCRIPT_DIR, "web")
else:
    WEB_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "web"))

class CustomHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def end_headers(self):
        # Enable CORS and COOP/COEP headers for Web Serial and ES modules
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS, HEAD')
        self.send_header('Access-Control-Allow-Headers', 'X-Requested-With, Content-Type, Accept, Range')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'credentialless')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200, "ok")
        self.end_headers()

def open_browser(port):
    time.sleep(1.0)
    url = f"http://localhost:{port}/index.html"
    print(f"\n[OK] Dang mo trinh duyet: {url}")
    webbrowser.open(url)

def main():
    global PORT
    print("=" * 65)
    print("   ROBOT MIU EILIK - LOCAL WEB FLASHER & SERIAL DIAGNOSTIC")
    print("=" * 65)
    print(f"[INFO] Thu muc Web: {WEB_DIR}")

    # Check for index.html
    index_file = os.path.join(WEB_DIR, "index.html")
    if not os.path.exists(index_file):
        print(f"[LOI] Khong tim thay {index_file}!")
        input("\nNhan Enter de thoat...")
        return

    socketserver.TCPServer.allow_reuse_address = True
    server = None
    for p in [8080, 8088, 8888, 9000]:
        try:
            server = socketserver.TCPServer(("", p), CustomHandler)
            PORT = p
            break
        except OSError:
            print(f"[WARN] Cong {p} dang ban, thu cong tiep theo...")

    if not server:
        print("[LOI] Khong the mo bat ky cong mang nao!")
        input("\nNhan Enter de thoat...")
        return

    print(f"[OK] May chu Web dang chay tai: http://localhost:{PORT}/index.html")
    print("[!] GIU CUA SO NAY MO de duy tri ket noi Web Serial va nap Firmware.")
    print("=" * 65)

    threading.Thread(target=open_browser, args=(PORT,), daemon=True).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[OK] Da dung may chu Web.")
    finally:
        server.server_close()

if __name__ == "__main__":
    main()
