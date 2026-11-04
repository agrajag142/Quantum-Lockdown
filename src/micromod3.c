/*
   A third, independent copy of the Micromod MOD player (micromod.c keeps its
   state in file-level variables, so one copy plays one tune): this one plays
   the title screen's tune (title.mod).
   Everything is the same as micromod.c; only the names change (micromod3_...).
*/
#define micromod_get_version            micromod3_get_version
#define micromod_calculate_mod_file_len micromod3_calculate_mod_file_len
#define micromod_initialise             micromod3_initialise
#define micromod_get_string             micromod3_get_string
#define micromod_calculate_song_duration micromod3_calculate_song_duration
#define micromod_set_position           micromod3_set_position
#define micromod_mute_channel           micromod3_mute_channel
#define micromod_set_gain               micromod3_set_gain
#define micromod_get_audio              micromod3_get_audio
#include "micromod.c"
