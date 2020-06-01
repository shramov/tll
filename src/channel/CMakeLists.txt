set(CHANNEL_SOURCES
	impl.c
	context.cc
	mem.cc
	timer.cc
)

add_library(channel OBJECT ${CHANNEL_SOURCES})
