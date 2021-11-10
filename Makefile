LOCAL_CFLAGS	?= $(CFLAGS) -std=gnu99 -Os -Wall
LOCAL_CXXFLAGS	?= $(CXXFLAGS) -std=gnu++11 -Os -Wall

VERSION ?= $(shell git describe --always)

ifeq ($(OS),Windows_NT)
	TARGET_OS := WINDOWS
	DIST_SUFFIX := windows
	ARCHIVE_CMD := 7z a
	ARCHIVE_EXTENSION := zip
	TARGET := mklfs.exe
	TARGET_CFLAGS := $(CFLAGS) -mno-ms-bitfields -Ilfs -I. -DVERSION=\"$(VERSION)\" -D__NO_INLINE__
	TARGET_LDFLAGS := $(LDFLAGS) -Wl,-static -static-libgcc
	TARGET_CXXFLAGS := $(CXXFLAGS) -Ilfs -I. -DVERSION=\"$(VERSION)\" -D__NO_INLINE__
	CC ?= gcc
	CXX ?= g++
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		TARGET_OS := LINUX
		UNAME_P := $(shell uname -p)
		ifeq ($(UNAME_P),x86_64)
			DIST_SUFFIX := linux64
		endif
		ifneq ($(filter %86,$(UNAME_P)),)
			DIST_SUFFIX := linux32
		endif
		CC ?= gcc
		CXX ?= g++
		TARGET_CFLAGS   = $(LOCAL_CFLAGS) -Ilfs -I. -D$(TARGET_OS) -DVERSION=\"$(VERSION)\" -D__NO_INLINE__
		TARGET_CXXFLAGS = $(LOCAL_CXXFLAGS) -Ilfs -I. -D$(TARGET_OS) -DVERSION=\"$(VERSION)\" -D__NO_INLINE__
	endif
	ifeq ($(UNAME_S),Darwin)
		TARGET_OS := OSX
		DIST_SUFFIX := osx
		CC ?= clang
		CXX ?= clang++
		TARGET_CFLAGS   = $(LOCAL_CFLAGS) -Ilfs -I. -D$(TARGET_OS) -DVERSION=\"$(VERSION)\" -D__NO_INLINE__ -mmacosx-version-min=10.7 -arch x86_64
		TARGET_CXXFLAGS = $(LOCAL_CXXFLAGS) -Ilfs -I. -D$(TARGET_OS) -DVERSION=\"$(VERSION)\" -D__NO_INLINE__ -mmacosx-version-min=10.7 -arch x86_64 -stdlib=libc++
		TARGET_LDFLAGS  = $(LDFLAGS) -arch x86_64 -stdlib=libc++
	endif
	ARCHIVE_CMD := tar czf
	ARCHIVE_EXTENSION := tar.gz
	TARGET := mklfs
endif

SRCS            := mklfs.c \
                   lfs/lfs.c \
                   lfs/lfs_util.c
OBJS            := $(SRCS:.c=.o)

VERSION ?= $(shell git describe --always)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(TARGET_CFLAGS) -o $(TARGET) $(OBJS) $(TARGET_LDFLAGS)

%.o: %.c Makefile
	$(CC) $(TARGET_CFLAGS) -c $< -o $@

clean:
	@rm -f $(OBJS)
	@rm -f $(TARGET)
