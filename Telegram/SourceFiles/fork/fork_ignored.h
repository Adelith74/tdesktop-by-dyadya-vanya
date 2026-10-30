/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
class PeerData;

namespace Fork {

[[nodiscard]] bool IsIgnoredSender(const PeerData *peer);
[[nodiscard]] bool IsIgnoredSender(const HistoryItem *item);

// Returns the new state.
bool ToggleIgnoredUser(not_null<PeerData*> peer);

} // namespace Fork
