/* ***************************************************************************
 * graph_display.c -- graphical display processing
 *
 * Copyright (C) 2012-2013 by
 * Liu Chao
 *
 * This file is part of the LCUI project, and may only be used, modified, and
 * distributed under the terms of the GPLv2.
 *
 * (GPLv2 is abbreviation of GNU General Public License Version 2)
 *
 * By continuing to use, modify, or distribute this file you indicate that you
 * have read the license and understand and accept it fully.
 *
 * The LCUI project is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GPL v2 for more details.
 *
 * You should have received a copy of the GPLv2 along with this file. It is
 * usually in the LICENSE.TXT file, If not, see <http://www.gnu.org/licenses/>.
 * ****************************************************************************/

/* ****************************************************************************
 * graph_display.c -- 图形显示处理
 *
 * 版权所有 (C) 2013 归属于
 * 刘超
 *
 * 这个文件是LCUI项目的一部分，并且只可以根据GPLv2许可协议来使用、更改和发布。
 *
 * (GPLv2 是 GNU通用公共许可证第二版 的英文缩写)
 *
 * 继续使用、修改或发布本文件，表明您已经阅读并完全理解和接受这个许可协议。
 *
 * LCUI 项目是基于使用目的而加以散布的，但不负任何担保责任，甚至没有适销性或特
 * 定用途的隐含担保，详情请参照GPLv2许可协议。
 *
 * 您应已收到附随于本文件的GPLv2许可协议的副本，它通常在LICENSE.TXT文件中，如果
 * 没有，请查看：<http://www.gnu.org/licenses/>.
 * ****************************************************************************/


//#define DEBUG
#include <LCUI_Build.h>
#include LC_LCUI_H
#include LC_GRAPH_H
#include LC_DISPLAY_H
#include LC_WIDGET_H
#include LC_CURSOR_H

#include <time.h>

#ifdef LCUI_BUILD_IN_WIN32
#include <Windows.h>
#endif

#define MS_PER_FRAME 1000/MAX_FRAMES_PER_SEC;	/**< 每帧画面的最少耗时(ms) */

static LCUI_BOOL i_am_init = FALSE;		/**< 标志，指示本模块是否初始化 */
static LinkedList screen_invalid_area;		/**< 屏幕无效区域记录 */
static LCUI_Mutex glayer_list_mutex;		/**< 图层列表的互斥锁 */
static LCUI_Screen screen;			/**< 屏幕信息 */

static int LCUIScreen_Init( int w, int h, int mode )
{
	if( screen.Init ) {
		return screen.Init( w, h, mode );
	}
	return -1;
}

static int LCUIScreen_Destroy( void )
{
	if( screen.Destroy ) {
		return screen.Destroy();
	}
	return -1;
}

static int LCUIScreen_SyncFrameBuffer( void )
{
	if( screen.Sync ) {
		return screen.Sync();
	}
	return -1;
}

LCUI_API int LCUIScreen_CatchGraph( LCUI_Graph *graph, LCUI_Rect rect )
{
	if( screen.CatchGraph ) {
		return screen.CatchGraph( graph, rect );
	}
	return -1;
}

LCUI_API int LCUIScreen_PutGraph( LCUI_Graph *graph, LCUI_Pos pos )
{
	if( screen.PutGraph ) {
		return screen.PutGraph( graph, pos );
	}
	return -1;
}

LCUI_API int LCUIScreen_MixGraph( LCUI_Graph *graph, LCUI_Pos pos )
{
	if( screen.MixGraph ) {
		return screen.MixGraph( graph, pos );
	}
	return -1;
}

LCUI_API int LCUIScreen_SetMode( int w, int h, int mode )
{
	if( screen.PutGraph ) {
		return screen.SetMode( w, h, mode );
	}
	return -1;
}

/** 获取屏幕宽度 */
LCUI_API int LCUIScreen_GetWidth( void )
{
	return screen.info.size.w;
}

/** 获取屏幕高度 */
LCUI_API int LCUIScreen_GetHeight( void )
{
	return screen.info.size.h;
}

/** 获取屏幕尺寸 */
LCUI_API void LCUIScreen_GetSize( LCUI_Size *size )
{
	*size = screen.info.size;
}

/** 设置屏幕内的指定区域为无效区域 */
LCUI_API int LCUIScreen_InvalidateArea( LCUI_Rect *rect )
{
	LCUI_Rect tmp_rect;
	if( !i_am_init ) {
		return -1;
	}
	if( !rect ) {
		tmp_rect.x = tmp_rect.y = 0;
		tmp_rect.w = screen.info.size.w;
		tmp_rect.h = screen.info.size.h;
		return DirtyRectList_Add( &screen_invalid_area, &tmp_rect );
	} 
	tmp_rect = *rect;
	DEBUG_MSG("rect: %d,%d,%d,%d\n", rect->x, rect->y, rect->w, rect->h);
	LCUIRect_ValidateArea( &tmp_rect, screen.info.size );
	return DirtyRectList_Add( &screen_invalid_area, &tmp_rect );
}

/** 获取屏幕每个像素点的色彩值所占的位数 */
LCUI_API int LCUIScreen_GetBits( void )
{
	return screen.info.bits;
}

/** 获取屏幕显示模式 */
LCUI_API int LCUIScreen_GetMode( void )
{
	return screen.info.mode;
}

/** 获取屏幕信息 */
LCUI_API void LCUIScreen_GetInfo( LCUI_ScreenInfo *info )
{
	*info = screen.info;
}

/** 设置屏幕信息 */
LCUI_API void LCUIScreen_SetInfo( LCUI_ScreenInfo *info )
{
	screen.info = *info;
}

/** 获取屏幕中心点的坐标 */
LCUI_API LCUI_Pos LCUIScreen_GetCenter( void )
{
	LCUI_Pos pos;
	pos.x = screen.info.size.w/2.0; 
	pos.y = screen.info.size.h/2.0;
	return pos;
}

/** 为图层树锁上互斥锁 */
LCUI_API void LCUIScreen_LockGraphLayerTree( void )
{
	LCUIMutex_Lock( &glayer_list_mutex );
}

/** 解除图层树互斥锁 */
LCUI_API void LCUIScreen_UnlockGraphLayerTree( void )
{
	LCUIMutex_Unlock( &glayer_list_mutex );
}

/** 获取屏幕中指定区域内实际要显示的图形 */
LCUI_API void LCUIScreen_GetRealGraph( LCUI_Rect rect, LCUI_Graph *graph )
{
	LCUI_Pos pos, cursor_pos;
	/* 设置互斥锁，避免在统计图层时，图层记录被其它线程修改 */
	LCUIScreen_LockGraphLayerTree();
	GraphLayer_GetGraph( RootWidget_GetGraphLayer(), graph, rect );
	LCUIScreen_UnlockGraphLayerTree();
	/* 如果游标不可见 */
	if ( !LCUICursor_IsVisible() ) {
		return;
	}
	/* 如果该区域与游标的图形区域重叠 */
	if ( LCUICursor_IsCoverRect( rect ) ) {
		LCUICursor_GetPos( &cursor_pos );
		pos.x = cursor_pos.x - rect.x;
		pos.y = cursor_pos.y - rect.y;
		/* 将鼠标游标的图形混合至当前图形里 */
		LCUICursor_MixGraph( graph, pos );
	}
}

#ifdef LCUI_BUILD_IN_WIN32
static void Win32_Clinet_InvalidArea( LCUI_Rect rect )
{
	HWND hWnd;
	RECT win32_rect;

	hWnd = Win32_GetSelfHWND();
	win32_rect.left = rect.x;
	win32_rect.top = rect.y;
	win32_rect.right = rect.x + rect.width;
	win32_rect.bottom = rect.y + rect.height;
	InvalidateRect( hWnd, &win32_rect, FALSE );
}
#endif

/** 更新无效区域内的图像 */
static int LCUIScreen_UpdateInvalidArea(void)
{
	int ret = 0;
	LCUI_Rect *p_rect;
	LCUI_Graph graph;

	DEBUG_MSG("enter, rect num: %d\n", screen_invalid_area.used_node_num);
	Graph_Init( &graph );
	LinkedList_Goto( &screen_invalid_area, 0 );
	while( i_am_init && !LinkedList_IsAtEnd(&screen_invalid_area) ) {
		p_rect = (LCUI_Rect*)LinkedList_Get( &screen_invalid_area );
		if( !p_rect ) {
			break;
		}
		ret = 1;
		DEBUG_MSG("get rect: %d,%d,%d,%d\n", p_rect->x, p_rect->y, p_rect->w, p_rect->h);
		/* 获取内存中对应区域的图形数据 */
		LCUIScreen_GetRealGraph( *p_rect, &graph );
		//Graph_FillColor( &graph, RGB(255,0,0) );
		//Graph_DrawBorder( &graph, Border(1,BORDER_STYLE_SOLID, RGB(255,0,0)) );
		/* 写入至帧缓冲，让屏幕显示图形 */
		LCUIScreen_PutGraph( &graph, Pos(p_rect->x, p_rect->y) );
		LinkedList_Delete( &screen_invalid_area );
	}
	DEBUG_MSG("quit\n");
	Graph_Free( &graph );
	return ret;
}

static FrameCtrlCtx fc_ctx;

/** 获取当前的屏幕内容每秒更新的帧数 */
LCUI_API int LCUIScreen_GetFPS(void)
{
	return fc_ctx.current_fps;
}

/** 更新屏幕内的图形显示 */
static void LCUIScreen_Update( void* unused )
{
	int ret;
	/* 先标记刷新整个屏幕区域 */
	LCUIScreen_InvalidateArea( NULL );
	/* 初始化帧数控制 */
	FrameControl_Init( &fc_ctx );
	FrameControl_SetMaxFPS( &fc_ctx, 1000/MAX_FRAMES_PER_SEC );
	while( LCUI_Sys.state == ACTIVE ) {
		/* 更新鼠标位置 */
		LCUICursor_UpdatePos();
		/* 处理所有部件消息 */
		LCUIWidget_ProcMessage();
		/* 更新各个部件的无效区域中的内容 */
		ret = LCUIWidget_ProcInvalidArea();
		/* 更新屏幕上各无效区域内的图像内容 */
		ret |= LCUIScreen_UpdateInvalidArea();
		if( ret > 0 ) {
			LCUIScreen_SyncFrameBuffer();
		}
		/* 让本帧停留一段时间 */
		FrameControl_Remain( &fc_ctx );
	}
	LCUIThread_Exit(NULL);
}

/** 初始化图形输出模块 */
int LCUIModule_Video_Init( int w, int h, int mode )
{
	if( i_am_init ) {
		return -1;
	}
	LCUIMutex_Init( &glayer_list_mutex );
#ifdef LCUI_BUILD_IN_WIN32
	LCUIScreen_UseWin32( &screen );
#endif
	LCUIScreen_Init( w, h, mode );
	i_am_init = TRUE;
	DirtyRectList_Init( &screen_invalid_area );
	return _LCUIThread_Create( &LCUI_Sys.display_thread,
			LCUIScreen_Update, NULL );
}

/** 停用图形输出模块 */
int LCUIModule_Video_End( void )
{
	if( !i_am_init ) {
		return -1;
	}
	LCUIScreen_Destroy();
	i_am_init = FALSE;
	DirtyRectList_Destroy( &screen_invalid_area );
	return _LCUIThread_Join( LCUI_Sys.display_thread, NULL );
}
