# TASK-005-pairing-docs-real-uids

User report (2026-09-05, bench): after TASK-004 the operator provisioned
the REAL reader key (copied the real key from the readers table their
`./run setup` printed — the earlier 401 was solved), then ran the
§Prerequisites-2 verification curl **verbatim from docs/PAIRING.md**:

```text
curl -i -X POST http://192.168.1.6:8000/api/v1/events/tap \
     -H "Authorization: Bearer <READER_API_KEY-from-readers-table>" \
     -d '{"credential_uid": "A-SEEDED-UID"}'
→ HTTP 404 {"status":"error","message":"Card not recognized"}
```

Verdict from the user: "Not working".

Investigation (main agent, this run):
- The 404 is the backend behaving CORRECTLY and it actually proves the
  key check PASSED: `ResolveReaderToken` (B2B-Core) runs before the tap
  handler and answers `401` for any key with no `readers` row; a `404`
  is only produced after the reader identity was accepted, when the
  card lookup fails (`TapService::registerTap` → no `cards` row for the
  UID). The user's key is fine.
- The defect was OURS, in the TASK-004 docs: the verification example
  carried the payload `"A-SEEDED-UID"` — a placeholder that READS as a
  real seeded value (the user reasonably expected a `200`). The doc
  prose did mention "404 = key fine with an invented UID", but only
  after the curl, and the example value contradicted it.
- Compounding gap: the docs showed ONLY the readers table from the
  `./run setup` output and never mentioned the **cards** table printed
  right above it — the only place the real seeded `credential_uid`s
  (random 12-char uppercase strings, one per student, minted at first
  seed, stable across re-runs via firstOrCreate) actually live. The
  operator had the values in their own scrollback and no doc pointed at
  them.

Scope: documentation fix ONLY — firmware code and B2B-Core are verified
correct and untouched. Same protocol as TASK-004 (branch → records →
merge --no-ff → push → fresh-clone verify).

Phases: A (PAIRING.md EN) → B (PAIRING.es.md mirror) → C (CHECKLIST
EN/ES cross-links) → D (.agent records) → E (verification: both builds
+ native suite + secret scan; docs-only change, build proof is a
regression guard) → F (merge, push, fresh-clone verify).

---

## Acceptance criteria — evaluation (2026-09-05)

```text
Phase A — docs/PAIRING.md (EN)
[x] §Prerequisites 1 now shows BOTH seeder tables (cards + readers,
    sample output) and states: seeded credential_uids are random
    12-char uppercase strings, one per student, stable across re-runs
[x] §Prerequisites 2 verification curl placeholder is unmistakably a
    placeholder ("PASTE-A-REAL-credential_uid-HERE")
[x] New "Where to get a real credential_uid" block: cards table of the
    ./run setup output / tinker Card::pluck one-liner / a card you
    paired yourself ([NFC] line)
[x] 401/404/200 outcome table where 404 is explicitly "the key check
    PASSED" (auth middleware ordering pinned)
[x] FAQ entry: "404 on the verification curl — is my key broken?" → No;
    401-vs-404 semantics

Phase B — docs/PAIRING.es.md
[x] Full mirror of every Phase A change in Spanish

Phase C — MANUAL_VERIFICATION_CHECKLIST EN/ES
[x] Setup §1 mentions BOTH tables (cards = the UIDs a tap recognizes)
[x] §2.1 points at the cards table + PAIRING.md Prerequisites 1/2

Phase D
[x] Task file + run ledger + state snapshot + PROJECT.md fact updated

Phase E
[x] pio run -e esp32dev SUCCESS (regression guard — docs-only change)
[x] pio run -e esp32dev-mock SUCCESS
[x] pio test -e native 65/65 PASS (unchanged suite)
[x] No secret material in tracked files (git grep scan)

Phase F
[x] Branch merged --no-ff to main and pushed
[x] Fresh clone of merged main re-verified
```

Honesty boundary (protocol Section 0.1): verified here = the doc facts
against B2B-Core source read this run (ResolveReaderToken ordering,
TapService 404 conditions, DemoSeeder firstOrCreate + printCredentials
output shape, run/setup.sh seeding path) plus the standard build/test
regression suite. NOT verified here = a live curl against the user's
backend (their 404 transcript is the evidence; the corrected doc tells
them how to reach the 200).

## Out-of-scope guard

- No firmware code change (the 401/404/409/422 remediation lines and
  mode hints from TASK-004 stand).
- b2b-core untouched (main stays @ e1350db). In particular NOT done
  (possible follow-ups, not started): deterministic/fixed seeded
  credential_uids so docs could show literal values; a card-listing
  admin endpoint or dashboard panel; a GET /reader/me key-check
  endpoint.
