/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "fork/fork_channel.h"

#include "apiwrap.h"
#include "base/random.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "fork/fork_config.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"

#include <QtCore/QFile>

namespace Fork {
namespace {

constexpr auto kSyncDelay = crl::time(5000);
constexpr auto kMaxUploadSize = 512 * 1024;

struct LastMessage {
	MsgId id = 0;
	QString text;
};

[[nodiscard]] LastMessage LastFrom(const MTPmessages_Messages &result) {
	auto message = LastMessage();
	const auto check = [&](const MTPVector<MTPMessage> &messages) {
		for (const auto &entry : messages.v) {
			if (entry.type() != mtpc_message) {
				continue;
			}
			const auto &data = entry.c_message();
			const auto text = QString::fromUtf8(data.vmessage().v);
			if (!text.isEmpty()) {
				message.id = MsgId(data.vid().v);
				message.text = text;
				return;
			}
		}
	};
	result.match([&](const MTPDmessages_messages &data) {
		check(data.vmessages());
	}, [&](const MTPDmessages_messagesSlice &data) {
		check(data.vmessages());
	}, [&](const MTPDmessages_channelMessages &data) {
		check(data.vmessages());
	}, [](const MTPDmessages_messagesNotModified &) {
	});
	return message;
}

[[nodiscard]] PeerId ChannelIdFromUpdates(const MTPUpdates &updates) {
	auto result = PeerId(0);
	const auto check = [&](const MTPVector<MTPUpdate> &list) {
		for (const auto &update : list.v) {
			switch (update.type()) {
			case mtpc_updateChannel: {
				result = peerFromChannel(ChannelId(
					update.c_updateChannel().vchannel_id().v));
			} break;
			case mtpc_updateNewChannelMessage: {
				const auto &data = update.c_updateNewChannelMessage();
				if (data.vmessage().type() == mtpc_message) {
					result = peerFromMTP(
						data.vmessage().c_message().vpeer_id());
				}
			} break;
			}
		}
	};
	updates.match([&](const MTPDupdates &data) {
		check(data.vupdates());
	}, [&](const MTPDupdatesCombined &data) {
		check(data.vupdates());
	}, [](const MTPDupdatesTooLong &) {
	});
	return result;
}

} // namespace

SettingsChannel::SettingsChannel(not_null<Main::Session*> session)
: _session(session)
, _syncTimer([=] { push(); }) {
}

void SettingsChannel::start() {
	const auto stored = PeerId(_session->settings().forkChannelId());
	if (stored) {
		adopt(stored);
		return;
	}
	scanSavedMessages();
}

void SettingsChannel::scheduleSync() {
	if (_channelId) {
		_syncTimer.callOnce(kSyncDelay);
	}
}

void SettingsChannel::adopt(PeerId channelId) {
	_channelId = channelId;
	const auto peer = _session->data().peerLoaded(channelId);
	if (const auto channel = peer ? peer->asChannel() : nullptr) {
		pull(channel);
	}
}

void SettingsChannel::scanSavedMessages() {
	_session->api().request(MTPmessages_GetHistory(
		MTP_inputPeerSelf(),
		MTP_int(0),
		MTP_int(0),
		MTP_int(0),
		MTP_int(10),
		MTP_int(0),
		MTP_int(0),
		MTP_long(0)
	)).done(crl::guard(_session, [=](const MTPmessages_Messages &result) {
		const auto text = LastFrom(result).text;
		if (text.startsWith(kPointerPrefix)) {
			if (const auto id = text.mid(int(qstrlen(kPointerPrefix)))
					.toULongLong()) {
				_session->settings().setForkChannelId(id);
				adopt(PeerId(id));
				return;
			}
		}
		create();
	})).fail(crl::guard(_session, [=](const MTP::Error &) {
		create();
	})).send();
}

void SettingsChannel::create() {
	using Flag = MTPchannels_CreateChannel::Flag;
	_session->api().request(MTPchannels_CreateChannel(
		MTP_flags(Flag::f_broadcast),
		MTP_string(kTitle),
		MTP_string(kAbout)
	)).done(crl::guard(_session, [=](const MTPUpdates &result) {
		created(result);
	})).send();
}

void SettingsChannel::created(const MTPUpdates &updates) {
	_session->api().applyUpdates(updates);
	const auto channelId = ChannelIdFromUpdates(updates);
	if (!channelId) {
		return;
	}
	_channelId = channelId;
	_session->settings().setForkChannelId(channelId.value);
	const auto peer = _session->data().peerLoaded(channelId);
	if (const auto channel = peer ? peer->asChannel() : nullptr) {
		archive(channel);
		pointer();
		push();
	}
}

void SettingsChannel::archive(not_null<ChannelData*> channel) {
	_session->api().request(MTPfolders_EditPeerFolders(
		MTP_vector<MTPInputFolderPeer>(
			1,
			MTP_inputFolderPeer(channel->input(), MTP_int(1)))
	)).send();
}

void SettingsChannel::pointer() {
	const auto text = QString::fromUtf8(kPointerPrefix)
		+ QString::number(_channelId.value);
	_session->api().request(MTPmessages_SendMessage(
		MTP_flags(0),
		MTP_inputPeerSelf(),
		MTPInputReplyTo(),
		MTP_string(text),
		MTP_long(base::RandomValue<uint64>()),
		MTPReplyMarkup(),
		MTPVector<MTPMessageEntity>(),
		MTP_int(0),
		MTP_int(0),
		MTPInputPeer(),
		MTPInputQuickReplyShortcut(),
		MTP_long(0),
		MTP_long(0),
		MTPInputSuggestedPost(),
		MTPInputRichMessage()
	)).send();
}

void SettingsChannel::push() {
	if (!_channelId) {
		return;
	}
	const auto peer = _session->data().peerLoaded(_channelId);
	const auto channel = peer ? peer->asChannel() : nullptr;
	if (!channel) {
		return;
	}
	const auto text = QString::fromUtf8(
		_session->data().forkConfig().serialize());
	if (_stateId) {
		_session->api().request(MTPmessages_EditMessage(
			MTP_flags(0),
			channel->input(),
			MTP_int(_stateId.bare),
			MTP_string(text),
			MTPInputMedia(),
			MTPReplyMarkup(),
			MTPVector<MTPMessageEntity>(),
			MTP_int(0),
			MTP_int(0),
			MTPint(),
			MTPInputRichMessage()
		)).send();
	} else {
		_session->api().request(MTPmessages_SendMessage(
			MTP_flags(0),
			channel->input(),
			MTPInputReplyTo(),
			MTP_string(text),
			MTP_long(base::RandomValue<uint64>()),
			MTPReplyMarkup(),
			MTPVector<MTPMessageEntity>(),
			MTP_int(0),
			MTP_int(0),
			MTPInputPeer(),
			MTPInputQuickReplyShortcut(),
			MTP_long(0),
			MTP_long(0),
			MTPInputSuggestedPost(),
			MTPInputRichMessage()
		)).send();
		pull(channel);
	}
}

void SettingsChannel::pull(not_null<ChannelData*> channel) {
	_session->api().request(MTPmessages_GetHistory(
		channel->input(),
		MTP_int(0),
		MTP_int(0),
		MTP_int(0),
		MTP_int(1),
		MTP_int(0),
		MTP_int(0),
		MTP_long(0)
	)).done(crl::guard(_session, [=](const MTPmessages_Messages &result) {
		const auto last = LastFrom(result);
		if (!last.text.isEmpty()) {
			_stateId = last.id;
			_session->data().forkConfig().applySerialized(
				last.text.toUtf8());
		}
	})).send();
}

void SettingsChannel::uploadFile(const QString &path, Fn<void(uint64)> done) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	const auto bytes = file.readAll();
	if (bytes.isEmpty() || bytes.size() > kMaxUploadSize) {
		return;
	}
	const auto fileId = base::RandomValue<uint64>();
	const auto name = QFileInfo(path).fileName();
	_session->api().request(MTPupload_SaveFilePart(
		MTP_long(fileId),
		MTP_int(0),
		MTP_bytes(bytes)
	)).done(crl::guard(_session, [=](const MTPBool &) {
		const auto media = MTP_inputMediaUploadedDocument(
			MTP_flags(0),
			MTP_inputFile(
				MTP_long(fileId),
				MTP_int(1),
				MTP_string(name),
				MTP_string(QString())),
			MTPInputFile(),
			MTP_string(u"image/png"_q),
			MTP_vector<MTPDocumentAttribute>(
				1,
				MTP_documentAttributeFilename(MTP_string(name))),
			MTPVector<MTPInputDocument>(),
			MTPInputPhoto(),
			MTP_int(0),
			MTP_int(0));
		_session->api().request(MTPmessages_UploadMedia(
			MTP_flags(0),
			MTPstring(),
			MTP_inputPeerSelf(),
			media
		)).done(crl::guard(_session, [=](const MTPMessageMedia &result) {
			if (result.type() != mtpc_messageMediaDocument) {
				return;
			}
			const auto &data = result.c_messageMediaDocument();
			if (data.vdocument().type() != mtpc_document) {
				return;
			}
			const auto document = _session->data().processDocument(
				data.vdocument()).get();
			if (done) {
				done(document->id.value);
			}
		})).send();
	})).send();
}

} // namespace Fork
