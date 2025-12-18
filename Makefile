all: main

main:
	ya -b main


run: main
	ya -r main

clean:
	rm -rf build