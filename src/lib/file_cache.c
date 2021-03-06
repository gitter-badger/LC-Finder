﻿/* ***************************************************************************
* file_cache.c -- file list cache, it used for file list changes detection.
*
* Copyright (C) 2015-2016 by Liu Chao <lc-soft@live.cn>
*
* This file is part of the LC-Finder project, and may only be used, modified,
* and distributed under the terms of the GPLv2.
*
* By continuing to use, modify, or distribute this file you indicate that you
* have read the license and understand and accept it fully.
*
* The LC-Finder project is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GPL v2 for more details.
*
* You should have received a copy of the GPLv2 along with this file. It is
* usually in the LICENSE.TXT file, If not, see <http://www.gnu.org/licenses/>.
* ****************************************************************************/

/* ****************************************************************************
* file_cache.c -- 文件列表的缓存，方便检测文件列表的增删状态。
*
* 版权所有 (C) 2015-2016 归属于 刘超 <lc-soft@live.cn>
*
* 这个文件是 LC-Finder 项目的一部分，并且只可以根据GPLv2许可协议来使用、更改和
* 发布。
*
* 继续使用、修改或发布本文件，表明您已经阅读并完全理解和接受这个许可协议。
*
* LC-Finder 项目是基于使用目的而加以散布的，但不负任何担保责任，甚至没有适销
* 性或特定用途的隐含担保，详情请参照GPLv2许可协议。
*
* 您应已收到附随于本文件的GPLv2许可协议的副本，它通常在 LICENSE 文件中，如果
* 没有，请查看：<http://www.gnu.org/licenses/>.
* ****************************************************************************/

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/font/charset.h>
#include "file_cache.h"
#include "sha1.h"

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define MAX_PATH_LEN	2048
#define FILE_HEAD_TAG	"[LC-Finder Files Cache]"
#define WCSLEN(STR)	(sizeof( STR ) / sizeof( wchar_t ))
#define GetDirStats(T)	(DirStats)(((char*)(T)) + sizeof(SyncTaskRec))


/** 文件夹内的文件变更状态统计 */
typedef struct DirStatsRec_ {
	Dict *files;		/**< 之前已缓存的文件列表 */
	Dict *added_files;	/**< 新增的文件 */
	Dict *deleted_files;	/**< 删除的文件 */
} DirStatsRec, *DirStats;

static const wchar_t *match_suffixs[] = {L".png", L".bmp", L".jpg", L".jpeg"};

static unsigned int Dict_KeyHash( const wchar_t *buf )
{
	unsigned int hash = 5381;
	while( *buf ) {
		hash = ((hash << 5) + hash) + (*buf++);
	}
	return hash;
}

static int Dict_KeyCompare( void *privdata, const void *key1, const void *key2 )
{
	if( wcscmp( key1, key2 ) == 0 ) {
		return 1;
	}
	return 0;
}

static void *Dict_KeyDup( void *privdata, const void *key )
{
	wchar_t *newkey = malloc( (wcslen( key ) + 1)*sizeof( wchar_t ) );
	wcscpy( newkey, key );
	return newkey;
}

static void Dict_KeyDestructor( void *privdata, void *key )
{
	free( key );
}

static DictType DictType_Files = {
	Dict_KeyHash,
	Dict_KeyDup,
	NULL,
	Dict_KeyCompare,
	Dict_KeyDestructor,
	NULL
};

SyncTask SyncTask_New( const char *data_dir, const char *scan_dir )
{
	SyncTask t;
	wchar_t *wcs_data_dir, *wcs_scan_dir;
	size_t len1 = strlen( data_dir ) + 1;
	size_t len2 = strlen( scan_dir ) + 1;
	wcs_data_dir = malloc( sizeof( wchar_t )*len1 );
	wcs_scan_dir = malloc( sizeof( wchar_t )*len2 );
	LCUI_DecodeString( wcs_data_dir, data_dir, len1, ENCODING_UTF8 );
	LCUI_DecodeString( wcs_scan_dir, scan_dir, len2, ENCODING_UTF8 );
	t = SyncTask_NewW( wcs_data_dir, wcs_scan_dir );
	free( wcs_data_dir );
	free( wcs_scan_dir );
	return t;
}

SyncTask SyncTask_NewW( const wchar_t *data_dir, const wchar_t *scan_dir )
{
	int n, len;
	wchar_t *name;
	const wchar_t suffix[] = L".tmp";
	size_t len1 = wcslen( data_dir ) + 1;
	size_t len2 = wcslen( scan_dir ) + 1;
	SyncTask t = malloc( sizeof(SyncTaskRec) + sizeof(DirStatsRec) );
	DirStats ds = GetDirStats( t );
	ds->deleted_files = Dict_Create( &DictType_Files, NULL );
	ds->added_files = Dict_Create( &DictType_Files, NULL );
	ds->files = Dict_Create( &DictType_Files, NULL );
	t->data_dir = malloc( sizeof( wchar_t ) * len1 );
	t->scan_dir = malloc( sizeof( wchar_t ) * len2 );
	wcscpy( t->data_dir, data_dir );
	wcscpy( t->scan_dir, scan_dir );
	name = EncodeSHA1( t->scan_dir );
	n = wcslen( t->data_dir );
	len = n + wcslen( name ) + 2 + WCSLEN( suffix );
	t->tmpfile = malloc( len * sizeof( wchar_t ) );
	t->file = malloc( len * sizeof( wchar_t ) );
	wcscpy( t->tmpfile, t->data_dir );
	if( t->tmpfile[n - 1] != PATH_SEP ) {
		t->tmpfile[n++] = PATH_SEP;
		t->tmpfile[n] = 0;
	}
	wsprintf( t->file, L"%s%s", t->tmpfile, name );
	wsprintf( t->tmpfile, L"%s%s", t->file, suffix );
	t->state = STATE_NONE;
	t->deleted_files = 0;
	t->total_files = 0;
	t->added_files = 0;
	return t;
}

void SyncTask_Delete( SyncTask *tptr )
{
	SyncTask t = *tptr;
	DirStats ds = GetDirStats( t );
	free( t->scan_dir );
	free( t->data_dir );
	free( t->file );
	free( t->tmpfile );
	t->file = NULL;
	t->tmpfile = NULL;
	t->scan_dir = NULL;
	t->data_dir = NULL;
	Dict_Release( ds->files );
	Dict_Release( ds->added_files );
	Dict_Release( ds->deleted_files );
	free( t );
	*tptr = NULL;
}

static int FileDict_ForEach( Dict *d, FileHanlder func, void *data )
{
	int count = 0;
	DictEntry *entry;
	DictIterator *iter = Dict_GetIterator( d );
	while( (entry = Dict_Next( iter )) ) {
		func( data, DictEntry_GetKey( entry ) );
		++count;
	}
	Dict_ReleaseIterator( iter );
	return count;
}

int SyncTask_InAddedFiles( SyncTask t, FileHanlder func, void *func_data )
{
	DirStats ds = GetDirStats( t );
	return FileDict_ForEach( ds->added_files, func, func_data );
}

int SyncTask_InDeletedFiles( SyncTask t, FileHanlder func, void *func_data )
{
	DirStats ds = GetDirStats( t );
	return FileDict_ForEach( ds->deleted_files, func, func_data );
}

/** 从缓存记录中载入文件列表 */
static int SyncTask_LoadCache( SyncTask t, FILE *fp )
{
	wchar_t *path;
	char buf[MAX_PATH_LEN];
	int count = 0, len, size;
	DirStats ds = GetDirStats( t );
	if( !fgets( buf, MAX_PATH_LEN, fp ) ) {
		return 0;
	}
	if( !strstr( buf, FILE_HEAD_TAG ) ) {
		return 0;
	}
	while( !feof( fp ) ) {
		size = fread( &len, sizeof( int ), 1, fp );
		if( size < 1 || len < 1 ) {
			break;
		}
		size = fread( buf, sizeof( wchar_t ), len, fp );
		if( size < len || size > MAX_PATH_LEN ) {
			_DEBUG_MSG("size = %d, len = %d, feof(fp) = %d\n", size, len, feof(fp));
			((wchar_t*)buf)[size] = 0;
			wprintf(L"%s\n", buf);
			break;
		}
		path = (wchar_t*)buf;
		path[len] = 0;
		Dict_Add( ds->files, path, (void*)1 );
		Dict_Add( ds->deleted_files, path, (void*)1 );
		wprintf(L"cached file: %s, len: %d\n", path, len);
		++count;
	}
	t->deleted_files = count;
	return count;
}

/** 扫描文件 */
static int SyncTask_ScanFilesW( SyncTask t, const wchar_t *dirpath, FILE *fp )
{
	LCUI_Dir dir;
	LCUI_DirEntry *entry;
	wchar_t filepath[2048], *name;
	int i, n, len, dir_len = wcslen( dirpath );
	DirStats ds = GetDirStats( t );
	wcscpy( filepath, dirpath );
	if( filepath[dir_len - 1] != PATH_SEP ) {
		filepath[dir_len++] = PATH_SEP;
		filepath[dir_len] = 0;
	}
	LCUI_OpenDir( filepath, &dir );
	n = sizeof( match_suffixs ) / sizeof( *match_suffixs );
	while( (entry = LCUI_ReadDir( &dir )) && t->state == STATE_STARTED ) {
		name = LCUI_GetFileName( entry );
		/* 忽略 . 和 .. 文件夹 */
		if( name[0] == '.' ) {
			if( name[1] == 0 || (name[1] == '.' && name[2] == 0) ) {
				continue;
			}
		}
		wcscpy( filepath + dir_len, name );
		len = wcslen( filepath );
		if( LCUI_FileIsDirectory( entry ) ) {
			SyncTask_ScanFilesW( t, filepath, fp );
			continue;
		}
		if( !LCUI_FileIsArchive( entry ) ) {
			continue;
		}
		for( i = 0; i < n; ++i ) {
			if( wcsstr( name, match_suffixs[i] ) ) {
				break;
			}
		}
		if( i >= n ) {
			continue;
		}
		/* 若该文件路径存在于之前的缓存中，说明未被删除，否则将之
		 * 视为新增的文件。
		 */
		if( Dict_FetchValue( ds->files, filepath ) ) {
			Dict_Delete( ds->deleted_files, filepath );
			--t->deleted_files;
			wprintf(L"unchange: %s\n", filepath);
		} else {
			wprintf(L"added: %s\n", filepath);
			Dict_Add( ds->added_files, filepath, (void*)1 );
			++t->added_files;
		}
		fwrite( &len, sizeof( int ), 1, fp );
		i = fwrite( filepath, sizeof( wchar_t ), len, fp );
		wprintf(L"scan file: %s, len: %d, writed: %d\n", filepath, len, i);
		++t->total_files;
	}
	LCUI_CloseDir( &dir );
	return t->total_files;
}

int SyncTask_Start( SyncTask t )
{
	int n;
	FILE *fp;
	wprintf( L"\n\nscan dir: %s\n", t->scan_dir );
	fp = _wfopen( t->file, L"rb" );
	if( fp ) {
		SyncTask_LoadCache( t, fp );
		fclose( fp );
	}
	fp = _wfopen( t->tmpfile, L"wb" );
	if( !fp ) {
		return -1;
	}
	t->state = STATE_STARTED;
	fputs( FILE_HEAD_TAG"\n", fp );
	n = SyncTask_ScanFilesW( t, t->scan_dir, fp );
	t->state = STATE_FINISHED;
	fclose( fp );
	return n;
}

void SyncTask_Commit( SyncTask t )
{
	_wremove( t->file );
	_wrename( t->tmpfile, t->file );
}

void LCFinder_StopSync( SyncTask t )
{
	t->state = STATE_NONE;
}
