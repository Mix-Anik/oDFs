#include "g_local.h"

/* Stub implementations — bot AI is not included in this build */

void     G_InitBots( qboolean restart )            { (void)restart; }
void     G_RemoveQueuedBotBegin( int clientNum )   { (void)clientNum; }
qboolean G_BotConnect( int clientNum, qboolean restart ) { (void)clientNum; (void)restart; return qfalse; }
void     Svcmd_AddBot_f( void )                    {}
void     Svcmd_BotList_f( void )                   {}
void     BotInterbreedEndMatch( void )             {}

int      BotAISetup( int restart )                 { (void)restart; return 0; }
int      BotAIShutdown( int restart )              { (void)restart; return 0; }
int      BotAILoadMap( int restart )               { (void)restart; return 0; }
int      BotAISetupClient( int client, struct bot_settings_s *settings, qboolean restart ) { (void)client; (void)settings; (void)restart; return 0; }
int      BotAIShutdownClient( int client, qboolean restart ) { (void)client; (void)restart; return 0; }
int      BotAIStartFrame( int time )               { (void)time; return 0; }
void     BotTestAAS( vec3_t origin )               { (void)origin; }
