/*
   A sixth, independent copy of the Micromod MOD player (micromod.c keeps its
   state in file-level variables, so one copy plays one tune): this one plays
   the bad game over's tune (gameoverbad.mod).
   Everything is the same as micromod.c; only the names change (micromod6_...).
*/
#define micromod_get_version            micromod6_get_version
#define micromod_calculate_mod_file_len micromod6_calculate_mod_file_len
#define micromod_initialise             micromod6_initialise
#define micromod_get_string             micromod6_get_string
#define micromod_calculate_song_duration micromod6_calculate_song_duration
#define micromod_set_position           micromod6_set_position
#define micromod_mute_channel           micromod6_mute_channel
#define micromod_set_gain               micromod6_set_gain
#define micromod_get_audio              micromod6_get_audio
#include "micromod.c"
