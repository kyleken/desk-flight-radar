# Changelog

## 1.0.11 (2026-11-19): photos follow planespotters' rules
- Fixed: planespotters refused the board's photo requests (HTTP 403), so planes showed "Photo
  failed" even when adsb.fi had a photo. The board now follows planespotters' photo API terms:
  - identifies itself with a link to this project;
  - asks planespotters only when you tap a plane (no lookups in advance);
  - loads the photo unchanged from planespotters' image server (no resizing service);
  - doesn't keep the photo once the popup closes, and keeps planespotters' answers 60 min.
- If planespotters refuses a request, it's left alone for 10 minutes.
- If planespotters has no photo, or sends one the board can't draw (progressive JPEG), the
  airport-data.com photo is shown instead.
- Nearby planes' text (type, owner, route) is still looked up in advance; the photo now takes
  a second or two after a tap.

## 1.0.10 (2026-11-19): photo reliability
- Fixed: one failed photo lookup (timeout, busy server, low memory) was remembered for an hour,
  so tapping that plane kept showing "No photo". Only a definite "nothing on file" is remembered
  now (30 min); anything that failed is tried again on the next tap.
- Fixed: a photo too big for the cache was dropped and the plane then showed "No photo" on every
  later tap. The photo's address is kept, so it's simply downloaded again.
- Planespotters and wsrv.nl requests retry once after a connection error, timeout or busy
  server (HTTP 429/5xx).
- Every downloaded photo is checked before use; progressive JPEGs, PNGs and error pages are
  skipped instead of failing to draw. If the resized download fails, the original file is used
  when it's drawable, and airport-data.com's photo is tried when planespotters' won't load.
- Planespotters answers read like the adsb.fi map does (thumbnail as object or string, then
  `thumbnail_large`); registration and type are URL-encoded.
- The popup says "Photo failed" when a download failed (tap again to retry) and "No photo" only
  when there's none on file. Clearer serial log lines for each step.

## 1.0.9 (2026-11-16): production release
- Weather loads promptly: weather now runs ahead of the aircraft refresh, and the 5-second
  aircraft timer counts from the end of a fetch, so slow fetches can't hog the connection.
- Weather retries every 15 s until it first succeeds (then every 60 s).
- Weather status dot (green / amber / red); tap the weather to refresh; weather status on the
  About screen.

## 1.0.8
- Photo lookup sends the aircraft type, like the adsb.fi map, so planespotters returns a
  same-type photo when it has none of the exact aircraft. Such photos are labeled "Same type".

## 1.0.7
- Military highlight color is selectable (8 colors, default lavender) with a live preview, and
  can be turned off. New Settings row: Military aircraft.
- Fixed: night-dimming settings could load blank after upgrading from v1.0.0–1.0.3.

## 1.0.6
- Military aircraft are highlighted (feed database flag, plus the US military address block),
  with a MILITARY tag on the photo.

## 1.0.5
- Wrong Wi-Fi password no longer locks up the network list. Failures are recognized within
  seconds and explained ("Wrong password", "Can't find network"); background retries pause on
  the Wi-Fi screens.

## 1.0.4
- Night dimming has its own menu: start and end times, night brightness down to screen off,
  tap to wake for 30 s, 3-second preview.

## 1.0.3
- Faster photos: nearest 6 aircraft looked up in advance, last 10 cached, photo fetched before
  text, a tap interrupts the aircraft refresh.

## 1.0.2
- City on its own centered line in the weather strip.
- Photos resized by wsrv.nl to baseline JPEGs the device can decode; fallback lookups by
  registration and airport-data.com; details popup waits for the photo.

## 1.0.1
- Fixed screen colors (pixel byte order and BGR panel).

## 1.0.0
- First release: 43.2 mm square radar with weather strip, tap-for-details, zoom, settings,
  phone setup hotspot, on-screen Wi-Fi keyboard, ZIP or lat/long location, themes, night
  dimming, alert chime, Santa tracker.
