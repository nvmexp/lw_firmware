all: $(patsubst %.adoc, %.pdf, $(shell find . -name '*.adoc'))

%.pdf: %.html
	xvfb-run wkhtmltopdf --enable-internal-links -B 25 -L 25 -R 25 -T 25 $< $@ 

%.html: %.adoc 
	asciidoctor $<

.PHONY: clean
clean:
	rm -rf libos.html libos.pdf
