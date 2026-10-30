# Fork features: settings channel, deleted messages, custom avatars, sticker blur

A personal fork, not an upstream feature set. This document records the decisions
behind the fork's extra features so they are not re-litigated later.

## Fixed decisions

**Settings storage.** One JSON document uploaded to a private broadcast channel
owned by the account, edited in place. Custom avatar images are uploaded to the
same channel as separate photos, and the JSON holds only their file ids. The
channel is created lazily, has no username, and is archived with
`folders.editPeerFolders`. Its id is also written to Saved Messages as the
bootstrap pointer used to find it again; a marker in the channel `about` is the
fallback before creating a new one.

**Custom avatars.** Local display override only: the fork changes how a peer is
drawn for this account, nothing reaches other users. Applied through
`Ui::UserpicView` rather than at each paint site.

**Deleted messages.** The row is kept in place locally and marked deleted, with
the deletion time, instead of being erased. The server does not report the
author: in private chats the fork can say the other side deleted it, while
groups and channels only show that it was deleted.

**Retention.** Unlimited by default, with an optional size cap exposed in
settings.

**Edits.** Only the last version before deletion is kept; earlier revisions are
not preserved.

**Media.** Only what was already downloaded stays. No speculative downloading of
attachments that might be deleted later, so a never-opened photo is lost when it
is deleted and the row falls back to text.

**Always excluded** from retention, with no setting to enable them: secret chats
and self-destructing media or anything else carrying a TTL period.

**Export.** `Export::Settings` gains an option to include deleted messages, and
the exported HTML and JSON mark them as deleted.

**Blurred sticker sets.** A per-set flag set from the sticker's context menu in
chat. Rendering reuses the existing spoiler blur; animated stickers blur a static
first frame instead of every frame.

## Terms of Service boundary

The Telegram API Terms of Service rule 1.4 forbids interfering with basic
functionality, including preventing self-destructing content from disappearing.
Local retention of an already received message that the server deletes does not
cross that line while secret chats and TTL content stay untouched.

Rule 2.1 requires an own `api_id` for the application. Personal builds stay on
the credentials already configured in this tree. **Before sharing a build with
anyone: register a dedicated `api_id` at `my.telegram.org`, pass it through
`TDESKTOP_API_ID` and `TDESKTOP_API_HASH`, and change the app name so it does not
use the word "Telegram" without "Unofficial" and does not use the official
logo.** Rule 4.1 sends violation notices to the account owning the `api_id`, so
staying on someone else's credentials exposes that app rather than this one.

## Order of work

1. Deleted messages: interception, local store, marked rows, export option.
2. Settings store and the private channel it syncs through.
3. Custom peer avatars.
4. Blurred sticker sets.

Deleted messages do not depend on the settings channel, being entirely local, so
step 1 and step 2 can proceed side by side.

## Implementation status

Code lives in `SourceFiles/fork/`, reached from upstream through thin calls.

Done:

- Deleted messages: the server update is intercepted in
  `Session::processMessagesDeleted` and `Session::processNonChannelMessagesDeleted`,
  the item is kept and marked instead of destroyed, the "no longer exists"
  bookkeeping runs through `HistoryItem::applyDeletedBookkeeping`, and the marks
  survive a restart in `Main::SessionSettings`. The marker is drawn by
  `BottomInfo` next to the date, in the same style as "edited".
- Sticker set blur: per-set flag in `Fork::Config`, toggled from the sticker
  context menu, painted by reusing the sensitive-sticker blur path so animated
  sets show a blurred static frame instead of playing.
- Custom peer avatars: local override stored per peer, painted in the single
  avatar funnel `PeerData::paintUserpic`, picked from the chat menu.
- Ignored users: a per-user flag toggled from the right-click menu of the
  participants list. A hidden sender never reaches `History::addItemToBlock`, so
  their messages are not laid out in the chat at all, and `Reply` shows the
  ignored-user label instead of the quoted original. Toggling rebuilds the views
  of the chat through `History::rebuildViewsForHiddenSenders`, and
  `History::lastAvailableMessage` skips hidden senders so the chats list preview
  never shows them either.
- Settings: the Advanced section gained a "Keep Deleted Messages" checkbox and
  the two reset buttons for ignored users and blurred sticker sets.

Not done yet:

- Export: done. `Export::Settings::deletedMessages` adds the checkbox, and
  `AppendDeletedMessages` in `export_controller.cpp` merges the retained local
  items of the exported peer into every `Data::MessagesSlice` before it reaches
  the writer, marking each one with a "deleted at" line so it shows in both HTML
  and JSON. The merge is skipped when the checkbox is off.
- Media pinning: exempting the files of retained messages from the media cache
  limit needs `Storage::Cache`, whose sources are the uninitialized `lib/`
  submodule. Nothing can be written against it until the submodules are fetched.
- Settings channel: `Fork::SettingsChannel` creates the private broadcast
  channel, archives it, and writes a `fork-settings:<id>` pointer into Saved
  Messages so another device adopts it instead of creating a second one. The
  `Fork::Config` JSON is written into the channel as message text and read back
  through `messages.getHistory`. Every write is coalesced by a five second
  timer into an edit of one state message, whose id is learned from the channel
  history. Each setting carries an `updatedAt` and `applySerialized` merges key
  by key, so two devices converge instead of overwriting each other.
- Avatars across devices: the chosen image is uploaded into the same channel
  with `upload.saveFilePart` plus `messages.uploadMedia`, and the document id is
  stored next to the local path. The device that picked the file paints from the
  path; another device resolves the document through `Data::Session::document`,
  downloads it into the media cache and repaints when it arrives.
- Settings: the blur and avatar toggles live in context menus only.
