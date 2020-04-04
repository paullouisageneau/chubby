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
import logging

from .message import Message


logger = logging.getLogger('websockets')
logger.setLevel(logging.INFO)
logger.addHandler(logging.StreamHandler(sys.stdout))

clients = {}


def one_line(s):
    return s.replace('\r', '\\r').replace('\n', '\\n')


async def handle_websocket(ws, path):
    client_id = None
    try:
        splitted = path.split('/')
        splitted.pop(0)
        client_id = splitted.pop(0)
        print('Client {} connected'.format(client_id))

        clients[client_id] = ws
        while True:
            data = await ws.recv()
            print('Client {} >> {}'.format(client_id, one_line(data)))
            message = Message.parse(data)
            dest_id = message.id
            dest_ws = clients.get(dest_id)
            if dest_ws is not None:
                message.id = client_id
                data = str(message)
                print('Client {} << {}'.format(dest_id, one_line(data)))
                await dest_ws.send(data)
            else:
                error = Message(dest_id, "error", ["not_found"])
                data = str(error)
                print('Client {} << {}'.format(client_id, one_line(data)))
                await ws.send(str(error))

    except Exception as e:
        print(e)

    finally:
        if client_id:
            del clients[client_id]
            print('Client {} disconnected'.format(client_id))
