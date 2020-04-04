#
# Copyright (c) 2020 Paul-Louis Ageneau
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys
import ssl
import asyncio
import websockets

from . import server


def main():
    try:
        port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
        ssl_cert = sys.argv[2] if len(sys.argv) > 2 else None

        if ssl_cert:
            ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ssl_context.load_cert_chain(ssl_cert)
        else:
            ssl_context = None

        print("Listening on port {}".format(port))
        host = "127.0.0.1"
        start_server = websockets.serve(server.handle_websocket, host, port, ssl=ssl_context)
        asyncio.get_event_loop().run_until_complete(start_server)
        asyncio.get_event_loop().run_forever()
    except KeyboardInterrupt:
        return 0

    return 1


if __name__ == "__main__":
    sys.exit(main())
