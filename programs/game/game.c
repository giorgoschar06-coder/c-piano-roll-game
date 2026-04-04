#include <stdio.h>
#include <assert.h>
#include "raylib.h"
#include "k08midi.h"
#include "interface.h"
#include "state.h"

State state;

// Αναπαραγωγή του event, χρησιμοποιώντας τις k08midi_synth_* συναρτήσεις.
static void dispatch_event(MidiEvent event) {
    switch (event->type) {
        case MIDI_NOTE:
            k08midi_synth_note(event->channel, event->key, event->velocity);
            printf("[MIDI_NOTE] t=%.3f ch=%u key=%u velocity=%u\n",
                   event->time, event->channel, event->key, event->velocity);
            break;

        case MIDI_CONTROL_CHANGE:
            k08midi_synth_set_control(event->channel, event->control, event->control_value);
            printf("[MIDI_CONTROL_CHANGE] t=%.3f ch=%u control=%u value=%u\n",
                   event->time, event->channel, event->control, event->control_value);
            break;

        case MIDI_PROGRAM_CHANGE:
            // Τα drums δεν ρυθμίζονται από το program number, όπως τα περισσότερα όργανα,
            // αλλά βρίσκονται από σύμβαση στο channel 10 (id 9).
            k08midi_synth_set_program(event->channel, event->program, event->channel == 9);
            printf("[MIDI_PROGRAM_CHANGE] t=%.3f ch=%u program=%u\n",
                   event->time, event->channel, event->program);
            break;

        case MIDI_MARKER:
            printf("[MIDI_MARKER] t=%.3f text=\"%s\"\n",
                   event->time, event->marker != NULL ? event->marker : "");
            break;

        default:
            break;
    }
}

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
	
	List next_event = state_playback_events(state, elapsed_seconds);

	for (ListNode node = list_first(next_event);
		 node != LIST_EOF;
		 node = list_next(next_event, node)) {
			MidiEvent event = list_node_value(next_event, node);
            dispatch_event(event);
		 }

	//  Reset αν πατηθεί R
	//if (IsKeyPressed(KEY_R) || next_event == LIST_EOF) {
    //	       elapsed_seconds = 0.0;
    //        next_event = list_first(next_event);
    //        k08midi_synth_note_off_all();
    //    }

	list_destroy(next_event);

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

	k08midi_synth_note_off_all();
	k08midi_synth_close();

	state_destroy(state);

	return 0;
}



