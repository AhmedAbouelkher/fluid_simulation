BUILD_DIR = build

build: clean
	mkdir -p $(BUILD_DIR)
	gcc -o $(BUILD_DIR)/fluid_simulation main.c $$(pkg-config --cflags --libs raylib)

run: build
	$(BUILD_DIR)/fluid_simulation

clean:
	rm -rf $(BUILD_DIR)