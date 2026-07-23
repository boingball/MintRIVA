#!/usr/bin/env python3
"""Local Range/redirect server for MintRIVA HTTP source regression tests."""

import argparse
import http.server
import pathlib
import ssl
import urllib.parse


class FixtureHandler(http.server.BaseHTTPRequestHandler):
    server_version = "MintRIVATestHTTP/1.0"
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        if self.server.verbose:
            super().log_message(fmt, *args)

    def _route(self):
        path = urllib.parse.urlsplit(self.path).path
        chunked = False
        head_length = False
        streaming = False
        if path.startswith("/chunked-head/"):
            chunked = True
            head_length = True
            path = path[len("/chunked-head"):]
        elif path.startswith("/chunked/"):
            chunked = True
            path = path[len("/chunked"):]
        elif path.startswith("/stream/"):
            # Length-less forward-only stream: chunked, no Content-Length, and
            # Range is ignored (always 200). Models a CDN serving live-style TS.
            chunked = True
            streaming = True
            path = path[len("/stream"):]
        return path, chunked, head_length, streaming

    def _candidate(self, path):
        if not path.startswith("/media/"):
            return None
        relative = urllib.parse.unquote(path[len("/media/"):])
        candidate = (self.server.root / relative).resolve()
        try:
            candidate.relative_to(self.server.root)
        except ValueError:
            self.send_error(403)
            return None
        if not candidate.is_file():
            self.send_error(404)
            return None
        return candidate

    def do_HEAD(self):
        path, _chunked, _head_length, streaming = self._route()
        if path.startswith("/redirect/"):
            self.send_response(302)
            self.send_header("Location", "/media/" + path[len("/redirect/"):])
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        candidate = self._candidate(path)
        if candidate is None:
            if not self.wfile.closed and not path.startswith("/media/"):
                self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        if streaming:
            # No Content-Length and no Accept-Ranges: HEAD reveals no length,
            # so the client must fall back to forward-only streaming.
            self.send_header("Transfer-Encoding", "chunked")
        else:
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Length", str(candidate.stat().st_size))
        self.send_header("Connection", "close")
        self.end_headers()

    def do_GET(self):
        path, chunked, head_length, streaming = self._route()
        if path.startswith("/redirect/"):
            location = "/media/" + path[len("/redirect/"):]
            if head_length:
                location = "/chunked-head" + location
            elif streaming:
                location = "/stream" + location
            elif chunked:
                location = "/chunked" + location
            self.send_response(302)
            self.send_header("Location", location)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        candidate = self._candidate(path)
        if candidate is None:
            if not path.startswith("/media/"):
                self.send_error(404)
            return

        size = candidate.stat().st_size
        start = 0
        status = 200
        range_header = self.headers.get("Range")
        # A streaming endpoint ignores Range entirely and always sends the whole
        # body with 200 + chunked, so the client can never learn a length.
        ignore_zero_open_range = streaming or (head_length and
                                               range_header == "bytes=0-")
        if range_header and not ignore_zero_open_range:
            if not range_header.startswith("bytes=") or "," in range_header:
                self.send_error(416)
                return
            value = range_header[6:].split("-", 1)[0]
            try:
                start = int(value)
            except ValueError:
                self.send_error(416)
                return
            if start < 0 or start >= size:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            status = 206
            if start > 0 and self.server.range_marker:
                self.server.range_marker.touch()

        length = size - start
        self.send_response(status)
        self.send_header("Content-Type", "application/octet-stream")
        if not streaming:
            self.send_header("Accept-Ranges", "bytes")
        if chunked:
            self.send_header("Transfer-Encoding", "chunked")
        else:
            self.send_header("Content-Length", str(length))
        if status == 206:
            self.send_header("Content-Range",
                             f"bytes {start}-{size - 1}/{size}")
        self.send_header("Connection", "close")
        self.end_headers()

        with candidate.open("rb") as source:
            source.seek(start)
            remaining = length
            while remaining:
                block = source.read(min(8191 if chunked else 64 * 1024,
                                        remaining))
                if not block:
                    break
                try:
                    if chunked:
                        self.wfile.write(
                            f"{len(block):X};fixture=yes\r\n".encode("ascii"))
                    self.wfile.write(block)
                    if chunked:
                        self.wfile.write(b"\r\n")
                except (BrokenPipeError, ConnectionResetError):
                    break
                remaining -= len(block)
            if chunked and not remaining:
                try:
                    self.wfile.write(b"0\r\nX-Fixture: done\r\n\r\n")
                except (BrokenPipeError, ConnectionResetError):
                    pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--port-file", required=True)
    parser.add_argument("--cert")
    parser.add_argument("--key")
    parser.add_argument("--range-marker")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), FixtureHandler)
    server.root = root
    server.verbose = args.verbose
    server.range_marker = (pathlib.Path(args.range_marker)
                           if args.range_marker else None)
    if args.cert or args.key:
        if not args.cert or not args.key:
            parser.error("--cert and --key must be supplied together")
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(args.cert, args.key)
        server.socket = context.wrap_socket(server.socket, server_side=True)

    pathlib.Path(args.port_file).write_text(str(server.server_port),
                                            encoding="ascii")
    server.serve_forever()


if __name__ == "__main__":
    main()
