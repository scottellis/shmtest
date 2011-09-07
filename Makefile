CC = gcc

CFLAGS = -Wall

LIBS = -lrt

TARGET = shmtest

$(TARGET) : shmtest.c
	$(CC) $(CFLAGS) $(LIBS) shmtest.c -o $(TARGET)
	
	
clean:
	rm -f $(TARGET)
