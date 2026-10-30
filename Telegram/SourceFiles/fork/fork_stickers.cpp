/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "fork/fork_stickers.h"

#include "data/data_document.h"
#include "data/data_session.h"
#include "fork/fork_config.h"

namespace Fork {
namespace {

[[nodiscard]] uint64 SetId(not_null<DocumentData*> document) {
	const auto sticker = document->sticker();
	return (sticker && !sticker->set.empty()) ? sticker->set.id : uint64(0);
}

} // namespace

bool StickerSetBlurred(not_null<DocumentData*> document) {
	const auto setId = SetId(document);
	return setId && document->owner().forkConfig().stickerSetBlurred(setId);
}

bool ToggleStickerSetBlur(not_null<DocumentData*> document) {
	const auto setId = SetId(document);
	if (!setId) {
		return false;
	}
	auto &config = document->owner().forkConfig();
	const auto blurred = !config.stickerSetBlurred(setId);
	config.setStickerSetBlurred(setId, blurred);
	document->owner().requestDocumentViewRepaint(document);
	return blurred;
}

} // namespace Fork
