//-----------------------------------------------------------------------------
// TNG File Handling Related Code
//
//-----------------------------------------------------------------------------

#include "g_local.h"
#include "tng_filehandler.h"
#include <time.h>

typedef enum {
    FS_FREE,
    FS_REAL,
    FS_PAK,
#if USE_ZLIB
    FS_ZIP,
    FS_GZ,
#endif
    FS_BAD
} filetype_t;

typedef struct packfile_s {
    char        *name;
    unsigned    namelen;
    unsigned    filepos;
    unsigned    filelen;
#if USE_ZLIB
    unsigned    complen;
    byte        compmtd;    // compression method, 0 (stored) or Z_DEFLATED
    bool        coherent;   // true if local file header has been checked
#endif
    struct packfile_s *hash_next;
} packfile_t;

typedef struct {
    filetype_t  type;       // FS_PAK or FS_ZIP
    unsigned    refcount;   // for tracking pack users
    FILE        *fp;
    unsigned    num_files;
    unsigned    hash_size;
    packfile_t  *files;
    packfile_t  **file_hash;
    char        *names;
    char        *filename;
} pack_t;

typedef struct {
    filetype_t  type;
    unsigned    mode;
    FILE        *fp;
#if USE_ZLIB
    void        *zfp;       // gzFile for FS_GZ or zipstream_t for FS_ZIP
#endif
    packfile_t  *entry;     // pack entry this handle is tied to
    pack_t      *pack;      // points to the pack entry is from
    qboolean        unique;     // if true, then pack must be freed on close
    int         error;      // stream error indicator from read/write operation
    unsigned    rest_out;   // remaining unread length for FS_PAK/FS_ZIP
    int64_t     length;     // total cached file length
} file_t;

size_t Q_vscnprintf(char *dest, size_t size, const char *fmt, va_list argptr)
{
    if (size) {
        size_t ret = Q_vsnprintf(dest, size, fmt, argptr);
        return min(ret, size - 1);
    }

    return 0;
}

// Writing to independent log file

/// Stole all of these from Q2Pro

static qboolean     com_logNewline;
static file_t       fs_files[MAX_FILE_HANDLES];

static file_t *file_for_handle(qhandle_t f)
{
    file_t *file;

    if (f < 1 || f > MAX_FILE_HANDLES)
        return NULL;

    file = &fs_files[f - 1];
    if (file->type == FS_FREE)
        return NULL;

    if (file->type < FS_FREE || file->type >= FS_BAD)
        Com_Error(ERR_FATAL, "%s: bad file type", __func__);

    return file;
}

int FS_Write(const void *buf, size_t len, qhandle_t f)
{
    file_t  *file = file_for_handle(f);

    if (!file)
        return Q_ERR_BADF;

    if ((file->mode & FS_MODE_MASK) == FS_MODE_READ)
        return Q_ERR_INVAL;

    // can't continue after error
    if (file->error)
        return file->error;

    if (len > INT_MAX)
        return Q_ERR_INVAL;

    if (len == 0)
        return 0;

    switch (file->type) {
    case FS_REAL:
        if (fwrite(buf, 1, len, file->fp) != len) {
            file->error = Q_ERR_FAILURE;
            return file->error;
        }
        break;
    default:
        Com_Error(ERR_FATAL, "%s: bad file type", __func__);
    }

    return len;
}

int FS_FCloseFile(qhandle_t f)
{
    file_t *file = file_for_handle(f);
    int ret;

    if (!file)
        return Q_ERR_BADF;

    ret = file->error;
    switch (file->type) {
    case FS_REAL:
        if (fclose(file->fp))
            ret = Q_ERRNO;
        break;
    default:
        ret = Q_ERR_NOSYS;
        break;
    }

    memset(file, 0, sizeof(*file));
    return ret;
}

static void statlogfile_close(void)
{
    if (!com_statlogFile) {
        return;
    }

    Com_Printf("Closing stat log.\n");

    FS_FCloseFile(com_statlogFile);
    com_statlogFile = 0;
}

static void statlogfile_open(void)
{
    char buffer[MAX_OSPATH];
    unsigned mode;
    qhandle_t f;

    mode = statlogfile->value > 1 ? FS_MODE_APPEND : FS_MODE_WRITE;
    // Always write the full stat line, don't buffer
    mode |= FS_BUF_NONE;

    f = FS_EasyOpenFile(buffer, sizeof(buffer), mode | FS_FLAG_TEXT,
                        "stats/", logfile_name->string, ".stats");
    if (!f) {
        gi.cvar_forceset("statlogfile", "0");
        return;
    }

    com_statlogFile = f;
    com_logNewline = true;
    Com_Printf("Logging stats to %s\n", buffer);
}

static void statlogfile_changed(cvar_t *self)
{
    statlogfile_close();
    if (self->value) {
        statlogfile_open();
    }
}

static void statlogfile_write(const char *s)
{
    char text[MAXPRINTMSG];
    char buf[MAX_QPATH];
    char *p, *maxp;
    size_t len;
    int ret;
    int c;

    len = 0;
    p = text;
    maxp = text + sizeof(text) - 1;
    while (*s) {
        if (com_logNewline) {
            if (len > 0 && len < maxp - p) {
                memcpy(p, buf, len);
                p += len;
            }
            com_logNewline = false;
        }

        if (p == maxp) {
            break;
        }

        c = *s++;
        if (c == '\n') {
            com_logNewline = true;
        } else {
            c = Q_charascii(c);
        }

        *p++ = c;
    }
    *p = 0;

    len = p - text;
    ret = FS_Write(text, len, com_statlogFile);
    if (ret != len) {
        // zero handle BEFORE doing anything else to avoid recursion
        qhandle_t tmp = com_statlogFile;
        com_statlogFile = 0;
        FS_FCloseFile(tmp);
        Com_Printf("Couldn't write stat log: %s\n", Q_ErrorString(ret));
        gi.cvar_forceset("statlogfile", "0");
    }
}

#ifndef _WIN32
/*
=============
Com_FlushLogs

When called from SIGHUP handler on UNIX-like systems,
will close and reopen logfile handle for rotation.
=============
*/

void Com_StatFlushLogs(void)
{
    if (statlogfile) {
        statlogfile_changed(statlogfile);
    }
}

#endif

void Com_StatPrintf(const char *fmt, ...)
{
    va_list     argptr;
    char        msg[MAXPRINTMSG];
    size_t      len;

    // may be entered recursively only once
    if (com_printEntered >= 2) {
        return;
    }

    com_printEntered++;

    va_start(argptr, fmt);
    len = Q_vscnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

        // logfile
        if (com_statlogFile) {
            statlogfile_write(msg);
		}
    com_printEntered--;
}

// End log file handling
