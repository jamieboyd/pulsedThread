NAME := pulsedThread
MAJOR := 0
MINOR := 1
INSTALLPATH :=/usr/local/lib
LINKPATH := /usr/lib
HEADERPATH := /usr/include

CC := g++
CFLAGS := -c -O3 -std=gnu++11 -fPIC -Wall
LDFLAGS := -lpthread -shared

VERSION := v$(MAJOR).$(MINOR)
TARGET_LIB := $(NAME)_$(VERSION).so

SOURCES :=pulsedThread.cpp
HEADERS :=pulsedThread.h pyPulsedThread.h
OBJECTS :=$(SOURCES:.cpp=.o)

all: $(SOURCES) $(TARGET_LIB) 
    
$(TARGET_LIB): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	
greeter:
	$(CC) -O3 -std=gnu++11 -lpulsedThread Greeter.cpp -o Greeter
	
minimalGreeter:
	$(CC) -O3 -std=gnu++11 -lpulsedThread minimalGreeter.cpp -o MinimalGreeter

clean:
	rm -f  $(OBJECTS)
	rm -f $(TARGET_LIB)
	rm -f Greeter

build: all

install: build
	cp $(TARGET_LIB) $(INSTALLPATH)/
	ln -sf $(INSTALLPATH)/$(TARGET_LIB)  $(INSTALLPATH)/lib$(NAME).so
	ln -sf $(INSTALLPATH)/$(TARGET_LIB)  $(LINKPATH)/lib$(NAME).so
	cp $(HEADERS) $(HEADERPATH)/
