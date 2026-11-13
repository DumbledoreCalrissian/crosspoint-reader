#!/usr/bin/env python3
"""Real local HTTP against production HttpDownloader and the pinned SDK fetch contract."""
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
PROGRAM = ROOT.parent / 'qa-artifacts/sudoku-download/transfer-test'
DATA = bytes(range(256)) * 512
REQUESTS = []

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        mode = self.path.strip('/')
        REQUESTS.append((mode, self.headers.get('Range')))
        if mode == 'redirect':
            self.send_response(302)
            self.send_header('Location', '/success')
            self.end_headers()
            return
        if mode == 'stallcancel':
            time.sleep(1)
        offset = int(self.headers.get('Range', 'bytes=0-')[6:-1])
        if mode == 'ignore':
            offset = 0
        self.send_response(206 if offset else 200)
        self.send_header('Content-Length', str(len(DATA) - offset))
        if offset:
            first = offset + 1 if mode == 'invalid' else offset
            self.send_header('Content-Range', f'bytes {first}-{len(DATA)-1}/{len(DATA)}')
        self.end_headers()
        body = bytearray(DATA[offset:])
        if mode == 'wrong':
            body[100] ^= 1
        if mode == 'interrupt' or (mode == 'cut' and offset == 0):
            body = body[:2048]
        try:
            for start in range(0, len(body), 2048):
                self.wfile.write(body[start:start+2048])
                self.wfile.flush()
                if mode == 'cancel':
                    time.sleep(.02)
            if mode == 'interrupt' or (mode == 'cut' and offset == 0):
                self.connection.shutdown(socket.SHUT_RDWR)
                self.connection.close()
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass


def main():
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory) / 'data'
            fixture.write_bytes(DATA)
            for mode in ('success', 'redirect', 'resume', 'ignore', 'invalid', 'cut', 'interrupt', 'cancel',
                         'stallcancel', 'wrong', 'oversize', 'full', 'readonly', 'close', 'complete', 'swapfail', 'lowmemory'):
                endpoint = mode if mode not in ('resume', 'full', 'readonly', 'close', 'complete', 'swapfail', 'oversize') else 'success'
                url = f'http://127.0.0.1:{server.server_port}/{endpoint}'
                REQUESTS.clear()
                result = subprocess.run([str(PROGRAM), mode, url, str(fixture), hashlib.sha256(DATA).hexdigest()],
                                        cwd=ROOT, capture_output=True, text=True, timeout=30)
                print(result.stdout + result.stderr, end='', flush=True)
                assert result.returncode == 0, mode
                if mode == 'resume':
                    assert REQUESTS[0][1] == 'bytes=8192-'
                if mode == 'cut':
                    assert len(REQUESTS) > 1 and REQUESTS[1][1] == 'bytes=2048-'
                if mode in ('complete', 'oversize'):
                    assert not REQUESTS
            print('Local HTTP and failure-injected storage checks PASS', flush=True)
    finally:
        server.shutdown()
        server.server_close()

if __name__ == '__main__':
    main()
