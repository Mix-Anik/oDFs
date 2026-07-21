# func_0x1a44a — Inactivity kick handler

**Address:** `0x1a44a`  
**Stack frame:** `enter 0x1c`  
**Called from:** Pmove outer (`0x1a866`), once per frame per client  
**Args:** `(gclient_t *client)`  
**Returns:** 1 = client still active; 0 = client was dropped

---

## Purpose

Checks whether the client has been idle (no movement input, no attack) for longer than a
configurable timeout and drops them with "Dropped due to inactivity". Sends a 10-second
warning before kicking. This is a DeFRaG-specific inactivity system distinct from the
standard Q3 idle kick.

**This function is NOT related to the race timer.** The earlier identification as "race
timer handler" in the README was incorrect.

---

## Pseudocode

```c
int InactivityCheck(gclient_t *client) {
    if (*(0x19ecc) == 0) {
        // Inactivity disabled — reset timer with 60-second window
        client->field_0x334 = level.time + 60000;
        client->field_0x338 = 0;
        return 1;
    }

    // Check if any movement key is pressed (cmd bytes in client struct):
    int forwardmove = (signed char)client->field_0x1ed;
    int rightmove   = (signed char)client->field_0x1ee;
    int upmove      = (signed char)client->field_0x1ef;
    int attackBit   = client->field_0x1e8 & 1;

    if (forwardmove != 0 || rightmove != 0 || upmove != 0) {
        // Player is moving — reset inactivity timer
        client->field_0x334 = level.time + 1000 * *(0x19ecc);
        client->field_0x338 = 0;
        return 1;
    }

    if (attackBit != 0) {
        // Attack key held — also counts as active
        goto check_timeout;  // fall through to timer check
    }

    // No input at all — check 'exempt' flag:
    if (client->field_0x1f0 != 0) return 1;   // exempt from inactivity kick

    // Has timeout expired?
    if (level.time > client->field_0x334) {
        // Kick the player
        int clientNum = (client - g_clients) / sizeof(gclient_t);
        trap_DropClient(clientNum, "Dropped due to inactivity");
        return 0;
    }

    // Within timeout window — check if warning should be sent
check_timeout:
    if (level.time - client->field_0x334 > -10000) {
        // Less than 10 seconds remain
        if (client->field_0x338 == 0) {
            client->field_0x338 = 1;
            int clientNum = (client - g_clients) / sizeof(gclient_t);
            trap_SendServerCommand(clientNum,
                "cp \"^1Ten seconds until inactivity drop!\"\n\"\"");
        }
    }
    return 1;
}
```

---

## Key fields and globals

| Address / Offset | Description |
|-----------------|-------------|
| `0x19ecc` | Inactivity timeout cvar (seconds). 0 = disabled |
| `client+0x1e8` | Buttons field (bit 0 = attack) |
| `client+0x1ed` | `cmd.forwardmove` byte (signed) |
| `client+0x1ee` | `cmd.rightmove` byte (signed) |
| `client+0x1ef` | `cmd.upmove` byte (signed) |
| `client+0x1f0` | Inactivity exempt flag (non-zero = skip kick) |
| `client+0x334` | Inactivity deadline = level.time + timeout |
| `client+0x338` | Warning sent flag (0 = not sent, 1 = sent) |
| `0x60000` | 60000 ms = 60 seconds (default window when cvar = 0) |
| `0x2710` | 10000 ms = 10 seconds (warning lead time) |

---

## oDFs relevance

oDFs does not have an inactivity kick system. This function could be added independently
of the race timer. It requires:
- A new cvar (equivalent of `0x19ecc`) for the timeout duration
- Two new gclient_t fields (`field_0x334` and `field_0x338`) for deadline and warning state
- Per-frame call from the Pmove outer equivalent in `g_active.c`
