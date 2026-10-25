#!/bin/bash
# Starts one oDFs server and restarts it when it exits.
#   usage: start_server.sh <name> <port> <lan|public>     e.g. start_server.sh cpm 27960 lan
# Layout (BASE = the folder of this script, HOME_ROOT = $BASE/homes; set the variables before calling to change them):
#   $BASE/oDFe.ded.x64            the dedicated engine 
#   $BASE/baseq3/pak0.pk3 ..      the Quake 3 data
#   $BASE/defrag/*.pk3            DeFRaG media, maps and zzz-defrag_vm.pk3 (shared by all servers)
#   $HOME_ROOT/<name>/defrag/    that server's own files: <name>.cfg (copy of server/<name>.cfg + common.cfg), games.log,
#                                 q3config_server.cfg, system/checksums
# File names on Linux are case sensitive: baseq3, defrag, pak0.pk3 and the *.cfg names must be lower case.
# lan = unlisted, public = listed on the master servers (the engine's "dedicated" 1 / 2, 1 and 2 are accepted too). There is no default.
# "dedicated" is write protected once the engine runs, so it can only be given on the command line (here), not in a cfg file.
NAME=${1:?server name (cpm, vq3, mixed)}
PORT=${2:?port}
case "${3:?lan or public}" in
	lan|1)    DEDICATED=1 ;;
	public|2) DEDICATED=2 ;;
	*) echo "third argument must be lan or public, not '$3'"; exit 1 ;;
esac
BASE=${BASE:-$(cd "$(dirname "$0")" && pwd)}
HOME_ROOT=${HOME_ROOT:-$BASE/homes}

cd "$BASE" || exit 1
mkdir -p "$HOME_ROOT/$NAME/defrag"
while true; do
	./oDFe.ded.x64 +set fs_basepath "$BASE" +set fs_homepath "$HOME_ROOT/$NAME" +set fs_game defrag \
		+set net_port "$PORT" +set dedicated "$DEDICATED" +exec "$NAME.cfg"
	echo "server $NAME exited ($?), restarting in 5 s (Ctrl+C to stop)"
	sleep 5
done
