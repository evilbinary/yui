all: yui

yui:
	ya -b yui


run: yui
	ya -r yui

clean:
	rm -rf build