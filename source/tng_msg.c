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
// tng_net.c - CURL integration for various uses
//

#include "g_local.h"

cvar_t* logfile_name;
void Write_MsgToLog(const char logType, const char* msg, ...)
{
	va_list	argptr;
	char	msg_cpy[1024];
	char	logpath[MAX_QPATH];
	FILE* 	f;

	// Do nothing if logfile_msgs is 0
	if(!logfile_msgs->value)
		return;

	va_start(argptr, msg);
	vsprintf(msg_cpy, msg, argptr);
	va_end(argptr);

	logfile_name = gi.cvar("logfile_name", "", CVAR_NOSET);

	// Log to the appropriate file, stats go in one, everything else to the other
	if (Q_stricmp(logType, "stats") == 0) {
		sprintf(logpath, "action/logs/%s.%s", logfile_name->string, "stats");
	} else {
		sprintf(logpath, "action/logs/%s.%s", logfile_name->string, "srv");
	}

	if ((f = fopen(logpath, "a")) != NULL)
	{
		fprintf(f, "%s", msg_cpy);
		fclose(f);
	}
	else
		gi.dprintf("Write_MsgToLog: Error writing to %s\n", logfile_name->string);

}