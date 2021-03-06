﻿/* ***************************************************************************
* thumb_db.c -- thumbnail database
*
* Copyright (C) 2016 by Liu Chao <lc-soft@live.cn>
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
* thumb_db.c -- 缩略图缓存数据库
*
* 版权所有 (C) 2016 归属于 刘超 <lc-soft@live.cn>
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

#define __THUMBNAIL_CACHE_C__
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include "thumb_db.h"

#define THUMB_MAX_SIZE 8553600

typedef struct ThumbDataBlockRec_ {
	size_t width;
	size_t height;
	size_t mem_size;
	int color_type;
	int modify_time;
} ThumbDataBlockRec, *ThumbDataBlock;

ThumbDB ThumbDB_New( const char *filepath )
{
	int rc;
	ThumbDB cache;
	rc = unqlite_open( &cache, filepath, UNQLITE_OPEN_CREATE );
	if( rc != UNQLITE_OK ) {
		return NULL;
	}
	return cache;
}

ThumbData ThumbDB_Load( ThumbDB cache, const char *filepath )
{
	int rc;
	ThumbData data;
	ThumbDataBlock block;
	unqlite_int64 size;
	rc = unqlite_kv_fetch( cache, filepath, -1, NULL, &size );
	if( rc != UNQLITE_OK ) {
		return NULL;
	}
	block = malloc( (size_t)size );
	data = NEW( ThumbDataRec, 1 );
	Graph_Init( &data->graph );
	data->graph.width = block->width;
	data->graph.height = block->height;
	data->graph.mem_size = block->mem_size;
	data->graph.color_type = block->color_type;
	data->graph.bytes = (uchar_t*)block + sizeof(ThumbDataBlockRec);
	data->modify_time = block->modify_time;
	return data;
}

int ThumbDB_Save( ThumbDB cache, const char *filepath, ThumbData data )
{
	int rc;
	uchar_t *buff;
	ThumbDataBlock block;
	size_t head_size = sizeof( ThumbDataBlockRec );
	size_t size = head_size + data->graph.mem_size;
	if( size > THUMB_MAX_SIZE ) {
		return -1;
	}
	block = malloc( size );
	buff = (uchar_t*)block + head_size;
	block->width = data->graph.width;
	block->height = data->graph.height;
	block->mem_size = data->graph.mem_size;
	block->modify_time = data->modify_time;
	block->color_type = data->graph.color_type;
	memcpy( buff, data->graph.bytes, data->graph.mem_size );
	rc = unqlite_kv_store( cache, filepath, -1, block, size );
	free( block );
	return rc == UNQLITE_OK ? 0 : -2;
}
