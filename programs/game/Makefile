# paths
LIB = ../../lib
INCLUDE = ../../include

# compiler
CC = gcc

# Compile options. Το -I<dir> λέει στον compiler να αναζητήσει εκεί include files
CFLAGS = -Wall -Werror -g -I$(INCLUDE)
LDFLAGS = -lm

# Αρχεία .o
OBJS = game.o ../../modules/state.o ../../modules/interface.o $(LIB)/k08.a $(LIB)/k08midi.a $(LIB)/libraylib.a
# Για να τρέξει το εναλακτικό version του state
OBJS_alt = game.o ../../modules/state_alt.o ../../modules/interface.o $(LIB)/k08.a $(LIB)/k08midi.a $(LIB)/libraylib.a

# Το εκτελέσιμο πρόγραμμα
EXEC = game
EXEC_alt = game_alt

# Παράμετροι για δοκιμαστική εκτέλεση
ARGS =

# Παράμετροι της βιβλιοθήκης raylib
include $(LIB)/libraylib.mk


all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)

all_alt: $(EXEC_alt)

$(EXEC_alt): $(OBJS_alt)
	$(CC) $(OBJS_alt) -o $(EXEC_alt) $(LDFLAGS)

# Για να φτιάξουμε τα k08.a/libraylib.a τρέχουμε το make στο lib directory.
$(LIB)/%.a:
	$(MAKE) -C $(LIB) $*.a

clean:
	rm -f $(OBJS) $(EXEC)

clean_alt:
	rm -f $(OBJS_alt) $(EXEC_alt)

run: $(EXEC)
	./$(EXEC) $(ARGS)

run_alt: $(EXEC_alt)
	./$(EXEC_alt) $(ARGS)

valgrind: $(EXEC)
	valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all ./$(EXEC) $(ARGS)

valgrind_alt: $(EXEC_alt)
	valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all ./$(EXEC_alt) $(ARGS)
