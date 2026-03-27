//////////////////////////////////////////////////////////////////
//
// Test για το state.h module
//
//////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "acutest.h"			// Απλή βιβλιοθήκη για unit testing

#include "state.h"


///// Βοηθητικές συναρτήσεις ////////////////////////////////////////
//
// σύγκριση double
static bool double_equal(double a, double b) {
	return fabs(a-b) < 1e-6;
}

// αρχικοποίηση πλήκτρων (κανένα πατημένο)
static struct key_state empty_key_state() {
	struct key_state ks = { .space=false, .n=false };
	memset(ks.active_keys, 0, sizeof(ks.active_keys));
	memset(ks.changed_keys, -1, sizeof(ks.changed_keys));
	return ks;
}
/////////////////////////////////////////////////////////////////////


void test_state_create() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	StateInfo info = state_info(state);
	TEST_ASSERT(info != NULL);
	TEST_ASSERT(info->paused);
	TEST_ASSERT(!info->game_over);
	TEST_ASSERT(double_equal(info->time, 0.0));
	TEST_ASSERT(info->level == 1);

	state_destroy(state);
}

void test_state_basic_functions() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	TEST_ASSERT(state_playing_channel(state) == 9);
	TEST_ASSERT(double_equal(state_measure_duration(state), 1.93548));
	TEST_ASSERT(double_equal(state_total_duration(state), 18.987));

	state_destroy(state);
}

void test_displayed_notes_function() {
	State state = state_create("test.mid"); // Δημιουργία λίστας για έλεγχο
	TEST_ASSERT(state != NULL); // Έλεγχος ότι η λίστα δεν είναι άδεια

	// Παραδείγματα λιστών, ένα με μικρό time_window και ένα με μεγάλο που ξεπερνά το duration του τραγουδιού
	List test_list_small_time_window = state_displayed_notes(state, 4.0); 
	List test_list_big_time_window = state_displayed_notes(state, 19.0); 

	// Έλεγχος ότι οι λίστες έχουν δημιουργηθεί σωστά 
	TEST_ASSERT(test_list_small_time_window != NULL);
	TEST_ASSERT(test_list_big_time_window != NULL);

	// Παίρνω τα μεγέθοι των δύο λιστών δηλαδή τον αριθμών των νοτών που περαστήκαν
	int list_size_small = list_size(test_list_small_time_window);
	int list_size_big = list_size(test_list_big_time_window);

	TEST_ASSERT( list_size_small >= 0); // Ελέγχω ότι είναι μέσα στα όρια (μπορεί να είναι 0 εαν δεν παιχτήκαν νότες εως τα 4sec)
	TEST_ASSERT( list_size_small <= list_size_big && list_size_big != 0); // Συγκρίνω τα μεγέθοι μεταξύ τους και ελέγχω ότι έχει περαστεί τουλάχιστον μια νότα

	// Στον πάρακατω έλεγχο, διασχώ όλα τα στοιχεία της μεγάλης λίστας και τα ελέγχω/συγκρίνω εαν είναι όλα ταξινομημένα σωστά σε αύξουσα σειρά με βάση τον χρόνο
	List list = test_list_big_time_window;

	for (ListNode node = list_first(list);
		node != LIST_EOF;
		node = list_next(list, node)) {

			if (node != list_last(list)) { // Σταματώ στον τελευτέο κόμβο για να μην ξεπεράσω το όριο της λίστας
			MidiEvent list_node_value_1 = list_node_value(list, node);

			ListNode list_node_2 = list_next(list, node);
			MidiEvent list_node_value_2 = list_node_value(list, list_node_2);

			TEST_ASSERT(list_node_value_1->time <= list_node_value_2->time);
			}
			else
			break;
		}
	
	// Καταστρέφω τις λίστες για να αποφύγω τυχόν memory leaks
	list_destroy(test_list_small_time_window);
	list_destroy(test_list_big_time_window);

	state_destroy(state);
}

void test_playback_events_function() {
	State state = state_create("test.mid"); // Δημιουργία λίστας για έλεγχο
	TEST_ASSERT(state != NULL); // Έλεγχος ότι η λίστα δεν είναι άδεια
	
	// Υποχρεωτικός ορισμός ενός key με empty_key_state για την χρήση του state_update
	struct key_state ks = empty_key_state(); 
	
	// Υποχρεωτική χρήση του state_update για να ανανεωθεί ο χρόνος του κομματιού σε επιθυμιτό χρόνο έτσι ώστε να μπορούν να γίνουν οι απαραίτητοι ελέγχοι
	state_update(state, &ks, 18.987);

	// Παραδείγματα λιστών, ένα με μικρό since, ένα με μεγάλο και ένα που ξεπερνά το duration του τραγουδιού και βγαίνει αρνητικό το since
	List test_list_small_since = state_playback_events(state, 2.0); 
	List test_list_big_since = state_playback_events(state, 18.0); 
	List test_list_off_since = state_playback_events(state, 20.0);

	// Έλεγχος ότι οι λίστες έχουν δημιουργηθεί σωστά
	TEST_ASSERT(test_list_small_since != NULL);
	TEST_ASSERT(test_list_big_since != NULL);
	TEST_ASSERT(test_list_off_since != NULL);

	// Παίρνω τα μεγέθοι των λιστών δηλαδή τον αριθμών των events που περαστήκαν
	int list_size_small = list_size(test_list_small_since);
	int list_size_big = list_size(test_list_big_since);
	int list_size_off = list_size(test_list_off_since);

	TEST_ASSERT( list_size_small >= 0); // Ελέγχω ότι είναι μέσα στα όρια (μπορεί να είναι 0 εαν δεν υπήρχαν events εως τα 2sec)
	TEST_ASSERT( list_size_small <= list_size_big); // Συγκρίνω τα μεγέθοι μεταξύ τους
	TEST_ASSERT( list_size_off >= 0); 

	// Στον πάρακατω έλεγχο, διασχώ όλα τα στοιχεία της μεγάλης λίστας και τα ελέγχω/συγκρίνω εαν είναι όλα ταξινομημένα σωστά σε μη φθίνουσα σειρά με βάση τον χρόνο
	List list = test_list_big_since;

	for (ListNode node = list_first(list);
		node != LIST_EOF;
		node = list_next(list, node)) {

			if (node != list_last(list)) { // Σταματώ στον τελευτέο κόμβο για να μην ξεπεράσω το όριο της λίστας
			MidiEvent list_node_value_1 = list_node_value(list, node);

			ListNode list_node_2 = list_next(list, node);
			MidiEvent list_node_value_2 = list_node_value(list, list_node_2);

			TEST_ASSERT(list_node_value_1->time <= list_node_value_2->time);
			}
			else
			break;
		}
	
	// Καταστρέφω τις λίστες για να αποφύγω τυχόν memory leaks
	list_destroy(test_list_small_since);
	list_destroy(test_list_big_since);
	list_destroy(test_list_off_since);

	state_destroy(state);
}

void test_state_update() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	// Παράδειγμα:
	struct key_state ks = empty_key_state();
	ks.space = true;

	state_update(state, &ks, 0.0);
	TEST_ASSERT(!state_info(state)->paused);

	ks.space = false;
	state_update(state, &ks, 0.5);
	TEST_ASSERT(!state_info(state)->paused);
	TEST_ASSERT(state_info(state)->time == 0.5);

	ks.space = true;
	state_update(state, &ks, 0.5);
	TEST_ASSERT(state_info(state)->paused);
	TEST_ASSERT(state_info(state)->time == 1.0);

	ks.space = true;
	state_update(state, &ks, 10.0);
	TEST_ASSERT(!state_info(state)->paused);
	TEST_ASSERT(state_info(state)->time == 1.0);
	
	ks.space = true;
	state_update(state, &ks, 1.0);
	TEST_ASSERT(state_info(state)->paused);
	TEST_ASSERT(state_info(state)->time == 2.0);
	ks.space = false;
	ks.n = true;
	state_update(state, &ks, 0.5);
	TEST_ASSERT(state_info(state)->paused);
	TEST_ASSERT(state_info(state)->time == 2.5);

	state_update(state, &ks, 1.5);
	TEST_ASSERT(state_info(state)->paused);
	TEST_ASSERT(state_info(state)->time == 4.0);
	
	ks.space = true;
	ks.n = false;
	state_update(state, &ks, 0.0);
	ks.space = false;

	ks.active_keys[42] = 1;
	ks.changed_keys[42] = 0;

	state_update(state, &ks, 5.0);

	ks.active_keys[42] = 0;
	ks.changed_keys[42] = -1;

	//--------------------------------------

	ks.active_keys[43] = 1;
	ks.changed_keys[43] = 0;

	state_update(state, &ks, 5.0);

	ks.active_keys[43] = 0;
	ks.changed_keys[43] = -1;

	//-------------------------------------

	ks.active_keys[50] = 1;
	ks.changed_keys[50] = 0;

	ks.active_keys[55] = 1;
	ks.changed_keys[55] = 0;

	state_update(state, &ks, 5.0);

	ks.active_keys[50] = 0;
	ks.changed_keys[50] = -1;

	ks.active_keys[55] = 0;
	ks.changed_keys[55] = -1;

	state_update(state, &ks, 1.0);
	TEST_ASSERT(state_info(state)->time == 20.0);

	List event_test = state_playback_events(state, 20.0);
	
    TEST_ASSERT(event_test != NULL);
	TEST_ASSERT(list_size(event_test) > 0);
	TEST_ASSERT(list_node_value(event_test, list_first(event_test)) != NULL);
    TEST_ASSERT(list_node_value(event_test, list_last(event_test)) != NULL);

    list_destroy(event_test);
	state_destroy(state);
}


// Λίστα με όλα τα tests προς εκτέλεση
TEST_LIST = {
	{ "test_state_create", test_state_create },
	{ "test_state_basic_functions", test_state_basic_functions },
	{ "test_displayed_notes_function", test_displayed_notes_function },
	{ "test_playback_events_function", test_playback_events_function },
	{ "test_state_update", test_state_update },

	{ NULL, NULL } // τερματίζουμε τη λίστα με NULL
};
