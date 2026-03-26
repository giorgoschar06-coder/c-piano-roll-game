#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ADTList.h"
#include "ADTVector.h"
#include "k08midi.h"
#include "state.h"


// Οι ολοκληρωμένες πληροφορίες της κατάστασης του παιχνιδιού.
// Ο τύπος State είναι pointer σε αυτό το struct, αλλά το ίδιο το struct
// δεν είναι ορατό στον χρήστη.

struct state {
	struct state_info info;  // Γενικές πληροφορίες για την κατάσταση του παιχνιδιού
	MidiFile midi_file;      // Το .mid αρχείο που διαβάσαμε με τη k08midi_file_read
	List midi_events;        // MIDI events προς αναπαραγωγή
	Vector clips;            // Clip προς ηχογράφηση
	uint recording_index;    // index του τρέχοντος (ή επόμενου) clip
};

// Τμήμα του τραγουδιού στο οποίο υπάρχει marker "rec" και ο χρήστης πρέπει να
// ηχογραφήσει, παίζοντας τις νότες από το αντίστοιχο channel του τραγουδιού. Το
// ηχογραφημένο τμήμα αναπαράγεται σε loop στα σημεία που υπάρχουν οι
// αντίστοιχοι "play" markers.

typedef struct clip {
	String name;		// όνομα
	int channel;		// MIDI κανάλι
	double start;		// χρονικό σημείο έναρξης
	double duration;	// διάρκεια
	List recorded;  	// Ηχογραφημένα MIDI_NOTE events
	List plays;     	// Λίστα με τα σημεία αναπαραγωγής του clip (τύπος ClipPlay)
}* Clip;

// Τμήμα αναπαραγωγής ενός clip, που προκύπτει από τα "play" markers.

typedef struct clip_play {
	double start;		// χρονικό σημείο έναρξης
	double duration;	// διάρκεια
}* ClipPlay;


// Bοηθητικές συναρτήσεις /////////////////////////////////////////////////////////////////////////////////
//
// Δημιουργεί και επιστρέφει ένα σημείο αναπαραγωγής clip
static ClipPlay create_clip_play(double start, double duration) {
	ClipPlay play = malloc(sizeof(*play));
	play->start = start;
	play->duration = duration;
	return play;
}

// Δημιουργεί και επιστρέφει ένα clip
static Clip create_clip(State state, String name, int channel, double start, double duration) {
	Clip clip = malloc(sizeof(*clip));
	clip->name = strdup(name);
	clip->channel = channel;
	clip->start = start;
	clip->duration = duration;
	clip->recorded = list_create(free);
	clip->plays = list_create(free);
	return clip;
}

// Καταστρέφει ένα clip
static void destroy_clip(Pointer value) {
	Clip clip = value;
	free(clip->name);
	list_destroy(clip->recorded);
	list_destroy(clip->plays);
	free(clip);
}

// Βρίσκει και επιστρέφει ένα clip με βάση το όνομά του, NULL αν δεν υπάρχει.
static Clip find_clip_by_name(State state, String clip_name) {
	for (int ci = 0; ci < vector_size(state->clips); ci++) {
		Clip clip = vector_get_at(state->clips, ci);
		if (strcmp(clip->name, clip_name) == 0)
			return clip;
	}
	return NULL;
}

// Αντιγράφει στο state->midi_events όλα τα PROGRAM_CHANGE (αλλαγή οργάνου) και
// CONTROL_CHANGE (έλεγχος volume, modulation, sustain, κλπ), ώστε να είναι
// έτοιμα για αναπαραγωγή μαζί με τις ηχογραφημένες νότες. 
static void append_song_control_events(State state) {
	for (ListNode node = list_first(state->midi_file->events); node != LIST_EOF; node = list_next(state->midi_file->events, node)) {
		MidiEvent event = list_node_value(state->midi_file->events, node);
		if (event->type == MIDI_CONTROL_CHANGE || event->type == MIDI_PROGRAM_CHANGE) {
			MidiEvent clone = malloc(sizeof(*clone));
			*clone = *event; // clone
			list_insert_next(state->midi_events, list_last(state->midi_events), clone);
		}
	}
}

// Δημιουργεί clips και clip_plays με βάση τα markers του αρχείου MIDI
static void create_clips(State state) {
	state->clips = vector_create(0, destroy_clip);
	double measure_duration = state_measure_duration(state);

	for (ListNode node = list_first(state->midi_file->events); node != LIST_EOF; node = list_next(state->midi_file->events, node)) {
		MidiEvent event = list_node_value(state->midi_file->events, node);
		if (event->type != MIDI_MARKER)
			continue;

		int channel = 0;
		int measures = 0;
		char clip_name[51]; // στη sscanf διαβάζουμε το πολύ 50 χαρακτήρες για όνομα

		// rec και play markers
		if (sscanf(event->marker, "rec,%50[^,],%d,%d", clip_name, &channel, &measures) == 3) {
			// "rec,<name>,<channel>,<measures>" marker, ορίζει ένα clip προς ηχογράφηση
			Clip clip = create_clip(state, clip_name, channel, event->time, measures * measure_duration);
			vector_insert_last(state->clips, clip);

		} else if (sscanf(event->marker, "play,%50[^,],%d", clip_name, &measures) == 2) {
			// "play,<name>,<measures>" marker, ορίζει ένα σημείο αναπαραγωγής του clip
			Clip clip = find_clip_by_name(state, clip_name);
			assert(clip);	// το .mid αρχείο πρέπει να έχει φτιαχτεί σωστά, για κάθε play να προηγείται ένα rec
			list_insert_next(clip->plays, list_last(clip->plays), create_clip_play(event->time, measures * measure_duration));
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////

State state_create(String midi_file) {
	State state = malloc(sizeof(*state));
	assert(state != NULL);

	state->info.paused = true;
	state->info.game_over = false;
	state->info.accuracy = 0.0;
	state->info.score = 0.0;
	state->info.time = 0.0;
	state->info.level = 1;

	state->midi_file = NULL;
	state->midi_events = list_create(free);

	state->midi_file = k08midi_file_read(midi_file);
	assert(state->midi_file != NULL);

	create_clips(state);
	assert(vector_size(state->clips) > 0);
	state->recording_index = 0;

	list_destroy(state->midi_events);
	state->midi_events = list_create(free);
	append_song_control_events(state);

	return state;
}

StateInfo state_info(State state) {
	// Στέλνω τις πληροφορίες (info) με την διεύθυνση μνήμης του state->info για να συμβαδίζει με τον τύπο του StateInfo που ανήκει στο state
	return &state->info;
}

int state_playing_channel(State state) {
	// Το state->recording_index ξεκινάει από το 0 και δείχνει πάντα είτε στο
	// clip που ηχογραφείται τώρα, είτε (αν δεν υπάρχει ενεργή ηχογράφηση) στο
	// αμέσως επόμενο clip. Αφού ολοκληρωθούν όλες οι ηχογραφήσεις το
	// recording_index συνεχίζει να δείχνει το τελευταίο clip.
	//
	// Το κανάλι στο οποίο παίζει "live" ο χρήστης είναι το κανάλι του clip στο
	// οποίο δείχνει το recording_index.

	// Διάσχηση όλων των clips μορφής vector που περιέχουν το channel κάθε clip
	for (VectorNode node = vector_first(state->clips);
		node != VECTOR_EOF;
		node = vector_next(state->clips, node)) {

		Clip clip = vector_node_value(state->clips, node); // Παίρνω το κάθε clip ξεχωριστά
		
		state->recording_index = (uint)clip->channel; // Παίρνω από το καθένα μόνο το channel του και το εισάγω στο recording_index
	}

	uint live_channel_playing = state->recording_index; // Πέρνω το channel από το recording_index
		
	int int_live_channel_playing = (int)live_channel_playing; // Kάνω direference το channel για να μπορώ να το επιστρέψω
	
	return int_live_channel_playing;
	
}

double state_measure_duration(State state) {
	MidiFile midi = state->midi_file;
	return midi->time_signature[0] * (60.0 / midi->tempo) * (4.0 / midi->time_signature[1]);
}

double state_total_duration(State state) {
	// Παίρνω και επιστρέφω τον συνολικό χρόνο του τραγουδιού από το midi_file
	MidiFile midi = state->midi_file;
	double total_duration = midi->duration; 
	return total_duration;
}

List state_displayed_notes(State state, double time_window) {
	double time_now = state->info.time; // Χρόνος που έχω τώρα
	double time_at_end = time_now + time_window; // Χρόνος που θα έχω στο τέλος
	if (time_at_end > state_total_duration(state)) { // Απλός έλεγχος για μικρή βελτιστοποίηση προγράμματος
		time_at_end = state_total_duration(state);
	}
	
	List displayed_notes = list_create(NULL); // Δημιουργία λίστας για να εισαχθούν οι νότες σε αύξουσα σειρά
	
	List list = state->midi_file->events; // Ανοίγω την λίστα που βρίσκονται όλα τα events για να έχω πρόσβαση στα MIDI_NOTES
	
	int channel_playing_now = state_playing_channel(state); // Παίνω το κανάλι που βρίσκομαι τώρα για έλεγχο ορθότητας αργότερα
	
	for (ListNode node = list_first(list); // Διαπερνώ την λίστα με όλα τα events για να πάρω όλα τα MIDI_NOTES και να τα
		node != LIST_EOF;				   // εισάγω ταξινομημένα στην δικιά μου λίστα
		node = list_next(list, node)) {

		MidiEvent event = list_node_value(list, node);

		if (event->time <= time_at_end) { // Έλεγχος για να μην ξεπεράσω το χρονικό όριο του time_window
			// Όλοι οι απαραίτητοι ελέγχοι για εγκυρότητα των στοιχείων των events + νοτών και εισαγωγή νοτών στην λίστα σε αύξουσα σειρά με βάση τον χρόνο (event->time >= time_now)
			if (event->time >= time_now && event->channel == channel_playing_now && event->type == MIDI_NOTE) {
				if((uint8_t)event->key >= 0 && (uint8_t)event->key <= 127 && (uint8_t)event->velocity >= 0) {
					list_insert_next(displayed_notes, list_last(displayed_notes), event); // Για να μπουν όλες οι νότες στην σωστή σειρά (ταξινομημένες) προσθέτω πάντα από το τέλος
				}
			}
		}
		else
		break;
	}

	return displayed_notes; // Επιστροφή ολοκληρωμένης λίστας
}

List state_playback_events(State state, double since) {
	double time_now = state->info.time; // Χρόνος που έχω τώρα
	double time_since = time_now - since; // Χρόνος πριν since δευτερόλεπτα
	if (time_since <= 0) { // Εαν πάρω αρνητικό time_since, το μηδενίζω για να πάρω τα events σπό την αρχή του κομματιού
		time_since = 0;
	}

	List playback_events = list_create(NULL); // Δημιουργία λίστας για να μπουν τα events ταξινομημένα 

	List list = state->midi_file->events; // Δημιουργία λίστας με όλα τα events του τραγουδιού

	for (ListNode node = list_first(list); // Διάσχηση όλων των events της λίστας για να κρατήσω αυτά που συμβαδίζουν με τα χρονικά μου πλαίσια
		 node != LIST_EOF;
		 node = list_next(list, node)) {

			MidiEvent event = list_node_value(list, node);

			// Εαν το event είναι μεταξύ του χρόνου since και του χρόνου τώρα, το εισάγω στην λίστα σε μη φθίνουσα σειρά με βάση τον χρόνο
			if (event->time >= time_since && event->time <= time_now) {
				list_insert_next(playback_events, list_last(playback_events), event);
			}
			else if (event->time > time_now) { // Εαν ο χρόνος βγει εκτός ορίων, τερματίζω την διάσχηση
				break;
			}
		 }

	return playback_events; // Επιστροφή ολοκληρομένης λίστας
}

void state_update(State state, KeyState ks, double elapsed_time) {
	if (ks->space == true){ // Εαν το space πατηθεί μια φορά, το παιχνίδι γίνετε paused (γίνετε unpaused όταν ξαναπατηθεί)
		state->info.paused = !state->info.paused; // Η αλλαγή κατάστασης του paused είναι σε μορφή μοχλού με βάση το not
		if (state->info.game_over == true) { // Εαν το παιχνίδι είναι σε game over, τότε το φεύγει από αυτή την καρτάσταση
			state->info.game_over = false;
		}
	}
	if (ks->n == true && state->info.paused == true && state->info.game_over == false) { // Εαν το παιχνίδι είναι paused και πατιθεί το κουμπί n,
			state->info.time = state->info.time + elapsed_time; // η κατάσταση του παιχνιδιού κινείται ένα frame μπροστά (αυξάνετε ο χρόνος κατά μια φορά σαν το παιχνίδι είναι σε κατάσταση pause)
		}
	if (ks->space == false && state->info.paused == false && state->info.game_over == false) { // Εαν το παιχνίδι δεν είναι paused και το space δεν είναι πατημένο, 
		state->info.paused = false; 														   // τότε η ροή του παιχνιδιού κινείται κανονικά με τον χρόνο να ενημερώνετε
		state->info.time = state->info.time + elapsed_time;
	}

	Vector all_clips = state->clips;

	int channel_playing_now = state_playing_channel(state);

	for (VectorNode node = vector_first(all_clips);
		 node != VECTOR_EOF;
		 node = vector_next(all_clips, node)) {

			Clip clip = vector_node_value(all_clips, node);
			
			if (clip->channel == channel_playing_now 
				&& clip->start >= state->info.time
				&& state->info.time <= (clip->start + clip->duration)) {

					list_insert_next(clip->recorded, list_last(clip->recorded), ks->changed_keys);
			}
			else if (state->info.time > (clip->start + clip->duration)
					 && clip->channel == channel_playing_now) {
					
					List list = state->midi_events;

					for (ListNode node = list_first(list);
						 node != LIST_EOF;
						 node = list_next(list, node)) {

							MidiEvent event = list_node_value(list, node);

							if (event->time >= clip->start
								&& clip->channel == channel_playing_now
								&& event->time <= (clip->start + clip->duration)) {

								event->velocity = 0;
							}
						 }

				    if (vector_next(all_clips, node) != VECTOR_EOF) {
						state->recording_index++;
					}
						 
					break;
			}

		 }

}

void state_destroy(State state) {
	// Προς υλοποίηση
}
