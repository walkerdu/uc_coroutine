src = client.cpp access.cpp server.cpp
bin = $(subst .cpp,, $(src))

.PHONY: all

all : $(bin) 

$(bin): msg.pb.cc $(src) 
	g++ $@.cpp msg.pb.cc -g -fpermissive -std=c++11 \
		-I /usr/local/protobuf-3.2.0/include /usr/local/protobuf-3.2.0/lib/libprotobuf.a \
		-o $@

msg.pb.cc : msg.proto
	protoc --cpp_out=. $<

.PHONY : clean
clean:
	rm msg.pb* $(bin) 
