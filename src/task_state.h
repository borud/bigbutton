#ifndef TASK_STATE_H
#define TASK_STATE_H

// Initialize the state task. Creates mutex semaphore.
void init_state_task();

// Task for reading the pill state.  Performs HTTP GET on SERVER_DISPLAY_ENDPOINT
// and indirectly updates the minutesSinceLastPill through the callback
// client_event_get_handler.
void read_pill_state_task(void *arg);

// returns the minutes since last pill.
int get_minutes_since_last_pill();

// reset the minutes since last pill to zero.
void reset_minutes_since_last_pill();

#endif
