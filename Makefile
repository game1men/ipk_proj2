build:	ipkcpc.c
	gcc -std=gnu99 ipkcpc.c -o ipkcpc
test: ipkcpc test.py
	python3 test.py

clean:
	rm ipkcpc

