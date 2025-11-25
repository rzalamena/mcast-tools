all: mcast-join mcast-query

mcast-join: mcast-join.c
	$(CC) -Wall -Wextra -g $< -o $@

mcast-query: mcast-query.c
	$(CC) -Wall -Wextra -g $< -o $@
