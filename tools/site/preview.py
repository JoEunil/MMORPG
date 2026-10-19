#!/usr/bin/env python3
"""Serve or package the built site at its GitHub Pages project path."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit
from zipfile import ZIP_DEFLATED, ZipFile
import argparse
import mimetypes

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
PREFIX = "/MMORPG/"


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = unquote(urlsplit(self.path).path)
        if path == "/MMORPG":
            self.send_response(301)
            self.send_header("Location", PREFIX)
            self.end_headers()
            return
        if not path.startswith(PREFIX):
            self.send_error(404)
            return
        relative = path[len(PREFIX):] or "index.html"
        target = (SITE / relative).resolve()
        if target.is_dir():
            target = target / "index.html"
        if not target.is_relative_to(SITE.resolve()) or not target.is_file():
            self.send_error(404)
            return
        content = target.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", mimetypes.guess_type(target.name)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, format, *args):
        print(f"preview {self.address_string()} {format % args}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serve", action="store_true", help="serve at http://127.0.0.1:8000/MMORPG/")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--zip", action="store_true", help="write site-preview.zip with the /MMORPG/ site layout")
    args = parser.parse_args()
    if args.zip:
        archive = ROOT / "site-preview.zip"
        with ZipFile(archive, "w", ZIP_DEFLATED) as output:
            for path in SITE.rglob("*"):
                if path.is_file():
                    output.write(path, Path("MMORPG") / path.relative_to(SITE))
        print(f"Created {archive}")
    if args.serve:
        print(f"Preview: http://127.0.0.1:{args.port}/MMORPG/")
        ThreadingHTTPServer(("127.0.0.1", args.port), Handler).serve_forever()
    if not args.serve and not args.zip:
        parser.error("choose --serve or --zip")


if __name__ == "__main__":
    main()
