SIGGEN CFG FILE FORMAT RULES

A) Adding a new entry in *.cfg file corresponding to a HS Overlay

   Rules:

   1) While adding a new Overlay entry please follow same sequence order as that followed in uproc/disp/dpu/build/OverlaysImem.pm

   2) Do not add entry for Overlays that do not get compiled ie. which have __overlay_id as 0

