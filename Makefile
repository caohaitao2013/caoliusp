CC:=g++
CFLAGS:=-Wall -g
SRC:=$(wildcard *.cpp)
OBJ:=$(SRC:.cpp=.o)
LIB:=boost_system boost_date_time pthread

TARGET:= caoliusp

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(addprefix -l,$(LIB))

%.o: %.cpp
	$(CC) $(CFLAGS) -c $^

clean:
	$(RM) $(TARGET) $(OBJ)
