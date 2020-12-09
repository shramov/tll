ALL = $(wildcard *.rst)

all: html pdf
html: $(ALL:.rst=.html)
pdf: $(ALL:.rst=.pdf)
clean::
	-rm -f $(ALL:.rst=.html) $(ALL:.rst=.pdf)

%.pdf: %.rst
	rst2pdf -s dejavu $< -o $@
%.html: %.rst tll.css
	rst2html --stylesheet=tll.css $< $@
