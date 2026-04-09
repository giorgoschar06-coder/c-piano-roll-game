#include "raylib.h"

#include "state.h"
#include "interface.h"

// Μεγάλη επιρροή από /programs/game_example/interface.c
void interface_init() {
	// Αρχικοποίηση του παραθύρου
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "game");
	SetTargetFPS(60);
    //InitAudioDevice();

}

void interface_close() {
	//CloseAudioDevice();
	CloseWindow();
}

// Draw game (one frame)
void interface_draw_frame(State state) {
	BeginDrawing();

	// Καθαρισμός, θα τα σχεδιάσουμε όλα από την αρχή
	ClearBackground(DARKGRAY);

	// Παίρνω το info του παιχνιδιού για να μπορώ αργότερα να το προβάλω στον χρήστη
	StateInfo info = state_info(state); 

    int piano_keys = 128; // Αριθμός συνολικών πλήκτρων
    int piano_keys_height = 150; // Custom μέγεθος πλήκτρου

    int piano_keys_start_y = SCREEN_HEIGHT - piano_keys_height; // Ύψος από το οποίο θα ξεκινάνε τα πλήκτρα

    for (int k = 0 ; k < piano_keys ; k++) { // αναδρομή όλων των πλήκτρων του πιάνου για να τα σχεδιάσω

		// Χωρίζω το πλάτος της οθόνης σε 128 μέρη για να χωρέσω όλα τα πλήκρα του πιάνου 
        int x = k * SCREEN_WIDTH / piano_keys; // Θέση που θα αρχίζει το κάθε πλήκτρο
        int piano_keys_next_x = (k + 1) * SCREEN_WIDTH / piano_keys; // Θέση που θα τελειώνει κάθε πλήτρο και θα αρχίζει το επόμενο
        int piano_keys_width = piano_keys_next_x - x; // Πλάτος των πλήκτρων
		
		// Αφού έχω όλα τα στοιχεία και τις συντεταγμένες, σχεδιάζω ένα ένα τα 128 πλήκτρα
		DrawRectangle(x, piano_keys_start_y, piano_keys_width, piano_keys_height, RAYWHITE);
		
		// Σχεδιασμός μαύρου περιγράμματος του κάθε πλήκτρου για να ξεχωρίζουν μεταξύ τους
		DrawRectangleLines(x, piano_keys_start_y, piano_keys_width, piano_keys_height, BLACK);
		
		int key_major = k % 12; // Μεταβλητή για να ξεχωρίζω πότε μια νότα είναι δίαιση με βάση το υπόλιπο κάθε οκτάβας (12 νότες)
	 
		// Εαν το πλήκτρο ανά οκτάβα έχει υπόλοιπο 1, 3, 6, 8 ή 10, τότε η νότα είναι δίαιση
		if (key_major == 1 || key_major == 3 || key_major == 6 || key_major == 8 || key_major == 10) {
		// Εαν η νότα είναι δίαιση, σχεδιάζω το μισό πάνω μέρος της νότας μαύρο για να το ξεχωρίσει ο χρήστης, όπως είναι περίπου και σε ένα κανονικό πιάνο
		DrawRectangle(x, piano_keys_start_y, piano_keys_width, piano_keys_height - 75, BLACK);
	
		// Περίγραμα για λόγους οπτικής ομορφίας
		DrawRectangleLines(x, piano_keys_start_y, piano_keys_width, piano_keys_height - 75, BLACK);
		}
	}

	double time_window = 5.0; // Χρόνος που έχουν οι νότες να εμφανιστούν στον χρήστη μέχρι να πέσουν στο πιάνο
		
	List notes_falling = state_displayed_notes(state, time_window); // Λίστα με τις νότες που θα εμφανιστούν μέχρι το time_window
	
	// Σε αυτό το σημείο βρίσκω από πιο σημείο θα σχεδιάσω τα γράμματα των πλήκτρων που θα φαίνονται στον χρήστη
	int note_base = 60; // Κεντρική νότα Ντο του πιάνου σε μεσαία οκτάβα την οποία χρησιμοποιώ σαν βάση

	// Εαν μέχρι τα επόμενα 5.0 δευτερόλεπτα θα παιχτούν νότες
	if (list_size(notes_falling) > 0) {
		// Μεταβλητή που θα χρησιμοποιήσω για να εντοπίσω το σημείο που θα εμφανίσω τα γράματα στα πλήκτρα
		int min = 127; // Την αρχικοποιώ με 127 που είναι η ψηλότερη νότα του πιάνου

		// Πλήρης διάσχηση των νοτών που θα παιχτούν τα επόμενα 5sec για να βρώ από πιο σημείο θα ξεκινήσω να ζωγραφίζω τις νότες
		for (ListNode node = list_first(notes_falling);
			 node != LIST_EOF;
			 node = list_next(notes_falling, node)) {
				MidiEvent event = list_node_value(notes_falling, node);

				if ((int)event->key < min) {
					min = event->key; // Από όλες τις νότες της λίστας βρίσκω την χαμηλότερη
				}
			 }
		if (min < 127) { // Εαν εντόπισα έστω και μια νότα
			note_base = (min / 12) * 12; // Βρίσκω την οκτάβα στο πιάνο που καλύπτει και την χαμηλότερη νότα της λίστας
		}
	}

	// Ξανά πλήρης διάσχηση της λίστας για να σχεδιάσω τις νότες που θα παίφτουν τα επόμενα 5sec
	for (ListNode node = list_first(notes_falling);
		 node != LIST_EOF;
		 node = list_next(notes_falling, node)) {

			MidiEvent note = list_node_value(notes_falling, node);

			if (note->velocity == 0) { // Εαν η νότα δεν έχει ήχο την κάνουμε skip
				continue;
			}

			double time_diff = note->time - info->time; // Διαφορά χρόνου μεταξύ του χρόνου της νότας και του χρόνου που βρίσκετε το κομμάτι 
			
			if (time_diff < -0.2) { // Για να μην ξαναεμφανιστεί η νότα όταν μπει στο loop την κάνω skip
				continue;
			}

			int note_key = note->key; // Το πλήκτρο της νότας στο πιάνο
			int x = note_key * SCREEN_WIDTH / piano_keys; // Η θέση στον άξονα x που θα πέφτει η νότα πρέπει να έχει ακριβός την ίδια θέση με το πλήκτρο της νότας
			int note_width = (note_key + 1) * SCREEN_WIDTH / piano_keys - x; // Τα πάχος της νότας πρέπει να είναι όσο και το πάχος του πλήκτρου της

			// Η θέση y που όσο προχωρά ο χρόνος, η θέση θα μειώνετε δηλαδή η νότα θα πέφτει μέχρι να ακουμπήσει το πλήκτρο της
			int y = piano_keys_start_y - (int)((time_diff / time_window) * piano_keys_start_y); 

			int channel = (int)note->channel; // Το channel στο οποίο βρίσκετε η νότα

			// Ανάλογα με το channel που βρίσκετε το κομμάτι, οι νότες έχουν διαφορετικό χρώμα για να γνωρίζει ο χρήστης πότε αλλάζει το όργανο
			switch (channel) {
				case 0:
				DrawRectangle(x, y - 30, note_width, 30, BEIGE); // Σχεδιασμός νότας με βάση τα στοιχεία και τις συντεταγμένες που υπολόγισα πιο πάνω
				break;											 // Το 30 δεξιά του χρώματος είναι το μάκρος της νότας το οποίο είναι σταθερό αφού δεν έχω το duration της κάθε νότας

				case 1:
				DrawRectangle(x, y - 30, note_width, 30, YELLOW);
				break;

				case 2:
				DrawRectangle(x, y - 30, note_width, 30, RED);
				break;

				case 3:
				DrawRectangle(x, y - 30, note_width, 30, ORANGE);
				break;

				case 4:
				DrawRectangle(x, y - 30, note_width, 30, GREEN);
				break;

				case 5:
				DrawRectangle(x, y - 30, note_width, 30, BLUE);
				break;

				case 6:
				DrawRectangle(x, y - 30, note_width, 30, VIOLET);
				break;

				case 7:
				DrawRectangle(x, y - 30, note_width, 30, SKYBLUE);
				break;

				case 8:
				DrawRectangle(x, y - 30, note_width, 30, PINK);
				break;

				case 9:
				DrawRectangle(x, y - 30, note_width, 30, MAROON);
				break;

				case 10:
				DrawRectangle(x, y - 30, note_width, 30, BROWN);
				break;

				default:
				DrawRectangle(x, y - 30, note_width, 30, LIME);
				break;
			}

		 }

	DrawRectangle(0 ,0, GetScreenWidth(), 150, DARKGRAY); // Για να κρύψω τις νότες έτσι ώστε να μην φαίνοντε πριν να αρχίσει το παιχνίδι

	// Struct με περιεχόμενο τα στοιχεία των γραμμάτων
	typedef struct {
		char* letter; // χαρακτήρας
		int pos; // θέση πλήκτρου στο πιάνο
	}letters_on_keys;

	// Πίνακας που περιέχει τους χαρακτήρες και των 19 πλήκτρων που θα είναι συνεχώς έτοιμα να αναπαράξουν ήχο
	letters_on_keys letters[] = { 
		{"A", 0}, {"W", 1}, {"S", 2},
		{"E", 3}, {"D", 4}, {"F", 5},
		{"T", 6}, {"G", 7}, {"Y", 8},
		{"H", 9}, {"U", 10}, {"J", 11},
		{"K", 12}, {"O", 13}, {"L", 14},
		{"P", 15}, {";", 16}, {"'", 17},
		{"]", 18}, {"\\", 19}
	};

	// Σε αυτό το σημείο σχεδιάζω τα γράμματα των πλήκτρων που πρέπει να πατήσει ο χρήστης για να παίξει ένα πλήκτρο του πιάνο,
	// τα οποία γράμματα μετακινούνται με βάση την περιοχή που θα παιχτούν οι νότες, με βάση το original τραγούδι

	int num_keys = sizeof(letters) / sizeof(letters[0]); // Αριθμός των πλήκτρων
	int letters_size = 10; // Μέγεθος των γραμμάτων το οποίο πρέπει να είναι μικρό για να χωρέσουν μέσα στα πλήκτρα

	for (int k = 0 ; k < num_keys ; k++) {
		int key_note = note_base + letters[k].pos; // Εντοπίζω την νότα που αντιστηχεί κάθε πλήκτρο στην οκτάβα που βρίσκομαι

		int x = key_note * SCREEN_WIDTH / piano_keys; // Θέση x που θα ζωγραφιστεί το γράμμα η οποία πρέπει να σημβαδίζει με την θέση του πλήκτρου
		int x_next = (key_note + 1) * SCREEN_WIDTH / piano_keys; // Θέση που θα τελειώνει κάθε γράμμα με βάση το πλήτρο και θα αρχίζει το επόμενο
		int key_width = x_next - x; // Μεταξύ απόσταση των δύο πλήκτρων για το πάχος του πλήκτρου

		int text_size = MeasureText(letters[k].letter, letters_size); // Μέγεθος χαρακτήρων με την χρήση MeasureText
		int x_final = x + (key_width / 2) - (text_size / 2); // Τελική θέση x για το γράμμα έτσι ώστε να μπει ακριβός στο κέντρο του πλήκτρου για να φένετε καθαρά

		int y = piano_keys_start_y + piano_keys_height - 15; // Όλα τα γράμματα θα τοποθετηθούν στο ίδιο ύψος
		DrawText(letters[k].letter, x_final, y, letters_size, BLACK); // Σχεδιασμός γραμμάτων με βάση τις συντεταγμένες και τις πληροφορίες που υπολόγισα πιο πάνω
	}
	
	// Σχεδιάζουμε το σκορ και το FPS counter, τον χρόνο, το accuracy και το level, στο πάνω μέρος της οθόνης που θα είναι συνέχεια ορατό
	DrawText(TextFormat("Time: %.2f", info->time), 20, 20, 40, GOLD);
	DrawText(TextFormat("Score: %.2f", info->score), 320, 20, 40, GOLD);
	DrawText(TextFormat("Accuracy: %.2f", info->accuracy), 650, 20, 40, GOLD);
	DrawText(TextFormat("Level: %d", info->level), 1020, 20, 40, GOLD);
	DrawFPS(SCREEN_WIDTH - 80, 0);

	// Αν το παιχνίδι έχει τελειώσει, σχεδιάζουμε το μήνυμα για να ξαναρχίσει
	if (info->game_over == true) {
		DrawText(
			"      GAME OVER \nPRESS [R] TO PLAY AGAIN",
			 GetScreenWidth() / 2 - MeasureText("GAME OVER \n PRESS [ENTER] TO PLAY AGAIN", 20) / 2,
			 GetScreenHeight() / 2 - 50, 20, RED
		);
	}

	// Στην αρχή του παινιδιού και κάθε φορά που ο χρήστης κάνει pause το παιχνίδι, εμφανίζονται οι αρχικές επιλογές με τις οδηγίες
	// χρήσης του παιχνιδιού, δηλαδή πως κάνουμε pause, πως restart και πως προχωράμε frame
	if (info->paused == true) {
		DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.08f));

		int x_center = GetScreenWidth() / 3;
		int y_center = GetScreenHeight() / 3;

		DrawText("PRESS [SPACE] TO PLAY / PAUSE GAME", x_center, y_center, 20, GOLD);
		DrawText("PRESS [N] TO MOVE TO THE NEXT FRAME", x_center, y_center + 30, 20, GOLD);
		DrawText("PRESS [R] TO RESTART GAME", x_center, y_center + 60, 20, GOLD);

	}
	EndDrawing();
}
