/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef PINK_CURSOR_MGR_H
#define PINK_CURSOR_MGR_H

#include "common/rect.h"

#include "pink/objects/object.h"

namespace Pink {

class CursorActor;
class Page;
class PinkEngine;

class CursorMgr : public Object {
public:
	CursorMgr(PinkEngine *game, Page *page);

	void update();
	void setCursor(uint index, const Common::Point point, const Common::String &itemName);
	void setCursor(const Common::String &cursorName, const Common::Point point);

	void setPage(Page *page) { _page = page; }

private:
	void startAnimation(uint index);

	void showItem(const Common::String &itemName, const Common::Point point);
	void hideItem();

private:
	Page *_page;
	PinkEngine *_game;
	CursorActor *_actor;
	uint _time;
	uint _firstFrameIndex;
	bool _isPlayingAnimation;
	bool _isSecondFrame;
};

} // End of namespace Pink

#endif
