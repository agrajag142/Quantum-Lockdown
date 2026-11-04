/*
   A fourth, independent copy of the Micromod MOD player (micromod.c keeps its
   state in file-level variables, so one copy plays one tune): this one plays
   stage 2's tune (stage2.mod).
   Everything is the same as micromod.c; only the names change (micromod4_...).
*/
#define micromod_get_version            micromod4_get_version
#define micromod_calculate_mod_file_len micromod4_calculate_mod_file_len
#define micromod_initialise             micromod4_initialise
#define micromod_get_string             micromod4_get_string
#define micromod_calculate_song_duration micromod4_calculate_song_duration
#define micromod_set_position           micromod4_set_position
#define micromod_mute_channel           micromod4_mute_channel
#define micromod_set_gain               micromod4_set_gain
#define micromod_get_audio              micromod4_get_audio
#include "micromod.c"
