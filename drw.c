/* See LICENSE file for copyright and license details. */
#if USE_WINAPI
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT			0x0500
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if USE_XLIB
#include <X11/Xlib.h>
#elif USE_WINAPI
#include <windows.h>
#include <winuser.h>
#endif

#include "drw.h"
#include "util.h"

Drw *
drw_create(Display *dpy, int screen, Window root, unsigned int w, unsigned int h) {
	Drw *drw = (Drw *)calloc(1, sizeof(Drw));
	if(!drw)
		return NULL;
	drw->dpy = dpy;
	drw->screen = screen;
	drw->root = root;
	drw->w = w;
	drw->h = h;
#if USE_XLIB
	drw->drawable = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
	drw->gc = XCreateGC(dpy, root, 0, NULL);
	XSetLineAttributes(dpy, drw->gc, 1, LineSolid, CapButt, JoinMiter);
#elif USE_WINAPI
	drw->gc = GetWindowDC(root);
	drw->drawable = CreateCompatibleDC(drw->gc);
    drw->hbmp = CreateCompatibleBitmap(drw->gc, w, h);
    SelectObject(drw->drawable, drw->hbmp);
#endif
	return drw;
}

void
drw_resize(Drw *drw, unsigned int w, unsigned int h) {
	if(!drw)
		return;
	drw->w = w;
	drw->h = h;
#if USE_XLIB
	if(drw->drawable != 0)
		XFreePixmap(drw->dpy, drw->drawable);
	drw->drawable = XCreatePixmap(drw->dpy, drw->root, w, h, DefaultDepth(drw->dpy, drw->screen));
#elif USE_WINAPI
	if (drw->drawable)
	{
		DeleteObject(drw->hbmp);
		DeleteDC(drw->drawable);
	}
	drw->w = w;
	drw->h = h;
	drw->drawable = CreateCompatibleDC(drw->gc);
    drw->hbmp = CreateCompatibleBitmap(drw->gc, w, h);
    SelectObject(drw->drawable, drw->hbmp);	
#endif
}

void
drw_free(Drw *drw) {
#if USE_XLIB
	XFreePixmap(drw->dpy, drw->drawable);
	XFreeGC(drw->dpy, drw->gc);
#elif USE_WINAPI
	if (drw->font->font)
		DeleteObject(drw->font->font);
	if (drw->drawable)
	{
		DeleteObject(drw->hbmp);
		DeleteDC(drw->drawable);
	}
#endif
	free(drw);
}

Fnt *
#if USE_XLIB
drw_font_create(Display *dpy, const char *fontname) {
#elif USE_WINAPI
drw_font_create(Display *dpy, const char *fontname, HWND canvas) {
#endif
	Fnt *font;
	char *def, **missing;
	int n;

	font = (Fnt *)calloc(1, sizeof(Fnt));
	if(!font)
		return NULL;
#if USE_XLIB
	font->set = XCreateFontSet(dpy, fontname, &missing, &n, &def);
	if(missing) {
		while(n--)
			fprintf(stderr, "drw: missing fontset: %s\n", missing[n]);
		XFreeStringList(missing);
	}
	if(font->set) {
		XFontStruct **xfonts;
		char **font_names;

		XExtentsOfFontSet(font->set);
		n = XFontsOfFontSet(font->set, &xfonts, &font_names);
		while(n--) {
			font->ascent = MAX(font->ascent, (*xfonts)->ascent);
			font->descent = MAX(font->descent,(*xfonts)->descent);
			xfonts++;
		}
	}
	else {
		if(!(font->xfont = XLoadQueryFont(dpy, fontname))
		&& !(font->xfont = XLoadQueryFont(dpy, "fixed")))
			die("error, cannot load font: '%s'\n", fontname);
		font->ascent = font->xfont->ascent;
		font->descent = font->xfont->descent;
	}
#elif USE_WINAPI
	if ((font->font = CreateFont(10,0,0,0,0,0,0,0,0,0,0,0,0,TEXT(fontname))))
	{
		HDC hdc = GetDC(canvas);
		SelectObject(hdc, font->font);
		GetTextMetrics(hdc, &font->tm);
		font->ascent = font->tm.tmAscent;
		font->descent = font->tm.tmDescent;
		font->h = font->tm.tmHeight;
	}
#endif
	font->h = font->ascent + font->descent;
	return font;
}

void
drw_font_free(Display *dpy, Fnt *font) {
	if(!font)
		return;
#if USE_XLIB
	if(font->set)
		XFreeFontSet(dpy, font->set);
	else
		XFreeFont(dpy, font->xfont);
#elif USE_WINAPI
	if (font->font)
		DeleteObject(font->font);
#endif
	free(font);
}

Clr *
drw_clr_create(Drw *drw, const char *clrname) {
	Clr *clr;
#if USE_XLIB
	Colormap cmap;
	XColor color;
#endif

	if(!drw)
		return NULL;
	clr = (Clr *)calloc(1, sizeof(Clr));
	if(!clr)
		return NULL;
#if USE_XLIB
	cmap = DefaultColormap(drw->dpy, drw->screen);
	if(!XAllocNamedColor(drw->dpy, cmap, clrname, &color, &color))
		die("error, cannot allocate color '%s'\n", clrname);
	clr->rgb = color.pixel;
#elif USE_WINAPI
	unsigned int r,g,b;
	sscanf(clrname, "#%2x%2x%2x", &r,&g,&b);
	clr->rgb = RGB((BYTE)r,(BYTE)g,(BYTE)b); /* bbggrr */
#endif
	return clr;
}

void
drw_clr_free(Clr *clr) {
	if(clr)
		free(clr);
}

void
drw_setfont(Drw *drw, Fnt *font) {
	if(drw)
		drw->font = font;
}

void
drw_setscheme(Drw *drw, ClrScheme *scheme) {
	if(drw && scheme) 
		drw->scheme = scheme;
}

void
drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h, int filled, int empty, int invert) {
	int dx;

	if(!drw || !drw->font || !drw->scheme)
		return;
#if USE_XLIB
	XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme->bg->rgb : drw->scheme->fg->rgb);
#elif USE_WINAPI
	SetDCBrushColor(drw->drawable, invert ? drw->scheme->bg->rgb : drw->scheme->fg->rgb); /* bbggrr */
#endif
	dx = (drw->font->ascent + drw->font->descent + 2) / 4;
	if(filled)
#if USE_XLIB
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x+1, y+1, dx+1, dx+1);
#elif USE_WINAPI
	{
		RECT r;
		HBRUSH hbr = CreateSolidBrush(invert ? drw->scheme->fg->rgb : drw->scheme->bg->rgb); /* bbggrr */
		SetRect(&r, x+1, y+1, x+1 + dx+1, y+1 + dx+1);
		FillRect(drw->drawable, &r, hbr);
	}
#endif
	else if(empty)
#if USE_XLIB
		XDrawRectangle(drw->dpy, drw->drawable, drw->gc, x+1, y+1, dx, dx);
#elif USE_WINAPI
		Rectangle(drw->drawable, x+1, y+1, x+1 + dx, y+1 + dx);
#endif
}

void
drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h, const char *text, int invert) {
	char buf[256];
	int i, tx, ty, th, len, olen;
	Extnts tex;

	if(!drw || !drw->scheme)
		return;
#if USE_XLIB
	XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme->fg->rgb : drw->scheme->bg->rgb);
	XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
#elif USE_WINAPI
	{
		RECT r;
		HBRUSH hbr = CreateSolidBrush(invert ? drw->scheme->fg->rgb : drw->scheme->bg->rgb); /* bbggrr */
		SetRect(&r, x, y, x + w, y + h);
		FillRect(drw->drawable, &r, hbr);
	}
#endif
	if(!text || !drw->font)
		return;
	olen = strlen(text);
#if USE_XLIB
	drw_font_getexts(drw->font, text, olen, &tex);
	th = drw->font->ascent + drw->font->descent;
	ty = y + (h / 2) - (th / 2) + drw->font->ascent;
	tx = x + (h / 2);
#elif USE_WINAPI
	drw_font_getexts(drw, drw->font, text, olen, &tex);
	th = drw->font->ascent + drw->font->descent;
	ty = y - (th / 2) + drw->font->ascent;
	tx = x + (th / 2);
#endif
	/* shorten text if necessary */
	for(len = MIN(olen, sizeof buf); len && (tex.w > w - tex.h || w < tex.h); len--)
#if USE_XLIB	
		drw_font_getexts(drw->font, text, len, &tex);
#elif USE_WINAPI
		drw_font_getexts(drw, drw->font, text, len, &tex);
#endif
	if(!len)
		return;
	memcpy(buf, text, len);
	if(len < olen)
		for(i = len; i && i > len - 3; buf[--i] = '.');
#if USE_XLIB
	XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme->bg->rgb : drw->scheme->fg->rgb);
	if(drw->font->set)
		XmbDrawString(drw->dpy, drw->drawable, drw->font->set, drw->gc, tx, ty, buf, len);
	else
		XDrawString(drw->dpy, drw->drawable, drw->gc, tx, ty, buf, len);
#elif USE_WINAPI
	SelectObject(drw->drawable, drw->font->font);
	SetTextColor(drw->drawable, invert ? drw->scheme->bg->rgb : drw->scheme->fg->rgb);
	SetBkColor(drw->drawable, invert ? drw->scheme->fg->rgb : drw->scheme->bg->rgb);
	TextOut(drw->drawable, tx, ty, TEXT(buf), len);
#endif
}

void
drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h) {
	if(!drw)
		return;
#if USE_XLIB
	XCopyArea(drw->dpy, drw->drawable, win, drw->gc, x, y, w, h, x, y);
	XSync(drw->dpy, False);
#elif USE_WINAPI
	BitBlt(drw->gc, x, y, w, h, drw->drawable, x, y, SRCCOPY);
#endif
}


void
#if USE_XLIB		
drw_font_getexts(Fnt *font, const char *text, unsigned int len, Extnts *tex) {
	XRectangle r;
#elif USE_WINAPI
drw_font_getexts(Drw *drw, Fnt *font, const char *text, unsigned int len, Extnts *tex) {
#endif

	if(!font || !text)
		return;
#if USE_XLIB		
	if(font->set) {
		XmbTextExtents(font->set, text, len, NULL, &r);
		tex->w = r.width;
		tex->h = r.height;
	}
	else {
		tex->h = font->ascent + font->descent;
		tex->w = XTextWidth(font->xfont, text, len);
	}
#elif USE_WINAPI
	if (font->font)
	{
		RECT r;
		SelectObject(drw->drawable, font->font);
		GetTextMetrics(drw->drawable, &font->tm);
		DrawText(drw->drawable,text,len,&r,DT_CALCRECT);
		tex->w = r.right - r.left;
		tex->h = r.bottom - r.top;
	}
#endif
}

unsigned int
#if USE_XLIB
drw_font_getexts_width(Fnt *font, const char *text, unsigned int len) {
#elif USE_WINAPI
drw_font_getexts_width(Drw *drw, Fnt *font, const char *text, unsigned int len) {
#endif
	Extnts tex;

	if(!font)
		return -1;
	tex.w = 0;
	tex.h = 0;
#if USE_XLIB
	drw_font_getexts(font, text, len, &tex);
#elif USE_WINAPI
	drw_font_getexts(drw, font, text, len, &tex);
#endif
	return tex.w;
}

Cur *
drw_cur_create(Drw *drw, int shape) {
	Cur *cur = (Cur *)calloc(1, sizeof(Cur));

	if(!drw || !cur)
		return NULL;
#if USE_XLIB		
	cur->cursor = XCreateFontCursor(drw->dpy, shape);
#elif USE_WINAPI
	cur->cursor = NULL;
#endif
	return cur;
}

void
drw_cur_free(Drw *drw, Cur *cursor) {
	if(!drw || !cursor)
		return;
#if USE_XLIB		
	XFreeCursor(drw->dpy, cursor->cursor);
#elif USE_WINAPI
	cursor->cursor = NULL;
#endif
	free(cursor);
}
