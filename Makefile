#
# This is the Makefile that can be used to create the server/client executable
# To create server/client executable, do:
#	make
#
all : serverM serverA serverB serverC clientA clientB


serverM: serverM.c
	gcc -Wall -o serverM -g serverM.c

serverA: serverA.c
	gcc -Wall -o serverA -g serverA.c

serverB: serverB.c
	gcc -Wall -o serverB -g serverB.c

serverC: serverC.c
	gcc -Wall -o serverC -g serverC.c

clientA: clientA.c
	gcc -Wall -o clientA -g clientA.c

clientB: clientB.c
	gcc -Wall -o clientB -g clientB.c

clean:
	rm -f serverM serverA serverB serverC clientA clientB