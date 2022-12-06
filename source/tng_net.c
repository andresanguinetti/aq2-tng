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
// tng_net.c - Discord webhook integration for chat relay
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
void cURL_Init(void)
{
    if (curl_global_init(CURL_GLOBAL_NOTHING)) {
            Com_Printf("curl_global_init failed\n");
            return;
        }

        curl_initialized = true;
        Com_Printf("%s initialized.\n", curl_version());
    }

void cURL_Shutdown(void)
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
int cURL_Easy_Send(int payloadType, const char *payload, ...)
{
    int handles = 1;
	va_list argptr;
	char text[151];
    char jsonmsg[1024];
    char url[512];
    char dhost[64];

    /// Sanity checks before we do anything
    // If sv_curl_enable is disabled, take a hike
	if (!sv_curl_enable->value){
		return 0;
	}

    // Check if calling to status API is disabled
    if (strcmp(url,"disabled") == 0) {
        return 0;
    }

    /// So far so good, time to go to work
    // Get a string representation of these cvars
    strcpy(url, sv_curl_status_api_url->string);

    if (payloadType == CURL_STATUS_API) {
        Com_sprintf(jsonmsg, sizeof(jsonmsg), "{\"status\": \"(%s) - %s\"}", dhost, text);
    } else if (payloadType == CURL_DISCORD_CHAT) {
        // Scraping that beautiful data payload
        va_start (argptr, payload);
        vsnprintf (text, sizeof(text), payload, argptr);
        va_end (argptr);

        strcpy(dhost, hostname->string);
        text[strcspn(text, "\n")] = 0;

        Com_sprintf(jsonmsg, sizeof(jsonmsg), "{\"content\": \"```(%s) - %s```\"}", dhost, text);
    } else {
        // Invalid payloadType supplied, do nothing
        gi.dprintf("cURL_Easy_Send error: invalid payloadType %i", payloadType);
        return 0;
    }

	CURL *curl = curl_easy_init();
    CURLM *multi_handle;
    multi_handle = curl_multi_init();

    // Format JSON payload
    

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

        // !!!
        // Debug area //
        // Do not print responses from curl request
        // Comment this line...
        //curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        // ...and uncomment this line for full debug mode
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        // End Debug //
    }
    
    curl_multi_add_handle(multi_handle, curl);
    curl_multi_perform(multi_handle, &handles);
    curl_multi_remove_handle(multi_handle, curl);
    curl_multi_cleanup(multi_handle);
    curl_easy_cleanup(curl);
    return 0;
}