#pragma once

#include <ncurses.h>
#include "imap.h"

void action_transfer(struct ImapServer src, struct ImapServer dst, WINDOW *win);
void action_validate(struct ImapServer src, struct ImapServer dst, WINDOW *win);
void action_clear(struct ImapServer src, struct ImapServer dst, WINDOW *win);
void run_action(struct ImapServer src, struct ImapServer dst, int action);
