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
