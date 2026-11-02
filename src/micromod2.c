/*
   A second, independent copy of the Micromod MOD player (micromod.c keeps its
   state in file-level variables, so one copy plays one tune). With two, the
   lift's tune and the stage's tune each keep their place while the other plays.
   Everything is the same as micromod.c; only the names change (micromod2_...).
*/
#define micromod_get_version            micromod2_get_version
#define micromod_calculate_mod_file_len micromod2_calculate_mod_file_len
#define micromod_initialise             micromod2_initialise
#define micromod_get_string             micromod2_get_string
#define micromod_calculate_song_duration micromod2_calculate_song_duration
#define micromod_set_position           micromod2_set_position
#define micromod_mute_channel           micromod2_mute_channel
#define micromod_set_gain               micromod2_set_gain
#define micromod_get_audio              micromod2_get_audio
#include "micromod.c"
