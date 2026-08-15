/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"

#if defined(POSIX)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>
#include <cstring>

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "gui/debugger.h"

#include "engines/scumm/remote_console.h"

namespace Scumm {

namespace {

const uint32 kReadBufferSize = 1024;
const int kDefaultPort = 44567;

// Buffer that collects debugger output while a remote command is running.
// A single RemoteConsole instance exists per game, running on the engine
// thread, so a file-scope buffer is safe here.
Common::String g_captureBuffer;

void remoteLogCallback(LogMessageType::Type type, int level, uint32 debugChannel, const char *message) {
	if (!message)
		return;

	g_captureBuffer += message;
	if (!g_captureBuffer.hasSuffix("\n"))
		g_captureBuffer += '\n';
}

} // End of anonymous namespace

RemoteConsole::RemoteConsole(GUI::Debugger *debugger) : _debugger(debugger), _listenSocket(-1), _clientSocket(-1) {
}

RemoteConsole::~RemoteConsole() {
	stop();
}

bool RemoteConsole::start() {
	if (_listenSocket >= 0)
		return true;

	int port = kDefaultPort;
	if (ConfMan.hasKey("remote_debug_port"))
		port = ConfMan.getInt("remote_debug_port");

	if (port <= 0 || port > 65535) {
		debug("RemoteConsole: disabled (remote_debug_port=%d)", port);
		return false;
	}

	_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenSocket < 0) {
		warning("RemoteConsole: unable to create socket");
		return false;
	}

	int yes = 1;
	setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((uint16)port);

	if (bind(_listenSocket, (sockaddr *)&addr, sizeof(addr)) < 0) {
		warning("RemoteConsole: unable to bind to port %d", port);
		close(_listenSocket);
		_listenSocket = -1;
		return false;
	}

	if (listen(_listenSocket, 1) < 0) {
		warning("RemoteConsole: unable to listen on port %d", port);
		close(_listenSocket);
		_listenSocket = -1;
		return false;
	}

	fcntl(_listenSocket, F_SETFL, O_NONBLOCK);

	debug("RemoteConsole: listening on port %d", port);
	return true;
}

void RemoteConsole::poll() {
	if (_listenSocket < 0)
		return;

	if (_clientSocket < 0) {
		// Try to accept a new client.
		sockaddr_in clientAddr;
		socklen_t addrLen = sizeof(clientAddr);
		int client = accept(_listenSocket, (sockaddr *)&clientAddr, &addrLen);
		if (client >= 0) {
			fcntl(client, F_SETFL, O_NONBLOCK);
			_clientSocket = client;
			_lineBuffer.clear();
			_outputBuffer.clear();
			debug("RemoteConsole: client connected from %s", inet_ntoa(clientAddr.sin_addr));
		}
		// accept() failed (EAGAIN): no client waiting, nothing to do.
		return;
	}

	handleClient();
}

void RemoteConsole::handleClient() {
	char buf[kReadBufferSize];

	for (;;) {
		ssize_t n = recv(_clientSocket, buf, kReadBufferSize - 1, 0);
		if (n > 0) {
			buf[n] = '\0';
			_lineBuffer += buf;
		} else if (n == 0) {
			// EOF: client closed the connection.
			debug("RemoteConsole: client disconnected");
			closeClient();
			return;
		} else {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break; // No more data for now.
			debug("RemoteConsole: read error, dropping client");
			closeClient();
			return;
		}
	}

	// Execute any complete command lines that have arrived.
	for (;;) {
		uint32 newlinePos = _lineBuffer.find('\n');
		if (newlinePos == Common::String::npos)
			break;

		Common::String line = _lineBuffer.substr(0, newlinePos);
		_lineBuffer = _lineBuffer.substr(newlinePos + 1);

		line.trim();
		if (!line.empty() && line.lastChar() == '\r')
			line.deleteLastChar();
		line.trim();

		if (line.empty())
			continue;

		debug("RemoteConsole: executing '%s'", line.c_str());
		if (!executeCommand(line)) {
			// The command asked the console to close (e.g. "quit").
			closeClient();
			return;
		}

		sendOutput();
	}
}

bool RemoteConsole::executeCommand(const Common::String &cmd) {
	g_captureBuffer.clear();

	Common::LogWatcher previousWatcher = Common::getLogWatcher();
	Common::setLogWatcher(remoteLogCallback);

	bool result = false;
	if (_debugger)
		result = _debugger->execCommand(cmd.c_str());

	Common::setLogWatcher(previousWatcher);

	if (!g_captureBuffer.empty())
		_outputBuffer += g_captureBuffer;

	return result;
}

void RemoteConsole::sendOutput() {
	while (!_outputBuffer.empty()) {
		ssize_t n = send(_clientSocket, _outputBuffer.c_str(), _outputBuffer.size(), 0);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return; // Try again on the next poll.
			debug("RemoteConsole: write error, dropping client");
			closeClient();
			return;
		}
		_outputBuffer = _outputBuffer.substr((size_t)n);
	}
}

void RemoteConsole::closeClient() {
	if (_clientSocket >= 0) {
		close(_clientSocket);
		_clientSocket = -1;
	}
	_lineBuffer.clear();
	_outputBuffer.clear();
}

void RemoteConsole::stop() {
	closeClient();
	if (_listenSocket >= 0) {
		close(_listenSocket);
		_listenSocket = -1;
	}
}

} // End of namespace Scumm

#endif // POSIX
