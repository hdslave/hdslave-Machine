CXX ?= c++
CXXFLAGS ?= -O3
OBJECTS = build/machine_ui.o build/context.o
all: build/libmachine_ui.a
build/%.o: %.cpp machine_ui.h raster.h fonts.h art.h
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -c $< -o $@
build/libmachine_ui.a: $(OBJECTS)
	$(AR) rcs $@ $^
clean:
	rm -rf build
.PHONY: all clean
