/* See LICENSE file for copyright and license details. */

/* for portibility */
#if USE_WINAPI
#define True TRUE
#define False FALSE
#define Bool BOOL
#define Window HWND
#define Atom int
#define Display void*
#define true TRUE
#define false FALSE
#endif

/* appearance */
#if USE_XLIB
static const char font[]            = "-*-terminus-medium-r-*-*-16-*-*-*-*-*-*-*";
#elif USE_WINAPI
static const char font[]            = "Small Font";
#endif
static const char normbordercolor[] = "#444444";
static const char normbgcolor[]     = "#222222";
static const char normfgcolor[]     = "#bbbbbb";
static const char selbordercolor[]  = "#005577";
static const char selbgcolor[]      = "#005577";
static const char selfgcolor[]      = "#eeeeee";
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const Bool showbar           = True;     /* False means no bar */
static const Bool topbar            = True;     /* False means bottom bar */

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            True,        -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       False,       -1 },
};

/* layout(s) */
static const float mfact      = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster      = 1;    /* number of clients in master area */
#if USE_XLIB
static const Bool resizehints = True; /* True means respect size hints in tiled resizals */
#elif USE_WINAPI
static const BOOL resizehints = TRUE; /* True means respect size hints in tiled resizals */
#endif

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* key definitions */
#if USE_XLIB
#define MODKEY Mod1Mask
#elif USE_WINAPI
#define MODKEY 		(MOD_CONTROL | MOD_ALT)
#endif
#if USE_XLIB
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },
#elif USE_WINAPI
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|MOD_CONTROL,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|MOD_SHIFT,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|MOD_CONTROL|MOD_SHIFT, KEY,      toggletag,      {.ui = 1 << TAG} },
#endif

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
#if USE_XLIB
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", font, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbgcolor, "-sf", selfgcolor, NULL };
#elif USE_WINAPI
static const char *dmenucmd[] = { "dmenu.bat", "-m", dmenumon, "-fn", font, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbgcolor, "-sf", selfgcolor, NULL };
#endif
#if USE_XLIB
static const char *termcmd[]  = { "st", NULL };
#elif USE_WINAPI
static const char *termcmd[]  = { "cmd.exe", NULL };
#endif

#if USE_XLIB
static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};
#elif USE_WINAPI
static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       0x50/*XK_p*/,      spawn,          {.v = dmenucmd } },
	{ MODKEY|MOD_SHIFT,             VK_RETURN/*XK_Return*/, spawn,          {.v = termcmd } },
	{ MODKEY,                       0x42/*XK_b*/,      togglebar,      {0} },
	{ MODKEY,                       0x4A/*XK_j*/,      focusstack,     {.i = +1 } },
	{ MODKEY,                       0x4B/*XK_k*/,      focusstack,     {.i = -1 } },
	{ MODKEY,                       0x49/*XK_i*/,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       0x44/*XK_d*/,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       0x48/*XK_h*/,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       0x4C/*XK_l*/,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       VK_RETURN/*XK_Return*/, zoom,           {0} },
	{ MODKEY,                       VK_TAB/*XK_Tab*/,    view,           {0} },
	{ MODKEY|MOD_SHIFT,             0x43/*XK_c*/,      killclient,     {0} },
	{ MODKEY,                       0x54/*XK_t*/,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       0x46/*XK_f*/,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       0x4D/*XK_m*/,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       VK_SPACE/*XK_space*/,  setlayout,      {0} },
	{ MODKEY|MOD_SHIFT,             VK_SPACE/*XK_space*/,  togglefloating, {0} },
	{ MODKEY,                       0x4E/*n*/,       toggleborder,        {0} },
	{ MODKEY,                       0x45/*e*/,       toggleexplorer,      {0} },
	{ MODKEY,                       0x30/*XK_0*/,      view,           {.ui = ~0 } },
	{ MODKEY|MOD_SHIFT,             0x30/*XK_0*/,      tag,            {.ui = ~0 } },
	{ MODKEY,                       VK_OEM_COMMA/*XK_comma*/,  focusmon,       {.i = -1 } },
	{ MODKEY,                       VK_OEM_PERIOD/*XK_period*/, focusmon,       {.i = +1 } },
	{ MODKEY|MOD_SHIFT,             VK_OEM_COMMA/*XK_comma*/,  tagmon,         {.i = -1 } },
	{ MODKEY|MOD_SHIFT,             VK_OEM_PERIOD/*XK_period*/, tagmon,         {.i = +1 } },
	TAGKEYS(                        0x31/*XK_1*/,                      0)
	TAGKEYS(                        0x32/*XK_2*/,                      1)
	TAGKEYS(                        0x33/*XK_3*/,                      2)
	TAGKEYS(                        0x34/*XK_4*/,                      3)
	TAGKEYS(                        0x35/*XK_5*/,                      4)
	TAGKEYS(                        0x36/*XK_6*/,                      5)
	TAGKEYS(                        0x37/*XK_7*/,                      6)
	TAGKEYS(                        0x38/*XK_8*/,                      7)
	TAGKEYS(                        0x39/*XK_9*/,                      8)
	{ MODKEY|MOD_SHIFT,             0x51/*XK_q*/,      quit,           {0} },
};
#endif

/* button definitions */
/* click can be ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
#if USE_XLIB
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
#elif USE_WINAPI
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              WM_LBUTTONDOWN,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              WM_RBUTTONDOWN,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              WM_MBUTTONDOWN,        zoom,           {0} },
	{ ClkStatusText,        0,              WM_MBUTTONDOWN,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         WM_LBUTTONDOWN,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         WM_MBUTTONDOWN,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         WM_RBUTTONDOWN,        resizemouse,    {0} },
	{ ClkTagBar,            0,              WM_LBUTTONDOWN,        view,           {0} },
	{ ClkTagBar,            0,              WM_RBUTTONDOWN,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         WM_LBUTTONDOWN,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         WM_RBUTTONDOWN,        toggletag,      {0} },
#endif
};

