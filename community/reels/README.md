# Reels for the Discord

The Riff House bot posts every entry in `community/reels.json` to #content once, within a minute of it landing on `main` (or within 30 minutes of its `post_at` time), and shares it with servers that follow the channel.

Add an entry when a reel or the trailer goes live:

```json
{
  "id": "trailer-2026-11",
  "caption": "the trailer is out. free amp app, no paywall on tone",
  "link": "https://www.instagram.com/reel/...",
  "video": "community/reels/trailer-2026-11.mp4",
  "announce": "the Riff House trailer is out, go watch it"
}
```

- `id`: short and unique, never reused.
- `caption`: Natik's voice. Short, lowercase is fine, no em dashes, no AI phrasing.
- `link`: the public post (Instagram, YouTube). Optional if `video` is there.
- `post_at`: optional. Queue it for later: `"2026-11-15T20:00:00+05:30"` (India time if no zone is given). The bot posts it on the first run after that time.
- `video`: optional mp4 in this folder. Under 10 MB it is uploaded so it plays right inside Discord; bigger files are skipped and only the link is posted.

- `announce`: optional, for big ones like the trailer. Also posts this line in #announcements with @everyone and a link to the #content post.

Entries are posted oldest first. Removing an entry never deletes the Discord post.
