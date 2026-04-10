#include <stdio.h>
#include <assert.h>
#include "raylib.h"
#include "k08midi.h"
#include "interface.h"
#include "state.h"

State state;

// Copy Paste συνάρτησης από /programs/midi/midi.c η οποία αναπαράγει όλα τα events που δίνονται από την state_playback_events
// προς στον χρήστη καθώς και τυπώνει αυτά τα events στο τερματικό
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

// Συνάρτηση πυρήνας που κάνει όλη την δουλειά όσο αφορά την συνεχής ενημέρωση του παιχνιδιού
void update_and_draw() {
	double elapsed_seconds = GetFrameTime(); // O χρόνος που περνά είναι ανάλογα με το frame

	struct key_state key = {0}; // Το struct πλήκτρο που αντιπροσωπεύει όλα τα πλήκτρα με το οποίο θα ενημερώνετε συνεχώς η state_update
	
	if (IsKeyPressed(KEY_SPACE)) { // Εαν πατηθεί το space από τον χρήστη, ενημερώνουμε το key.space
            key.space = true;
        }
	
	if (IsKeyPressed(KEY_N)) { // Αντίστοιχα με το space γίνετε και για το κουμπί Ν
			key.n = true;
		}

	// Πίνακας με συγκεκριμένα κουμπιά του πληκτρολογίου που αντιπροσωπεύουν τα πλήκτρα ενός κανονικού πιάνου
	int keys_to_notes [][2] = {  
		{KEY_A, 0}, {KEY_W, 1}, {KEY_S, 2},
		{KEY_E, 3}, {KEY_D, 4}, {KEY_F, 5},
		{KEY_T, 6}, {KEY_G, 7}, {KEY_Y, 8},
		{KEY_H, 9}, {KEY_U, 10}, {KEY_J, 11},
		{KEY_K, 12}, {KEY_O, 13}, {KEY_L, 14},
		{KEY_P, 15}, {KEY_SEMICOLON, 16}, {KEY_APOSTROPHE, 17},
		{KEY_RIGHT_BRACKET, 18}, {KEY_BACKSLASH, 19},
	};

	// Σε αυτό το σημείο βρίσκω από πιο σημείο θα σχεδιαστούν τα γράμματα των πλήκτρων που θα φαίνονται στον χρήστη
	int num_keys = sizeof(keys_to_notes) / sizeof(keys_to_notes[0]); // Αριθμός των πλήκτρων

	static int note_base = 60; // Κεντρική νότα Ντο του πιάνου σε μεσαία οκτάβα την οποία χρησιμοποιώ σαν βάση

	List list = state_displayed_notes(state, 3.0); // Λίστα με όλες τις νότες που έχουν παιχτεί τα τελευτέα 3 δευτερόλεπτα

	if (list_size(list) > 0) { // Εαν υπάρχουν έστω και λίγες νότες
		int min = 127;

		for (ListNode node = list_first(list);
			 node != LIST_EOF;
			 node = list_next(list, node)) {
				MidiEvent event = list_node_value(list, node);

				if ((int)event->key < min) {
					min = event->key;
				}
			 }
		if (min < 127) {
			note_base = (min / 12) * 12; // Βρίσκω σε ποια οκτάβα να ενεργοποιήσω τα πλήκρα
		}
	}

	list_destroy(list); // Αφού βρίκα σε πιο σημείο του πιάνου να σχεδιάσω τα γράμματα, η λίστα μου είναι πλέον αχρίαστη για
						// αυτό και την καταστρέφω για να απογύφω και memory leaks

	int channel = state_playing_channel(state); // Το channel που βρίσκετε το κομμάτι την συγκεκριμένη στιγμή

	// Σε αυτή την αναδρομή όλων των πλήκτρων, ενημερώνετε η κατάσταση του κύριου key μου με βάση όσα κουμπιά πάτησε ο χρήστης για να τα στείλει στην state_update
	for (int k = 0 ; k < num_keys ; k++) {
		int key_now = keys_to_notes[k][0];
		int note = keys_to_notes[k][1] + note_base; // νότα στην σωστή οκτάβα που παίζετε με βάση το κομμάτι

		// Εαν ο χρήστης πατά συνεχώμενα κουμπί σε νότα που είναι ίδη ενεργοποιημένη, συνεχήζει η νότα να αναπαράγει ήχο
		if (IsKeyDown(key_now)) {
			key.active_keys[note] = 1;
		}

		// Ότι κουμπί πάτησε ο χρήστης από αυτά που αντιπροσωπεύουν τα πλήκτρα του πιάνου, ενεργοποιήτε και η αντίστηχη νότα και αναπαράγει ήχο
		if (IsKeyPressed(key_now)) {
			key.changed_keys[note] = 1;
			k08midi_synth_note(channel, note, 127);
		}

		// Όταν ο χρήστης σταματήσει να πατάει το κουμπί-πλήκτρο του πιάνου, η νότα απενεργοποιήτε και σταματά τον ήχο
		if (IsKeyReleased(key_now)) {
			key.changed_keys[note] = 1;
			k08midi_synth_note(channel, note, 0);
		}
	}
	
	state_update(state, &key, elapsed_seconds); // Αφού ενημερώθηκε η κατάσταση όλων των κουμπιών του πληκτρολογίου που χρησιμοποιούμαι, καλούμε
												// την state_update για να ενημερώσουμε και την κατάσταση του παιχνιδιού
	
	// Σε αυτό το σημείο χρησιμοποιούμε την λίστα state_playback_events για αναπαράξουμε τις νότες μιας χρονικής διάρκειας στην οθόνη του χρήστη
	List next_event = state_playback_events(state, elapsed_seconds);

	for (ListNode node = list_first(next_event);
		 node != LIST_EOF;
		 node = list_next(next_event, node)) {
			MidiEvent event = list_node_value(next_event, node);
            dispatch_event(event); // Η συνάρτηση που στέλνει όλα τα events προς αναπαραγωγή 
		 }

	StateInfo info = state_info(state); // Πληροφορίες όλου του παιχνιδιού για να γνωρίζουμε σε τι κατάσταση βρίσκετε

	//  Reset αν πατηθεί R ή εαν τελείωσε το παιχνίδι για να το κάνει reset και να ξαναπαίξει
	if (IsKeyPressed(KEY_R) || (IsKeyPressed(KEY_ENTER) && info->game_over == true)) {
            k08midi_synth_note_off_all(); // Σταματάμε όλες τις νότες
			state_destroy(state); // Καταστροφή του τωρινού state για απογυφή memory leak 
			state = state_create("./assets/back-to-black.mid"); // και δημιουργία νέου καθαρού state
			GetFrameTime();
        }

	list_destroy(next_event); // Αφού έχουν παιχτεί όλα τα events, καταστρέφουμε την λίστα για να μην μασ δημιουργήση memory leak

	// Επειδή τα κουμπιά λειτουργούν σαν μοχλούς, εαν πατηθούν μια φορά από τον χρήστη, πρέπει να τα απενεργοποιήσω εγώ manually
	if (key.space == true) {
		key.space = false;
	}

	if (key.n == true) {
		key.n = false;
	}

	interface_draw_frame(state); // Η συνάρτηση στο interface.c που αναπαράγει τα γραφικά του παιχνιδιού για να μπορεί να παίξει ο χρήστης
}

int main() {
	// Αρχικοποίηση του software synthesizer, χρησιμοποιώντας ένα soundfont
	// (.sf3) που περιέχει samples από όλα τα βασικά MIDI όργανα.
    bool synth_ok = k08midi_synth_init("./assets/GeneralUserGS.sf3");
    assert(synth_ok);

	state = state_create("./assets/back-to-black.mid"); // Δημιουργία του state με βάση το τραφούδι που θα παίξει ο χρήστης
	assert(state != NULL);

	interface_init(); // Αρχικοποίηση του interface

	// Η κλήση αυτή καλεί συνεχόμενα την update_and_draw μέχρι ο χρήστης να κλείσει το παράθυρο
	start_main_loop(update_and_draw);

	interface_close(); // Αφού τελειώσει ο χρήστης με το παιχνίδι και κλείσει το παράθυρο, απενεργοποιώ και τα γραφικά

	k08midi_synth_note_off_all(); // Απενεργοποίηση όλων των νοτών
	k08midi_synth_close(); // Απενεργοποιήση όλου του synthesizer

	state_destroy(state); // Καταστροφή της state για αποφυγή memory leak

	return 0;
}



