//
//sv_discord.c
//
void Discord_HTTP_Init(void);
void Discord_HTTP_Shutdown(void);
int HTTP_Discord_Webhook(const char *payload, ...);
