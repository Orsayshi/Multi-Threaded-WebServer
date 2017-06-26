all: myhttpd.cpp
	gcc -O myhttpd.cpp -o myhttpd -pthread

clean:
	$(RM) myhttpd

