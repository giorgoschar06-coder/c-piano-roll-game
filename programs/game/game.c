#include <stdio.h>
#include <assert.h>
#include "raylib.h"
#include "k08midi.h"
#include "interface.h"
#include "state.h"

State state;

void update_and_draw() {
	double elapsed_seconds = GetFrameTime();

	struct key_state key = {0};
	
	if (IsKeyPressed(KEY_SPACE)) {
            key.space = true;
        }
	
	if (IsKeyPressed(KEY_N)) {
			key.n = true;
		}

	state_update(state, &key, elapsed_seconds);
	
	if (key.space == true) {
		key.space = false;
	}

	if (key.n == true) {
		key.n = false;
	}

	interface_draw_frame(state);
}

int main() {
	// Αρχικοποίηση του software synthesizer, χρησιμοποιώντας ένα soundfont
	// (.sf3) που περιέχει samples από όλα τα βασικά MIDI όργανα.
    bool synth_ok = k08midi_synth_init("./assets/GeneralUserGS.sf3");
    assert(synth_ok);

	state = state_create("./assets/back-to-black.mid");
	assert(state != NULL);

	interface_init();

	// Η κλήση αυτή καλεί συνεχόμενα την update_and_draw μέχρι ο χρήστης να κλείσει το παράθυρο
	start_main_loop(update_and_draw);

	interface_close();

	return 0;
}



