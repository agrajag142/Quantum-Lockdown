Files put here are copied into the app (`/app0/assets` on the console) by
`build.sh`. (This README is left out.)

- `elevator.mod` - the tune while he stands in the lift car
- `stage1.mod` - stage 1's tune, the rest of the time

ProTracker MODs. Without one, that part is silent and the title screen says
so. Their levels are `ELEVATOR_MUSIC_VOLUME` and `STAGE1_MUSIC_VOLUME` at the
top of `src/game.cpp`.

To try a new tune without building again, copy it over FTP straight into the
installed app: `/data/homebrew/PPSA99903/assets/` on the console.

(`.mod` files are left out of the git repository - `.gitignore` - so the tune
stays on your PC.)
