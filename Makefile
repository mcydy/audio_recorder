CC = arm-linux-gcc
CFLAGS = -Wall

alsa_recorder:alsa_recorder.c head4audio.h
	$(CC) -o $@ $< $(CFLAGS)

clean:
	$(RM) alsa_recorder .*.sw? core