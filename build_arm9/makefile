SDK            := /home/zivku/git/som1_ame_202404/buildroot-mchp/output/host
CXX            := $(SDK)/bin/arm-buildroot-linux-gnueabihf-g++ -std=c++17
SRC_DIR        := ../src
EGT_CXXFLAGS   := $(shell $(SDK)/bin/pkg-config libegt --cflags)
EGT_LDFLAGS    := $(shell $(SDK)/bin/pkg-config libegt --libs)
#JPEG_CXXFLAGS   := $(shell $(SDK)/bin/pkg-config libjpeg --cflags)
#JPEG_LDFLAGS    := $(shell $(SDK)/bin/pkg-config libjpeg --libs)
LIBS           := -lplanes -ljpeg

SRC            := $(SRC_DIR)/*
TARGET         := koli

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EGT_CXXFLAGS) $^ -o $@ $(EGT_LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)
