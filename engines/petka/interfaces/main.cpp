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

#include "common/system.h"
#include "common/stream.h"
#include "common/events.h"
#include "common/ini-file.h"

#include "petka/flc.h"
#include "petka/objects/object_case.h"
#include "petka/objects/object_cursor.h"
#include "petka/objects/object_star.h"
#include "petka/interfaces/main.h"
#include "petka/q_system.h"
#include "petka/q_manager.h"
#include "petka/sound.h"
#include "petka/petka.h"
#include "petka/video.h"
#include "petka/objects/object_case.h"
#include "petka/objects/heroes.h"
#include "petka/objects/text.h"

namespace Petka {

InterfaceMain::InterfaceMain() {
	Common::ScopedPtr<Common::SeekableReadStream> stream(g_vm->openFile("backgrnd.bg", true));
	if (!stream)
		return;
	_bgs.resize(stream->readUint32LE());
	for (uint i = 0; i < _bgs.size(); ++i) {
		_bgs[i].objId = stream->readUint16LE();
		_bgs[i].attachedObjIds.resize(stream->readUint32LE());
		for (uint j = 0; j < _bgs[i].attachedObjIds.size(); ++j) {
			_bgs[i].attachedObjIds[j] = stream->readUint16LE();
			QMessageObject *obj = g_vm->getQSystem()->findObject(_bgs[i].attachedObjIds[j]);
			obj->_x = stream->readSint32LE();
			obj->_y = stream->readSint32LE();
			obj->_z = stream->readSint32LE();
			obj->_walkX = stream->readSint32LE();
			obj->_walkY = stream->readSint32LE();
		}
	}

	_objs.push_back(g_vm->getQSystem()->_cursor.get());
	_objs.push_back(g_vm->getQSystem()->_case.get());
	_objs.push_back(g_vm->getQSystem()->_star.get());
}

void InterfaceMain::start(int id) {
	g_vm->getQSystem()->update();
	g_vm->getQSystem()->_isIniting = 0;

	_objs.push_back(g_vm->getQSystem()->_petka.get());
	_objs.push_back(g_vm->getQSystem()->_chapayev.get());

	Common::ScopedPtr<Common::SeekableReadStream> bgsStream(g_vm->openFile("BGs.ini", false));
	Common::INIFile bgsIni;
	bgsIni.allowNonEnglishCharacters();
	bgsIni.loadFromStream(*bgsStream);
	Common::String startRoom;
	bgsIni.getKey("StartRoom", "Settings", startRoom);
	loadRoom(g_vm->getQSystem()->findObject(startRoom)->_id, false);
}

void InterfaceMain::loadRoom(int id, bool fromSave) {
	QSystem *sys = g_vm->getQSystem();
	sys->_currInterface->stop();
	if (_roomId == id)
		return;
	unloadRoom(fromSave);
	_roomId = id;
	const BGInfo *info = findBGInfo(id);
	QObjectBG *room = (QObjectBG *)sys->findObject(id);
	g_vm->getQSystem()->_room = room;
	g_vm->resMgr()->loadBitmap(room->_resourceId);
	_objs.push_back(room);
	for (uint i = 0; i < info->attachedObjIds.size(); ++i) {
		QMessageObject *obj = g_vm->getQSystem()->findObject(info->attachedObjIds[i]);
		obj->loadSound();
		if (obj->_isShown || obj->_isActive)
			g_vm->resMgr()->loadFlic(obj->_resourceId);
		_objs.push_back(obj);
	}
	if (sys->_musicId != room->_musicId) {
		g_vm->soundMgr()->removeSound(g_vm->resMgr()->findSoundName(sys->_musicId));
		Sound *sound = g_vm->soundMgr()->addSound(g_vm->resMgr()->findSoundName(room->_musicId), Audio::Mixer::kMusicSoundType);
		if (sound) {
			sound->play(true);
		}
		sys->_musicId = room->_musicId;
	}
	if (sys->_fxId != room->_fxId) {
		g_vm->soundMgr()->removeSound(g_vm->resMgr()->findSoundName(sys->_fxId));
		Sound *sound = g_vm->soundMgr()->addSound(g_vm->resMgr()->findSoundName(room->_fxId), Audio::Mixer::kMusicSoundType);
		if (sound) {
			sound->play(true);
		}
		sys->_fxId = room->_fxId;
	}
	if (!fromSave)
		g_vm->getQSystem()->addMessageForAllObjects(kInitBG, 0, 0, 0, 0, room);
	g_vm->videoSystem()->updateTime();
}

const BGInfo *InterfaceMain::findBGInfo(int id) const {
	for (uint i = 0; i < _bgs.size(); ++i) {
		if (_bgs[i].objId == id)
			return &_bgs[i];
	}
	return nullptr;
}

void InterfaceMain::unloadRoom(bool fromSave) {
	if (_roomId == -1)
		return;
	QSystem *sys = g_vm->getQSystem();
	QObjectBG *room = (QObjectBG *) sys->findObject(_roomId);
	if (room) {
		if (!fromSave)
			sys->addMessageForAllObjects(kLeaveBG, 0, 0, 0, 0, room);
		g_vm->resMgr()->clearUnneeded();
		g_vm->soundMgr()->removeSoundsWithType(Audio::Mixer::kSFXSoundType);
		const BGInfo *info = findBGInfo(_roomId);
		if (!info)
			return;
		for (uint i = 0; i < _objs.size();) {
			bool removed = false;
			if (_roomId == ((QMessageObject *) _objs[i])->_id) {
				_objs.remove_at(i);
				removed = true;
			} else {
				for (uint j = 0; j < info->attachedObjIds.size(); ++j) {
					if (info->attachedObjIds[j] == ((QMessageObject *) _objs[i])->_id) {
						QMessageObject *o = (QMessageObject *) _objs.remove_at(i);
						o->removeSound();
						removed = true;
						break;
					}
				}
			}
			if (!removed)
				++i;
		}
	}
}

void InterfaceMain::onLeftButtonDown(const Common::Point p) {
	QObjectCursor *cursor = g_vm->getQSystem()->_cursor.get();
	if (!cursor->_isShown) {
		_dialog.next(-1);
		return;
	}

	for (int i = _objs.size() - 1; i >= 0; --i) {
		if (_objs[i]->isInPoint(p.x, p.y)) {
			_objs[i]->onClick(p.x, p.y);
			return;
		}
	}

	switch (cursor->_actionType) {
	case kActionWalk: {
		QObjectPetka *petka = g_vm->getQSystem()->_petka.get();
		if (petka->_heroReaction) {
			for (uint i = 0; i < petka->_heroReaction->messages.size(); ++i) {
				if (petka->_heroReaction->messages[i].opcode == kGoTo) {
					QObjectChapayev *chapay = g_vm->getQSystem()->_chapayev.get();
					chapay->stopWalk();
					break;
				}
			}
			delete petka->_heroReaction;
			petka->_heroReaction = nullptr;
		}
		petka->walk(p.x, p.y);
		break;
	}
	case kActionObjUseChapayev: {
		QObjectChapayev *chapay = g_vm->getQSystem()->_chapayev.get();
		chapay->walk(p.x, p.y);
		break;
	}
	default:
		break;
	}
}

void InterfaceMain::onRightButtonDown(const Common::Point p) {
	QObjectStar *star = g_vm->getQSystem()->_star.get();
	// QObjectCase *objCase = g_vm->getQSystem()->_case.get();
	QObjectCursor *cursor = g_vm->getQSystem()->_cursor.get();
	if (!star->_isActive)
		return;
	if (g_vm->getQSystem()->_case.get()->_isShown && cursor->_actionType == kActionObjUse) {
		cursor->setAction(kActionTake);
	} else {
		star->setPos(p);
		star->show(star->_isShown == 0);
	}
}

void InterfaceMain::onMouseMove(const Common::Point p) {
	QMessageObject *prevObj = (QMessageObject *)_objUnderCursor;
	_objUnderCursor = nullptr;

	QObjectCursor *cursor = g_vm->getQSystem()->_cursor.get();
	if (cursor->_isShown) {
		for (int i = _objs.size() - 1; i >= 0; --i) {
			if (_objs[i]->isInPoint(p.x, p.y)) {
				_objs[i]->onMouseMove(p.x, p.y);
				break;
			}
		}
	}

	cursor->_animate = _objUnderCursor != nullptr;
	cursor->setCursorPos(p.x, p.y, true);

	if (prevObj != _objUnderCursor && _objUnderCursor && !_dialog.isActive()) {
		Graphics::PixelFormat fmt = g_system->getScreenFormat();
		QMessageObject *obj = (QMessageObject *)_objUnderCursor;
		if (!obj->_nameOnScreen.empty()) {
			setText(Common::convertToU32String(obj->_nameOnScreen.c_str(), Common::kWindows1251), fmt.RGBToColor(0xC0, 0xFF, 0xFF), fmt.RGBToColor(0xA, 0xA, 0xA));
		} else {
			setText(Common::convertToU32String(obj->_name.c_str(), Common::kWindows1251), fmt.RGBToColor(0x80, 0, 0), fmt.RGBToColor(0xA, 0xA, 0xA));
		}
	} else if (prevObj && !_objUnderCursor && !_dialog.isActive()) {
		setText(Common::U32String(""), 0, 0);
	}
}

void InterfaceMain::setTextChoice(const Common::Array<Common::U32String> &choices, uint16 color, uint16 selectedColor) {
	removeTexts();
	_objUnderCursor = nullptr;
	_objs.push_back(new QTextChoice(choices, color, selectedColor));
}

void InterfaceMain::setTextDescription(const Common::U32String &text, int frame) {
	removeTexts();
	QObjectStar *star = g_vm->getQSystem()->_star.get();
	star->_isActive = 0;
	_objUnderCursor = nullptr;
	_hasTextDesc = true;
	_objs.push_back(new QTextDescription(text, frame));
}

void InterfaceMain::removeTextDescription() {
	_hasTextDesc = false;
	_objUnderCursor = nullptr;
	g_vm->getQSystem()->_star->_isActive = true;
	removeTexts();
}

} // End of namespace Petka