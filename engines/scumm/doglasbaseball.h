/*
 * Better organizing stuff by making my own class for management
 */

#ifndef SCUMM_DOGLASBAEBALL_H
#define SCUMM_DOGLASBASEBALL_H

#include "gui/debugger.h"
#include "common/file.h"
#include "scumm.h"

namespace Scumm {

class ScummEngine; // had to declare DoglasBaseball a friend class to get access to private/protected engine methods

class DoglasBaseball {

public:
	DoglasBaseball(ScummEngine *s);

	void dumpRoomPalette(int room);
	void dumpResource(ResType resType, int idx);
	void loadAndLock(ResType resType, int id, GUI::Debugger *debugger);
	void dumpActorCostume(int actor);
	void exploreActorCostumeAnimation(int actor, GUI::Debugger *debugger);
	void playerNamesScript(GUI::Debugger *debugger);

private:
	ScummEngine *_vm;

protected:
	void writeBitmapHeader(Common::DumpFile *file, int width, int height, const byte *palette);
	void writePixelData(Common::DumpFile *file, int width, int height, byte *pixelData);
	void override_player_names_to_player_numbers(byte *script);
	byte *nuke_array(byte *script, byte varNumber);
	byte *push_local_var_on_stack(byte *script, byte localVar);
	byte *push_const_byte_on_stack(byte *script, byte b);
	byte *push_word_on_stack(byte *script, byte b1, byte b2);
	byte *add_string_to_buffer(byte *script, const char *str);
	byte *formatted_string_to_var(byte *script, byte var);
	byte *array_ops(byte *script, byte op, byte var);
	void stop(byte *script);

};

} // End of namespace Scumm

#endif
