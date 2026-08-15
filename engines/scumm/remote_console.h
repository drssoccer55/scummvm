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

#ifndef SCUMM_REMOTE_CONSOLE_H
#define SCUMM_REMOTE_CONSOLE_H

#include "common/scummsys.h"
#include "common/str.h"

namespace GUI {
class Debugger;
}

namespace Scumm {

#if defined(POSIX)

/**
 * Remote debug console.
 *
 * Listens on a TCP port for debugger commands (e.g. "actors") and sends
 * the debugger output back over the connection. This allows driving the
 * engine's debugger programmatically from an external tool.
 *
 * Only a single client is served at a time. All operations are
 * non-blocking and polled once per frame from the main loop, so a slow
 * or unresponsive client can never stall the game.
 */
class RemoteConsole {
public:
	RemoteConsole(GUI::Debugger *debugger);
	~RemoteConsole();

	/**
	 * Create and bind the listening socket.
	 * The port is taken from the "remote_debug_port" config option
	 * (default 44567).
	 */
	bool start();

	/**
	 * Poll the listening socket and any connected client. This should
	 * be called once per main loop iteration.
	 */
	void poll();

	/** Close the listening socket and any active client connection. */
	void stop();

private:
	void handleClient();
	bool executeCommand(const Common::String &cmd);
	void sendOutput();
	void closeClient();

	GUI::Debugger *_debugger;

	int _listenSocket;
	int _clientSocket;
	Common::String _lineBuffer;
	Common::String _outputBuffer;
};

#else // !POSIX

class RemoteConsole {
public:
	RemoteConsole(GUI::Debugger *) {}
	bool start() { return false; }
	void poll() {}
	void stop() {}
};

#endif

} // End of namespace Scumm

#endif
