# Reels for the Discord

The Riff House bot posts every entry in `community/reels.json` to #announcements once, within 10 minutes of it landing on `main`, and shares it with servers that follow the channel.

Add an entry when a reel or the trailer goes live:

```json
{
  "id": "trailer-2026-11",
  "caption": "the trailer is out. free amp app, no paywall on tone",
  "link": "https://www.instagram.com/reel/...",
  "video": "community/reels/trailer-2026-11.mp4"
}
```

- `id`: short and unique, never reused.
- `caption`: Natik's voice. Short, lowercase is fine, no em dashes, no AI phrasing.
- `link`: the public post (Instagram, YouTube). Optional if `video` is there.
- `video`: optional mp4 in this folder. Under 10 MB it is uploaded so it plays right inside Discord; bigger files are skipped and only the link is posted.

Entries are posted oldest first. Removing an entry never deletes the Discord post.
