/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "fork/fork_config.h"

#include "base/unixtime.h"
#include "data/data_session.h"
#include "fork/fork_channel.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Fork {
namespace {

constexpr auto kBlurKey = "blur";
constexpr auto kIgnoredKey = "ignored";
constexpr auto kAvatarsKey = "avatars";
constexpr auto kKeepDeletedKey = "keepDeleted";

[[nodiscard]] QJsonArray FlagToJson(const bool value, TimeId updatedAt) {
	auto result = QJsonArray();
	result.append(value ? 1 : 0);
	result.append(qint32(updatedAt));
	return result;
}

[[nodiscard]] bool FlagFromJson(const QJsonValue &value, TimeId *at) {
	const auto list = value.toArray();
	if (list.size() != 2) {
		return false;
	}
	if (at) {
		*at = TimeId(list[1].toInt());
	}
	return (list[0].toInt() != 0);
}

[[nodiscard]] QJsonObject ReadObject(const QByteArray &state) {
	if (state.isEmpty()) {
		return QJsonObject();
	}
	const auto document = QJsonDocument::fromJson(state);
	return document.isObject() ? document.object() : QJsonObject();
}

} // namespace

Config::Config(not_null<Main::Session*> session)
: _session(session) {
	applySerialized(_session->settings().forkSettingsState());
}

bool Config::stickerSetBlurred(uint64 setId) const {
	const auto i = _blurredSets.find(setId);
	return (i != end(_blurredSets)) && i->second.value;
}

void Config::setStickerSetBlurred(uint64 setId, bool blurred) {
	if (!setId) {
		return;
	}
	_blurredSets[setId] = Flag{
		.value = blurred,
		.updatedAt = base::unixtime::now(),
	};
	save();
}

const Config::Avatar *Config::peerAvatar(PeerId peerId) const {
	const auto i = _avatars.find(peerId);
	return (i != end(_avatars)) ? &i->second : nullptr;
}

QString Config::peerAvatarPath(PeerId peerId) const {
	const auto avatar = peerAvatar(peerId);
	return avatar ? avatar->path : QString();
}

uint64 Config::peerAvatarDocumentId(PeerId peerId) const {
	const auto avatar = peerAvatar(peerId);
	return avatar ? avatar->documentId : uint64(0);
}

void Config::setPeerAvatarPath(PeerId peerId, const QString &path) {
	if (!peerId) {
		return;
	}
	auto &avatar = _avatars[peerId];
	avatar.path = path;
	avatar.updatedAt = base::unixtime::now();
	save();
}

void Config::setPeerAvatarDocumentId(PeerId peerId, uint64 documentId) {
	if (!peerId || !documentId) {
		return;
	}
	auto &avatar = _avatars[peerId];
	avatar.documentId = documentId;
	avatar.updatedAt = base::unixtime::now();
	save();
}

void Config::clearPeerAvatar(PeerId peerId) {
	if (_avatars.contains(peerId)) {
		_avatars.remove(peerId);
		save();
	}
}

bool Config::userIgnored(PeerId peerId) const {
	const auto i = _ignored.find(peerId);
	return (i != end(_ignored)) && i->second.value;
}

void Config::setUserIgnored(PeerId peerId, bool ignored) {
	if (!peerId) {
		return;
	}
	_ignored[peerId] = Flag{
		.value = ignored,
		.updatedAt = base::unixtime::now(),
	};
	save();
}

void Config::setDeletedRetained(bool retained) {
	if (_retainDeleted.value == retained) {
		return;
	}
	_retainDeleted = Flag{
		.value = retained,
		.updatedAt = base::unixtime::now(),
	};
	save();
}

int Config::ignoredCount() const {
	auto result = 0;
	for (const auto &[peerId, flag] : _ignored) {
		result += flag.value ? 1 : 0;
	}
	return result;
}

int Config::blurredSetsCount() const {
	auto result = 0;
	for (const auto &[setId, flag] : _blurredSets) {
		result += flag.value ? 1 : 0;
	}
	return result;
}

void Config::clearIgnored() {
	auto changed = false;
	for (auto &[peerId, flag] : _ignored) {
		if (flag.value) {
			flag = Flag{ .value = false, .updatedAt = base::unixtime::now() };
			changed = true;
		}
	}
	if (changed) {
		save();
	}
}

void Config::clearBlurredSets() {
	auto changed = false;
	for (auto &[setId, flag] : _blurredSets) {
		if (flag.value) {
			flag = Flag{ .value = false, .updatedAt = base::unixtime::now() };
			changed = true;
		}
	}
	if (changed) {
		save();
	}
}

QByteArray Config::serialize() const {
	auto object = QJsonObject();

	auto blur = QJsonObject();
	for (const auto &[setId, flag] : _blurredSets) {
		blur.insert(
			QString::number(setId),
			FlagToJson(flag.value, flag.updatedAt));
	}
	if (!blur.isEmpty()) {
		object.insert(kBlurKey, blur);
	}

	auto ignored = QJsonObject();
	for (const auto &[peerId, flag] : _ignored) {
		ignored.insert(
			QString::number(peerId.value),
			FlagToJson(flag.value, flag.updatedAt));
	}
	if (!ignored.isEmpty()) {
		object.insert(kIgnoredKey, ignored);
	}

	auto avatars = QJsonObject();
	for (const auto &[peerId, avatar] : _avatars) {
		auto list = QJsonArray();
		list.append(avatar.path);
		list.append(QString::number(avatar.documentId));
		list.append(qint32(avatar.updatedAt));
		avatars.insert(QString::number(peerId.value), list);
	}
	if (!avatars.isEmpty()) {
		object.insert(kAvatarsKey, avatars);
	}

	object.insert(
		kKeepDeletedKey,
		FlagToJson(_retainDeleted.value, _retainDeleted.updatedAt));

	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void Config::applySerialized(const QByteArray &state) {
	const auto object = ReadObject(state);
	if (object.isEmpty()) {
		return;
	}

	const auto mergeFlags = [&](
			const QJsonObject &source,
			auto &target) {
		for (auto i = source.begin(); i != source.end(); ++i) {
			auto updatedAt = TimeId(0);
			const auto value = FlagFromJson(i.value(), &updatedAt);
			const auto key = i.key().toULongLong();
			if (!key) {
				continue;
			}
			const auto j = target.find(key);
			if (j != end(target) && j->second.updatedAt > updatedAt) {
				continue;
			}
			target[key] = Flag{ .value = value, .updatedAt = updatedAt };
		}
	};
	mergeFlags(object.value(kBlurKey).toObject(), _blurredSets);

	const auto ignored = object.value(kIgnoredKey).toObject();
	for (auto i = ignored.begin(); i != ignored.end(); ++i) {
		auto updatedAt = TimeId(0);
		const auto value = FlagFromJson(i.value(), &updatedAt);
		const auto key = i.key().toULongLong();
		if (!key) {
			continue;
		}
		const auto j = _ignored.find(PeerId(key));
		if (j != end(_ignored) && j->second.updatedAt > updatedAt) {
			continue;
		}
		_ignored[PeerId(key)] = Flag{ .value = value, .updatedAt = updatedAt };
	}

	const auto avatars = object.value(kAvatarsKey).toObject();
	for (auto i = avatars.begin(); i != avatars.end(); ++i) {
		const auto list = i.value().toArray();
		const auto key = i.key().toULongLong();
		if (list.size() != 3 || !key) {
			continue;
		}
		const auto updatedAt = TimeId(list[2].toInt());
		const auto j = _avatars.find(PeerId(key));
		if (j != end(_avatars) && j->second.updatedAt > updatedAt) {
			continue;
		}
		_avatars[PeerId(key)] = Avatar{
			.path = list[0].toString(),
			.documentId = list[1].toString().toULongLong(),
			.updatedAt = updatedAt,
		};
	}

	auto updatedAt = TimeId(0);
	const auto keep = FlagFromJson(object.value(kKeepDeletedKey), &updatedAt);
	if (updatedAt >= _retainDeleted.updatedAt) {
		_retainDeleted = Flag{ .value = keep, .updatedAt = updatedAt };
	}
}

void Config::save() {
	_session->settings().setForkSettingsState(serialize());
	_session->saveSettingsDelayed();
	_session->data().forkChannel().scheduleSync();
}

} // namespace Fork
