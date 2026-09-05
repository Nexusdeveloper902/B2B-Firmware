# STATE SNAPSHOT — after RUN-2026-09-05-firmware-005

## Repository state

- Branch: main at the TASK-005 merge commit (feature/
  TASK-005-pairing-docs-real-uids merged --no-ff; see `git log` for
  the hash)
- Working tree: clean
- b2b-core: untouched by this run (its main stays at e1350db+)

## What the firmware does now (delta vs RUN-004)

No behavior change — TASK-005 was a documentation-accuracy fix:

- docs/PAIRING.md + .es.md Prerequisites now show BOTH tables the
  B2B-Core seeder prints (cards = the real seeded `credential_uid`s —
  random 12-char uppercase, one per student, stable across re-runs;
  readers = the Bearer keys), name the exact place a real UID comes
  from (cards table of `./run setup` output / tinker pluck / your own
  paired card's `[NFC]` line), and use an unmistakable placeholder in
  the verification curl.
- The 401/404/200 semantics of the verification curl are now a table:
  401 = key not provisioned; **404 = the key check PASSED** (auth
  middleware accepted the reader; only the card lookup failed);
  200 = key + card both live. FAQ entry mirrors it.
- CHECKLIST EN/ES session-setup names both tables; §2.1 points at the
  cards table.
- Firmware binary content: identical to TASK-004 (rebuild verified
  green as a regression guard — esp32dev + esp32dev-mock + 65/65).

## Confirmed facts (cumulative, still current)

- Default build = real RC522 (ADR-004); mock is explicit opt-in
- Mode switching = serial password (ADR-005); boots OPERATION
- Pairing = arm-then-pair (B2B-Core ADR-020, 45 s window); the device
  prints guidance hints + per-status remediation lines (ADR-006)
- Backend auth: `ResolveReaderToken` matches the Bearer key against
  `readers` (401 before the handler); `404 Card not recognized` is
  emitted only AFTER identity acceptance → a 404 proves the key works
- Seeded demo cards: random 12-char uppercase `credential_uid`s, one
  per student, printed in the cards table of every `./run setup` run
  (firstOrCreate ⇒ same values re-printed, never regenerated)

## Bench status (user, live backend 192.168.1.6:8000)

- Wi-Fi + RC522 + serial password mode switching: WORKING (user
  transcripts)
- Reader key: PROVEN WORKING as of this task (404-not-401 transition
  between the user's two transcripts — middleware accepted the key)
- Still to bench: a 200 tap with a real seeded UID; arm → pair card
  62041607; the TASK-004 serial guidance lines in situ

## Next steps for whoever picks this up

1. User: verification curl with a real `credential_uid` → 200; then
   arm (admin PAT) + PAIRING-mode tap of card 62041607 (checklist §5);
   the remediation/hint lines should now walk them through it.
2. Optional backend follow-ups (their repo, NOT started here):
   deterministic seeded UIDs, card-listing surface, GET /reader/me.
3. Keep the honesty boundary: agent runs verify compile + host tests +
   doc facts vs source; live-HTTP behavior stays a bench item.
