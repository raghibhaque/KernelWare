#ifndef KW_GAMES_H
#define KW_GAMES_H

int kw_game_start(int game_id);
void kw_game_stop(void);
void kw_set_timer_ms(unsigned int ms);
extern unsigned int timer_duration_ms;

void kw_game_handle_input(unsigned char event);


// Game input functions
void pipe_drain(void);

void rotbrain_start(void);
int rotbrain_check_answer(const char *input);
int kw_games_pick(int prev);

#endif
