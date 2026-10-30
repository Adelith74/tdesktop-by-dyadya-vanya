/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_peer.h"

#include <QtCore/QPointer>

class QWidget;
class QPainter;

namespace Fork {

[[nodiscard]] bool HasCustomUserpic(not_null<const PeerData*> peer);

// Paints the local override and returns true when there is one.
bool PaintCustomUserpic(
	not_null<const PeerData*> peer,
	QPainter &p,
	const PaintUserpicContext &context);

void ChooseCustomUserpic(not_null<PeerData*> peer, QPointer<QWidget> parent);
void ClearCustomUserpic(not_null<PeerData*> peer);

} // namespace Fork
