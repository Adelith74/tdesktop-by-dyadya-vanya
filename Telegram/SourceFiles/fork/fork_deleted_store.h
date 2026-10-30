/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/flat_map.h"
#include "base/flat_set.h"
#include "data/data_msg_id.h"

class HistoryItem;

namespace Data {
class Session;
} // namespace Data

namespace Fork {

// Keeps locally the messages that the server deletes, marked as deleted.
class DeletedStore final {
public:
	explicit DeletedStore(not_null<Data::Session*> owner);

	// Returns true when the item was kept instead of being destroyed.
	bool retain(not_null<HistoryItem*> item);

	// Restores the mark on an item restored from the local storage.
	void restore(not_null<HistoryItem*> item) const;

	// Marks the ids the user deletes here, so the echo is not "by other".
	void noteOwnDeletion(const std::vector<FullMsgId> &ids);

	[[nodiscard]] int count() const;
	[[nodiscard]] QByteArray serialize() const;
	void addFromSerialized(const QByteArray &data);

private:
	struct Entry {
		TimeId deletedAt = 0;
		bool byOther = false;
	};

	void save();

	const not_null<Data::Session*> _owner;
	base::flat_map<FullMsgId, Entry> _entries;
	base::flat_set<FullMsgId> _ownDeletions;

};

} // namespace Fork
