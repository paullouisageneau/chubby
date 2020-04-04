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

class Message:
    def __init__(self, _id, _type, params = [], body = ""):
        self.id = _id
        self.type = _type
        self.params = params
        self.body = body

    def __str__(self):
        header = " ".join([self.id, self.type] + self.params)
        return header + "\n" + (self.body if self.body else "")

    @staticmethod
    def parse(string):
        lines = string.split("\n")
        header = lines.pop(0)
        body = "\n".join(lines)
        params = header.split(" ")
        id = params.pop(0)
        type = params.pop(0)
        return Message(id, type, params, body)


