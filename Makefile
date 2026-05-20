BUILD_DIR = build

WEB_BUILD_DIR = $(BUILD_DIR)/web
RAYLIB_WEB_PATH = ./raylib-web
RAYLIB_WEB_FLAGS = $(RAYLIB_WEB_PATH)/lib/libraylib.a -I$(RAYLIB_WEB_PATH)/include
BUILD_WEB_SHELL       ?= minshell.html

build: 
	mkdir -p $(BUILD_DIR)
	rm -rf $(BUILD_DIR)/fluid_simulation
	gcc -o $(BUILD_DIR)/fluid_simulation main.c $$(pkg-config --cflags --libs raylib)

run: build
	$(BUILD_DIR)/fluid_simulation

build-web:
	mkdir -p $(WEB_BUILD_DIR)
	emcc -o $(WEB_BUILD_DIR)/fluid_simulation.html main.c -Os -Wall -DPLATFORM_WEB \
		$(RAYLIB_WEB_FLAGS) -sUSE_GLFW=3 -sASYNCIFY -sFORCE_FILESYSTEM=1 -sMINIFY_HTML=1 \
		--shell-file $(BUILD_WEB_SHELL)
		

build-web-deploy: build-web
	rm -rf ./docs
	cp -r $(WEB_BUILD_DIR) ./docs
	cp $(WEB_BUILD_DIR)/fluid_simulation.html ./docs/index.html
	rm -rf ./docs/fluid_simulation.html