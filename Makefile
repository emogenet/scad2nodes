
src := $(wildcard *.scad)
csg := $(patsubst %.scad,%.csg,${src})
py := $(patsubst %.csg,%.py,${csg})

all: ${py} z.py
	@echo ready.

csg: ${csg}

z.py:hull.py
	cp hull.py z.py

%.csg : %.scad
	openscad -o $@ $<

%.py : %.csg parse
	./parse <$< >$@ || rm -f $@

parse : parse.cpp
	c++ -o parse -g parse.cpp

clean:
	rm -f ${csg} ${py} parse z.py

