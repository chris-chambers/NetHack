/* NetHack 3.6	winmisc.c	$NHDT-Date: 1452593730 2016/01/12 10:15:30 $  $NHDT-Branch: NetHack-3.6.0 $:$NHDT-Revision: 1.15 $ */
/* Copyright (c) Dean Luick, 1992                                 */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Misc. popup windows: player selection and extended commands.
 *
 *      + Global functions: player_selection() and get_ext_cmd().
 */

#ifndef SYSV
#define PRESERVE_NO_SYSV /* X11 include files may define SYSV */
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xos.h> /* for index() */
#include <X11/Xatom.h>

#ifdef PRESERVE_NO_SYSV
#ifdef SYSV
#undef SYSV
#endif
#undef PRESERVE_NO_SYSV
#endif

#include "hack.h"
#include "func_tab.h"
#include "winX.h"

static Widget extended_command_popup = 0;
static Widget extended_command_form;
static Widget *extended_commands = 0;
static int extended_command_selected; /* index of the selected command; */
static int ps_selected;               /* index of selected role */
#define PS_RANDOM (-50)
#define PS_QUIT (-75)
static const char ps_randchars[] = "*@";
static const char ps_quitchars[] = "\033qQ";

#define EC_NCHARS 32
static boolean ec_active = FALSE;
static int ec_nchars = 0;
static char ec_chars[EC_NCHARS];
static Time ec_time;

static const char extended_command_translations[] = "#override\n\
     <Key>Left: scroll(4)\n\
     <Key>Right: scroll(6)\n\
     <Key>Up: scroll(8)\n\
     <Key>Down: scroll(2)\n\
     <Key>: ec_key()";

static const char player_select_translations[] = "#override\n\
     <Key>: ps_key()";
static const char race_select_translations[] = "#override\n\
     <Key>: race_key()";
static const char gend_select_translations[] = "#override\n\
     <Key>: gend_key()";
static const char algn_select_translations[] = "#override\n\
     <Key>: algn_key()";

static void FDECL(popup_delete, (Widget, XEvent *, String *, Cardinal *));
static void NDECL(ec_dismiss);
static Widget FDECL(make_menu,
                    (const char *, const char *, const char *, const char *,
                     XtCallbackProc, const char *, XtCallbackProc, int,
                     const char **, Widget **, XtCallbackProc, Widget *));
static void NDECL(init_extended_commands_popup);
static void FDECL(ps_quit, (Widget, XtPointer, XtPointer));
static void FDECL(ps_random, (Widget, XtPointer, XtPointer));
static void FDECL(ps_select, (Widget, XtPointer, XtPointer));

/* Player Selection --------------------------------------------------------
 */
/* ARGSUSED */
static void
ps_quit(w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{
    nhUse(w);
    nhUse(client_data);
    nhUse(call_data);

    ps_selected = PS_QUIT;
    exit_x_event = TRUE; /* leave event loop */
}

/* ARGSUSED */
static void
ps_random(w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{
    nhUse(w);
    nhUse(client_data);
    nhUse(call_data);

    ps_selected = PS_RANDOM;
    exit_x_event = TRUE; /* leave event loop */
}

/* ARGSUSED */
static void
ps_select(w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{
    nhUse(w);
    nhUse(call_data);

    ps_selected = (int) client_data;
    exit_x_event = TRUE; /* leave event loop */
}

/* ARGSUSED */
void
ps_key(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
    char ch, *mark;
    char rolechars[QBUFSZ];
    int i;

    nhUse(w);
    nhUse(params);
    nhUse(num_params);

    (void) memset(rolechars, '\0', sizeof rolechars); /* for index() */
    for (i = 0; roles[i].name.m; ++i) {
        ch = lowc(*roles[i].name.m);
        /* if (flags.female && roles[i].name.f) ch = lowc(*roles[i].name.f);
         */
        /* this supports at most two roles with the same first letter */
        if (index(rolechars, ch))
            ch = highc(ch);
        rolechars[i] = ch;
    }
    ch = key_event_to_char((XKeyEvent *) event);
    if (ch == '\0') { /* don't accept nul char/modifier event */
        /* don't beep */
        return;
    }
    mark = index(rolechars, ch);
    if (!mark)
        mark = index(rolechars, lowc(ch));
    if (!mark)
        mark = index(rolechars, highc(ch));
    if (!mark) {
        if (index(ps_randchars, ch))
            ps_selected = PS_RANDOM;
        else if (index(ps_quitchars, ch))
            ps_selected = PS_QUIT;
        else {
            X11_nhbell(); /* no such class */
            return;
        }
    } else
        ps_selected = (int) (mark - rolechars);
    exit_x_event = TRUE;
}

/* ARGSUSED */
void
race_key(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
    char ch, *mark;
    char racechars[QBUFSZ];
    int i;

    nhUse(w);
    nhUse(params);
    nhUse(num_params);

    (void) memset(racechars, '\0', sizeof racechars); /* for index() */
    for (i = 0; races[i].noun; ++i) {
        ch = lowc(*races[i].noun);
        /* this supports at most two races with the same first letter */
        if (index(racechars, ch))
            ch = highc(ch);
        racechars[i] = ch;
    }
    ch = key_event_to_char((XKeyEvent *) event);
    if (ch == '\0') { /* don't accept nul char/modifier event */
        /* don't beep */
        return;
    }
    mark = index(racechars, ch);
    if (!mark)
        mark = index(racechars, lowc(ch));
    if (!mark)
        mark = index(racechars, highc(ch));
    if (!mark) {
        if (index(ps_randchars, ch))
            ps_selected = PS_RANDOM;
        else if (index(ps_quitchars, ch))
            ps_selected = PS_QUIT;
        else {
            X11_nhbell(); /* no such race */
            return;
        }
    } else
        ps_selected = (int) (mark - racechars);
    exit_x_event = TRUE;
}

/* ARGSUSED */
void
gend_key(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
    char ch, *mark;
    static char gendchars[] = "mf";

    nhUse(w);
    nhUse(params);
    nhUse(num_params);

    ch = key_event_to_char((XKeyEvent *) event);
    if (ch == '\0') { /* don't accept nul char/modifier event */
        /* don't beep */
        return;
    }
    mark = index(gendchars, ch);
    if (!mark)
        mark = index(gendchars, lowc(ch));
    if (!mark) {
        if (index(ps_randchars, ch))
            ps_selected = PS_RANDOM;
        else if (index(ps_quitchars, ch))
            ps_selected = PS_QUIT;
        else {
            X11_nhbell(); /* no such gender */
            return;
        }
    } else
        ps_selected = (int) (mark - gendchars);
    exit_x_event = TRUE;
}

/* ARGSUSED */
void
algn_key(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
    char ch, *mark;
    static char algnchars[] = "LNC";

    nhUse(w);
    nhUse(params);
    nhUse(num_params);

    ch = key_event_to_char((XKeyEvent *) event);
    if (ch == '\0') { /* don't accept nul char/modifier event */
        /* don't beep */
        return;
    }
    mark = index(algnchars, ch);
    if (!mark)
        mark = index(algnchars, highc(ch));
    if (!mark) {
        if (index(ps_randchars, ch))
            ps_selected = PS_RANDOM;
        else if (index(ps_quitchars, ch))
            ps_selected = PS_QUIT;
        else {
            X11_nhbell(); /* no such alignment */
            return;
        }
    } else
        ps_selected = (int) (mark - algnchars);
    exit_x_event = TRUE;
}

/* Global functions =========================================================
 */
void
X11_player_selection()
{
    int num_roles, num_races, num_gends, num_algns, i, availcount, availindex;
    Widget popup, player_form;
    const char **choices;
    char qbuf[QBUFSZ], plbuf[QBUFSZ];

    /* avoid unnecessary prompts further down */
    rigid_role_checks();

    (void) root_plselection_prompt(plbuf, QBUFSZ - 1, flags.initrole,
                                   flags.initrace, flags.initgend,
                                   flags.initalign);

    while (flags.initrole < 0) {
        if (flags.initrole == ROLE_RANDOM || flags.randomall) {
            flags.initrole = pick_role(flags.initrace, flags.initgend,
                                       flags.initalign, PICK_RANDOM);
            break;
        }

        /* select a role */
        for (num_roles = 0; roles[num_roles].name.m; ++num_roles)
            continue;
        choices = (const char **) alloc(sizeof(char *) * num_roles);
        for (;;) {
            availcount = 0;
            for (i = 0; i < num_roles; i++) {
                choices[i] = 0;
                if (ok_role(i, flags.initrace, flags.initgend,
                            flags.initalign)) {
                    choices[i] = roles[i].name.m;
                    if (flags.initgend >= 0 && flags.female
                        && roles[i].name.f)
                        choices[i] = roles[i].name.f;
                    ++availcount;
                }
            }
            if (availcount > 0)
                break;
            else if (flags.initalign >= 0)
                flags.initalign = -1; /* reset */
            else if (flags.initgend >= 0)
                flags.initgend = -1;
            else if (flags.initrace >= 0)
                flags.initrace = -1;
            else
                panic("no available ROLE+race+gender+alignment combinations");
        }
        Sprintf(qbuf, "Choose your %s Role", s_suffix(plbuf));
        popup =
            make_menu("player_selection", qbuf, player_select_translations,
                      "quit", ps_quit, "random", ps_random, num_roles,
                      choices, (Widget **) 0, ps_select, &player_form);

        ps_selected = -1;
        positionpopup(popup, FALSE);
        nh_XtPopup(popup, (int) XtGrabExclusive, player_form);

        /* The callbacks will enable the event loop exit. */
        (void) x_event(EXIT_ON_EXIT);

        nh_XtPopdown(popup);
        XtDestroyWidget(popup);
        free((genericptr_t) choices), choices = 0;

        if (ps_selected == PS_QUIT || program_state.done_hup) {
            clearlocks();
            X11_exit_nhwindows((char *) 0);
            terminate(0);
        } else if (ps_selected == PS_RANDOM) {
            flags.initrole = ROLE_RANDOM;
        } else if (ps_selected < 0 || ps_selected >= num_roles) {
            panic("player_selection: bad role select value %d", ps_selected);
        } else {
            flags.initrole = ps_selected;
        }
    }

    (void) root_plselection_prompt(plbuf, QBUFSZ - 1, flags.initrole,
                                   flags.initrace, flags.initgend,
                                   flags.initalign);

    while (!validrace(flags.initrole, flags.initrace)) {
        if (flags.initrace == ROLE_RANDOM || flags.randomall) {
            flags.initrace = pick_race(flags.initrole, flags.initgend,
                                       flags.initalign, PICK_RANDOM);
            break;
        }
        /* select a race */
        for (num_races = 0; races[num_races].noun; ++num_races)
            continue;
        choices = (const char **) alloc(sizeof(char *) * num_races);
        for (;;) {
            availcount = availindex = 0;
            for (i = 0; i < num_races; i++) {
                choices[i] = 0;
                if (ok_race(flags.initrole, i, flags.initgend,
                            flags.initalign)) {
                    choices[i] = races[i].noun;
                    ++availcount;
                    availindex = i; /* used iff only one */
                }
            }
            if (availcount > 0)
                break;
            else if (flags.initalign >= 0)
                flags.initalign = -1; /* reset */
            else if (flags.initgend >= 0)
                flags.initgend = -1;
            else
                panic("no available role+RACE+gender+alignment combinations");
        }

        if (availcount == 1) {
            flags.initrace = availindex;
            free((genericptr_t) choices), choices = 0;
        } else {
            Sprintf(qbuf, "Pick your %s race", s_suffix(plbuf));
            popup =
                make_menu("race_selection", qbuf, race_select_translations,
                          "quit", ps_quit, "random", ps_random, num_races,
                          choices, (Widget **) 0, ps_select, &player_form);

            ps_selected = -1;
            positionpopup(popup, FALSE);
            nh_XtPopup(popup, (int) XtGrabExclusive, player_form);

            /* The callbacks will enable the event loop exit. */
            (void) x_event(EXIT_ON_EXIT);

            nh_XtPopdown(popup);
            XtDestroyWidget(popup);
            free((genericptr_t) choices), choices = 0;

            if (ps_selected == PS_QUIT || program_state.done_hup) {
                clearlocks();
                X11_exit_nhwindows((char *) 0);
                terminate(0);
            } else if (ps_selected == PS_RANDOM) {
                flags.initrace = ROLE_RANDOM;
            } else if (ps_selected < 0 || ps_selected >= num_races) {
                panic("player_selection: bad race select value %d",
                      ps_selected);
            } else {
                flags.initrace = ps_selected;
            }
        } /* more than one race choice available */
    }

    (void) root_plselection_prompt(plbuf, QBUFSZ - 1, flags.initrole,
                                   flags.initrace, flags.initgend,
                                   flags.initalign);

    while (!validgend(flags.initrole, flags.initrace, flags.initgend)) {
        if (flags.initgend == ROLE_RANDOM || flags.randomall) {
            flags.initgend = pick_gend(flags.initrole, flags.initrace,
                                       flags.initalign, PICK_RANDOM);
            break;
        }
        /* select a gender */
        num_gends = 2; /* genders[2] isn't allowed */
        choices = (const char **) alloc(sizeof(char *) * num_gends);
        for (;;) {
            availcount = availindex = 0;
            for (i = 0; i < num_gends; i++) {
                choices[i] = 0;
                if (ok_gend(flags.initrole, flags.initrace, i,
                            flags.initalign)) {
                    choices[i] = genders[i].adj;
                    ++availcount;
                    availindex = i; /* used iff only one */
                }
            }
            if (availcount > 0)
                break;
            else if (flags.initalign >= 0)
                flags.initalign = -1; /* reset */
            else
                panic("no available role+race+GENDER+alignment combinations");
        }

        if (availcount == 1) {
            flags.initgend = availindex;
            free((genericptr_t) choices), choices = 0;
        } else {
            Sprintf(qbuf, "Your %s gender?", s_suffix(plbuf));
            popup =
                make_menu("gender_selection", qbuf, gend_select_translations,
                          "quit", ps_quit, "random", ps_random, num_gends,
                          choices, (Widget **) 0, ps_select, &player_form);

            ps_selected = -1;
            positionpopup(popup, FALSE);
            nh_XtPopup(popup, (int) XtGrabExclusive, player_form);

            /* The callbacks will enable the event loop exit. */
            (void) x_event(EXIT_ON_EXIT);

            nh_XtPopdown(popup);
            XtDestroyWidget(popup);
            free((genericptr_t) choices), choices = 0;

            if (ps_selected == PS_QUIT || program_state.done_hup) {
                clearlocks();
                X11_exit_nhwindows((char *) 0);
                terminate(0);
            } else if (ps_selected == PS_RANDOM) {
                flags.initgend = ROLE_RANDOM;
            } else if (ps_selected < 0 || ps_selected >= num_gends) {
                panic("player_selection: bad gender select value %d",
                      ps_selected);
            } else {
                flags.initgend = ps_selected;
            }
        } /* more than one gender choice available */
    }

    (void) root_plselection_prompt(plbuf, QBUFSZ - 1, flags.initrole,
                                   flags.initrace, flags.initgend,
                                   flags.initalign);

    while (!validalign(flags.initrole, flags.initrace, flags.initalign)) {
        if (flags.initalign == ROLE_RANDOM || flags.randomall) {
            flags.initalign = pick_align(flags.initrole, flags.initrace,
                                         flags.initgend, PICK_RANDOM);
            break;
        }
        /* select an alignment */
        num_algns = 3; /* aligns[3] isn't allowed */
        choices = (const char **) alloc(sizeof(char *) * num_algns);
        for (;;) {
            availcount = availindex = 0;
            for (i = 0; i < num_algns; i++) {
                choices[i] = 0;
                if (ok_align(flags.initrole, flags.initrace, flags.initgend,
                             i)) {
                    choices[i] = aligns[i].adj;
                    ++availcount;
                    availindex = i; /* used iff only one */
                }
            }
            if (availcount > 0)
                break;
            else
                panic("no available role+race+gender+ALIGNMENT combinations");
        }

        if (availcount == 1) {
            flags.initalign = availindex;
            free((genericptr_t) choices), choices = 0;
        } else {
            Sprintf(qbuf, "Your %s alignment?", s_suffix(plbuf));
            popup = make_menu("alignment_selection", qbuf,
                              algn_select_translations, "quit", ps_quit,
                              "random", ps_random, num_algns, choices,
                              (Widget **) 0, ps_select, &player_form);

            ps_selected = -1;
            positionpopup(popup, FALSE);
            nh_XtPopup(popup, (int) XtGrabExclusive, player_form);

            /* The callbacks will enable the event loop exit. */
            (void) x_event(EXIT_ON_EXIT);

            nh_XtPopdown(popup);
            XtDestroyWidget(popup);
            free((genericptr_t) choices), choices = 0;

            if (ps_selected == PS_QUIT || program_state.done_hup) {
                clearlocks();
                X11_exit_nhwindows((char *) 0);
                terminate(0);
            } else if (ps_selected == PS_RANDOM) {
                flags.initalign = ROLE_RANDOM;
            } else if (ps_selected < 0 || ps_selected >= num_algns) {
                panic("player_selection: bad alignment select value %d",
                      ps_selected);
            } else {
                flags.initalign = ps_selected;
            }
        } /* more than one alignment choice available */
    }
}

int
X11_get_ext_cmd()
{
    static Boolean initialized = False;

    if (!initialized) {
        init_extended_commands_popup();
        initialized = True;
    }

    extended_command_selected = -1; /* reset selected value */

    positionpopup(extended_command_popup, FALSE); /* center on cursor */
    nh_XtPopup(extended_command_popup, (int) XtGrabExclusive,
               extended_command_form);

    /* The callbacks will enable the event loop exit. */
    (void) x_event(EXIT_ON_EXIT);

    return extended_command_selected;
}

/* End global functions =====================================================
 */

/* Extended Command --------------------------------------------------------
 */
/* ARGSUSED */
static void
extend_select(w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{
    int selected = (int) client_data;

    nhUse(w);
    nhUse(call_data);

    if (extended_command_selected != selected) {
        /* visibly deselect old one */
        if (extended_command_selected >= 0)
            swap_fg_bg(extended_commands[extended_command_selected]);

        /* select new one */
        swap_fg_bg(extended_commands[selected]);
        extended_command_selected = selected;
    }

    nh_XtPopdown(extended_command_popup);
    /* reset colors while popped down */
    swap_fg_bg(extended_commands[extended_command_selected]);
    ec_active = FALSE;
    exit_x_event = TRUE; /* leave event loop */
}

/* ARGSUSED */
static void
extend_dismiss(w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{
    nhUse(w);
    nhUse(client_data);
    nhUse(call_data);

    ec_dismiss();
}

/* ARGSUSED */
static void
extend_help(w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{
    nhUse(w);
    nhUse(client_data);
    nhUse(call_data);

    /* We might need to make it known that we already have one listed. */
    (void) doextlist();
}

/* ARGSUSED */
void
ec_delete(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
    if (w == extended_command_popup) {
        ec_dismiss();
    } else {
        popup_delete(w, event, params, num_params);
    }
}

/* ARGSUSED */
static void
popup_delete(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
    nhUse(event);
    nhUse(params);
    nhUse(num_params);

    ps_selected = PS_QUIT;
    nh_XtPopdown(w);
    exit_x_event = TRUE; /* leave event loop */
}

static void
ec_dismiss()
{
    /* unselect while still visible */
    if (extended_command_selected >= 0)
        swap_fg_bg(extended_commands[extended_command_selected]);
    extended_command_selected = -1; /* dismiss */
    nh_XtPopdown(extended_command_popup);
    ec_active = FALSE;
    exit_x_event = TRUE; /* leave event loop */
}

/* ARGSUSED */
void
ec_key(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
    char ch;
    int i;
    int pass;
    XKeyEvent *xkey = (XKeyEvent *) event;

    nhUse(w);
    nhUse(params);
    nhUse(num_params);

    ch = key_event_to_char(xkey);

    if (ch == '\0') { /* don't accept nul char/modifier event */
        /* don't beep */
        return;
    } else if (ch == '?') {
        extend_help();
        return;
    } else if (index("\033\n\r", ch)) {
        if (ch == '\033') {
            /* unselect while still visible */
            if (extended_command_selected >= 0)
                swap_fg_bg(extended_commands[extended_command_selected]);
            extended_command_selected = -1; /* dismiss */
        }

        nh_XtPopdown(extended_command_popup);
        /* unselect while invisible */
        if (extended_command_selected >= 0)
            swap_fg_bg(extended_commands[extended_command_selected]);

        exit_x_event = TRUE; /* leave event loop */
        ec_active = FALSE;
        return;
    }

    /*
     * If too much time has elapsed, treat current key as starting a new
     * choice, otherwise it is a continuation of the choice in progress.
     * Extra letters might be needed to disambiguate between choices
     * ("ride" vs "rub", for instance), or player may just be typing in
     * the whole word.
     */
    if ((xkey->time - ec_time) > 2500) /* 2.5 seconds */
        ec_active = FALSE;

    if (!ec_active) {
        ec_nchars = 0;
        ec_active = TRUE;
    }

    ec_time = xkey->time;
    ec_chars[ec_nchars++] = ch;
    if (ec_nchars >= EC_NCHARS)
        ec_nchars = EC_NCHARS - 1; /* don't overflow */

    for (pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            /* first pass finished, but no matching command was found */
            /* start a new one with the last char entered */
            if (extended_command_selected >= 0)
                swap_fg_bg(extended_commands[extended_command_selected]);
            extended_command_selected = -1; /* dismiss */
            ec_chars[0] = ec_chars[ec_nchars-1];
            ec_nchars = 1;
        }
        for (i = 0; extcmdlist[i].ef_txt; i++) {
            if (extcmdlist[i].ef_txt[0] == '?')
                continue;

            if (!strncmp(ec_chars, extcmdlist[i].ef_txt, ec_nchars)) {
                if (extended_command_selected != i) {
                    /* I should use set() and unset() actions, but how do */
                    /* I send the an action to the widget? */
                    if (extended_command_selected >= 0)
                        swap_fg_bg(extended_commands[extended_command_selected]);
                    extended_command_selected = i;
                    swap_fg_bg(extended_commands[extended_command_selected]);
                }
                return;
            }
        }
    }
}

/*
 * Use our own home-brewed version menu because simpleMenu is designed to
 * be used from a menubox.
 */
static void
init_extended_commands_popup()
{
    int i, num_commands;
    const char **command_list;

    /* count commands */
    for (num_commands = 0; extcmdlist[num_commands].ef_txt; num_commands++)
        ; /* do nothing */

    /* If the last entry is "help", don't use it. */
    if (strcmp(extcmdlist[num_commands - 1].ef_txt, "?") == 0)
        --num_commands;

    command_list =
        (const char **) alloc((unsigned) num_commands * sizeof(char *));

    for (i = 0; i < num_commands; i++)
        command_list[i] = extcmdlist[i].ef_txt;

    extended_command_popup =
        make_menu("extended_commands", "Extended Commands",
                  extended_command_translations, "dismiss", extend_dismiss,
                  "help", extend_help, num_commands, command_list,
                  &extended_commands, extend_select, &extended_command_form);

    free((char *) command_list);
}

/* -------------------------------------------------------------------------
 */

/*
 * Create a popup widget of the following form:
 *
 *                    popup_label
 *              ----------- ------------
 *              |left_name| |right_name|
 *              ----------- ------------
 *              ------------------------
 *              |       name1          |
 *              ------------------------
 *              ------------------------
 *              |       name2          |
 *              ------------------------
 *                        .
 *                        .
 *              ------------------------
 *              |       nameN          |
 *              ------------------------
 */
static Widget
make_menu(popup_name, popup_label, popup_translations, left_name,
          left_callback, right_name, right_callback, num_names, widget_names,
          command_widgets, name_callback, formp)
const char *popup_name;
const char *popup_label;
const char *popup_translations;
const char *left_name;
XtCallbackProc left_callback;
const char *right_name;
XtCallbackProc right_callback;
int num_names;
const char **widget_names; /* return array of command widgets */
Widget **command_widgets;
XtCallbackProc name_callback;
Widget *formp; /* return */
{
    Widget popup, form, label, above, left, right, view;
    Widget *commands, *curr;
    int i;
    Arg args[8];
    Cardinal num_args;
    Dimension width, other_width, max_width, border_width,
              height, cumulative_height, screen_height;
    int distance, skip;

    commands = (Widget *) alloc((unsigned) num_names * sizeof (Widget));

    num_args = 0;
    XtSetArg(args[num_args], XtNallowShellResize, True); num_args++;
    popup = XtCreatePopupShell(popup_name, transientShellWidgetClass,
                               toplevel, args, num_args);
    XtOverrideTranslations(
        popup, XtParseTranslationTable("<Message>WM_PROTOCOLS: ec_delete()"));

    num_args = 0;
    XtSetArg(args[num_args], XtNforceBars, False); num_args++;
    XtSetArg(args[num_args], XtNallowVert, True); num_args++;
    XtSetArg(args[num_args], XtNtranslations,
             XtParseTranslationTable(popup_translations)); num_args++;
    view = XtCreateManagedWidget("menuformview", viewportWidgetClass, popup,
                                 args, num_args);

    num_args = 0;
    *formp = form = XtCreateManagedWidget("menuform", formWidgetClass, view,
                                          args, num_args);

    /*
     * Get the default distance between objects in the viewport widget.
     * (Something is fishy here:  'distance' ends up being 0 but there
     * is a non-zero gap between the borders of the internal widgets.
     * It matches exactly the default value of 4 for defaultDistance.)
     */
    num_args = 0;
    XtSetArg(args[num_args], nhStr(XtNdefaultDistance), &distance); num_args++;
    XtSetArg(args[num_args], nhStr(XtNborderWidth), &border_width); num_args++;
    XtGetValues(view, args, num_args);
    if (!distance)
        distance = 4;

    /*
     * Create the label.
     */
    num_args = 0;
    XtSetArg(args[num_args], XtNborderWidth, 0); num_args++;
    label = XtCreateManagedWidget(popup_label, labelWidgetClass, form, args,
                                  num_args);

    cumulative_height = 0;
    XtSetArg(args[0], XtNheight, &height);
    XtGetValues(label, args, ONE);
    cumulative_height += distance + height; /* no border for label */

    /*
     * Create the left button.
     */
    num_args = 0;
    XtSetArg(args[num_args], nhStr(XtNfromVert), label); num_args++;
#if 0
    XtSetArg(args[num_args], nhStr(XtNshapeStyle),
                              XmuShapeRoundedRectangle); num_args++;
#endif
    left = XtCreateManagedWidget(left_name, commandWidgetClass, form, args,
                                 num_args);
    XtAddCallback(left, XtNcallback, left_callback, (XtPointer) 0);
    skip = (distance < 4) ? 8 : 2 * distance;

    num_args = 0;
    XtSetArg(args[0], XtNheight, &height);
    XtGetValues(left, args, ONE);
    cumulative_height += distance + height + 2 * border_width;

    /*
     * Create right button.
     */
    num_args = 0;
    XtSetArg(args[num_args], nhStr(XtNfromHoriz), left); num_args++;
    XtSetArg(args[num_args], nhStr(XtNhorizDistance), skip); num_args++;
    XtSetArg(args[num_args], nhStr(XtNfromVert), label); num_args++;
#if 0
    XtSetArg(args[num_args], nhStr(XtNshapeStyle),
                              XmuShapeRoundedRectangle); num_args++;
#endif
    right = XtCreateManagedWidget(right_name, commandWidgetClass, form, args,
                                  num_args);
    XtAddCallback(right, XtNcallback, right_callback, (XtPointer) 0);

    XtInstallAccelerators(form, left);
    XtInstallAccelerators(form, right);

    /*
     * Create and place the command widgets.
     */
    for (i = 0, above = left, curr = commands; i < num_names; i++) {
        if (!widget_names[i])
            continue;
        num_args = 0;
        XtSetArg(args[num_args], nhStr(XtNfromVert), above); num_args++;
        if (above == left) {
            /* if first, we are farther apart */
            XtSetArg(args[num_args], nhStr(XtNvertDistance), skip); num_args++;
            cumulative_height += skip;
        } else
            cumulative_height += distance;
        cumulative_height += height + 2 * border_width;

        *curr = XtCreateManagedWidget(widget_names[i], commandWidgetClass,
                                      form, args, num_args);
        XtAddCallback(*curr, XtNcallback, name_callback, (XtPointer) i);
        above = *curr++;
    }
    cumulative_height += distance; /* space at bottom of form */

    /*
     * Now find the largest width.  Start with width of left + right buttons
     * ('dismiss' + 'help' or 'quit' + 'random'), since they are adjacent.
     */
    XtSetArg(args[0], XtNwidth, &max_width);
    XtGetValues(left, args, ONE);
    XtSetArg(args[0], XtNwidth, &width);
    XtGetValues(right, args, ONE);
    /* doesn't count leftmost 'distance + border_width' and
       rightmost 'border_width + distance' since all entries have those */
    max_width = max_width + border_width + skip + border_width + width;

    /* Next, the title. */
    XtSetArg(args[0], XtNwidth, &width);
    XtGetValues(label, args, ONE);
    if (width > max_width)
        max_width = width;

    /* Finally, the commands. */
    for (i = 0, curr = commands; i < num_names; i++) {
        if (!widget_names[i])
            continue;
        XtSetArg(args[0], XtNwidth, &width);
        XtGetValues(*curr, args, ONE);
        if (width > max_width)
            max_width = width;
        curr++;
    }

    /*
     * Re-do the two side-by-side widgets to take up half the width each.
     *
     * With max_width and skip both having even values, we never have to
     * tweak left or right to maybe be one pixel wider than the other.
     */
    if (max_width % 2)
        ++max_width;
    XtSetArg(args[0], XtNwidth, &width);
    XtGetValues(left, args, ONE);
    XtSetArg(args[0], XtNwidth, &other_width);
    XtGetValues(right, args, ONE);
    if (width + border_width + skip / 2 < max_width / 2
        && other_width + border_width + skip / 2 < max_width / 2) {
        /* both are narrower than half */
        width = other_width = max_width / 2 - border_width - skip / 2;
        XtSetArg(args[0], XtNwidth, width);
        XtSetValues(left, args, ONE);
        XtSetArg(args[0], XtNwidth, other_width);
        XtSetValues(right, args, ONE);
    } else if (width + border_width + skip / 2 < max_width / 2) {
        /* 'other_width' (right) is half or more */
        width = max_width - other_width - 2 * border_width - skip;
        XtSetArg(args[0], XtNwidth, width);
        XtSetValues(left, args, ONE);
    } else if (other_width + border_width + skip / 2 < max_width / 2) {
        /* 'width' (left) is half or more */
        other_width = max_width - width - 2 * border_width - skip;
        XtSetArg(args[0], XtNwidth, other_width);
        XtSetValues(right, args, ONE);
    } else {
        ; /* both are exactly half... */
    }

    /*
     * Finally, set all of the single line widgets to the largest width.
     */
    XtSetArg(args[0], XtNwidth, max_width);
    XtSetValues(label, args, ONE);

    for (i = 0, curr = commands; i < num_names; i++) {
        if (!widget_names[i])
            continue;
        XtSetArg(args[0], XtNwidth, max_width);
        XtSetValues(*curr, args, ONE);
        curr++;
    }

    if (command_widgets)
        *command_widgets = commands;
    else
        free((char *) commands);

    /*
     * We actually want height of topmost background window, which
     * may or may not be the root window.
     *
     * On OSX, screen height includes the space taken up by the
     * desktop title bar, which isn't accessible to applications
     * unless the preference settings for X11 are changed to force
     * full-screen mode (so by default, this 'screen_height' value
     * ends up being bigger than the available size...).
     */
    screen_height = XHeightOfScreen(XtScreen(popup));

    /*
     * If the menu's complete height is too big for the display,
     * forcing the height to be smaller will cause the vertical
     * scroll bar (enabled but not forced above) to be included.
     */
    if (cumulative_height >= screen_height) {
        /* trial and error:  25 is a guesstimate for scrollbar width on
           width adjustment and for title bar height on height adjustment */
        num_args = 0;
        XtSetArg(args[num_args], XtNwidth, max_width + 25); num_args++;
        XtSetArg(args[num_args], XtNheight, screen_height - 25); num_args++;
        XtSetValues(popup, args, num_args);
    }
    XtRealizeWidget(popup);
    XSetWMProtocols(XtDisplay(popup), XtWindow(popup), &wm_delete_window, 1);

    return popup;
}
