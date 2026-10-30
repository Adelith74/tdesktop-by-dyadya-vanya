/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "fork/fork_ignored.h"

#include "data/data_session.h"
#include "data/data_peer.h"
#include "history/history_item.h"
#include "fork/fork_config.h"

namespace Fork {

bool IsIgnoredSender(const PeerData *peer) {
	return peer
		&& peer->owner().forkConfig().userIgnored(peer->id);
}

bool IsIgnoredSender(const HistoryItem *item) {
	return item && IsIgnoredSender(item->from().get());
}

bool ToggleIgnoredUser(not_null<PeerData*> peer) {
	auto &config = peer->session().data().forkConfig();
	const auto ignored = !config.userIgnored(peer->id);
	config.setUserIgnored(peer->id, ignored);
	return ignored;
}

} // namespace Fork
