/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class DocumentData;

namespace Fork {

[[nodiscard]] bool StickerSetBlurred(not_null<DocumentData*> document);

// Returns the new state, and repaints the visible stickers of the set.
bool ToggleStickerSetBlur(not_null<DocumentData*> document);

} // namespace Fork
