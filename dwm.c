/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance.  Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag.  Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#if USE_WINAPI
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT			0x0500
#endif

#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#if USE_XLIB
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#elif USE_WINAPI
#include <windows.h>
#include <winuser.h>
#include <shellapi.h>
#include <stdlib.h>
#define NAME					"dwm-win32" 	/* Used for window name/class */
#endif

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#if USE_XLIB
#define TEXTW(X)                (drw_font_getexts_width(drw->font, X, strlen(X)) + drw->font->h)
#elif USE_WINAPI
#define TEXTW(X)                (drw_font_getexts_width(drw, drw->font, X, strlen(X)) + drw->font->h)
#endif

/* for portibility */
#if USE_WINAPI
#define Window HWND
#define XWindowAttributes int
#define True TRUE
#define False FALSE
#define Bool BOOL
#define bool BOOL
#define Window HWND
#define Atom int
#define Display void*
#define true TRUE
#define false FALSE
#define XA_WM_NAME 0
#define XA_WINDOW 0
#define XA_ATOM 0
#define XA_STRING 0
#define None 0
#endif

#if USE_WINAPI
/* Shell hook stuff */

typedef BOOL (*RegisterShellHookWindowProc) (HWND);
RegisterShellHookWindowProc _RegisterShellHookWindow;
#endif

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeLast }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	Bool isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	Bool isalive;
	Bool wasvisible;
	Bool ignore;
	Bool isminimized;
	Bool border;
	Client *next;
	Client *snext;
	Monitor *mon;
#if USE_WINAPI
	HWND hwnd;
	HWND parent;
	HWND root;
	DWORD threadid;
#endif
	Window win;
};

typedef struct {
	unsigned int mod;
#if USE_XLIB
	KeySym keysym;
#elif USE_WINAPI
	DWORD keysym;
#endif
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	Bool showbar;
	Bool topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	Bool isfloating;
	int monitor;
} Rule;

/* function declarations */
static void applyrules(Client *c);
static Bool applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
#if USE_XLIB
static void buttonpress(XEvent *e);
#endif
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clearurgent(Client *c);
#if USE_XLIB
static void clientmessage(XEvent *e);
#endif
static void configure(Client *c);
#if USE_XLIB
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
#endif
static Monitor *createmon(void);
#if USE_XLIB
static void destroynotify(XEvent *e);
#endif
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
#if USE_XLIB
static void enternotify(XEvent *e);
static void expose(XEvent *e);
#endif
static void focus(Client *c);
#if USE_XLIB
static void focusin(XEvent *e);
#endif
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Bool getrootptr(int *x, int *y);
static long getstate(Window w);
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, Bool focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
#if USE_XLIB
static void keypress(XEvent *e);
#elif USE_WINAPI
static void keypress(WPARAM wParam);
#endif
static void killclient(const Arg *arg);
#if USE_WINAPI
static Client * managechildwindows(Client *p);
#endif
#if USE_XLIB
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
#elif USE_WINAPI
static Client *manage(Window w, XWindowAttributes *wa);
#endif
static void monocle(Monitor *m);
#if USE_XLIB
static void motionnotify(XEvent *e);
#endif
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *);
#if USE_XLIB
static void propertynotify(XEvent *e);
#endif
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, Bool interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
#if USE_XLIB
static void scan(void);
#elif USE_WINAPI
static BOOL CALLBACK scan(HWND hwnd, LPARAM lParam);
#endif
static Bool sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, Bool fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
#if USE_XLIB
static void setup(void);
#elif USE_WINAPI
static void setup(HINSTANCE hInstance);
#endif
#if USE_WINAPI
void setupbar(HINSTANCE hInstance);
#endif
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
#if USE_WINAPI
static void toggleborder(const Arg *arg);
static void toggleexplorer(const Arg *arg);
#endif
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, Bool setfocus);
static void unmanage(Client *c, Bool destroyed);
#if USE_XLIB
static void unmapnotify(XEvent *e);
#endif
static Bool updategeom(void);
#if USE_WINAPI
static void updatebar(void);
#endif
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatewindowtype(Client *c);
static void updatetitle(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
#if USE_XLIB
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
#endif
static void zoom(const Arg *arg);
#if USE_WINAPI
static Client *getclient(HWND hwnd);
static LPSTR getclienttitle(HWND hwnd);
static HWND getroot(HWND hwnd);
static void setselected(Client *c);
static void setvisibility(HWND hwnd, bool visibility);
#endif

/* variables */
#if USE_WINAPI
static UINT shellhookid;	/* Window Message id */
static HWND dwmhwnd, barhwnd;
#endif
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
#if USE_XLIB
static int (*xerrorxlib)(Display *, XErrorEvent *);
#endif
static unsigned int numlockmask = 0;
#if USE_XLIB
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress, /* important */
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress, /* important */
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
#endif
static Atom wmatom[WMLast], netatom[NetLast];
static Bool running = True;
static Cur *cursor[CurLast];
static ClrScheme scheme[SchemeLast];
static Display *dpy;
static Drw *drw;
static Fnt *fnt;
static Monitor *mons, *selmon;
static Window root;
#if USE_WINAPI
static HWND dwmhwnd, barhwnd;
//static int wx, wy, ww, wh; /* window area geometry x, y, width, height, bar excluded */
static int sx, sy, sw, sh; /* X display screen geometry x, y, width, height */ 

void
eprint(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	fflush(stderr);
	va_end(ap);
}

#ifdef NDEBUG
# define debug(format, args...) do { } while(false)
#else
# define debug eprint
#endif
#endif

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };


#if USE_WINAPI
void
setselected(Client *c) {
	if(!c || !ISVISIBLE(c))
		for(c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if(c->mon->sel && c->mon->sel != c)
		drawborder(c->mon->sel, normbordercolor);
	if(c) {
		if(c->isurgent)
			clearurgent(c);
		detachstack(c);
		attachstack(c);
		drawborder(c, selbordercolor);
		c->mon->sel = c;
		drawbar(c->mon);
	}
}
#endif

#if USE_WINAPI
void
setborder(Client *c, bool border) {
	if (border) {
		SetWindowLong(c->hwnd, GWL_STYLE, (GetWindowLong(c->hwnd, GWL_STYLE) | (WS_CAPTION | WS_SIZEBOX)));
	} else {		
		/* XXX: ideally i would like to use the standard window border facilities and just modify the 
		 *      color with SetSysColor but this only seems to work if we leave WS_SIZEBOX enabled which
		 *      is not optimal.
		 */
		SetWindowLong(c->hwnd, GWL_STYLE, (GetWindowLong(c->hwnd, GWL_STYLE) & ~(WS_CAPTION | WS_SIZEBOX)) | WS_BORDER | WS_THICKFRAME);
		SetWindowLong(c->hwnd, GWL_EXSTYLE, (GetWindowLong(c->hwnd, GWL_EXSTYLE) & ~(WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE)));
	}
	SetWindowPos(c->hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER );
	c->border = border;
}
#endif

#if USE_WINAPI
void
drawborder(Client *c, COLORREF color) {
#if 0
	HDC hdc = GetWindowDC(c->hwnd);

#if 0
	/* this would be another way, but it uses standard sytem colors */
	RECT area = { .left = 0, .top = 0, .right = c->w, .bottom = c->h };
	DrawEdge(hdc, &area, BDR_RAISEDOUTER | BDR_SUNKENINNER, BF_RECT);
#else
	HPEN pen = CreatePen(PS_SOLID, borderpx, color);
	SelectObject(hdc, pen);
	MoveToEx(hdc, 0, 0, NULL);
	LineTo(hdc, c->w, 0);
	LineTo(hdc, c->w, c->h);
	LineTo(hdc, 0, c->h);
	LineTo(hdc, 0, 0);
	DeleteObject(pen);
#endif

	ReleaseDC(c->hwnd, hdc);
#endif
}
#endif

#if USE_WINAPI
LPSTR
getclientclassname(HWND hwnd) {
	static TCHAR buf[128];
	GetClassName(hwnd, buf, sizeof buf);
	return buf;
}

LPSTR
getclienttitle(HWND hwnd) {
	static TCHAR buf[128];
	GetWindowText(hwnd, buf, sizeof buf);
	return buf;
}
#endif

/* function implementations */
void
applyrules(Client *c) {
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	LPSTR cn, ct;
#if USE_XLIB
	XClassHint ch = { NULL, NULL };
#elif USE_WINAPI
#endif

	/* rule matching */
	c->isfloating = c->tags = 0;
#if USE_XLIB
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;
#elif USE_WINAPI
	class    = (cn = getclientclassname(c->win)) ? cn : broken;
	instance = (ct = getclienttitle(c->win))     ? ct : broken;
#endif

	for(i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			for(m = mons; m && m->num != r->monitor; m = m->next);
			if(m)
				c->mon = m;
		}
	}
#if USE_XLIB
	if(ch.res_class)
		XFree(ch.res_class);
	if(ch.res_name)
		XFree(ch.res_name);
#elif USE_WINAPI
#endif
	c->tags = c->mon->tagset[c->mon->seltags];
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

Bool
applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact) {
	Bool baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if(interact) {
		if(*x > sw)
			*x = sw - WIDTH(c);
		if(*y > sh)
			*y = sh - HEIGHT(c);
		if(*x + *w + 2 * c->bw < 0)
			*x = 0;
		if(*y + *h + 2 * c->bw < 0)
			*y = 0;
	}
	else {
		if(*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if(*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if(*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if(*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if(*h < bh)
		*h = bh;
	if(*w < bh)
		*w = bh;
	if(resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if(!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if(c->mina > 0 && c->maxa > 0) {
			if(c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if(c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if(baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if(c->incw)
			*w -= *w % c->incw;
		if(c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if(c->maxw)
			*w = MIN(*w, c->maxw);
		if(c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m) {
	if(m)
		showhide(m->stack);
	else for(m = mons; m; m = m->next)
		showhide(m->stack);
	if(m) {
		arrangemon(m);
		restack(m);
	} else for(m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m) {
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if(m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c) {
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachstack(Client *c) {
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

#if USE_XLIB
void
buttonpress(XEvent *e) {
	unsigned int i, x, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, True);
		selmon = m;
		focus(NULL);
	}
	if(ev->window == selmon->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while(ev->x >= x && ++i < LENGTH(tags));
		if(i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		}
		else if(ev->x < x + blw)
			click = ClkLtSymbol;
		else if(ev->x > selmon->ww - TEXTW(stext))
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	}
	else if((c = wintoclient(ev->window))) {
		focus(c);
		click = ClkClientWin;
	}
	for(i = 0; i < LENGTH(buttons); i++)
		if(click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}
#elif USE_WINAPI
void
buttonpress(unsigned int button, POINTS *point) {
	unsigned int i, x, click;
	Arg arg = {0};

	/* XXX: hack */
	drw->hdc = GetWindowDC(barhwnd);

	i = x = 0;

	do { x += TEXTW(tags[i]); } while(point->x >= x && ++i < LENGTH(tags));
	if(i < LENGTH(tags)) {
		click = ClkTagBar;
		arg.ui = 1 << i;
	}
	else if(point->x < x + blw)
		click = ClkLtSymbol;
	else if(point->x > selmon->wx + selmon->ww - TEXTW(stext))
		click = ClkStatusText;
	else
		click = ClkWinTitle;

	if (GetKeyState(VK_SHIFT) < 0)
		return;

	for(i = 0; i < LENGTH(buttons); i++) {
		if(click == buttons[i].click && buttons[i].func && buttons[i].button == button
			&& (!buttons[i].mask || GetKeyState(buttons[i].mask) < 0)) {
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
			break;
		}
	}
}
#endif

void
checkotherwm(void) {
#if USE_XLIB
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
#elif USE_WINAPI
#endif
}

void
cleanup(void) {
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;

#if USE_WINAPI
	KillTimer(barhwnd, 1);
#endif

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for(m = mons; m; m = m->next)
		while(m->stack)
			unmanage(m->stack, False);
#if USE_XLIB
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
#endif
	while(mons)
		cleanupmon(mons);
	drw_cur_free(drw, cursor[CurNormal]);
	drw_cur_free(drw, cursor[CurResize]);
	drw_cur_free(drw, cursor[CurMove]);
	drw_font_free(dpy, fnt);
	drw_clr_free(scheme[SchemeNorm].border);
	drw_clr_free(scheme[SchemeNorm].bg);
	drw_clr_free(scheme[SchemeNorm].fg);
	drw_clr_free(scheme[SchemeSel].border);
	drw_clr_free(scheme[SchemeSel].bg);
	drw_clr_free(scheme[SchemeSel].fg);
	drw_free(drw);
#if USE_XLIB
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
#endif
#if USE_WINAPI
	HWND hwnd = FindWindow("Progman", "Program Manager");
	if (hwnd)
		setvisibility(hwnd, TRUE);

	hwnd = FindWindow("Shell_TrayWnd", NULL);
	if (hwnd)
		setvisibility(hwnd, TRUE);
#endif
}

void
cleanupmon(Monitor *mon) {
	Monitor *m;

	if(mon == mons)
		mons = mons->next;
	else {
		for(m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
#if USE_XLIB
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
#endif
	free(mon);
}

void
clearurgent(Client *c) {
#if USE_XLIB
	XWMHints *wmh;

	c->isurgent = False;
	if(!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags &= ~XUrgencyHint;
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
#elif USE_WINAPI
#endif
}

#if USE_XLIB
void
clientmessage(XEvent *e) {
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if(!c)
		return;
	if(cme->message_type == netatom[NetWMState]) {
		if(cme->data.l[1] == netatom[NetWMFullscreen] || cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
			              || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	}
	else if(cme->message_type == netatom[NetActiveWindow]) {
		if(!ISVISIBLE(c)) {
			c->mon->seltags ^= 1;
			c->mon->tagset[c->mon->seltags] = c->tags;
		}
		pop(c);
	}
}
#endif

void
configure(Client *c) {
#if USE_XLIB
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
#use USE_WINAPI
#endif
}

#if USE_XLIB
void
configurenotify(XEvent *e) {
	Monitor *m;
	XConfigureEvent *ev = &e->xconfigure;
	Bool dirty;

	// TODO: updategeom handling sucks, needs to be simplified
	if(ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if(updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for(m = mons; m; m = m->next)
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
			focus(NULL);
			arrange(NULL);
		}
	}
}
#endif

#if USE_XLIB
void
configurerequest(XEvent *e) {
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if((c = wintoclient(ev->window))) {
		if(ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if(c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if(ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if(ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if(ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if(ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if(ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		}
		else
			configure(c);
	}
	else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}
#endif

Monitor *
createmon(void) {
	Monitor *m;

	if(!(m = (Monitor *)calloc(1, sizeof(Monitor))))
		die("fatal: could not malloc() %u bytes\n", sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
#if USE_WINAPI
#endif
	return m;
}

#if USE_XLIB
void
destroynotify(XEvent *e) {
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if((c = wintoclient(ev->window)))
		unmanage(c, True);
}
#endif

void
detach(Client *c) {
	Client **tc;

	for(tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c) {
	Client **tc, *t;

	for(tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if(c == c->mon->sel) {
		for(t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir) {
	Monitor *m = NULL;

	if(dir > 0) {
		if(!(m = selmon->next))
			m = mons;
	}
	else if(selmon == mons)
		for(m = mons; m->next; m = m->next);
	else
		for(m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawbar(Monitor *m) {
	int x, xx, w;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	for(c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if(c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for(i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme(drw, m->tagset[m->seltags] & 1 << i ? &scheme[SchemeSel] : &scheme[SchemeNorm]);
		drw_text(drw, x, 0, w, bh, tags[i], urg & 1 << i);
		drw_rect(drw, x, 0, w, bh, m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
		           occ & 1 << i, urg & 1 << i);
		x += w;
	}
	w = blw = TEXTW(m->ltsymbol);
	drw_setscheme(drw, &scheme[SchemeNorm]);
	drw_text(drw, x, 0, w, bh, m->ltsymbol, 0);
	x += w;
	xx = x;
	if(m == selmon) { /* status is only drawn on selected monitor */
		w = TEXTW(stext);
		x = m->ww - w;
		if(x < xx) {
			x = xx;
			w = m->ww - xx;
		}
		drw_text(drw, x, 0, w, bh, stext, 0);
	}
	else
		x = m->ww;
	if((w = x - xx) > bh) {
		x = xx;
		if(m->sel) {
			drw_setscheme(drw, m == selmon ? &scheme[SchemeSel] : &scheme[SchemeNorm]);
			drw_text(drw, x, 0, w, bh, m->sel->name, 0);
			drw_rect(drw, x, 0, w, bh, m->sel->isfixed, m->sel->isfloating, 0);
		}
		else {
			drw_setscheme(drw, &scheme[SchemeNorm]);
			drw_text(drw, x, 0, w, bh, NULL, 0);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void
drawbars(void) {
	Monitor *m;

	for(m = mons; m; m = m->next)
		drawbar(m);
}

#if USE_WINAPI
void
toggleborder(const Arg *arg) {
	if (!selmon->sel)
		return;
	setborder(selmon->sel, !selmon->sel->border);
}
#endif

#if USE_WINAPI
void
toggleexplorer(const Arg *arg) {
	HWND hwnd = FindWindow("Progman", "Program Manager");
	if (hwnd)
		setvisibility(hwnd, !IsWindowVisible(hwnd));

	hwnd = FindWindow("Shell_TrayWnd", NULL);
	if (hwnd)
		setvisibility(hwnd, !IsWindowVisible(hwnd));
	
	updategeom();
	updatebar();
	arrange(selmon);		
}
#endif

#if USE_XLIB
void
enternotify(XEvent *e) {
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if(m != selmon) {
		unfocus(selmon->sel, True);
		selmon = m;
	}
	else if(!c || c == selmon->sel)
		return;
	focus(c);
}
#endif

#if USE_XLIB
void
expose(XEvent *e) {
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if(ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}
#endif

void
focus(Client *c) {
#if USE_XLIB
	if(!c || !ISVISIBLE(c))
		for(c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	/* was if(selmon->sel) */
	if(selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, False);
	if(c) {
		if(c->mon != selmon)
			selmon = c->mon;
		if(c->isurgent)
			clearurgent(c);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, True);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel].border->rgb);
		setfocus(c);
	}
	else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
#elif USE_WINAPI
	setselected(c);
	if (selmon->sel)
		SetForegroundWindow(selmon->sel->hwnd);
#endif
	
}

#if USE_XLIB
void
focusin(XEvent *e) { /* there are some broken focus acquiring clients */
	XFocusChangeEvent *ev = &e->xfocus;

	if(selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}
#endif

void
focusmon(const Arg *arg) {
	Monitor *m;

	if(!mons->next)
		return;
	if((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, False); /* s/True/False/ fixes input focus issues
					in gedit and anjuta */
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg) {
	Client *c = NULL, *i;

	if(!selmon->sel)
		return;
	if(arg->i > 0) {
		for(c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if(!c)
			for(c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	}
	else {
		for(i = selmon->clients; i != selmon->sel; i = i->next)
			if(ISVISIBLE(i))
				c = i;
		if(!c)
			for(; i; i = i->next)
				if(ISVISIBLE(i))
					c = i;
	}
	if(c) {
		focus(c);
		restack(selmon);
	}
}

Atom
getatomprop(Client *c, Atom prop) {
#if USE_XLIB
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if(XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
	                      &da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
#elif USE_WINAPI
#endif
}

Bool
getrootptr(int *x, int *y) {
#if USE_XLIB
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
#elif USE_WINAPI
#endif
}

long
getstate(Window w) {
	int format;
	long result = -1;
#if USE_XLIB
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if(XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
	                      &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if(n != 0)
		result = *p;
	XFree(p);
#elif USE_WINAPI
#endif
	return result;
}

Bool
gettextprop(Window w, Atom atom, char *text, unsigned int size) {
#if USE_XLIB
	char **list = NULL;
	int n;
	XTextProperty name;

	if(!text || size == 0)
		return False;

	text[0] = '\0';
	XGetTextProperty(dpy, w, &name, atom);
	if(!name.nitems)
		return False;
	if(name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if(XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
#elif USE_WINAPI
	const char *title;
	strncpy(text, title = getclienttitle(w), size - 1);
#endif
	return True;
}

void
grabbuttons(Client *c, Bool focused) {
#if USE_XLIB
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if(focused) {
			for(i = 0; i < LENGTH(buttons); i++)
				if(buttons[i].click == ClkClientWin)
					for(j = 0; j < LENGTH(modifiers); j++)
						XGrabButton(dpy, buttons[i].button,
						            buttons[i].mask | modifiers[j],
						            c->win, False, BUTTONMASK,
						            GrabModeAsync, GrabModeSync, None, None);
		}
		else
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
			            BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
	}
#endif
}

void
grabkeys(void) {
#if USE_XLIB
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for(i = 0; i < LENGTH(keys); i++)
			if((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for(j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						 True, GrabModeAsync, GrabModeAsync);
	}
#elif USE_WINAPI
	int i;
	for (i = 0; i < LENGTH(keys); i++) {
		RegisterHotKey(dwmhwnd, i, keys[i].mod, keys[i].keysym);
	}
#endif
}

void
incnmaster(const Arg *arg) {
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#if USE_XLIB
#ifdef XINERAMA
static Bool
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info) {
	while(n--)
		if(unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return False;
	return True;
}
#endif /* XINERAMA */
#endif

#if USE_XLIB
void
keypress(XEvent *e) {
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for(i = 0; i < LENGTH(keys); i++)
		if(keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}
#elif USE_WINAPI
void
keypress(WPARAM wParam) {
	if (wParam >= 0 && wParam < LENGTH(keys)) {
		keys[wParam].func(&(keys[wParam ].arg));
	}
}
#endif

void
killclient(const Arg *arg) {
	if(!selmon->sel)
		return;
#if USE_XLIB
	if(!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
#elif USE_WINAPI
	PostMessage(selmon->sel->hwnd, WM_CLOSE, 0, 0);
#endif
}

#if USE_XLIB
void
#elif USE_WINAPI
Client *
#endif
manage(Window w, XWindowAttributes *wa) {
	Client *c, *t = NULL;
	Window trans = None;
#if USE_XLIB
	XWindowChanges wc;
#elif USE_WINAPI
	RECT r;
#endif
	if(!(c = calloc(1, sizeof(Client))))
		die("fatal: could not malloc() %u bytes\n", sizeof(Client));
#if USE_WINAPI
	debug(" manage %s\n", getclienttitle(w));

	WINDOWINFO wi = {
		.cbSize = sizeof(WINDOWINFO),
	};

	if (!GetWindowInfo(w, &wi))
		return NULL;
		
	c->hwnd = w;
	c->threadid = GetWindowThreadProcessId(w, NULL);
	c->parent = GetParent(w);
	c->root = getroot(w);
	c->isalive = true;
#endif
	c->win = w;
	updatetitle(c);
#if USE_XLIB
	if(XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	}
	else {
		c->mon = selmon;
		applyrules(c);
	}
#elif USE_WINAPI
	c->mon = selmon;
	applyrules(c);

	static WINDOWPLACEMENT wp = {
		.length = sizeof(WINDOWPLACEMENT),
		.showCmd = SW_RESTORE,
	};

	if (IsWindowVisible(c->win))
		SetWindowPlacement(w, &wp);
		
	/* maybe we could also filter based on 
	 * WS_MINIMIZEBOX and WS_MAXIMIZEBOX
	 */
	c->isfloating = (wi.dwStyle & WS_POPUP) || 
		(!(wi.dwStyle & WS_MINIMIZEBOX) && !(wi.dwStyle & WS_MAXIMIZEBOX));

//	debug(" window style: %d\n", wi.dwStyle);
//	debug("     minimize: %d\n", wi.dwStyle & WS_MINIMIZEBOX);
//	debug("     maximize: %d\n", wi.dwStyle & WS_MAXIMIZEBOX);
//	debug("        popup: %d\n", wi.dwStyle & WS_POPUP);
//	debug("   isfloating: %d\n", c->isfloating);
#endif
	/* geometry */
#if USE_XLIB
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
#elif USE_WINAPI
	GetWindowRect(w, &r);
	c->x = c->oldx = r.left;
	c->y = c->oldy = r.top;
	c->w = c->oldw = r.right-r.left+1;
	c->h = c->oldh = r.bottom-r.top+1;
	c->oldbw = GetSystemMetrics(SM_CXSIZEFRAME);
#endif

	if(c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if(c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
	           && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	c->bw = borderpx;

#if USE_XLIB
	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm].border->rgb);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
#endif
	grabbuttons(c, False);
	if(!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
#if USE_XLIB
	if(c->isfloating)
		XRaiseWindow(dpy, c->win);
#elif USE_WINAPI
	if (!c->isfloating)
		setborder(c, false);

	if (c->isfloating && IsWindowVisible(w)) {
		debug(" new floating window: x: %d y: %d w: %d h: %d\n", wi.rcWindow.left, wi.rcWindow.top, wi.rcWindow.right - wi.rcWindow.left, wi.rcWindow.bottom - wi.rcWindow.top);
		resize(c, wi.rcWindow.left, wi.rcWindow.top, wi.rcWindow.right - wi.rcWindow.left, wi.rcWindow.bottom - wi.rcWindow.top, FALSE); /*todo fix arg*/
	}
#endif
	attach(c);
	attachstack(c);
#if USE_XLIB
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
	                (unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
#endif
	if (c->mon == selmon)
		unfocus(selmon->sel, False);
	c->mon->sel = c;
	arrange(c->mon);
#if USE_XLIB
	XMapWindow(dpy, c->win);
#endif
	focus(NULL);
	return c;
}

#if USE_XLIB
void
mappingnotify(XEvent *e) {
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if(ev->request == MappingKeyboard)
		grabkeys();
}
#endif

#if USE_XLIB
void
maprequest(XEvent *e) {
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if(!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if(wa.override_redirect)
		return;
	if(!wintoclient(ev->window))
		manage(ev->window, &wa);
}
#endif

void
monocle(Monitor *m) {
	unsigned int n = 0;
	Client *c;

	for(c = m->clients; c; c = c->next)
		if(ISVISIBLE(c))
			n++;
	if(n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for(c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, False);
}

#if USE_XLIB
void
motionnotify(XEvent *e) {
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if(ev->window != root)
		return;
	if((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, True);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}
#endif

void
movemouse(const Arg *arg) {
#if USE_XLIB
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;

	if(!(c = selmon->sel))
		return;
	if(c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if(XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
	None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if(!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if(nx >= selmon->wx && nx <= selmon->wx + selmon->ww
			&& ny >= selmon->wy && ny <= selmon->wy + selmon->wh) {
				if(abs(selmon->wx - nx) < snap)
					nx = selmon->wx;
				else if(abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
					nx = selmon->wx + selmon->ww - WIDTH(c);
				if(abs(selmon->wy - ny) < snap)
					ny = selmon->wy;
				else if(abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
					ny = selmon->wy + selmon->wh - HEIGHT(c);
				if(!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
					togglefloating(NULL);
			}
			if(!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, True);
			break;
		}
	} while(ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
#elif USE_WINAPI
#endif
}

Client *
nexttiled(Client *c) {
	for(; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
pop(Client *c) {
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

#if USE_XLIB
void
propertynotify(XEvent *e) {
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if(ev->state == PropertyDelete)
		return; /* ignore */
	else if((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if(!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
			   (c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if(ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if(c == c->mon->sel)
				drawbar(c->mon);
		}
		if(ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}
#endif

#if USE_WINAPI
void
setvisibility(HWND hwnd, bool visibility) {
	SetWindowPos(hwnd, 0, 0, 0, 0, 0, (visibility ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}
#endif

#if USE_XLIB
void
resize(Client *c, int x, int y, int w, int h, Bool interact) {
	if(applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}
#elif USE_WINAPI

void
resize(Client *c, int x, int y, int w, int h, Bool interact) {
	if(w <= 0 && h <= 0) {
		setvisibility(c->hwnd, false);
		return;
	}
	if(x > sx + sw)
		x = sw - WIDTH(c);
	if(y > sy + sh)
		y = sh - HEIGHT(c);
	if(x + w + 2 * c->bw < sx)
		x = sx;
	if(y + h + 2 * c->bw < sy)
		y = sy;
	if(h < bh)
		h = bh;
	if(w < bh)
		w = bh;
	if(c->x != x || c->y != y || c->w != w || c->h != h) {
		c->x = x;
		c->y = y;
		c->w = w;
		c->h = h;
		debug(" resize %d: %s: x: %d y: %d w: %d h: %d\n", c->hwnd, getclienttitle(c->hwnd), x, y, w, h);
		SetWindowPos(c->hwnd, HWND_TOP, c->x, c->y, c->w, c->h, SWP_NOACTIVATE);
	}
}

#endif


void
quit(const Arg *arg) {
	running = False;
}

Monitor *
recttomon(int x, int y, int w, int h) {
	Monitor *m, *r = selmon;
	int a, area = 0;

	for(m = mons; m; m = m->next)
		if((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resizeclient(Client *c, int x, int y, int w, int h) {
#if USE_XLIB
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
#elif USE_WINAPI
#endif
}

void
resizemouse(const Arg *arg) {
#if USE_XLIB
	int ocx, ocy;
	int nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;

	if(!(c = selmon->sel))
		return;
	if(c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if(XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
	                None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if(c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if(!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if(!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, True);
			break;
		}
	} while(ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while(XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
#elif USE_WINAPI
#endif
}

void
restack(Monitor *m) {
#if USE_XLIB
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if(!m->sel)
		return;
	if(m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if(m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for(c = m->stack; c; c = c->snext)
			if(!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}

	XSync(dpy, False);
	while(XCheckMaskEvent(dpy, EnterWindowMask, &ev));
#elif USE_WINAPI
#endif
}

void
run(void) {
#if USE_XLIB
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while(running && !XNextEvent(dpy, &ev))
		if(handler[ev.type])
			handler[ev.type](&ev); /* call handler */
#elif USE_WINAPI
	MSG msg;
	while (running && GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	cleanup();
#endif
}

VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	updatestatus();
	PostMessage(barhwnd, WM_PAINT, 0, 0);
}

#if USE_WINAPI
LRESULT CALLBACK barhandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			updatebar();
			break;
		case WM_PAINT: {
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);
			drawbar(selmon);
			EndPaint(hwnd, &ps);
			break;
		}
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			buttonpress(msg, &MAKEPOINTS(lParam));
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam); 
	}

	return 0;
}

Client *
getclient(HWND hwnd) {
	Client *c;
	Monitor *m;

	for(m = mons; m; m = m->next)
	for(c = m->clients; c; c = c->next)
		if (c->hwnd == hwnd)
			return c;
	return NULL;
}

bool
ismanageable(HWND hwnd){
	if (getclient(hwnd))
		return true;

	HWND parent = GetParent(hwnd);	
	HWND owner = GetWindow(hwnd, GW_OWNER);
	int style = GetWindowLong(hwnd, GWL_STYLE);
	int exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	bool pok = (parent != 0 && ismanageable(parent));
	bool istool = exstyle & WS_EX_TOOLWINDOW;
	bool isapp = exstyle & WS_EX_APPWINDOW;

	if (pok && !getclient(parent))
		manage(parent, NULL);

//	debug("ismanageable: %s\n", getclienttitle(hwnd));
//	debug("    hwnd: %d\n", hwnd);
//	debug("  window: %d\n", IsWindow(hwnd));
//	debug(" visible: %d\n", IsWindowVisible(hwnd));
//	debug("  parent: %d\n", parent);
//	debug("parentok: %d\n", pok);
//	debug("   owner: %d\n", owner);
//	debug(" toolwin: %d\n", istool);
//	debug("  appwin: %d\n", isapp);

	/* XXX: should we do this? */
	if (GetWindowTextLength(hwnd) == 0) {
//		debug("   title: NULL\n");
//		debug("  manage: false\n");
		return false;
	}

	if (style & WS_DISABLED) {
//		debug("disabled: true\n");
//		debug("  manage: false\n");
		return false;
	}

	/*
	 *	WS_EX_APPWINDOW
	 *		Forces a top-level window onto the taskbar when 
	 *		the window is visible.
	 *
	 *	WS_EX_TOOLWINDOW
	 *		Creates a tool window; that is, a window intended 
	 *		to be used as a floating toolbar. A tool window 
	 *		has a title bar that is shorter than a normal 
	 *		title bar, and the window title is drawn using 
	 *		a smaller font. A tool window does not appear in 
	 *		the taskbar or in the dialog that appears when 
	 *		the user presses ALT+TAB. If a tool window has 
	 *		a system menu, its icon is not displayed on the 
	 *		title bar. However, you can display the system 
	 *		menu by right-clicking or by typing ALT+SPACE.
	 */

	if ((parent == 0 && IsWindowVisible(hwnd)) || pok) {
		if ((!istool && parent == 0) || (istool && pok)) {
//			debug("  manage: true\n");
			return true;
		}
		if (isapp && parent != 0) {
//		    debug("  manage: true\n");
			return true;
		}
	}
//	debug("  manage: false\n");
	return false;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			break;
		case WM_CLOSE:
			cleanup();
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_HOTKEY:
			keypress(wParam);
			break;
		default:
			if (msg == shellhookid) { /* Handle the shell hook message */
				Client *c = getclient((HWND)lParam);
				switch (wParam) {
					/* The first two events are also trigger if windows
					 * are being hidden or shown because of a tag
					 * switch, therefore we ignore them in this case.
					 */
					case HSHELL_WINDOWCREATED:
						//MessageBox(NULL, TEXT("yes"), TEXT("dwm"),MB_OK|MB_APPLMODAL);
						debug("window created: %s\n", getclienttitle((HWND)lParam));
						if (!c && ismanageable((HWND)lParam)) {
							c = manage((HWND)lParam, NULL);
							managechildwindows(c);
							arrange(NULL); /*CONFIRM ARG TODO */
						}
						break;
					case HSHELL_WINDOWDESTROYED:
						if (c) {
							debug(" window %s: %s\n", c->ignore ? "hidden" : "destroyed", getclienttitle(c->hwnd));
							if (!c->ignore)
								unmanage(c, true);
							else
								c->ignore = false;
						} else {
							debug(" unmanaged window destroyed\n");
						}
						break;
					case HSHELL_WINDOWACTIVATED:
						debug(" window activated: %s || %d\n", c ? getclienttitle(c->hwnd) : "unknown", (HWND)lParam);
						if (c) {
							Client *t = selmon->sel; /*TODO CHECK */
							managechildwindows(c);
							setselected(c);
							/* check if the previously selected 
							 * window got minimized
							 */
							if (t && (t->isminimized = IsIconic(t->hwnd))) {
								debug(" active window got minimized: %s\n", getclienttitle(t->hwnd));
								arrange(NULL); /* TODO CHECK */
							}
							/* the newly focused window was minimized */
							if (selmon->sel->isminimized) { /*todo check*/
								debug(" newly active window was minimized: %s\n", getclienttitle(selmon->sel->hwnd)); /*todo: check */
								selmon->sel->isminimized = false;								
								zoom(NULL);
							}
						} else  {
							/* Some window don't seem to generate 
							 * HSHELL_WINDOWCREATED messages therefore 
						 	 * we check here whether we should manage
						 	 * the window or not.
						 	 */
							if (ismanageable((HWND)lParam)) {
								c = manage((HWND)lParam, NULL);
								managechildwindows(c);
								setselected(c);
								arrange(NULL); /*todo confirm arg */
							}
						}
						break;
				}
			} else
				return DefWindowProc(hwnd, msg, wParam, lParam); 
	}

	return 0;
}

BOOL CALLBACK 
scan(HWND hwnd, LPARAM lParam) {
	Client *c = getclient(hwnd);
	if (c)
		c->isalive = true;
	else if (ismanageable(hwnd))
		manage(hwnd, NULL);
	return TRUE;
}

Client *
nextchild(Client *p, Client *c) {
	for(; c && c->parent != p->hwnd; c = c->next);
	return c;
}

Client *
managechildwindows(Client *p) {
	Monitor *m;
	Client *c, *t;
	EnumChildWindows(p->hwnd, scan, 0);
	/* remove all child windows which were not part
	 * of the enumeration above.
	 */
	for(m = mons; m; m = m->next)
	for(c = m->clients; c; ) {
		if (c->parent == p->hwnd) {
			/* XXX: ismanageable isn't that reliable or some
			 *      windows change over time which means they
			 *      were once reported as manageable but not
			 *      this time so we also check if they are
			 *      currently visible and if that's the case
			 *      we keep them in our client list.
			 */
			if (!c->isalive && !IsWindowVisible(c->hwnd)) {
				t = c->next;
				unmanage(c, false); /* fixme */
				c = t;
				continue;
			}
			/* reset flag for next check */
			c->isalive = false;
		}
		c = c->next;
	}

	return nextchild(p, mons->clients); /*todo check */
}
#endif

#if USE_XLIB
void
scan(void) {
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if(XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if(wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for(i = 0; i < num; i++) { /* now the transients */
			if(!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if(XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if(wins)
			XFree(wins);
	}
}
#endif

void
sendmon(Client *c, Monitor *m) {
	if(c->mon == m)
		return;
	unfocus(c, True);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state) {
	long data[] = { state, None };

#if USE_XLIB
	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
			PropModeReplace, (unsigned char *)data, 2);
#elif USE_WINAPI
#endif
}

Bool
sendevent(Client *c, Atom proto) {
#if USE_XLIB
	int n;
	Atom *protocols;
	Bool exists = False;
	XEvent ev;

	if(XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while(!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if(exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
#elif USE_WINAPI
#endif
}

void
setfocus(Client *c) {
#if USE_XLIB
	if(!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
 		                XA_WINDOW, 32, PropModeReplace,
 		                (unsigned char *) &(c->win), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
#elif USE_WINAPI
#endif
}

void
setfullscreen(Client *c, Bool fullscreen) {
#if USE_XLIB
	if(fullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
		                PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = True;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = True;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	}
	else {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
		                PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = False;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
#elif USE_WINAPI
#endif
}

void
setlayout(const Arg *arg) {
	if(!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if(arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if(selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutly */
void
setmfact(const Arg *arg) {
	float f;

	if(!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if(f < 0.1 || f > 0.9)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void
#if USE_XLIB
setup(void) {
	XSetWindowAttributes wa;
#elif USE_WINAPI
setup(HINSTANCE hInstance) {
	RECT wa;
	HWND hwnd = FindWindow("Shell_TrayWnd", NULL);
#endif

	/* clean up any zombies immediately */
	sigchld(0);

#if USE_WINAPI
	WNDCLASSEX winClass;

	winClass.cbSize = sizeof(WNDCLASSEX);
	winClass.style = 0;
	winClass.lpfnWndProc = WndProc;
	winClass.cbClsExtra = 0;
	winClass.cbWndExtra = 0;
	winClass.hInstance = hInstance;
	winClass.hIcon = NULL;
	winClass.hIconSm = NULL;
	winClass.hCursor = NULL;
	winClass.hbrBackground = NULL;
	winClass.lpszMenuName = NULL;
	winClass.lpszClassName = NAME;

	if (!RegisterClassEx(&winClass))
		die("Error registering window class");

	dwmhwnd = CreateWindowEx(0, NAME, NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

	if (!dwmhwnd)
		die("Error creating window");

#if USE_WINAPI
	sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
#endif
#endif

	/* init screen */
#if USE_XLIB
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
#elif USE_WINAPI
	root = NULL;
#endif
#if USE_XLIB
	fnt = drw_font_create(dpy, font);
#elif USE_WINAPI
	fnt = drw_font_create(dpy, font, dwmhwnd);
#endif
#if USE_XLIB
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	bh = fnt->h + 2;
#elif USE_WINAPI
	if (hwnd && IsWindowVisible(hwnd)) {	
		SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
		sx = wa.left;
		sy = wa.top;
		sw = wa.right - wa.left;
		sh = wa.bottom - wa.top;
	} else {
		sx = GetSystemMetrics(SM_XVIRTUALSCREEN);
		sy = GetSystemMetrics(SM_YVIRTUALSCREEN);
		sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	}

	bh = 20; /* XXX: fixed value */
#endif
	
	drw = drw_create(dpy, screen, root, sw, sh);
	drw_setfont(drw, fnt);
	updategeom();

	/* init atoms */
#if USE_XLIB
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
#elif USE_WINAPI
#endif
	/* init appearance */
	scheme[SchemeNorm].border = drw_clr_create(drw, normbordercolor);
	scheme[SchemeNorm].bg = drw_clr_create(drw, normbgcolor);
	scheme[SchemeNorm].fg = drw_clr_create(drw, normfgcolor);
	scheme[SchemeSel].border = drw_clr_create(drw, selbordercolor);
	scheme[SchemeSel].bg = drw_clr_create(drw, selbgcolor);
	scheme[SchemeSel].fg = drw_clr_create(drw, selfgcolor);
	/* init bars */
#if USE_WINAPI
	EnumWindows(scan, 0);

	setupbar(hInstance);
	drw_resize(drw, sw, bh);

	/* Get function pointer for RegisterShellHookWindow */
	_RegisterShellHookWindow = (RegisterShellHookWindowProc)GetProcAddress(GetModuleHandle("USER32.DLL"), "RegisterShellHookWindow");
	if (!_RegisterShellHookWindow)
		die("Could not find RegisterShellHookWindow");
	_RegisterShellHookWindow(dwmhwnd);
	/* Grab a dynamic id for the SHELLHOOK message to be used later */
	shellhookid = RegisterWindowMessage("SHELLHOOK");
#endif
	updatebars();
	updatestatus();
#if USE_XLIB
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select for events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|PointerMotionMask
	                |EnterWindowMask|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
#endif
	
	grabkeys();
	focus(NULL);
}



void
showhide(Client *c) {
	if(!c)
		return;
	if(ISVISIBLE(c)) { /* show clients top down */
#if USE_XLIB
		XMoveWindow(dpy, c->win, c->x, c->y);
#elif USE_WINAPI
		if (c->wasvisible) {
			setvisibility(c->hwnd, true);
		}
#endif
		if((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, False);
		showhide(c->snext);
	}
	else { /* hide clients bottom up */
		showhide(c->snext);
#if USE_XLIB
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
#elif USE_WINAPI
		if (IsWindowVisible(c->hwnd)) {
			c->ignore = true;
			c->wasvisible = true;		
			setvisibility(c->hwnd, false);
		}
#endif
	}
}

void
sigchld(int unused) {
	if(signal(SIGCHLD, sigchld) == SIG_ERR)
		die("Can't install SIGCHLD handler");
	while(0 < waitpid(-1, NULL, WNOHANG));
}

void
spawn(const Arg *arg) {
#if USE_XLIB
	if(arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if(fork() == 0) {
		if(dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
#elif USE_WINAPI
	debug("test");
	if(arg->v == dmenucmd)
	{
		int i;
		char args[MAX_PATH] = "";
		
		//dmenumon[0] = '0' + selmon->num;
		for(i = 1; ((char **)arg->v)[i] != NULL; i++)
		{
			if (i % 2 == 0 && i >= 2)
				sprintf(args, "%s \"%s\"", args, ((char **)arg->v)[i]);
			else
				sprintf(args, "%s %s", args, ((char **)arg->v)[i]);
		}
		debug("cmd: %s", args);
		ShellExecute(NULL, NULL, ((char **)arg->v)[0], args, NULL, SW_HIDE);
	}
	else
		ShellExecute(NULL, NULL, ((char **)arg->v)[0], ((char **)arg->v)[1], NULL, SW_SHOWDEFAULT);
#endif
}

void
tag(const Arg *arg) {
	if(selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void
tagmon(const Arg *arg) {
	if(!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
tile(Monitor *m) {
	unsigned int i, n, h, mw, my, ty;
	Client *c;

	for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if(n == 0)
		return;

	if(n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww;
	for(i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if(i < m->nmaster) {
			h = (m->wh - my) / (MIN(n, m->nmaster) - i);
			resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), False);
			my += HEIGHT(c);
		}
		else {
			h = (m->wh - ty) / (n - i);
			resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), False);
			ty += HEIGHT(c);
		}
}

void
togglebar(const Arg *arg) {
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
#if USE_XLIB
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
#elif USE_WINAPI
	updategeom();
	updatebar();
#endif
	arrange(selmon);
}

void
togglefloating(const Arg *arg) {
	if(!selmon->sel)
		return;
	if(selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if(selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
		       selmon->sel->w, selmon->sel->h, False);
	arrange(selmon);
}

void
toggletag(const Arg *arg) {
	unsigned int newtags;

	if(!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if(newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg) {
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

	if(newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, Bool setfocus) {
#if USE_XLIB
	if(!c)
		return;
	grabbuttons(c, False);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm].border->rgb);
	if(setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
#elif USE_WINAPI
#endif
}

void
unmanage(Client *c, Bool destroyed) {
	Monitor *m = c->mon;
 #if USE_XLIB
	XWindowChanges wc;

	/* The server grab construct avoids race conditions. */
	detach(c);
	detachstack(c);
	if(!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
#elif USE_WINAPI
	debug(" unmanage %s\n", getclienttitle(c->hwnd));
	if (c->wasvisible)
		setvisibility(c->hwnd, true);
	if (!c->isfloating)
		setborder(c, true);
	detach(c);
	detachstack(c);
	if(m->sel == c)
		focus(NULL);
	free(c);
	arrange(m);
#endif
}

#if USE_XLIB
void
unmapnotify(XEvent *e) {
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if((c = wintoclient(ev->window))) {
		if(ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, False);
	}
}
#endif

void
updatebars(void) {
#if USE_XLIB
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	for(m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
		                          CopyFromParent, DefaultVisual(dpy, screen),
		                          CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
	}
#elif USE_WINAPI
}

void
updatebar(void) {
	Monitor *m;
	for(m = mons; m; m = m->next) {
	//FIXME
	SetWindowPos(barhwnd, selmon->showbar ? HWND_TOPMOST : HWND_NOTOPMOST, 0, m->by, m->ww, bh, (selmon->showbar ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
	}
}

void
setupbar(HINSTANCE hInstance) {

	unsigned int i, w = 0;

	WNDCLASS winClass;
	memset(&winClass, 0, sizeof winClass);

	winClass.style = 0;
	winClass.lpfnWndProc = barhandler;
	winClass.cbClsExtra = 0;
	winClass.cbWndExtra = 0;
	winClass.hInstance = hInstance;
	winClass.hIcon = NULL;
	winClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	winClass.hbrBackground = NULL;
	winClass.lpszMenuName = NULL;
	winClass.lpszClassName = "dwm-bar";


	if (!RegisterClass(&winClass))
		die("Error registering window class");

	barhwnd = CreateWindowEx(
		WS_EX_TOOLWINDOW,
		"dwm-bar",
		NULL, /* window title */
		WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
		0, 0, 0, 0, 
		NULL, /* parent window */
		NULL, /* menu */
		hInstance,
		NULL
	);
	root = barhwnd;
	drw->gc = GetWindowDC(root); /* hack */
	/* calculate width of the largest layout symbol */
	drw->hdc = GetWindowDC(barhwnd);
	//HFONT font = (HFONT)GetStockObject(SYSTEM_FONT); 
	HFONT f = CreateFont(10,0,0,0,0,0,0,0,0,0,0,0,0,TEXT(font));
	SelectObject(drw->hdc, f);

	for(blw = i = 0; LENGTH(layouts) > 1 && i < LENGTH(layouts); i++) {
 		w = TEXTW(layouts[i].symbol);
		blw = MAX(blw, w);
	}

	DeleteObject(f); 
	ReleaseDC(barhwnd, drw->hdc);

	SetTimer(barhwnd, 1, 1000, TimerProc);

	PostMessage(barhwnd, WM_PAINT, 0, 0);
	updatebar();
}

#endif


void
updatebarpos(Monitor *m) {
	m->wy = m->my;
	m->wh = m->mh;
	if(m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	}
	else
		m->by = -bh;
}

void
updateclientlist() {
#if USE_XLIB
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for(m = mons; m; m = m->next)
		for(c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
			                XA_WINDOW, 32, PropModeAppend,
			                (unsigned char *) &(c->win), 1);
#elif USE_WINAPI
#endif
}

Bool
updategeom(void) {
	Bool dirty = False;
#if USE_XLIB
#ifdef XINERAMA
	if(XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for(n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		if(!(unique = (XineramaScreenInfo *)malloc(sizeof(XineramaScreenInfo) * nn)))
			die("fatal: could not malloc() %u bytes\n", sizeof(XineramaScreenInfo) * nn);
		for(i = 0, j = 0; i < nn; i++)
			if(isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if(n <= nn) {
			for(i = 0; i < (nn - n); i++) { /* new monitors available */
				for(m = mons; m && m->next; m = m->next);
				if(m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for(i = 0, m = mons; i < nn && m; m = m->next, i++)
				if(i >= n
				|| (unique[i].x_org != m->mx || unique[i].y_org != m->my
				    || unique[i].width != m->mw || unique[i].height != m->mh))
				{
					dirty = True;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		}
		else { /* less monitors available nn < n */
			for(i = nn; i < n; i++) {
				for(m = mons; m && m->next; m = m->next);
				while(m->clients) {
					dirty = True;
					c = m->clients;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attach(c);
					attachstack(c);
				}
				if(m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	}
	else
#endif /* XINERAMA */
#endif
	/* default monitor setup */
	{
		if(!mons)
			mons = createmon();
		if(mons->mw != sw || mons->mh != sh) {
			dirty = True;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if(dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void) {
#if USE_XLIB
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for(i = 0; i < 8; i++)
		for(j = 0; j < modmap->max_keypermod; j++)
			if(modmap->modifiermap[i * modmap->max_keypermod + j]
			   == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
#elif USE_WINAPI
#endif
}

void
updatesizehints(Client *c) {
#if USE_XLIB
	long msize;
	XSizeHints size;

	if(!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if(size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	}
	else if(size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	}
	else
		c->basew = c->baseh = 0;
	if(size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	}
	else
		c->incw = c->inch = 0;
	if(size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	}
	else
		c->maxw = c->maxh = 0;
	if(size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	}
	else if(size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	}
	else
		c->minw = c->minh = 0;
	if(size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	}
	else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->minw && c->maxh && c->minh
	             && c->maxw == c->minw && c->maxh == c->minh);
#elif USE_WINAPI
#endif
}

void
updatetitle(Client *c) {
	if (!c)
		return;
	if(!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if(c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatestatus(void) {
#if USE_XLIB
	if(!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-"VERSION);
#elif USE_WINAPI
	time_t result = time(NULL);
	strftime(stext, 20, "%c", localtime(&result));
	//strcpy(stext, "dwm-"VERSION);
#endif
	drawbar(selmon);
}

void
updatewindowtype(Client *c) {
#if USE_XLIB
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if(state == netatom[NetWMFullscreen])
		setfullscreen(c, True);
	if(wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = True;
#elif USE_WINAPI
#endif
}

void
updatewmhints(Client *c) {
#if USE_XLIB
	XWMHints *wmh;

	if((wmh = XGetWMHints(dpy, c->win))) {
		if(c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		}
		else
			c->isurgent = (wmh->flags & XUrgencyHint) ? True : False;
		if(wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = False;
		XFree(wmh);
	}
#elif USE_WINAPI
#endif
}

#if USE_WINAPI
HWND
getroot(HWND hwnd){
	HWND parent, deskwnd = GetDesktopWindow();

	while ((parent = GetWindow(hwnd, GW_OWNER)) != NULL && deskwnd != parent)
		hwnd = parent;

	return hwnd;
}
#endif

void
view(const Arg *arg) {
	if((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if(arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	focus(NULL);
	arrange(selmon);
}

Client *
wintoclient(Window w) {
	Client *c;
	Monitor *m;

	for(m = mons; m; m = m->next)
		for(c = m->clients; c; c = c->next)
			if(c->win == w)
				return c;
	return NULL;
}

Monitor *
wintomon(Window w) {
	int x, y;
	Client *c;
	Monitor *m;

	if(w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for(m = mons; m; m = m->next)
		if(w == m->barwin)
			return m;
	if((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

#if USE_XLIB
/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int
xerror(Display *dpy, XErrorEvent *ee) {
	if(ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
			ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}
#endif

#if USE_XLIB
int
xerrordummy(Display *dpy, XErrorEvent *ee) {
	return 0;
}
#endif

#if USE_XLIB
/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee) {
	die("dwm: another window manager is already running\n");
	return -1;
}
#endif

void
zoom(const Arg *arg) {
	Client *c = selmon->sel;

	if(!selmon->lt[selmon->sellt]->arrange
	|| (selmon->sel && selmon->sel->isfloating))
		return;
	if(c == nexttiled(selmon->clients))
		if(!c || !(c = nexttiled(c->next)))
			return;
	pop(c);
}

#if USE_XLIB
int
main(int argc, char *argv[]) {
#elif USE_WINAPI
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {	
#endif

#if USE_WINAPI
	int i;
	char **argv;
	int argc = 0, targc = 0;
	char buffer[512];
	char *t;

	for(i=0; i < strlen(lpCmdLine); i++)
		if (lpCmdLine[i] == ' ')
			targc++;
	targc+=2;
	argv = malloc(targc);
	argv[argc] = "dmenu.exe";
	argc++;
	t = strtok(lpCmdLine, " "); 
	targc--;
	buffer[0] = NULL;
	
	fflush(stdout); /*strange bug*/
	while(t != NULL && targc)
	{
		if (t[0] != '-')
		{
			do
			{
				strncat(buffer, t, 512);
				t = strtok(NULL, " ");
				if (t != NULL && t[0] != '-')
					strncat(buffer, " ", 512);
				targc--;
			} while (t != NULL && t[0] != '-' && targc);
			argv[argc]=strdup(buffer);
			argc++;
			buffer[0] = NULL;
		}
		else
		{
			argv[argc]=strdup(t);
			argc++;
			buffer[0] = NULL;
			t = strtok(NULL, " ");
			targc--;
		}
	}

#endif
	if(argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION", © 2006-2012 dwm engineers, see LICENSE for details\n");
	else if(argc != 1)
		die("usage: dwm [-v]\n");
#if USE_XLIB
	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
#elif USE_WINAPI
	if(!setlocale(LC_CTYPE, ""))
#endif
		fputs("warning: no locale support\n", stderr);
#if USE_XLIB
	if(!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display\n");
#endif
	checkotherwm();
#if USE_XLIB
	setup();
	scan();
#elif USE_WINAPI
	setup(hInstance);
#endif
	run();
	cleanup();
#if USE_XLIB
	XCloseDisplay(dpy);
#endif
#if USE_WINAPI
	free(argv);
#endif

	return EXIT_SUCCESS;
}
