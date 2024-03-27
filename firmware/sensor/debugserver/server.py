import http.server
import socketserver

class ReqHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/update?version="):
            # self.path = "patches/" + self.path.removeprefix("/update?version=") + ".patch"

            self.path = "patches/sensor.0.patch"
            return http.server.SimpleHTTPRequestHandler.do_GET(self)

            # self.send_response(200)
            # self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()

handler = ReqHandler
server = socketserver.TCPServer(("", 8080), handler)
server.allow_reuse_address = True
server.serve_forever()
