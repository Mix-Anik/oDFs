# `func_0x273f` — clear a client's `raceTimer_t` record

**Address:** `0x273f` (`enter 0xc`), arg0 = `gentity_t *ent`. Sole call site:
`0x1d6f7` in `ClientDisconnect` (`0x1d6dc`), inside the `ent->client != NULL` guard.

```c
void func_0x273f( gentity_t *ent ) {
    func_0x3a5f( ent->client );        // client = *(ent + 0x204)
}
```

`func_0x3a5f(client)`:
```c
raceTimer_t *rec = func_0x3588( client );    // 0x1318dc + ((client - 0x12a2f0) / 0x37c) * 32
trap_Memset( rec, 0, 0x20 );                 // syscall -0x65 (memset), 32 bytes
```
`func_0x3588` = `&raceTimers[ client - level.clients ]` (`0x12a2f0` = `g_clients`
base, `0x37c` = `sizeof(gclient_t)`, `0x1318dc` = timer table, 32-byte stride). The
whole table (`0x800` bytes = 64 clients) is zeroed by `func_0x3a77` (called from
`0x1b57`). Table layout: [../G_TRIGGER/race_timer_system.md](../G_TRIGGER/race_timer_system.md).

## Corrections
- Earlier docs labelled `0x273f` `ClientEvents`. Wrong: real `ClientEvents` is
  `0x1a63a` ([../G_ACTIVE/ClientEvents.md](../G_ACTIVE/ClientEvents.md)); vanilla
  `ClientDisconnect` never calls `ClientEvents`. Fixed in `ClientDisconnect.md`,
  `ClientThink_real.md`, `G_ACTIVE/README.md`, `G_CLIENT/README.md`.
- Purpose reading: disconnect resets that slot's race timer so a later client
  reusing the clientNum doesn't inherit it. That follows from the memset;
  nothing else in the bytecode states the intent.

## oDFs
**Ported** in `ClientDisconnect` (`g_client.c`): `if (ent->client) memset(G_GetTimerRecord(ent->client),0,sizeof(raceTimer_t))`, placed before the `CON_DISCONNECTED` early-out to match the bytecode's `ent->client != NULL`-only gate. Syntax-check clean.
