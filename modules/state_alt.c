#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ADTList.h"
#include "ADTVector.h"
#include "ADTSet.h" // To ADT set είναι το πιο αποδοτικό για την μέγιστη βελτιστοποίηση του προγράμματος
#include "k08midi.h"
#include "state.h"


// Οι ολοκληρωμένες πληροφορίες της κατάστασης του παιχνιδιού.
// Ο τύπος State είναι pointer σε αυτό το struct, αλλά το ίδιο το struct
// δεν είναι ορατό στον χρήστη.

struct state {
	struct state_info info;  // Γενικές πληροφορίες για την κατάσταση του παιχνιδιού
	MidiFile midi_file;      // Το .mid αρχείο που διαβάσαμε με τη k08midi_file_read
    Set set_midi_events;     // Οι original νότες του κομματιού
    Set midi_events;         // MIDI events προς αναπαραγωγή
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
int compare(Pointer event1, Pointer event2) {
    MidiEvent first_event = (MidiEvent)event1;
    MidiEvent second_event = (MidiEvent)event2;

    if (first_event->time > second_event->time) {
        return 1;
    }
    else if (first_event->time < second_event->time) {
        return -1;
    }
    else {
        
        if (first_event->channel > second_event->channel) {
            return 1;
        }
        else if (first_event->channel < second_event->channel) {
            return -1;
        }
        else {
            
            if (first_event->key > second_event->key) {
                return 1;
            }
            else if (first_event->key < second_event->key) {
                return -1;
            }
            else {
                 
                if (first_event->velocity > second_event->velocity) {
                    return 1;
                }
                else if (first_event->velocity < second_event->velocity) {
                    return -1;
                }
                else {
                    return 0;
                }
            }
        }
    }
}

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
			set_insert(state->midi_events, clone); // Απαραίτητη αλλαγή αφού πέρνω πλεον τα events μου και τα βάζω σε Set αντί για Λίστα
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

// Custom απαραίτητη συνάρτηση στην οποία κάνω restart το παιχνίδι είτε για να πάει ο παίχτης στο επόμενο level είτε για να ξεκινήσει από την αρχή
static void state_restart_game(State state) {
	
	// Αλλαγή της κατάστασης και των τιμών όλου του info του state για να γίνει restart
	state->info.paused = true;
	state->info.game_over = false;
	state->info.accuracy = 0.0;
	state->info.score = 0.0;
	state->info.time = 0.0;

	// Καταστρέφω όλα τα clips στο οποία ηχογράφησε ο χρήστης και δημιουργώ νέα τα οποία είναι άδεια και έτοιμα να γεμίσουν με τις νότες 
	// της νέας ηχογράφησης
	for (int k = 0 ; k < vector_size(state->clips) ; k++) {
		Clip clip = vector_get_at(state->clips, k);
		list_destroy(clip->recorded);
		clip->recorded = list_create(free);
	}

	state->recording_index = 0; // Το index του recording επαναφέρετε στο μηδέν για να ξεκινήσουν οι ηχογραφήσεις από την αρχή
}

// Custom level progression σύστημα αξιολόγησης της επίδοσης του χρήστης με custom ελάχιστο accuracy που απαιτήτε για να
// προχωρίσης στο επόμενο level. Ενδέχετε να αλλάξει ανάλογα με την δυσκολία του τραγουδιού
static void state_accuracy_level(State state, double accuracy) {

	// Εαν βρίσκομε σε ένα συγκεκριμένο level και πάρω πάνω από το επιθυμιτό accuracy,
	// τότε περνώ στο επόμενω level (ίδιο κομμάτι με μεγαλύτερο τουλάχιστο accuracy)
	// και κάνω restart το παιχνίδι, αλιώς το τερματίζω με game_over = true
	if (state->info.level == 1) {
		if (accuracy >= 0.10) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
	else if (state->info.level == 2) {
		if (accuracy >= 0.20) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
	else if (state->info.level == 3) {
		if (accuracy >= 0.30) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
	else if (state->info.level == 4) {
		if (accuracy >= 0.40) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
	else if (state->info.level == 5) {
		if (accuracy >= 0.50) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
	else if (state->info.level == 6) {
		if (accuracy >= 0.60) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
	else if (state->info.level == 7) {
		if (accuracy >= 0.70) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
	else if (state->info.level == 8) {
		if (accuracy >= 0.80) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
	else if (state->info.level == 9) {
		if (accuracy >= 0.90) {
			state->info.level++;
			state_restart_game(state);
		}
		else {
			state->info.game_over = true;
		}
	}
}

// Custom συνάρτηση στην οποία εφαρμώζω ένα scoring system μαζί με auto-correction της προσπάθειας του χρήστη
static void state_clip_accuracy(State state, Clip clip) {

	// Δημιουργία λίστας που θα περιέχει έτοιμα όλα τα clips του κομματιού 
	List clip_song = list_create(free);

	List list = state->midi_file->events;

	// Διαπερνώ την λίστα που περιέχει όλα τα events του κομματιού για να τα αντιγράψω και να τα περάσω με δείκτες στην νέα λίστα 
	// έτσι ώστε οι αλλαγές που θα γίνουν πάνω τους να μην είναι οριστικές και να χαλάσουν το midi κομμάτι, αλλά να έχω ένα πλήρες
	// copy του κομματιού που θα μπορώ να το επεξεργαστώ άφοβα
	for (ListNode node = list_first(list);
		 node != LIST_EOF;
		 node = list_next(list, node)) {

			MidiEvent temp_event = list_node_value(list, node);

			if (temp_event->channel == clip->channel &&
				temp_event->time >= clip->start &&
				temp_event->time <= (clip->start + clip->duration)) {

					MidiEvent copy_event = malloc(sizeof(struct midi_event));

					*copy_event = *temp_event; // Δείκτες για να αντιγράψω τα events χωρίς να πειράξω την original λίστα 

					list_insert_next(clip_song, list_last(clip_song), copy_event);
				}
		 }

	int song_note_on_count = 0; // όλες οι νότες που παιχτήκαν στο κομμάτι

	// Διαπερνώ κάθε event του clip_song για να πάρω τον αριθμό των νοτών που παιχτήκαν κατά την διάρκεια του κομματιού
	for (ListNode node_song = list_first(clip_song);
		 node_song != LIST_EOF;
		 node_song = list_next(clip_song, node_song)) {

			MidiEvent note_on_song = list_node_value(clip_song, node_song);

			if (note_on_song->velocity > 0) { // Παίχτηκε η νότα εαν το velocity της έγεινε μεγαλύτερο του 0
				song_note_on_count++;
			}
		}

	List clip_recorded = clip->recorded; // Η λίστα που είναι ηχογραφημένη η προσπάθεια του χρήστη

	double score = 0.0; // σκορ πόντων του παιχνιδιού

	// Διαπερνώ όλα τα events/νότες της προσπάθειας που μόλις ηχογράφησε ο χρήστης για να ελέξω νότα προς νότα με βάση τις νότες που παίζονται στο original κομμάτι
	for (ListNode node_rec = list_first(clip_recorded);
		 node_rec != LIST_EOF;
		 node_rec = list_next(clip_recorded, node_rec)) {

		MidiEvent note_on_rec = list_node_value(clip_recorded, node_rec);

		// Επειδή με την ενεργοποίηση μιας νότας, αργότερα θα πρέπει να απενεργοποιηθεί, σε αυτό το σημείο κάνω skip της νότες που απενεργοποιούνται
		if (note_on_rec->velocity == 0) { 
			continue;
		}

		// Μεταβλητή που θα κρατάω τον προηγούμενο κόμβο για διαγραφή των events που γίνονται match και την αρχικοποιώ με τον κόμβο πριν τον πρώτο της λίστας
		ListNode prev_node_song = LIST_BOF; 

		// Διαπερνώ όλα events με βάση το midi αρχείο δηλαδή της σωστές νότες που πρέπει να παίξει ο χρήστης με βάση το κομμάτι
		for (ListNode node_song = list_first(clip_song);
		 	node_song != LIST_EOF;
		 	node_song = list_next(clip_song, node_song)) {

				MidiEvent note_on_song = list_node_value(clip_song, node_song);

				if (note_on_song->velocity == 0) { // Εαν η νότα απενεργοποιήτε την κάνω ξανά skip και μεταφέρω τον προηγούμενο κόμβο στον τωρινό 
					prev_node_song = node_song;
					continue;
				}

				double time_between; // Χρόνος μεταξύ της νότας που έπαιξε ο χρήστης και της νότας που έπρεπε να παίξει με βάση το κομμάτι

				// Ελέγχω πια νότα μετακύ των δύο παίζετε πρώτη και υπολογίζω τον μεταξύ τους χρόνο αφαιρώντας τον μεγάλο χρόνο από τον μικρό
				if (note_on_rec->time >= note_on_song->time)
				time_between = note_on_rec->time - note_on_song->time;
				else
				time_between = note_on_song->time - note_on_rec->time;

				// Εαν ο χρόνος μεταξύ τους είναι ελάχιστα μικρός (<=0.1) και οι νότες είναι οι ίδιες (ενεργοποιημένες και στο ίδιο κανάλι), τότε βρήκαμε match
				if (time_between <= 0.1 && time_between >=0 &&
					note_on_song->channel == note_on_rec->channel &&
					note_on_song->key == note_on_rec->key &&
					note_on_song->velocity > 0 && note_on_rec->velocity > 0) {
						
						double note_rec_duration = 0.0; // Χρόνος που διαρκεί η ηχογραφημένη νότα του χρήστη

						MidiEvent correct_note_off_rec = NULL; // Copy εως εκεί που τελειώνει η νότα για σκοπούς auto-correction

						// Αφού βρήκα την στιγμή που ενεργοποιήτε μια νότα, συνεχίζω την διάσχηση της λίστας από το σημείο που βρίσκομε, μέχρι να βρω την στιγμή που
						// απενεργοποιήτε για να πάρω την διάρκεια που είναι ενεργή η νότα
						for (ListNode temp_node_rec = node_rec;
							temp_node_rec != LIST_EOF;
							temp_node_rec = list_next(clip_recorded, temp_node_rec)) {
								
								MidiEvent temp_note_on_rec = list_node_value(clip_recorded, temp_node_rec);

								// Εαν η νότα είναι η ίδια με αυτή που ενεργοποιήθηκε αλλά τώρα είναι απενεργοποιημένη, σημαίνει πως βρήκα την χρονική στιγμή που απενεργοποιήτε
								if (temp_note_on_rec->key == note_on_rec->key && 
									temp_note_on_rec->channel == note_on_rec->channel &&
							   		temp_note_on_rec->velocity == 0) {
										
										note_rec_duration = temp_note_on_rec->time - note_on_rec->time; // Χρόνος που είναι ενεργοποιημένη μια νότα που έπαιξε ο χρήστης
										correct_note_off_rec = temp_note_on_rec; // Αποθηκεύω την στιγμή που τελείωσε η νότα για να πετύχω αργότερα το auto-correction
										
										break; // Δεν υπάρχει λόγος να συνεχίσω την διάσχηση αφού βρήκα την νότα που ψάχνω για αυτό και τερματίζω την διάσχηση
									}
							}
						
						// Επαναλαμβάνω ακριβός την ίδια διαδικασία αλλά τώρα για την original νότα του κομματιού,
						// δηλαδή βρίσκω την διάρκεια που είναι ενεργοποιημένη η νότα στην οποία βρίσκετε το κομμάτι την συγκεκριμένη στιγμή
						double note_song_duration = 0.0; 

						for (ListNode temp_node_song = node_song;
							temp_node_song != LIST_EOF;
							temp_node_song = list_next(clip_song, temp_node_song)) {
								
								MidiEvent temp_note_on_song = list_node_value(clip_song, temp_node_song);

								if (temp_note_on_song->key == note_on_rec->key &&
									temp_note_on_song->channel == note_on_rec->channel &&
							   		temp_note_on_song->velocity == 0) {
										
										note_song_duration = temp_note_on_song->time - note_on_song->time;
										break;
									}
							}

						double time_diff = 0.0; // Θα αποθηκεύετε η διαφορά της στιγμής που έπαιξε η ηχογραφημένη νότα με την στιγμή που έπαιξε η original νότα

						// Βρίσκω πιος από τους δύο χρόνους είναι ο μεγαλύτερος και αφαιρώ τον ένα από τον άλλα για να πάρω το time_diff
						if (note_on_rec->time >= note_on_song->time) {

							time_diff = note_on_rec->time - note_on_song->time;
						}
						else {

							time_diff = note_on_song->time - note_on_rec->time;
						}
						
						double duration_diff = 0.0; // Αντίστιχη μεταβλητή στην οποία θα αποθηκεύετε η διαφορά της διάρκειας των δύο νοτών

						if (note_on_rec->channel == 9 || note_on_song->channel == 9) { // Εαν το channel είναι το 9 δηλαδή τα drums που έχουν στιγμιαίο ήχο, δεν υπάρχει η διαφορά της διάρκειας
							
							duration_diff = 0.0;
						}
						else {
							if (note_rec_duration >= note_song_duration) { // Για τα άλλα channels, υπολογίζω το duration_diff με τον ίδιο τρόπο που υπολοόγισα το time_diff

								duration_diff = note_rec_duration - note_song_duration;
							}
							else {

								duration_diff = note_song_duration - note_rec_duration;
							}
						}

						double diff = time_diff + duration_diff; // Συνολική διαφορά χρόνου = άρθροισμα των άλλων δύο χρόνων

						score = score + exp(- ((diff / 0.1) * (diff / 0.1))); // Ανανέωση του score με βάση τον τύπο που μου δίνετε

						// Στες δύο επόμενες εντολές εφαρμόζετε το auto-correction
						note_on_rec->time = note_on_song->time; // Αρχή νότας

						if (correct_note_off_rec != NULL) {	// Εαν εντοπίστηκε η νότα						
							correct_note_off_rec->time = note_on_rec->time + note_song_duration; // Τέλος νότας στον σωστό χρόνο (auto-correction)
						}

						// Διαγράφω τον προηγούμενο κόμβο (νότα) έτσι ώστε οι νότες της ηχογράφησης να μην γίνουν πολλές φορές match με τις νότες του κομματιού
						list_remove_next(clip_song, prev_node_song); 
						break; // Για να έφτασα σε αυτό το σημείο σημαίνει πως έγεινε το match μεταξύ των νοτών για αυτό και παω απεφθείας στην επόμενη νότα που ηχογραφήθηκε
					}
					prev_node_song = node_song; // Το σημείο που αποθηκεύω την προηγούμενη νότα για να την διαγράψω εαν γίνει match με κάποια άλλη
				}
			}

	int played_note_on_count = 0; // Ο αριθμός των νοτών που έπεξε ο χρήστης κατά την διάρκεια της ηχογράφησης

	// Διαπερνώ ακόμη μια φορά τις νότες της ηχογράφησης και όσες έχουν ενεργοποιηθεί τις προσθέτω στο played_note_on_count
	for (ListNode node_rec = list_first(clip_recorded);
		 node_rec != LIST_EOF;
		 node_rec = list_next(clip_recorded, node_rec)) {
		
			MidiEvent note_on_rec = list_node_value(clip_recorded, node_rec);

			if (note_on_rec->velocity > 0) {
				played_note_on_count ++;
			}
		 }

	// Αποθηκεύω στο max ττο μεγαλύτερο ανάμεσα στον αριθμό με στις νότες που έπαιξε ο χρήστης και στον αριθμό νοτών που έπρεπε να παίξει
	int max = (song_note_on_count > played_note_on_count) ? song_note_on_count : played_note_on_count;

	double accuracy = 0.0;

	if (score > 0 && (double)max > 0) { // Εαν βρείκε τουλάχιστο μια νότα σωστή και ανέβηκε λίγο το score
		accuracy = score / max; // Υπολογίζω το accuracy με βάση τον τύπο
	}
	else { // Αλιώς το score και το accurasy είναι 0 για αυτή την προσπάθεια του χρήστη
		state->info.score = state->info.score + 0.0;
		state->info.accuracy = state->info.accuracy + 0.0;

		list_destroy(clip_song); // Καταστροφή λίστας για αποφυγή memory leak 
		return; // Επιστροφή στο state_update
	}
	
	state->info.score = state->info.score + score; // To score που υπολογίστικε σε αυτό τον γύρο προσθέτετε στο συνολικό score του παιχνιδιού

	// Για να υπολογίσω το νέο συνολικό accuracy χωρίς να αλιόσω αυτά των προηγούμενων προσπαθειών κάνω (παλαιός μέσος όρος * πλήθος των παλιών clips) + νέο accuracy) / συνολικό πλήθος clips.
	state->info.accuracy = ((state->info.accuracy * state->recording_index) + accuracy) / (state->recording_index + 1); 

	list_destroy(clip_song); // Καταστροφή λίστας για αποφυγή memory leak 
	return; // Επιστροφή στο state_update
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

	state->midi_file = k08midi_file_read(midi_file);
	assert(state->midi_file != NULL);

    // Ο στόχος είναι να βάλω όλα τα events του midi αρχείου μέσα σε Set αντί για List
    // για να μπορώ να τα επεξεργαστώ αργότερα στο state_displayed_notes και στο state_playback_events
    state->set_midi_events = set_create(compare, NULL); // Δημιουργία Set

    List list_midi_events = state->midi_file->events; // List με όλα τα events του midi αρχείου

    // Διάσχηση όλων των event της λίστας για να τα μεταφέρω στο νεό μου Set
    for (ListNode node = list_first(list_midi_events);
         node != LIST_EOF;
         node = list_next(list_midi_events, node)) {

            MidiEvent set_event = list_node_value(list_midi_events, node); // Παίρνω το event σε μορφή Set

            set_insert(state->set_midi_events, set_event); // Μεταφορά event στο Set
         }

	create_clips(state);
	assert(vector_size(state->clips) > 0);
	state->recording_index = 0;

	//list_destroy(state->midi_events);
	state->midi_events = set_create(compare, free); // Εδώ που θα τοπεθετηθούν τα MIDI events προς αναπαραφωφή αλλά τώρα σε μορφή Set
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

	Vector state_clips = state->clips;
	
	int all_clips_num = vector_size(state_clips); // Πέρνω το συνολικό αριθμό των clips 

	int recording_index_now = (int)state->recording_index; // Πέρνω το channel από το recording_index
	
	if (all_clips_num > 0 && all_clips_num <= recording_index_now) { // Έλεγχος στην περίπτωση που δεν υπάρχουν άλλα clips, να συνεχίσει να δείχνει στο τελευτέο
		recording_index_now = all_clips_num - 1;
	}
	
	Clip clip_playing_now = vector_get_at(state_clips, recording_index_now); // Clip στο οποίο το κομμάτι βρίσκετε τώρα
	
	int live_channel_playing = clip_playing_now->channel; // Το channel του clip
	
	return live_channel_playing; // Επιστροφή channel
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
    
    // Με το Set μπορώ να έχω άμεση πρόσβαση σε οποιοδήποτε σημείο των midi_events χωρίς να ανατρέχω κάθε φορά όλο το αρχείο από την αρχή
    // με αποτέλεσμα να είναι πιο βελτιστοποιημένη επιλογή από τα Lists ειδικά σε μεγάλα κομμάτια
	Set set = state->set_midi_events; // Ανοίγω το Set που βρίσκονται όλα τα events για να έχω πρόσβαση στα MIDI_NOTES
	
	int channel_playing_now = state_playing_channel(state); // Παίνω το κανάλι που βρίσκομαι τώρα για έλεγχο ορθότητας αργότερα
	
    // Ένα custom event που θα το χρησιμοποιήσω για να ξέρω από που θα αρχίσω στο set με όλα τα events.
    // Με αυτό τον τρόπο δεν χρειάζετε να διατρέχω την λίστα κάθε φορά από την αρχή για αυτό και το πρόγραμμα έτσι γίνετε πιο αποδοτικό σε μεγάλα κομμάτια
    MidiEvent temp = malloc(sizeof(struct midi_event)); 
    temp->time = time_now;
    temp->channel = -1;
    temp->key = -1;
    temp->velocity = -1;

    // To εισάγω προσωρινά στο set για να πάει και να τοποθετηθεί ακριβός στο σημείο που πρέπει με βάση το time_now (ένα node πριν από αυτό που ψάχνω)
    set_insert(set, temp);

    // Βρίσκει το custom event που μόλις πέρασα στο set και βρίσκω το ένα node πριν το σημείο εκκίνησης
    SetNode node_temp = set_find_node(set, temp);

	for (SetNode node = set_next(set, node_temp); // Διαπερνώ το set με όλα τα events από το σημείο που θέλω (με βάση τον χρόνο), για να πάρω όλα τα MIDI_NOTES και να τα
		node != SET_EOF;				   // εισάγω ταξινομημένα στην δικιά μου λίστα
		node = set_next(set, node)) {

		MidiEvent event = set_node_value(set, node);

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

    // Σταματώ προσωρινά τα αυτόματα free που κάνει το set για να διαγράψω τον προσωρινό κόμβο temp που πρόσθεσα πιο πάνω,
    // έτσι ώστε να μην αλλοιωθεί το κομμάτι και αργότερα  τα επαναφέρω
    DestroyFunc destroy = set_set_destroy_value(set, NULL);
    set_remove(set, temp);
    set_set_destroy_value(set, destroy);

    free(temp); // Για αποφυγή memory leak

	return displayed_notes; // Επιστροφή ολοκληρωμένης λίστας
}

List state_playback_events(State state, double since) {
	double time_now = state->info.time; // Χρόνος που έχω τώρα
	double time_since = time_now - since; // Χρόνος πριν since δευτερόλεπτα
	if (time_since <= 0) { // Εαν πάρω αρνητικό time_since, το μηδενίζω για να πάρω τα events σπό την αρχή του κομματιού
		time_since = 0;
	}

	List playback_events = list_create(NULL); // Δημιουργία λίστας για να μπουν τα events ταξινομημένα 

	Set set = state->midi_events; // Ανοίγω το Set που βρίσκονται όλα τα events για να έχω πρόσβαση στα MIDI_NOTES

    // Ένα custom event που θα το χρησιμοποιήσω για να ξέρω από που θα αρχίσω στο set με όλα τα events (time_since).
    // Με αυτό τον τρόπο δεν χρειάζετε να διατρέχω την λίστα κάθε φορά από την αρχή για αυτό και το πρόγραμμα έτσι γίνετε πιο αποδοτικό σε μεγάλα κομμάτια
    MidiEvent temp = malloc(sizeof(struct midi_event)); 
    temp->time = time_since;
    temp->channel = -1;
    temp->key = -1;
    temp->velocity = -1;

    // To εισάγω προσωρινά στο set για να πάει και να τοποθετηθεί ακριβός στο σημείο που πρέπει με βάση το time_now (ένα node πριν από αυτό που ψάχνω)
    set_insert(set, temp);

    // Βρίσκει το custom event που μόλις πέρασα στο set και βρίσκω το ένα node πριν το σημείο εκκίνησης
    SetNode node_temp = set_find_node(set, temp);

    for (SetNode node = set_next(set, node_temp);  // Διαπερνώ το set με όλα τα events από το σημείο που θέλω (με βάση τον time_since), για να πάρω όλα τα MIDI_NOTES και να τα
		node != SET_EOF;				   // εισάγω ταξινομημένα στην δικιά μου λίστα
		node = set_next(set, node)) {

		MidiEvent event = set_node_value(set, node);

		// Εαν το event είναι μεταξύ του χρόνου since και του χρόνου τώρα, το εισάγω στην λίστα σε μη φθίνουσα σειρά με βάση τον χρόνο
		if (event->time >= time_since && event->time <= time_now) {
			list_insert_next(playback_events, list_last(playback_events), event);
		}
			else if (event->time > time_now) { // Εαν ο χρόνος βγει εκτός ορίων, τερματίζω την διάσχηση
			break;
		}
	}

    // Σταματώ προσωρινά τα αυτόματα free που κάνει το set για να διαγράψω τον προσωρινό κόμβο temp που πρόσθεσα πιο πάνω,
    // έτσι ώστε να μην αλλοιωθεί το κομμάτι και αργότερα  τα επαναφέρω
    DestroyFunc destroy = set_set_destroy_value(set, NULL);
    set_remove(set, temp);
    set_set_destroy_value(set, destroy);

    free(temp); // Για αποφυγή memory leak

	return playback_events; // Επιστροφή ολοκληρομένης λίστας
}

void state_update(State state, KeyState ks, double elapsed_time) {
	if (state->info.paused == false && state->info.game_over == false)  { // Εαν το παιχνίδι δεν είναι paused και το space δεν είναι πατημένο,
        state->info.time = state->info.time + elapsed_time;			      // τότε η ροή του παιχνιδιού κινείται κανονικά με τον χρόνο να ενημερώνετε
    }
	if (ks->n == true && state->info.paused == true && state->info.game_over == false) { // Εαν το παιχνίδι είναι paused και πατιθεί το κουμπί n,
        state->info.time = state->info.time + elapsed_time;								 // η κατάσταση του παιχνιδιού κινείται ένα frame μπροστά (αυξάνετε ο χρόνος κατά μια φορά σαν το παιχνίδι είναι σε κατάσταση pause)
    }
	if (ks->space == true){ // Εαν το space πατηθεί μια φορά, το παιχνίδι γίνετε paused (γίνετε unpaused όταν ξαναπατηθεί)
		if (state->info.game_over == true) { // Εαν το παιχνίδι είναι σε game over, τότε το φεύγει από αυτή την κατάσταση και γίνετε restart από το level 1
			state->info.level = 1;
			state_restart_game(state);
			state->info.game_over = false;
		}
		else 
		state->info.paused = !state->info.paused; // Η αλλαγή κατάστασης του paused είναι σε μορφή μοχλού με βάση το not
	}
	
	if (state->info.time >= state->midi_file->duration && // Έλεγχος εαν τελείωσε το παιχνίδι (από τον χρόνο ή από το εαν έχασε ο χρήστης)
	    state->info.game_over == false) {
		state_accuracy_level(state, state->info.accuracy); // Αφού η συγκεκριμένη προσπάθεια του χρήστη τελείωσε, βαθμολογήτε η επίδοση του από το σύστημα
		return; // τέλος συνάρτησης επειδή το παιχνίδι γίνετε restart και ξεκινάει από την αρχή
	}

	Vector all_clips = state->clips; // Vector για να έχω πρόσβαση σε όλα τα clips

	int clip_playing_now = (int)state->recording_index; // Αριθμός του clip που παίζει τώρα
	
	int all_clips_number = vector_size(all_clips); // Αριθμός των συνολικών clips

	if (clip_playing_now < all_clips_number) { // Έλεγχος για να μην ξεπεράσω τα συνολικά clips του τραγουδιού
		Clip clip = vector_get_at(all_clips, clip_playing_now); // Εισαγωγή στο clip που βρίσκετε το κομμάτι
	
		int channel_playing_now = state_playing_channel(state); // Το channel στο οποίο βρίσκετε το κομμάτι
			
			if (clip->channel == channel_playing_now  //Έλεγχος ότι ο χρόνος που βρίσκετε το κομμάτι τώρα είναι μεταξύ του χρόνου του clip που έχω
				&& state->info.time >= clip->start
				&& state->info.time <= (clip->start + clip->duration)) {

					for (int k = 0 ; k < 128 ; k++) { // Πέρασμα από όλα τις νότε - πλήκτρα του πιάνου
						 // Έλεγχος εαν τα πλήκτρα έχουν πατηθεί, για να δημιουργήσω τα δικά μου keys με βάση αυτές τις νότες έτσι ώστε να τις προσθέσω στο τέλος του clip->recorded
						 // Με λίγα λόγια γίνετε η ηχογράφηση των νοτών του κομματιού για το συγκεκριμένο clip
						if (ks->changed_keys[k] >= 0) {
							MidiEvent key_pressed = malloc(sizeof(struct midi_event));

							key_pressed->time = state->info.time;
							key_pressed->channel = channel_playing_now;
							key_pressed->key = k; // Η θέση του k αντιπροσωπεύει την νότα (0-127) που έχει παιχτεί
							key_pressed->velocity = (uint8_t)ks->active_keys[k];

							list_insert_next(clip->recorded, list_last(clip->recorded), key_pressed);
						}
					}	
			}
			else if (state->info.time > (clip->start + clip->duration) // Έλεγχος εαν ο χρόνος που βρίσκετε το κομμάτι ξεπέρασε τον χρόνο που παιζόταν το clip
					 && clip->channel == channel_playing_now) {
					
					// Ξανά μια πλήρης διάσχηση από όλες τις νότες για να εντοπίσω τις νότες που έχουν παιχτεί και είναι ενεργές,
					// και να τις απενεργοποιήσω δημιουργόντας για αυτές ένα αντίγραφο key χωρίς ήχο και να τες προσθέτω στο τέλος του clip->recorded
					for (int k = 0 ; k < 128 ; k++) {
						if (ks->active_keys[k] > 0) { // Έλεγχος εαν έχουν παιχτεί κατά την διάρκει του recording
							MidiEvent key_mute = malloc(sizeof(struct midi_event));

							key_mute->time = state->info.time;
							key_mute->channel = channel_playing_now;
							key_mute->key = k;
							key_mute->velocity = 0; // Μηδενίζω τον ήχο τους δηλαδή τις απενεργοποιώ

							list_insert_next(clip->recorded, list_last(clip->recorded), key_mute);
						}
					}

					state_clip_accuracy(state, clip);

					List clip_plays = clip->plays;

					// Διαπερνώ όλα τα plays του τραγουδιού από τα clips για να δημιουργήσω τα loops
					for (ListNode node = list_first(clip_plays);
						 node != LIST_EOF;
						 node = list_next(clip_plays, node)) {

						ClipPlay clip_play_now = list_node_value(clip_plays, node); // Παίρνω κάθε play του τραγουδιού

						double loop_starting_time = clip_play_now->start; // Ο χρόνος που ξεκινάω το loop είναι με βάση που ξεκινά ένα συγκεκριμένο play

						while (loop_starting_time < (clip_play_now->start + clip_play_now->duration)) { // Καθώς ο χρόνος του loop δεν ξεπέρασε το χρονικό όριο του play/segment

							List clip_recorded = clip->recorded;

							// Διάσχηση όλων των ηχογραφημένων clip για να τα βάζω να ξαναπαίζουν μέσα στο loop μέχρι να τελειώσει ο χρόνος του play
							for (ListNode node1 = list_first(clip_recorded);
						 		node1 != LIST_EOF;
						 		node1 = list_next(clip_recorded, node1)) {

									// Παίρνω όλα τα events που βρίσκονται στην ηχογράφηση των play
									MidiEvent event_now = list_node_value(clip_recorded, node1);

									double new_event_duration = loop_starting_time + event_now->time - clip->start; // Νέος χρόνος του event

									if (new_event_duration <= (clip_play_now->start + clip_play_now->duration)) { // Εαν το νέο event βγαίνει έξω από το χρονικό όριο του play/segment
										MidiEvent new_event = malloc(sizeof(struct midi_event));
										
										// Δημιουργώ ένα νέο event τύπου αντίγραφου με τον νέο χρόνο του και με τα ίδια του υπόλοιπα στοιχεία
										new_event->time = new_event_duration;
										new_event->channel = event_now->channel;
										new_event->key = event_now->key;
										new_event->velocity = event_now->velocity;

										set_insert(state->midi_events, new_event); // Εισάγω το νέο event στο τέλος του set με τα υπόλοιπα midi events
									}
								}	
							// Εδώ γίνετε το όλο loop που προσθέτω τον χρόνο του clip στον χρόνο που βρίσκετε το loop για να ξαναπαίξει
							loop_starting_time = loop_starting_time + clip->duration;
						}																									
					}
				state->recording_index++; // Αφού η ηχογράφηση ολοκληρώνετε με επιτυχία, αυξάνω το index για να προχωρίσω στο επόμενο event		
			}
	}
}

void state_destroy(State state) {
	if (state == NULL) { // Ελέγχω εαν η state είναι άδεια πρωτού καταστρέψω τις λίστες για να μην κάνει crash το πρόγραμμα
		return;
	}

	k08midi_file_destroy(state->midi_file); // Καταστροφή midi αρχείου

    set_destroy(state->set_midi_events); // Καταστροφή όλων των νοτών του κομματιού
    
	set_destroy(state->midi_events); // Καταστροφή όλων των midi events

	vector_destroy(state->clips); // Καταστροφή όλων των clips

	free(state); // Απελευθέρωση μνήμης από state για αποφυγή των memory leaks
}
