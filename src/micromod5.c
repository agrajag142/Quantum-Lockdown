/*
   A fifth, independent copy of the Micromod MOD player (micromod.c keeps its
   state in file-level variables, so one copy plays one tune): this one plays
   the good game over's tune (gameovergood.mod).
   Everything is the same as micromod.c; only the names change (micromod5_...).
*/
#define micromod_get_version            micromod5_get_version
#define micromod_calculate_mod_file_len micromod5_calculate_mod_file_len
#define micromod_initialise             micromod5_initialise
#define micromod_get_string             micromod5_get_string
#define micromod_calculate_song_duration micromod5_calculate_song_duration
#define micromod_set_position           micromod5_set_position
#define micromod_mute_channel           micromod5_mute_channel
#define micromod_set_gain               micromod5_set_gain
#define micromod_get_audio              micromod5_get_audio
#include "micromod.c"
