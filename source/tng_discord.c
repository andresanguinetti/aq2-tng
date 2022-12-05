/*

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

//
// sv_discord.c - Discord webhook integration for chat relay
//

#include "g_local.h"
#include <curl/curl.h>

static qboolean     curl_initialized;

/*
===============
HTTP_Init

Init libcurl and multi handle.
===============
*/
void Discord_HTTP_Init(void)
{
    if (curl_global_init(CURL_GLOBAL_NOTHING)) {
            Com_Printf("curl_global_init failed\n");
            return;
        }

        curl_initialized = true;
        Com_Printf("%s initialized.\n", curl_version());
    }

void Discord_HTTP_Shutdown(void)
{
    if (!curl_initialized)
        return;

    curl_global_cleanup();
    curl_initialized = false;
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}
int HTTP_Discord_Webhook(const char *payload, ...)
{
    int result = 1;
	va_list argptr;
	char text[151];
    char jsonmsg[151];
    char webhook_url[512];
    char dhost[128];
    long responseCode;

    strcpy(webhook_url, sv_webhook_discord_url->string);
    strcpy(dhost, hostname->string);

	// If sv_webhook_discord is disabled
	if (!sv_webhook_discord->value || strcmp(webhook_url,"disabled") == 0) {
		return 0;
	}

	va_start (argptr, payload);
	vsnprintf (text, sizeof(text), payload, argptr);
	va_end (argptr);

	CURL *curl = curl_easy_init();
    CURLM *multi_handle;
    multi_handle = curl_multi_init();

    // Format JSON payload
    text[strcspn(text, "\n")] = 0;
    Com_sprintf(jsonmsg, sizeof(jsonmsg), "{\"content\": \"```(%s) - %s```\"}", dhost, text);

    if(curl && multi_handle) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Expect:");
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Retain DNS info so we don't waste time doing lookups constantly
        curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 2L);
        //

        curl_easy_setopt(curl, CURLOPT_URL, webhook_url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonmsg);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);


        // !!!
        // Debug area //
        // Do not print responses from curl request
        // Comment this line...
        //curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        // ...and uncomment this line for full debug mode
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        // End Debug //
        
        while(result) {
            curl_multi_add_handle(multi_handle, curl);
            CURLMcode mc = curl_multi_perform(multi_handle, &result);

            if(result)
            /* wait for activity, timeout or "nothing" */
            mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);

            if(mc)
                break;
            }
        curl_multi_remove_handle(multi_handle, curl);
        curl_multi_cleanup(multi_handle);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    return (int) result;
}