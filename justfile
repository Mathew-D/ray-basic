# ray-basic — run any recipe from the repo root.
# Recipes wrap `nix develop`, so no manual shell setup is needed.

default:
    @just --list

# Build the game binary
build:
    nix develop -c make

# Build (if needed) and run the game
run: build
    nix develop -c ./game

# Remove the built binary
clean:
    nix develop -c make clean
