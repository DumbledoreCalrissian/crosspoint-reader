# Reading calendar

The Desk Home theme shows a monthly reading calendar, total minutes read this
month and a streak of days meeting your reading goal. The widget updates when you enter Home and is display-only;
buttons continue to select books and the app menu.

Choose **Settings → Reader → Daily reading goal**. The default is 30 minutes;
choices are 5, 10, 15, 20, 30, 45 and 60 minutes. Reading in short bursts adds up
throughout the day, across EPUB books. A calendar day is marked when its total
reaches the goal. Monthly minutes include qualifying reading on days below the
goal, too, and are rounded down after adding the month's time.

Changing the goal applies to today and future days. Past days keep their recorded
goals. Without a trusted date, a goal edit sets the goal for future dated reading
and does not rewrite stored days. A streak continues across months; yesterday's streak remains visible
until today ends, giving you today to meet your goal.

This uses the existing EPUB reading-time rules: visible-page stays of two seconds
to five minutes count; menus, loading and longer idle stays do not. Other formats
are unchanged. Daily history starts with this feature; old cumulative book totals
cannot reconstruct past reading dates. Resetting a book's statistics does not
reset the calendar.

## Clock and saves

Sync the device clock before using the calendar. The widget is hidden when the
clock is untrusted or history cannot be read. Reading without a trusted date
still counts toward book statistics, but cannot be assigned to a calendar day.
Large clock corrections and timezone/DST changes during a page stay can exclude
that stay from daily history; book totals are unaffected. If the local date is
before the newest recorded day (for example after correcting an accidentally
advanced clock), the calendar stays hidden and daily tracking pauses until the
date catches up. Existing history is preserved.

Daily history and the chosen goal are saved together on the SD card in
`/.crosspoint/reading-calendar-a.bin` and `reading-calendar-b.bin`. Keep both when
backing up the card. The two copies protect against an interrupted save; they do
not replace a backup on another device. Daily details retain a rolling 366 days;
a carried streak count allows a longer uninterrupted streak to continue.

A failed goal save keeps the previous setting and shows an error. Check the clock and card
access, then retry. Reader save failures are logged and retried while that reader
is open, including on exit; a power cut or leaving after repeated save failures
can lose unsaved time. Per-book statistics and calendar saves are separate.

If neither calendar copy is readable, back up both before restoring a good copy.
To deliberately start fresh with the default goal, move both calendar files out
of `/.crosspoint/`, then restart. Leave the per-book `reading-stats/` folder and
other application files alone.
