all:
# 	gcc src/broker.c -o run/broker
# 	./run/broker &
# 	sleep 2
# 	./run/broker
	gcc -c src/publisher.c -o run/publisher.o
	gcc -c src/subscriber.c -o run/subscriber.o
	gcc -c src/broker.c -o run/broker.o
	gcc run/broker.o run/subscriber.o run/publisher.o -o run/broker
	./run/broker

p:
	gcc -c src/publisher.c -o run/publisher.o
	gcc -c src/subscriber.c -o run/subscriber.o
	gcc -c src/broker.c -o run/broker.o
	gcc -c src/main_pub.c -o run/main_pub.o

	gcc run/main_pub.o run/subscriber.o run/publisher.o -o run/main_pub
	./run/main_pub

s:
	gcc -c src/publisher.c -o run/publisher.o
	gcc -c src/subscriber.c -o run/subscriber.o
	gcc -c src/broker.c -o run/broker.o
	gcc -c src/main_sub.c -o run/main_sub.o

	gcc run/main_sub.o run/subscriber.o run/publisher.o -o run/main_pub
	./run/main_pub

.PHONY: all p