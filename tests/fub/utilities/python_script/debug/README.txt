This file explains the usage of load_fub_on_sec2_from_headers_script.py


Platform requirements:

1. Linux distro is required preferred would be any Debian system(Testing was done for TInyLinux, Debian system only).
2. Python-3.5+ should be installed
3. Numpy package of python should be installed.
4. The GPUs need to be available in /sys/bus/pci/devices/…
5.  /dev/mem needs to be mappable.


Step to run python script:

sudo python3 ./load_fub_on_sec2_from_headers_script.py --gpu 0 --reset-with-sbr --update-fuses --log debug --fub-ucode-headers
g_acruc_tu10x_ahesasc_dbg.h g_acruc_tu10x_ahesasc_tu102_aes_sig.h


-reset-with-sbr : hot reset gpu before loading fub on sec2.
--update-fuses: loads fub on sec2 falcon.
--fub-ucode-headers: fub code is fetch friom *_dbg.h and signature is fetch from *_sig.h 





