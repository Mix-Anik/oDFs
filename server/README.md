# Running an oDFs server

## 1. Download

| What | Where |
|---|---|
| oDFe dedicated server (`oDFe.ded.x64.exe` on Windows, `oDFe.ded.x64` on Linux) | https://github.com/Defrag-racing/oDFe/releases |
| `zzz-defrag_vm.pk3` (the game code) | https://github.com/Mix-Anik/oDFs/releases |
| `pak0.pk3` .. `pak8.pk3` | your Quake 3 installation (`baseq3` folder) |
| DeFRaG pk3s and the maps | your DeFRaG installation / map downloads |

## 2. Folder structure

Same on Windows and Linux (the start script sits next to the server program; on Linux it is `oDFe.ded.x64` and `start_server.sh`):

```
odfs/
├── oDFe.ded.x64.exe
├── start_server.bat
├── baseq3/
│   └── pak0.pk3 ... pak8.pk3
├── defrag/
│   ├── zzz-defrag_vm.pk3
│   └── zz-defrag_media_*.pk3, zz-defrag_fix_*.pk3 and the map pk3s
└── homes/              one folder per server, each keeps its own configs, log and saved settings
    ├── cpm/defrag/     cpm.cfg, common.cfg
    ├── vq3/defrag/     vq3.cfg, common.cfg
    └── mixed/defrag/   mixed.cfg, common.cfg
```

Only create the servers you want. Delete older `zz*-defrag_vm*.pk3` files. On Linux file and folder names must be lower case
and the script must be executable (`chmod +x start_server.sh`).

## 3. Edit the configs

In `common.cfg` set `rconPassword` and `g_password` (empty = anyone can join). In `cpm.cfg` / `vq3.cfg` / `mixed.cfg` set
`sv_hostname` and the starting `map`.

## 4. Launch

The arguments are: config name (`cpm`, `vq3` or `mixed`), UDP port (a different one for each server), and `lan` or `public`.

```
:: Windows
start_server.bat cpm 27960 lan          not announced, for friends and LAN
start_server.bat cpm 27960 public       announced to the master servers

# Linux
./start_server.sh cpm 27960 lan
./start_server.sh cpm 27960 public
```

Open the UDP ports in your firewall / router. The script restarts the server if it stops; close the window (Ctrl+C) to stop it.

## 5. Run in the background (Linux, no root needed)

As root, once: `loginctl enable-linger defrag` (lets the user's services start at boot and keep running after logout).

Then as the `defrag` user (the `systemd/` folder from this repo sits next to `start_server.sh`; adjust the folder if yours is not `~/servers/odfs`):

```
mkdir -p ~/.config/systemd/user
cp ~/servers/odfs/systemd/odfs@.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now odfs@cpm odfs@vq3 odfs@mixed       # only the ones you want
```

The ports and `lan` / `public` of each server are in `systemd/cpm.env`, `vq3.env`, `mixed.env` (27960, 27961, 27962; change
`VISIBILITY=lan` to `public`). The servers start with the machine and restart if they crash.

```
systemctl --user status odfs@cpm          is it running
journalctl --user -u odfs@cpm -f          server console output
systemctl --user restart odfs@cpm         restart (after changing a cfg)
systemctl --user stop odfs@cpm            stop
```

Open the three UDP ports in the firewall (needs root, once). Use `rcon` from the game to give commands to a running server.
