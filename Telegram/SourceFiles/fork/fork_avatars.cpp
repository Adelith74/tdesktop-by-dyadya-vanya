/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "fork/fork_avatars.h"

#include "core/file_utilities.h"
#include "data/data_document.h"
#include "data/data_changes.h"
#include "data/data_session.h"
#include "data/data_peer.h"
#include "fork/fork_config.h"
#include "fork/fork_channel.h"
#include "lang/lang_keys.h"
#include "ui/image/image_prepare.h"
#include "ui/painter.h"
#include "ui/userpic_view.h"

#include <QtGui/QPainter>

namespace Fork {
namespace {

struct Cached {
	QImage image;
};

base::flat_map<QString, std::shared_ptr<Cached>> Cache;

[[nodiscard]] QString CacheKey(
		const QString &path,
		int size,
		Ui::PeerUserpicShape shape) {
	return path
		+ QChar(0x1f)
		+ QString::number(size)
		+ QChar(0x1f)
		+ QString::number(static_cast<int>(shape));
}

[[nodiscard]] std::shared_ptr<Cached> Load(
		const QString &path,
		int size,
		Ui::PeerUserpicShape shape) {
	auto image = QImage(path);
	if (image.isNull()) {
		return nullptr;
	}
	auto result = std::make_shared<Cached>();
	result->image = image.scaled(
		QSize(size, size),
		Qt::KeepAspectRatioByExpanding,
		Qt::SmoothTransformation);
	result->image = result->image.copy(
		(result->image.width() - size) / 2,
		(result->image.height() - size) / 2,
		size,
		size).convertToFormat(QImage::Format_ARGB32_Premultiplied);
	if (shape == Ui::PeerUserpicShape::Monoforum) {
		result->image = Ui::ApplyMonoforumShape(std::move(result->image));
	} else if (shape == Ui::PeerUserpicShape::Forum) {
		result->image = Images::Round(
			std::move(result->image),
			Images::CornersMask(
				size * Ui::ForumUserpicRadiusMultiplier()
				/ style::DevicePixelRatio()));
	} else {
		result->image = Images::Circle(std::move(result->image));
	}
	return result;
}

[[nodiscard]] QString LocalPath(not_null<const PeerData*> peer) {
	const auto &config = peer->session().data().forkConfig();
	if (const auto path = config.peerAvatarPath(peer->id); !path.isEmpty()) {
		return path;
	}
	const auto documentId = config.peerAvatarDocumentId(peer->id);
	if (!documentId) {
		return QString();
	}
	const auto document = peer->owner().document(
		DocumentId(documentId)).get();
	const auto path = document->filepath(true);
	if (path.isEmpty()) {
		document->save(Data::FileOrigin(), QString());
	}
	return path;
}

[[nodiscard]] std::shared_ptr<Cached> Custom(
		not_null<const PeerData*> peer,
		int size,
		Ui::PeerUserpicShape shape) {
	const auto path = LocalPath(peer);
	if (path.isEmpty()) {
		return nullptr;
	}
	const auto key = CacheKey(path, size, shape);
	const auto i = Cache.find(key);
	if (i != end(Cache)) {
		return i->second;
	}
	const auto result = Load(path, size, shape);
	Cache[key] = result;
	return result;
}

} // namespace

bool HasCustomUserpic(not_null<const PeerData*> peer) {
	const auto &config = peer->session().data().forkConfig();
	return !config.peerAvatarPath(peer->id).isEmpty()
		|| config.peerAvatarDocumentId(peer->id) != 0;
}

bool PaintCustomUserpic(
		not_null<const PeerData*> peer,
		QPainter &p,
		const PaintUserpicContext &context) {
	const auto ratio = style::DevicePixelRatio();
	const auto size = context.size * ratio;
	const auto cached = Custom(peer, size, context.shape);
	if (!cached) {
		return false;
	}
	p.drawImage(
		QRect(context.position, QSize(context.size, context.size)),
		cached->image);
	return true;
}

void ChooseCustomUserpic(not_null<PeerData*> peer, QPointer<QWidget> parent) {
	const auto session = &peer->session();
	const auto peerId = peer->id;
	FileDialog::GetOpenPath(
		parent,
		tr::lng_fork_choose_avatar(tr::now),
		FileDialog::ImagesFilter(),
		[=](FileDialog::OpenResult &&result) {
			const auto path = result.paths.isEmpty()
				? QString()
				: result.paths.front();
			const auto target = session->data().peerLoaded(peerId);
			if (path.isEmpty() || !target) {
				return;
			}
			Cache.clear();
			auto &config = session->data().forkConfig();
			config.setPeerAvatarPath(peerId, path);
			session->data().changes().peerUpdated(
				target,
				Data::PeerUpdate::Flag::Photo);
			session->data().forkChannel().uploadFile(
				path,
				[=](uint64 documentId) {
					config.setPeerAvatarDocumentId(peerId, documentId);
				});
		});
}

void ClearCustomUserpic(not_null<PeerData*> peer) {
	const auto peerId = peer->id;
	const auto session = &peer->session();
	Cache.clear();
	session->data().forkConfig().clearPeerAvatar(peerId);
	session->data().changes().peerUpdated(
		peer,
		Data::PeerUpdate::Flag::Photo);
}

} // namespace Fork
