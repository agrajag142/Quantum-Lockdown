Files put here are copied into the app (`/app0/assets` on the console) by
`build.sh`. (This README is left out.)

- `elevator.mod` - the tune that plays while he is in the lift area (a
  ProTracker MOD). Without it the game is silent and the title screen says so.
  Its level is `ELEVATOR_MUSIC_VOLUME` at the top of `src/game.cpp`.

To try a new tune without building again, copy it over FTP straight into the
installed app: `/data/homebrew/PPSA99903/assets/elevator.mod` on the console.

(`.mod` files are left out of the git repository - `.gitignore` - so the tune
stays on your PC.)
