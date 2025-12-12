#include "scumm/doglasbaseball.h"
#include <common/file.h>
#include "scumm/scumm.h"
#include "scumm/actor.h"
#include "gui/debugger.h"
#include "scumm/resource.h"

namespace Scumm {

struct AkhdStruct {
	uint16 versionNumber;
	uint16 costumeFlags;
	uint16 choreCount;
	uint16 celsCount;
	uint16 celCompressionCodec;
	uint16 layerCount;
};

struct AkciStruct {
	uint16 width;
	uint16 height;
	int16 rel_x;
	int16 rel_y;
	int16 move_x;
	int16 move_y;
};

struct AkofStruct {
	uint32 akcd; // offset into the akcd data
	uint16 akci; // offset into the akci data
};

static const Common::String DIR_PATH = "H:\\Programming\\Source\\scummvm\\doglas\\";

DoglasBaseball::DoglasBaseball(ScummEngine *s) {
	_vm = s;
}

void DoglasBaseball::dumpResource(ResType resType, int idx) {
	byte *ptr = _vm->getResourceAddress(resType, idx);
	// type is first uint32 and then size is next uint32
	uint size = READ_BE_UINT32(ptr + 4);
	Common::DumpFile dumpFile;
	const Common::String fileName = "tmpDumpFile.txt";
	dumpFile.open(DIR_PATH + fileName);
	dumpFile.write(ptr, size);
}

void DoglasBaseball::loadAndLock(ResType resType, int id, GUI::Debugger *debugger) {
	// Lock and Load would have been cooler to say but order matters unfortunately!
	if (!SearchMan.hasArchive("doglasCostumes")) {
		Common::FSDirectory* costumeArchive = new Common::FSDirectory(DIR_PATH + "costumes", 1, true);
		SearchMan.add("doglasCostumes", costumeArchive);
	}
	Common::File file;
	if (resType == rtCostume) {
		// Only supporting costume for now
		Common::String path = Common::String::format("%d.AKOS", id); // relative path bc flat archive is defined
		Common::Archive *cache = SearchMan.getArchive("doglasCostumes");
		bool opened = file.open(path, *cache);
		if (!opened) {
			debugger->debugPrintf("Restype %d costume id %d could not be loaded from path %s\n", resType, id, path.c_str());
			return;
		}
	} else {
		debugger->debugPrintf("Restype %d not yet supported\n", resType);
	}

	file.seek(4, SEEK_SET);            // tag unused but first 4 bytes (ex. AKOS)
	uint32 size = file.readUint32BE(); // Another 4 bytes ex 00 00 4D 5E = 19806 bytes
	file.seek(0, SEEK_SET);            // point back to start of file
	file.read(_vm->_res->createResource(resType, id, size), size); // create resource will nuke resource if already exists!
	_vm->_res->lock(resType, id);
}

void DoglasBaseball::dumpRoomPalette(int room) {
	Common::DumpFile paletteDumpFile;
	const Common::String fileName = "roomPalette.bmp";
	paletteDumpFile.open(DIR_PATH + Common::String::format("%d", room) + fileName);

	const byte *roomPalette = _vm->getPalettePtr(0, room);

	writeBitmapHeader(&paletteDumpFile, 256, 1, roomPalette);

	// Just try dumping in a single line for now
	for (int i = 0; i < 256; i++) {
		paletteDumpFile.writeByte(i);
	}

	paletteDumpFile.finalize();
	paletteDumpFile.close();
}

void DoglasBaseball::writeBitmapHeader(Common::DumpFile *file, int width, int height, const byte* palette) {
	// Making a bmp version 3: https://www.fileformat.info/format/bmp/egff.htm

	// HEADER is 14 bytes!
	file->writeString("BM"); // bfType must always be BM for bitmapfile - 2
	file->writeUint32LE(0);      // file size but for uncompressed bitmap sounds like this can be 0 - 4
	file->writeUint16LE(0);  // reserved 1 - 8
	file->writeUint16LE(0);  // reserved 2 - 10
	file->writeUint32LE(14 + 40 + (256 * 4)); // offset to dataStart - 14

	// Now info header is 40 bytes
	file->writeUint32LE(40);    // info header size - 4
	file->writeSint32LE(width); // width - 8
	file->writeSint32LE(height); // height - 12
	file->writeUint16LE(1);      // color planes always 1 - 14
	file->writeUint16LE(8);      // bits per pixel todo is this value right for rgb or different for using colortable? - 16
	file->writeUint32LE(0);      // 0 for uncompressed - 20
	file->writeUint32LE(0);      // size for bitmap but can be 0 when uncompressed - 24
	file->writeSint32LE(0);      // horz resolution used for printing - 28
	file->writeSint32LE(0);      // vert resolution - 32
	file->writeUint32LE(256);    // colors used in palette. I think if this was 0 it would default to 2^8 - 36
	file->writeUint32LE(0);      // important colors but 0 says all important - 40

	// Now color table
	for (int i = 0; i < 256; i++) {
		// b,g,r,reserved
		file->writeByte(palette[3 * i + 2]); // b
		file->writeByte(palette[3 * i + 1]); // g
		file->writeByte(palette[3 * i]);     // r
		file->writeByte(0);                  // reserved / padding
	}
}

void DoglasBaseball::writePixelData(Common::DumpFile* file, int width, int height, byte* pixelData) {
	// write bottom up and pad to 4 byte boundary on line
	for (int j = height - 1; j >= 0; j--) {
		for (int i = 0; i < width; i++) {
			file->writeByte(pixelData[width * j + i]);
		}
		int paddingNeeded = width % 4;
		if (paddingNeeded) {
			for (int p = 0; p < 4 - paddingNeeded; p++) {
				file->writeByte(0);
			}
		}
	}
	for (int i = 0; i < width * height; i++) {
		file->writeByte(pixelData[i]);
	}
}

void DoglasBaseball::dumpActorCostume(int actor) {
	Actor *a;
	byte *akos;
	a = _vm->_actors[actor];
	CostumeData costumeData = a->_cost;
	akos = _vm->getResourceAddress(rtCostume, a->_costume);
	const AkhdStruct *_akhd = (const AkhdStruct *)_vm->findResourceData(MKTAG('A', 'K', 'H', 'D'), akos);
	const byte *_akof = _vm->findResourceData(MKTAG('A', 'K', 'O', 'F'), akos); // defined
	const byte *_akci = _vm->findResourceData(MKTAG('A', 'K', 'C', 'I'), akos); // defined
	const byte *_aksq = _vm->findResourceData(MKTAG('A', 'K', 'S', 'Q'), akos); // defined
	const byte *_akcd = _vm->findResourceData(MKTAG('A', 'K', 'C', 'D'), akos); // defined
	const byte *_akpl = _vm->findResourceData(MKTAG('A', 'K', 'P', 'L'), akos); // defined
	const byte *_akct = _vm->findResourceData(MKTAG('A', 'K', 'C', 'T'), akos); // not defined
	const byte *_rgbs = _vm->findResourceData(MKTAG('R', 'G', 'B', 'S'), akos); // not defined

	const byte *roomPalette = _vm->getPalettePtr(0, a->_room); // get palette from room that actor is in
	int numColors = _vm->getResourceDataSize(_akpl);

	// 16 colors has shift 4 and mask 0xF
	byte shift = 4;
	byte mask = 0xF;

	if (numColors == 32) {
		mask = 0x7;
		shift = 3;
	} else if (numColors == 64) {
		mask = 0x3;
		shift = 2;
	}

	int maxSize = 0;
	// To save malloc too much just find max size
	for (int j = 0; j < _akhd->celsCount; j++) {
		const AkofStruct *_akof_frame = (AkofStruct *)(_akof + (j * 6)); // 6 is int32 and int16 with no padding
		const AkciStruct *akciData = (AkciStruct *)(_akci + _akof_frame->akci);

		if (akciData->width * akciData->height > maxSize) {
			maxSize = akciData->width * akciData->height;
		}
	}
	byte *pixelData;
	pixelData = (byte *)malloc(maxSize); // just treat like a 1d array

	for (int j = 0; j < _akhd->celsCount; j++) {
		const AkofStruct *_akof_frame = (AkofStruct *)(_akof + (j * 6)); // 6 is int32 and int16 with no padding

		const AkciStruct *akciData = (AkciStruct *)(_akci + _akof_frame->akci);
		const byte *akcdData = _akcd + _akof_frame->akcd;

		// Alrighty we now have the raw encoded data. We know the width and height of the frame from the akci struct. Lets try to see if we can recreate image
		int lastOffset = j == 0 ? 0 : ((AkofStruct *)(_akof + ((j - 1) * 6)))->akcd;
		// int dataLen = _akof_frame->akcd - lastOffset;
		int x = 0;
		int y = 0;
		bool allPixels = false;
		int b_pos = 0;
		while (!allPixels) {
			byte cur = akcdData[b_pos++];
			byte color = cur >> shift;
			byte rep = cur & mask;
			if (!rep) {                  // if rep count is 0 the count is stored as the full next byte
				rep = akcdData[b_pos++]; // full next byte is rep count!
			}
			while (rep > 0) {
				byte room_color = *(_akpl + color); // color is local to actorPalette. Need to apply to get to roomPalette.
				if (pixelData) {
					pixelData[akciData->width * y + x] = room_color;
				}
				rep--;
				y++;
				if (y >= akciData->height) {
					y = 0;
					x++;
					if (x >= akciData->width) {
						allPixels = true;
						break;
					}
				}
			}
		}
		// now all pixel data should be recorded, lets start making the file
		Common::DumpFile cosFrameFile;
		const Common::String fileName = ".bmp";
		Common::String filePath = DIR_PATH + Common::String::format("room%dcost%d\\", a->_room, a->_costume) + Common::String::format("room%d-cost%d-frame%d", a->_room, a->_costume, j) + fileName;
		cosFrameFile.open(filePath, true); // true for create path
		writeBitmapHeader(&cosFrameFile, akciData->width, akciData->height, roomPalette);
		writePixelData(&cosFrameFile, akciData->width, akciData->height, pixelData);
		cosFrameFile.finalize();
		cosFrameFile.close();
	}

	free(pixelData); // Free the malloc!
}

void DoglasBaseball::exploreActorCostumeAnimation(int actor, GUI::Debugger *debugger) {
	Actor *a;
	byte *akos;
	a = _vm->_actors[actor];
	CostumeData costumeData = a->_cost;
	akos = _vm->getResourceAddress(rtCostume, a->_costume);
	const AkhdStruct *_akhd = (const AkhdStruct *)_vm->findResourceData(MKTAG('A', 'K', 'H', 'D'), akos);
	const byte *_akch = _vm->findResourceData(MKTAG('A', 'K', 'C', 'H'), akos);

	debugger->debugPrintf("Actor %d costume %d\n", actor, a->_costume);
	debugger->debugPrintf("Chore Count: %d\n", _akhd->choreCount);
	debugger->debugPrintf("Num frames: %d\n", _akhd->celsCount);

	/*
	for (int i = 0; i < 16; i++) {
		debugger->debugPrintf("limb: %d, animType %d, curPos %d, start %d, end %d, frame %d\n", i, costumeData.animType[i], costumeData.curpos[i], costumeData.start[i], costumeData.end[i], costumeData.frame[i]);
	}
	*/

	for (int i = 0; i < _akhd->choreCount; i++) {
		uint offs = READ_LE_UINT16(_akch + i * sizeof(uint16));
		const byte *ptr = _akch + offs;
		uint16 mask = READ_LE_UINT16(ptr);
		ptr += 2;
		int limb = 0;
		do {
			byte code = *ptr++;
			if (code != 0) {

			}
			uint16 start = READ_LE_UINT16(ptr);
			ptr += 2;
			uint16 len = READ_LE_UINT16(ptr);
			ptr += 2;
			debugger->debugPrintf("Chore %d: Offset %d, Limb %d, Start %d, Len %d\n", i, offs, limb, start, len);
			mask <<= 1; // shift to next limb
			limb++;
		} while (mask);
	}

	//debugger->debugPrintf("Animation definitions\n");
	/*
	for (int i = 0; i < _akhd->choreCount; i++) {


		uint16 offset = (uint16)(_akch[i * 2]);

		uint16 limbMask = (uint16)_akch[offset];
		byte mode = _akch[offset + 2];
		if (mode != 0) {
			debugger->debugPrintf("Animation %d\n", i);
			debugger->debugPrintf("Offset %d, Mask %d, mode %d\n", offset, limbMask, mode);
			if (mode != 1 && mode != 4 && mode != 5) {
				uint16 start = _akch[offset + 3];
				uint16 len = _akch[offset + 5];
				debugger->debugPrintf("Start %d, Length %d\n", start, len);
			}
		}
	}
	*/

}

void DoglasBaseball::playerNamesScript(GUI::Debugger *debugger) {
	byte* ptr = _vm->getResourceAddress(rtScript, 131); // 131 is player names script
	const int totalSize = READ_BE_UINT32(ptr + 4);
	byte *script = ptr + 8;

	for (int i = 0; i < totalSize; i++) {
		byte b = script[i];
		debugger->debugPrintf("%d,", b);
	}

	// now let's overwrite the script
	byte *modifyScriptPtr = script;
	override_player_names_to_player_numbers(modifyScriptPtr);

	debugger->debugPrintf("\n\n,");

	_vm->_res->lock(rtScript, 131); // Lock the script in memory so doesn't change

	for (int i = 0; i < totalSize; i++) {
		byte b = script[i];
		debugger->debugPrintf("%d,", b);
	}
}

void DoglasBaseball::override_player_names_to_player_numbers(byte* script) {
	script = nuke_array(script, 200); // Clear var 200
	script = push_word_on_stack(script, 255, 255); // This is -1. 255 255 is 11111111 which as signed int invert and add 1 (2s complement for neg)
	script = push_local_var_on_stack(script, 0); // put local var 0 on stack (this is playerIdNum)
	script = push_const_byte_on_stack(script, 0); // this is number of args for string formatting minus 1 (it assumes at least 1)
	script = add_string_to_buffer(script, "Player %d"); // this string is part of runtime binary so doesn't need to be free'd btw
	script = formatted_string_to_var(script, 200); // write formatted string to var 200
	stop(script);
}

byte *DoglasBaseball::nuke_array(byte *script, byte varNumber) {
	script[0] = 0xBC; // o72_dimArray
	script[1] = 0xCC; // SO_UNDIM_ARRAY
	script[2] = varNumber; // LE first byte (limiting to 256 vars currently)
	script[3] = 0; // LE last byte
	return script + 4;
}

byte *DoglasBaseball::push_local_var_on_stack(byte *script, byte localVar) {
	script[0] = 0x03; // 06_pushWordVar
	script[1] = localVar; // LE first byte
	script[2] = 0x40; // This is a flag for local var
	return script + 3;
}

byte *DoglasBaseball::push_const_byte_on_stack(byte *script, byte b) {
	script[0] = 0x0; // o6_pushByte
	script[1] = b;
	return script + 2;
}

byte *DoglasBaseball::push_word_on_stack(byte *script, byte b1, byte b2) {
	script[0] = 0x1; // o6_pushWord
	script[1] = b1;
	script[2] = b2;
	return script + 3;
}

byte *DoglasBaseball::add_string_to_buffer(byte *script, const char *str) {
	script[0] = 0x04; // o72_getScriptString
	int i = 0;
	while (str[i] != 0) { // yolo expect string to be terminated
		script[1 + i] = str[i];
		i++;
	}
	script[1 + i] = 0;
	return script + 2 + i;
}

byte *DoglasBaseball::formatted_string_to_var(byte *script, byte var) {
	return array_ops(script, 194, var); // 194 is SO_FORMATTED_STRING
}

byte *DoglasBaseball::array_ops(byte *script, byte op, byte var) {
	script[0] = 0xA4; // o72_arrayOps
	script[1] = op; // SO_FORMATTED_STRING
	script[2] = var; // LE first byte (limiting to 256 vars currently)
	script[3] = 0; // LE last byte
	return script += 4;
}

void DoglasBaseball::stop(byte *script) {
	script[0] = 0x66; // o6_stopObjectCode
}



} // end namespace
