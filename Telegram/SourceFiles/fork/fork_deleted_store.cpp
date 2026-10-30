/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "fork/fork_deleted_store.h"

#include "fork/fork_config.h"
#include "base/unixtime.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"

#include <QtCore/QDataStream>

namespace Fork {
namespace {

constexpr auto kMagic = quint32(0x46444C31);
constexpr auto kVersion = qint32(1);
constexpr auto kOwnDeletionCap = 512;

} // namespace

DeletedStore::DeletedStore(not_null<Data::Session*> owner)
: _owner(owner) {
}

bool DeletedStore::retain(not_null<HistoryItem*> item) {
	if (!_owner->forkConfig().deletedRetained()) {
		return false;
	} else if (!item->isRegular() || item->history()->peer->isSelf()) {
		return false;
	} else if (item->ttlDestroyAt()
		|| item->history()->peer->messagesTTL()) {
		return false;
	}
	const auto id = item->fullId();
	if (const auto existing = _entries.find(id); existing != end(_entries)) {
		item->markDeletedLocally(
			existing->second.deletedAt,
			existing->second.byOther);
		return true;
	}
	const auto i = _ownDeletions.find(id);
	const auto byOther = (i == end(_ownDeletions));
	if (!byOther) {
		_ownDeletions.erase(i);
	}
	const auto now = base::unixtime::now();
	_entries[id] = Entry{ .deletedAt = now, .byOther = byOther };
	item->applyDeletedBookkeeping();
	item->markDeletedLocally(now, byOther);
	_owner->requestItemTextRefresh(item);
	if (item->history()->lastMessage() == item) {
		item->history()->requestChatListMessage();
	}
	save();
	return true;
}

void DeletedStore::restore(not_null<HistoryItem*> item) const {
	if (_entries.empty() || !item->isRegular()) {
		return;
	}
	const auto i = _entries.find(item->fullId());
	if (i != end(_entries)) {
		item->markDeletedLocally(i->second.deletedAt, i->second.byOther);
	}
}

void DeletedStore::noteOwnDeletion(const std::vector<FullMsgId> &ids) {
	if (_ownDeletions.size() > kOwnDeletionCap) {
		_ownDeletions.clear();
	}
	for (const auto &id : ids) {
		_ownDeletions.emplace(id);
	}
}

int DeletedStore::count() const {
	return int(_entries.size());
}

QByteArray DeletedStore::serialize() const {
	if (_entries.empty()) {
		return QByteArray();
	}
	auto result = QByteArray();
	{
		auto stream = QDataStream(&result, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_5_1);
		stream << kMagic << kVersion << qint32(_entries.size());
		for (const auto &[id, entry] : _entries) {
			stream
				<< quint64(id.peer.value)
				<< qint64(id.msg.bare)
				<< qint32(entry.deletedAt)
				<< quint8(entry.byOther ? 1 : 0);
		}
	}
	return result;
}

void DeletedStore::addFromSerialized(const QByteArray &data) {
	if (data.isEmpty()) {
		return;
	}
	auto stream = QDataStream(data);
	stream.setVersion(QDataStream::Qt_5_1);

	auto magic = quint32(0);
	auto version = qint32(0);
	auto count = qint32(0);
	stream >> magic >> version >> count;
	if (stream.status() != QDataStream::Ok
		|| magic != kMagic
		|| version != kVersion) {
		return;
	}
	for (auto i = 0; i != count; ++i) {
		auto peer = quint64(0);
		auto msg = qint64(0);
		auto deletedAt = qint32(0);
		auto byOther = quint8(0);
		stream >> peer >> msg >> deletedAt >> byOther;
		if (stream.status() != QDataStream::Ok) {
			break;
		}
		_entries.emplace(
			FullMsgId(PeerId(peer), MsgId(msg)),
			Entry{ .deletedAt = TimeId(deletedAt), .byOther = (byOther == 1) });
	}
}

void DeletedStore::save() {
	auto &settings = _owner->session().settings();
	settings.setForkDeletedState(serialize());
	_owner->session().saveSettingsDelayed();
}

} // namespace Fork
