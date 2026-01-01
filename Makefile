all: main playground mqjs
mqjs:
	ya -b mqjs
	ya -r mqjs

main:
	ya -b main
	ya -r main

playground:
	ya -b playground
	ya -r playground

run: main
	ya -r main

clean:
	rm -rf build