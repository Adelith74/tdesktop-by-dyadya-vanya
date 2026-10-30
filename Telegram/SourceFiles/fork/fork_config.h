/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/flat_map.h"
#include "data/data_peer_id.h"

namespace Main {
class Session;
} // namespace Main

namespace Fork {

// Small fork-wide state, one JSON document, merged between devices by key.
class Config final {
public:
	struct Avatar {
		QString path;
		uint64 documentId = 0;
		TimeId updatedAt = 0;
	};

	explicit Config(not_null<Main::Session*> session);

	[[nodiscard]] bool stickerSetBlurred(uint64 setId) const;
	void setStickerSetBlurred(uint64 setId, bool blurred);

	[[nodiscard]] const Avatar *peerAvatar(PeerId peerId) const;
	[[nodiscard]] QString peerAvatarPath(PeerId peerId) const;
	[[nodiscard]] uint64 peerAvatarDocumentId(PeerId peerId) const;
	void setPeerAvatarPath(PeerId peerId, const QString &path);
	void setPeerAvatarDocumentId(PeerId peerId, uint64 documentId);
	void clearPeerAvatar(PeerId peerId);

	[[nodiscard]] bool userIgnored(PeerId peerId) const;
	void setUserIgnored(PeerId peerId, bool ignored);

	[[nodiscard]] bool deletedRetained() const {
		return _retainDeleted;
	}
	void setDeletedRetained(bool retained);

	[[nodiscard]] int ignoredCount() const;
	[[nodiscard]] int blurredSetsCount() const;
	void clearIgnored();
	void clearBlurredSets();

	[[nodiscard]] QByteArray serialize() const;
	void applySerialized(const QByteArray &state);

private:
	struct Flag {
		bool value = false;
		TimeId updatedAt = 0;
	};

	void save();

	const not_null<Main::Session*> _session;
	base::flat_map<uint64, Flag> _blurredSets;
	base::flat_map<PeerId, Flag> _ignored;
	base::flat_map<PeerId, Avatar> _avatars;
	Flag _retainDeleted = { .value = true };

};

} // namespace Fork
