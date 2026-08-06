# Compiler
CC = x86_64-w64-mingw32-gcc

# Output executable name
TARGET = IEC61850-Scraper.exe

# Directories
INCLUDES = -Iincludes/ -Iheaders/
LDFLAGS = -Llibs/ -L. -static -static-libgcc

# Libraries
LIBS = -liec61850 -lhal -lxlsxwriter -lzs -liphlpapi -lws2_32 -lbcrypt -lraylib -lgdi32 -lwinmm

# Source files
SRCS = src/main.c \
       src/iec61850_helpers.c \
       src/xlsx_helpers.c \
       src/networking.c \
       src/gui.c \
       src/userpath_helpers.c \
       src/jsmn.c

# Object files (replaces .c with .o)
OBJS = $(SRCS:.c=.o)

# Default rule: builds the target
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LIBS)

# Rule to compile .c files into .o files
%.o: %.c
	$(CC) $(INCLUDES) -c $< -o $@

# Rule to clean up build files (run 'make clean')
clean:
	rm -f $(OBJS) $(TARGET)