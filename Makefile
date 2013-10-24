cpp2rtf.rtf: cpp2rtf cpp2rtf.cpp
	./cpp2rtf < cpp2rtf.cpp > cpp2rtf.rtf 2> debug.txt

cpp2rtf: cpp2rtf.cpp
	g++ -std=c++11 -stdlib=libc++ -g -o cpp2rtf cpp2rtf.cpp

