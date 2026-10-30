/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "data/data_msg_id.h"

namespace MTP {
class Error;
struct Updates;
struct Messages;
} // namespace MTP

class ChannelData;

namespace Main {
class Session;
} // namespace Main

namespace Fork {

// Keeps the fork settings in a private channel of the account and mirrors them
// between devices through it.
class SettingsChannel final {
public:
	static constexpr auto kTitle = "Fork Settings";
	static constexpr auto kAbout = "Private storage of the client settings.";
	static constexpr auto kPointerPrefix = "fork-settings:";

	explicit SettingsChannel(not_null<Main::Session*> session);

	void start();

	// Coalesces the changes of a few seconds into one channel write.
	void scheduleSync();

	// Uploads a small file into the channel, returns its document id.
	void uploadFile(const QString &path, Fn<void(uint64)> done);

	[[nodiscard]] PeerId channelId() const {
		return _channelId;
	}

private:
	void adopt(PeerId channelId);
	void scanSavedMessages();
	void created(const MTPUpdates &updates);
	void create();
	void archive(not_null<ChannelData*> channel);
	void push();
	void pull(not_null<ChannelData*> channel);
	void pointer();
	void rememberStateId(const MTPmessages_Messages &result);

	const not_null<Main::Session*> _session;
	PeerId _channelId = 0;
	MsgId _stateId = 0;
	base::Timer _syncTimer;

};

} // namespace Fork
