// df_version.h -- the DeFRaG version of this project, shared by game, cgame and ui (included from q_shared.h).
// To release a new version change the three numbers and DF_VERSION_NUMBER below.

#ifndef __DF_VERSION_H
#define __DF_VERSION_H

#define DF_VERSION_MAJOR	2
#define DF_VERSION_MINOR	1
#define DF_VERSION_PATCH	0

#define DF_STR_( x )		#x
#define DF_STR( x )			DF_STR_( x )

// "2.1.0": printed by the menu, scoreboard, console banner, time messages and the defrag_version_* cvars
#define DF_VERSION_STRING	DF_STR( DF_VERSION_MAJOR ) "." DF_STR( DF_VERSION_MINOR ) "." DF_STR( DF_VERSION_PATCH )

// 20100: the serverinfo cvar defrag_vers (major * 10000 + minor * 100 + patch). DeFRaG clients parse it as a number and
// compare it with 18100 / 19114 (cg_defrag.c), so it must stay numeric and not drop below 19114.
// (written out because the preprocessor cannot compute the string; the typedef below stops the two from disagreeing)
#define DF_VERSION_NUMBER	20100
#define DF_VERSION_NUMBER_STRING	DF_STR( DF_VERSION_NUMBER )
typedef char df_version_number_matches[ ( DF_VERSION_NUMBER == DF_VERSION_MAJOR * 10000 + DF_VERSION_MINOR * 100 + DF_VERSION_PATCH ) ? 1 : -1 ];

// Suffix after the version in the main menu and in the "Time performed by" line (the original prints " (beta)" / " beta").
#define DF_VERSION_TAG		""

// Protocol constants of DeFRaG 1.91.31, NOT the project version: they feed the hashes that game and cgame exchange (the
// df_auth handshake and the salt of the encoded state words that the demo validation checks), so they have to stay as they
// are for original DeFRaG clients, servers and recorded demos to keep agreeing with this code.
#define DF_PROTOCOL_VERSION_STRING	"1.91.31"
#define DF_PROTOCOL_SALT			19131u

#endif
