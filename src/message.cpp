/**
 * Copyright (c) 2020 by Paul-Louis Ageneau
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "message.hpp"

#include <sstream>

namespace chubby {

Message Message::Parse(const string &str) {
	Message msg;

	const size_t eol = str.find('\n');
	std::istringstream header(str.substr(0, eol));
	header >> msg.id;
	header >> msg.type;

	string param;
	while (header >> param)
		msg.params.emplace_back(std::move(param));

	msg.body = eol != string::npos ? str.substr(eol + 1) : "";

	return msg;
}

Message::operator string() const {
	std::ostringstream header;
	header << id << ' ' << type;
	for (const string &p : params)
		header << ' ' << p;

	return header.str() + '\n' + body;
}

} // namespace chubby

